// Fill out your copyright notice in the Description page of Project Settings.


#include "SandboxCharacter_Mover.h"
#include "DirectionRelative.h"
#include "SandboxCharacter_CMC.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "ChooserFunctionLibrary.h"
#include "MoverComponent.h"

// Sets default values
ASandboxCharacter_Mover::ASandboxCharacter_Mover()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ASandboxCharacter_Mover::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASandboxCharacter_Mover::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ASandboxCharacter_Mover::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}


#pragma region 暴露在蓝图的NPC避障节点和碰撞交互节点

	void ASandboxCharacter_Mover::UpdateObstacleAvoidance(FVector2D IA_Move, UCapsuleComponent* PlayerCapsule)
	{
		CheckRightWall();									// then_0 右墙检查
		CheckLeftWall();									// then_1 左墙检查
		UpdateTraceSideInput(IA_Move);						// then_2 平滑玩家输入轨迹方向以驱动探测方向偏移量
		UpdateCapsuleRadiusFromAvoidance(PlayerCapsule);	// then_3 更新胶囊体半径
		ResetAvoidanceState(IA_Move);						// then_4 当附近没有墙时重置规避
	}

	void ASandboxCharacter_Mover::NPCImpactReaction(AActor* OtherActor)
	{
		if (!CanTriggerImpactReaction(OtherActor))	// then_0：条件判断
		{
			return;
		}
	
		GetPlayerAngleRelativeToNPC();				//then_1：计算玩家相对于NPC的角度信息
		GetNPCAngleRelativeToPlayer();				//then_1：计算NPC相对于玩家的角度信息
	
		GetPlayerDirectionRelativeToNPC();			//then_2：计算玩家属于NPC的哪个方向
		GetNPCDirectionRelativeToPlayer();			//then_2：计算NPC属于玩家的哪个方向
	
		GetPlayerMoverSpeed();						//then_3：角色移动速度
	
		SetImpactReactionMontage();					//then_4：用选择表选择蒙太奇资产
		PlayReactionMontages();						//then_4：播放玩家和NPC的碰撞交互动画蒙太奇
	}

#pragma endregion


void ASandboxCharacter_Mover::BuildSideAvoidanceTrace(FVector& Start, FVector& End, bool IsRight)
{
	float SignedFactor = (IsRight)? 1 : -1;
	//起始偏移：前方*1+右*20（就在身体旁边）
	Start = (IsRight)?
					GetActorLocation() + GetActorForwardVector() + GetActorRightVector() * 20
				:	GetActorLocation() + GetActorForwardVector() + GetActorRightVector() * (-20);
	//结束偏移：前进*缩放+右*缩放（右侧空旷）
	End = (IsRight)?
				GetActorLocation()
				+ GetActorForwardVector() * UKismetMathLibrary::MapRangeClamped(TraceSideInput, 0, 1,56,1)
				+ GetActorRightVector() * UKismetMathLibrary::MapRangeClamped(TraceSideInput, 0, 1,50,120)
			:	GetActorLocation()
				+ GetActorForwardVector() * UKismetMathLibrary::MapRangeClamped(TraceSideInput, 0, -1,56,1)
				+ GetActorRightVector() * UKismetMathLibrary::MapRangeClamped(TraceSideInput, 0, 1,-50,-120);
}

float ASandboxCharacter_Mover::MapDistanceToAlpha(const FVector& TraceImpactPoint, float MinDistance, float MaxDistance,
	float MinAlpha, float MaxAlpha)
{
	return UKismetMathLibrary::MapRangeClamped(
		FVector::Distance(TraceImpactPoint,GetActorLocation()),
		MinDistance,
		MaxDistance,
		MinAlpha,
		MaxAlpha
		);
}

void ASandboxCharacter_Mover::CheckSideWall(bool IsRight, EDrawDebugTrace::Type DrawDebugType)
{
	//	1.防止两面墙同时互相争斗
	if (IsRight)
	{
		//只有当左墙还没被撞到时才走右线,如果左墙目前被击中→跳过并设置 IsRightWallDetected = false。
		if (IsLeftWallDetected)
		{
			IsRightWallDetected = false;
			return;
		}
	}
	else
	{
		if (IsRightWallDetected)
		{
			IsLeftWallDetected = false;
			return;
		}
	}

	//	2.执行球体射线
	FVector Start, End;
	if (IsRight)
	{
		BuildSideAvoidanceTrace(Start, End, true);
	}
	else
	{
		BuildSideAvoidanceTrace(Start, End, false);
	}
	FHitResult OutHit;
	bool IsSideWallDetected = UKismetSystemLibrary::SphereTraceSingle(
		this,
		Start,
		End,
		25,
		TraceTypeQuery1,
		false,
		TArray<AActor*>(),
		DrawDebugType,
		OutHit,
		true,
		FLinearColor::White,
		FLinearColor::Green,
		5.0f
	);
	if (IsRight)
	{
		IsRightWallDetected = IsSideWallDetected;
	}
	else
	{
		IsLeftWallDetected = IsSideWallDetected;
	}

	//3.如果击中：测量角色到撞击点的距离,地图距离 [120cm → 40cm] 到 SideAvoidDistanceBased [0 → ±1]（1：向右，-1：向左），越近，结果越大，意味着转弯动作更强。
	if (IsSideWallDetected)
	{
		ImpactLocation = OutHit.ImpactPoint;
		HitActor = OutHit.GetActor();
		//角色到撞击点的距离映射为0到±1的避开Alpha值
		AvoidanceAlphaSigned = (IsRight)?
									MapDistanceToAlpha(ImpactLocation, 120,40,0,1)
								:	MapDistanceToAlpha(ImpactLocation, -120,40,0,-1);
	}
}


void ASandboxCharacter_Mover::UpdateTraceSideInput(FVector2D IA_Move)
{
	TraceSideInput = UKismetMathLibrary::FInterpTo(TraceSideInput, IA_Move.X, TickDeltaTime,5);
}

void ASandboxCharacter_Mover::UpdateCapsuleRadiusFromAvoidance(UCapsuleComponent* PlayerCapsule)
{
	float Radius = UKismetMathLibrary::MapRangeClamped(abs(AvoidanceAlphaSigned),1,0,20,30);
    PlayerCapsule->SetCapsuleRadius(Radius);
}

void ASandboxCharacter_Mover::ResetAvoidanceState(FVector2D IA_Move)
{
	//如果玩家没有输入或者左右检测都没有击中
	if ( (IA_Move.X ==0 && IA_Move.Y == 0) | (!IsRightWallDetected && !IsLeftWallDetected) )
	{
		//重置Avoidance权重
		AvoidanceAlphaSigned = UKismetMathLibrary::FInterpTo(AvoidanceAlphaSigned,0,TickDeltaTime,3);
		//重置左右检测标志
		IsRightWallDetected = false;
		IsLeftWallDetected = false;
	}
}

bool ASandboxCharacter_Mover::CanTriggerImpactReaction(AActor* Other)
{
	//触发器是否是ASandboxCharacter_CMC类型的，不是就不能触发碰撞交互
	NPCRef = Cast<ASandboxCharacter_CMC>(Other);
	if (!NPCRef)
	{
		return false; // Cast Failed
	}

	USkeletalMeshComponent* Mesh = NPCRef->GetMesh();
	if (!Mesh)
	{
		return false;
	}

	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	if (!AnimInstance)
	{
		return false; // 没有动画实例
	}
	
	if (!AnimInstance->IsAnyMontagePlaying())
	{
		return true;	//没有有蒙太奇在播放
	}

	// 正在播：进度过半就允许
	UAnimMontage* CurMontage = AnimInstance->GetCurrentActiveMontage();
	if (!CurMontage) return false;

	const float Position = AnimInstance->Montage_GetPosition(CurMontage);
	const float Length   = CurMontage->GetPlayLength(); 

	if (Length <= KINDA_SMALL_NUMBER) return false;

	return (Position / Length) >= 0.5f;
}

void ASandboxCharacter_Mover::GetPlayerMoverSpeed()
{
	UMoverComponent* CharactorMover = GetComponentByClass<UMoverComponent>();
	if (!CharactorMover)
	{
		return;
	}
	
	PlayerMoverSpeed = CharactorMover->GetVelocity().Length();
}

void ASandboxCharacter_Mover::PlayAvoidanceImpactSound()
{
	USoundBase* SelectedSound;
	SelectedSound = (PlayerMoverSpeed > 200)? HeavyImpactSound: LightImpactSound;
	UGameplayStatics::PlaySoundAtLocation(this, SelectedSound, ImpactLocation);
}

void ASandboxCharacter_Mover::PlayPlayerHitMontage()
{
	USkeletalMeshComponent* PlayerMesh = GetComponentByClass<USkeletalMeshComponent>();
	if (!PlayerMesh || !PlayerMesh->GetAnimInstance() || !SelectedHitMontage)
	{
		return;
	}
	
	PlayerMesh->GetAnimInstance()->Montage_Play(SelectedHitMontage);
}

void ASandboxCharacter_Mover::PlayNPCImpactedMontage()
{
	if (!SelectedImpactedMontage)
	{
		return;
	}

	//调用NPC的受击事件
	NPCRef->PlayNPCImpactedMontage(SelectedImpactedMontage);
}

void ASandboxCharacter_Mover::GetAngleRelative(AActor* Actor1, AActor* Actor2, float& AngleRelative)
{
	if (!Actor1 || !Actor2)
	{
		return;
	}
	//玩家相对于NPC的位置
	FVector Actor1LocationRelativeToActor2 = UKismetMathLibrary::InverseTransformLocation(Actor1->GetActorTransform(), Actor2->GetActorLocation());
	//玩家相对于NPC的水平方向角度：-180 ~ 180
	AngleRelative = UKismetMathLibrary::DegAtan2(Actor1LocationRelativeToActor2.X, Actor1LocationRelativeToActor2.Y);
}

void ASandboxCharacter_Mover::GetDirectionRelative(float& AngleRelative, EDirectionRelative& DirectionRelative)
{
	// ====================== 8方向判断逻辑 ======================
	DirectionRelative = EDirectionRelative::Front;
	// 右：-15 ~ 15
	if (AngleRelative >= -15.0f && AngleRelative <= 15.0f)
		DirectionRelative = EDirectionRelative::Right;
	// 前右：15 ~ 75
	if (AngleRelative > 15.0f && AngleRelative <= 75.0f)
		DirectionRelative = EDirectionRelative::FrontRight;
	// 前：75 ~ 105
	if (AngleRelative > 75.0f && AngleRelative <= 105.0f)
		DirectionRelative = EDirectionRelative::Front;
	// 前左：105 ~ 165
	if (AngleRelative > 105.0f && AngleRelative <= 165.0f)
		DirectionRelative = EDirectionRelative::FrontLeft;
	// 左：165 ~ 180  或  -180 ~ -165
	if ((AngleRelative > 165.0f && AngleRelative <= 180.0f) || (AngleRelative >= -180.0f && AngleRelative <= -165.0f))
		DirectionRelative = EDirectionRelative::Left;
	// 后左：-165 ~ -105
	if (AngleRelative > -165.0f && AngleRelative <= -105.0f)
		DirectionRelative = EDirectionRelative::BackLeft;
	// 后：-105 ~ -75
	if (AngleRelative > -105.0f && AngleRelative <= -75.0f)
		DirectionRelative = EDirectionRelative::Back;
	// 后右：-75 ~ -15
	if (AngleRelative > -75.0f && AngleRelative < -15.0f)
		DirectionRelative = EDirectionRelative::BackRight;
}

void ASandboxCharacter_Mover::SetImpactReactionMontage()
{
	//结构体
	SetHitStructure();
	SetImpactedStructure();

	//选择表选择蒙太奇
	SelectedHitMontage = SelectMontage(CT_HitMontage, HitStructure);
	SelectedImpactedMontage = SelectMontage(CT_ImpactedMontage, ImpactedStructure);
}

void ASandboxCharacter_Mover::SetHitStructure()
{
	HitStructure.PlayerHitDirection = NPCDirectionRelativeToPlayer;
	HitStructure.PlayerSpeedAtImpact = PlayerMoverSpeed;
}

void ASandboxCharacter_Mover::SetImpactedStructure()
{
	ImpactedStructure.NPCImpactedDirection = PlayerDirectionRelativeToNPC;
	ImpactedStructure.PlayerSpeedAtImpact = PlayerMoverSpeed;
}

template <typename TStruct>
UAnimMontage* ASandboxCharacter_Mover::SelectMontage(UChooserTable* MontageChooserTable, TStruct& StructParams)
{
	// 创建上下文
	FChooserEvaluationContext EvalContext = UChooserFunctionLibrary::MakeChooserEvaluationContext();
	EvalContext.AddStructParam(StructParams);
	FInstancedStruct ObjectChooser = UChooserFunctionLibrary::MakeEvaluateChooser(MontageChooserTable);
	//选择表返回的蒙太奇对象
	UObject* SelectedMontageObject = UChooserFunctionLibrary::EvaluateObjectChooserBase(
		EvalContext,
		ObjectChooser,
		UAnimMontage::StaticClass(),
		/*bResultIsClass*/ false
	);
	
	//选择蒙太奇
	return Cast<UAnimMontage>(SelectedMontageObject);
}

void ASandboxCharacter_Mover::PlayReactionMontages()
{
	if (PlayerMoverSpeed < 100)
	{
		return;
	}
	
	//播放玩家Hit动画蒙太奇
	PlayPlayerHitMontage();
	
	//播放撞击声
	PlayAvoidanceImpactSound();
	
	//暂时缩小NPC胶囊体半径
	NPCRef->GetCapsuleComponent()->SetCapsuleRadius(15);
	
	//播放NPC的Impacted动画蒙太奇
	PlayNPCImpactedMontage();
}



