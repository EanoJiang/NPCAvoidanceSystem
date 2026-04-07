// Fill out your copyright notice in the Description page of Project Settings.


#include "SandboxCharacter_Mover.h"

#include "SandboxCharacter_CMC.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Interfaces/IHttpBase.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Runtime/Datasmith/CADKernel/Base/Public/Geo/GeoEnum.h"


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

bool ASandboxCharacter_Mover::CanTriggerNPCImpactReaction(AActor* Other)
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

	// 正在播：进度过大半就允许
	UAnimMontage* CurMontage = AnimInstance->GetCurrentActiveMontage();
	if (!CurMontage) return false;

	const float Position = AnimInstance->Montage_GetPosition(CurMontage);
	const float Length   = CurMontage->GetPlayLength(); 

	if (Length <= KINDA_SMALL_NUMBER) return false;

	return (Position / Length) >= 0.7f;
}

float ASandboxCharacter_Mover::GetRightOffsetDistance(AActor* NPCReference, float NPCHorizontalOffset,
	AActor* PlayerReference, float PlayerHorizontalOffset)
{
	if (NPCReference == nullptr || PlayerReference == nullptr)
	{
		return 0;
	}
	
	//基于NPC的右侧
	FVector RightLocationOfNPC = NPCReference->GetActorLocation() + NPCReference->GetActorRightVector() * NPCHorizontalOffset;
	//基于Player的右侧
	FVector RightLocationOfPlayer = PlayerReference->GetActorLocation() + PlayerReference->GetActorRightVector() * PlayerHorizontalOffset;
	return FVector::Distance(RightLocationOfNPC, RightLocationOfPlayer);
}

float ASandboxCharacter_Mover::GetPlayerMoverSpeed(UMoverComponent* Mover)
{
	return Mover->GetVelocity().Length();
}

UAnimMontage* ASandboxCharacter_Mover::SelectAvoidanceImpactedAnimMontage()
{
	TArray<UAnimMontage*> SelectedAnimMontages;
	// SelectedAnimMontages = (IsAtNPCRightSide)? RightAvoidanceImpactedAnimMontageArray: LeftAvoidanceImpactedAnimMontageArray;

	switch ( GetDirectionRelativeToNPC() )
	{
	case EDirectionRelativeToNPC::LeftFront:
		SelectedAnimMontages = LeftAvoidanceImpactedAnimMontageArray;
		break;
	case EDirectionRelativeToNPC::RightFront:
		SelectedAnimMontages = RightAvoidanceImpactedAnimMontageArray;
		break;
	case EDirectionRelativeToNPC::LeftBack:
		SelectedAnimMontages = RightAvoidanceImpactedAnimMontageArray;
		break;
	case EDirectionRelativeToNPC::RightBack:
		SelectedAnimMontages = RightAvoidanceImpactedAnimMontageArray;
		break;
	default:
		SelectedAnimMontages = TArray<UAnimMontage*>();
		break;
	}
	int RandomIndex = UKismetMathLibrary::RandomIntegerInRange(0,SelectedAnimMontages.Num() - 1);
	return SelectedAnimMontages[RandomIndex];
}

void ASandboxCharacter_Mover::PlayAvoidanceImpactSound()
{
	USoundBase* SelectedSound;
	SelectedSound = (PlayerMoverSpeed > 200)? AvoidanceImpactSound: AvoidanceLightImpactSound;
	UGameplayStatics::PlaySoundAtLocation(this, SelectedSound, ImpactLocation);
}

void ASandboxCharacter_Mover::PlayPlayerAvoidanceHitMontage(USkeletalMeshComponent* Mesh)
{
	if (!Mesh || !Mesh->GetAnimInstance())
	{
		return;
	}
	
	//选择撞击蒙太奇
	UAnimMontage* SelectedMontage;
	
	switch ( GetDirectionRelativeToNPC() )
	{
		case EDirectionRelativeToNPC::LeftFront:
			SelectedMontage = (PlayerMoverSpeed > 200)? AvoidanceHit_L_Montage: AvoidanceLightHit_L_Montage;
		break;
		case EDirectionRelativeToNPC::RightFront:
			SelectedMontage = (PlayerMoverSpeed > 200)? AvoidanceHit_R_Montage: AvoidanceLightHit_R_Montage;
		break;
		case EDirectionRelativeToNPC::LeftBack:
			SelectedMontage = (PlayerMoverSpeed > 200)? AvoidanceHit_R_Montage: AvoidanceLightHit_R_Montage;
		break;
		case EDirectionRelativeToNPC::RightBack:
			SelectedMontage = (PlayerMoverSpeed > 200)? AvoidanceHit_L_Montage: AvoidanceLightHit_L_Montage;
		break;
		default:
			SelectedMontage = nullptr;
		break;
	}
	
	// 蒙太奇有效才播放
	if (SelectedMontage)
	{
		Mesh->GetAnimInstance()->Montage_Play(SelectedMontage);
	}
}

float ASandboxCharacter_Mover::GetAngleRelativeToNPC()
{
	if (NPCRef == nullptr)
	{
		return 0;
	}
	//玩家相对于NPC的位置
	FVector PlayerLocationRelativeToNPC = UKismetMathLibrary::InverseTransformLocation(NPCRef->GetActorTransform(), GetActorLocation());
	//玩家相对于NPC的水平方向角度
	float PlayerAngleRelativeToNPC = UKismetMathLibrary::DegAtan2(PlayerLocationRelativeToNPC.X, PlayerLocationRelativeToNPC.Y);

	return PlayerAngleRelativeToNPC;
}

EDirectionRelativeToNPC ASandboxCharacter_Mover::GetDirectionRelativeToNPC()
{
	// 原生角度：-180 ~ 180
	float Angle = GetAngleRelativeToNPC();

	// 右前：0° ~ 90°
	if (Angle >= 0.0f && Angle <= 90.0f)
		return EDirectionRelativeToNPC::RightFront;

	// 左前：90° ~ 180°
	if (Angle > 90.0f && Angle <= 180.0f)
		return EDirectionRelativeToNPC::LeftFront;

	// 右后：-90° ~ 0°
	if (Angle >= -90.0f && Angle < 0.0f)
		return EDirectionRelativeToNPC::RightBack;

	// 左后：-180° ~ -90°
	return EDirectionRelativeToNPC::LeftBack;
}


#pragma region 暴露在蓝图的NPC避障节点和碰撞交互节点

void ASandboxCharacter_Mover::UpdateObstacleAvoidance(FVector2D IA_Move, UCapsuleComponent* PlayerCapsule)
{
	CheckRightWall();   // then_0 右墙检查
	CheckLeftWall();    // then_1 左墙检查
	UpdateTraceSideInput(IA_Move); // then_2 平滑玩家输入轨迹方向以驱动探测方向偏移量
	UpdateCapsuleRadiusFromAvoidance(PlayerCapsule); // then_3 更新胶囊体半径
	ResetAvoidanceState(IA_Move); // then_4 当附近没有墙时重置规避
}

void ASandboxCharacter_Mover::NPCImpactReaction(AActor* OtherActor, AActor* SelfRef, UMoverComponent* CharactorMover, USkeletalMeshComponent* PlayerMesh)
{
	// Branch：条件判断
	if (!CanTriggerNPCImpactReaction(OtherActor))
	{
		return;
	}

	//剩余逻辑
	//玩家与NPC之间的右侧水平距离
	RightDistanceToNPC = GetRightOffsetDistance(NPCRef, 20,SelfRef, 20);

	//是否在NPC的右侧
	IsAtNPCRightSide = RightDistanceToNPC < 60;

	//角色移动速度
	PlayerMoverSpeed = GetPlayerMoverSpeed(CharactorMover);

	//在NPC身上播放受击动画蒙太奇
	if (PlayerMoverSpeed < 100)
	{
		return;
	}
	NPCRef->PlayNPCImpactedMontage( SelectAvoidanceImpactedAnimMontage() );

	//暂时缩小NPC胶囊体半径
	NPCRef->GetCapsuleComponent()->SetCapsuleRadius(15);

	//播放撞击声
	PlayAvoidanceImpactSound();

	//播放玩家Hit动画蒙太奇
	PlayPlayerAvoidanceHitMontage(PlayerMesh);
}

void ASandboxCharacter_Mover::StartNPCFacelayer(AActor* NPCReference, float RotateAlpha)
{
	float NPCTurnAngle = IsAtNPCRightSide?
							UKismetMathLibrary::MapRangeClamped(RightDistanceToNPC,65,25,20,0)
						:	UKismetMathLibrary::MapRangeClamped(RightDistanceToNPC,0,53,0,-20);
	// 目标位置 = NPC位置 + 向右旋转的量
	FVector NPCTargetLocation = NPCReference->GetActorLocation() + GetActorRightVector() * NPCTurnAngle;
	//Lerp
	FVector FianlLocation = UKismetMathLibrary::VLerp(NPCReference->GetActorLocation(),NPCTargetLocation, RotateAlpha);
	//转向
	NPCRef->SetActorLocation(FianlLocation);
}

#pragma endregion



