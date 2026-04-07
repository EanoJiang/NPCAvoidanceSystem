// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "MoverComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetSystemLibrary.h"
#include "SandboxCharacter_Mover.generated.h"

class ASandboxCharacter_CMC;

//枚举：相对于NPC的方向
UENUM(BlueprintType)
enum class EDirectionRelativeToNPC : uint8
{
	// 正右 0°
	// 正前 90°
	// 正左 180°
	// 正后 -90°

	RightFront,	// 0° ~ 90°
	LeftFront,	// 90° ~ 180°
	RightBack,	// -90° ~ 0°
	LeftBack	// -180° ~ -90°
};

UCLASS()
class NPCAVOIDANCESYSTEM_API ASandboxCharacter_Mover : public APawn
{
	GENERATED_BODY()
#pragma region CheckSideWall相关函数(不暴露在蓝图)
//计算AvoidanceTrace的起点和终点
void BuildSideAvoidanceTrace(FVector& Start, FVector& End, bool IsRight);

//距离值映射为α值：将角色与墙壁击中距离转换为转向权重
float MapDistanceToAlpha(const FVector& TraceImpactPoint, float MinDistance, float MaxDistance,float MinAlpha, float MaxAlpha);

//SphereTrace检测身体两侧的墙
void CheckSideWall(bool IsRight, EDrawDebugTrace::Type DrawDebugType = EDrawDebugTrace::ForOneFrame);
#pragma endregion
	
public:
	// Sets default values for this character's properties
	ASandboxCharacter_Mover();
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

#pragma region 全局变量：NPC避障
	UPROPERTY(BlueprintReadWrite, Category = "NPCAvoidance")
	bool IsLeftWallDetected;
	UPROPERTY(BlueprintReadWrite, Category = "NPCAvoidance")
	bool IsRightWallDetected;
	UPROPERTY(BlueprintReadWrite, Category = "NPCAvoidance")
	float TraceSideInput;
	UPROPERTY(BlueprintReadWrite, Category = "NPCAvoidance")
	TObjectPtr<AActor> HitActor;
	UPROPERTY(BlueprintReadWrite, Category = "NPCAvoidance")
	float AvoidanceAlphaSigned;
	UPROPERTY(BlueprintReadWrite, Category = "NPCAvoidance")
	FVector ImpactLocation;
	UPROPERTY(BlueprintReadWrite, Category = "DeltaTime")
	float TickDeltaTime;
#pragma endregion

#pragma region 全局变量：NPC碰撞交互
	UPROPERTY(BlueprintReadWrite, Category = "NPCAvoidance|ImpactReaction")
	float RightDistanceToNPC;
	UPROPERTY(BlueprintReadWrite, Category = "NPCAvoidance|ImpactReaction")
	bool IsAtNPCRightSide;
	UPROPERTY(BlueprintReadWrite, Category = "Mover")
	float PlayerMoverSpeed;
	
	UPROPERTY(BlueprintReadWrite, Category = "NPCAvoidance|ImpactReaction")
	TObjectPtr<ASandboxCharacter_CMC> NPCRef;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPCAvoidance|ImpactReaction|ImpactedMontage")
	TArray<UAnimMontage*> LeftAvoidanceImpactedAnimMontageArray;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPCAvoidance|ImpactReaction|ImpactedMontage")
	TArray<UAnimMontage*> RightAvoidanceImpactedAnimMontageArray;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPCAvoidance|ImpactReaction|ImpactSound")
	USoundBase* AvoidanceLightImpactSound;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPCAvoidance|ImpactReaction|ImpactSound")
	USoundBase* AvoidanceImpactSound;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPCAvoidance|ImpactReaction|HitMontage")
	TObjectPtr<UAnimMontage> AvoidanceLightHit_L_Montage;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPCAvoidance|ImpactReaction|HitMontage")
	TObjectPtr<UAnimMontage> AvoidanceHit_L_Montage;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPCAvoidance|ImpactReaction|HitMontage")
	TObjectPtr<UAnimMontage> AvoidanceLightHit_R_Montage;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPCAvoidance|ImpactReaction|HitMontage")
	TObjectPtr<UAnimMontage> AvoidanceHit_R_Montage;
#pragma endregion 
	
#pragma region NPC避障内部逻辑节点
	//检测右墙
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void CheckRightWall(){CheckSideWall(true);}
	
	//检测左墙
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void CheckLeftWall(){CheckSideWall(false);}
	
	//平滑玩家输入轨迹方向以驱动探测方向偏移量
	//IA_Move：玩家的移动输入
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void UpdateTraceSideInput(FVector2D IA_Move);
	
	//根据Avoidance的权重Alpha调整胶囊体大小
	//越靠近碰撞，胶囊体越大，稍宽的胶囊有助于将NPC或墙体推离，防止穿模。
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void UpdateCapsuleRadiusFromAvoidance(UCapsuleComponent* PlayerCapsule);
	
	//当附近没有墙时，重置Avoidance回初始状态
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void ResetAvoidanceState(FVector2D IA_Move);
#pragma endregion

#pragma region NPC碰撞交互内部逻辑节点
	//可以触发NPC反应的条件
	UFUNCTION(BlueprintPure, Category = "NPCAvoidance|ImpactReaction")
	bool CanTriggerNPCImpactReaction(AActor* Other);
	
	//获取玩家与NPC之间的右侧水平距离
	UFUNCTION(BlueprintPure, Category = "NPCAvoidance|ImpactReaction")
	float GetRightOffsetDistance(AActor* NPCReference, float NPCHorizontalOffset, AActor* PlayerReference, float PlayerHorizontalOffset);
	
	//获取玩家移动速度
	UFUNCTION(BlueprintPure, Category = "Mover")
	float GetPlayerMoverSpeed(UMoverComponent* Mover);
	
	//根据IsAtNPCRightSide播放相应的随机动画蒙太奇列表
	UFUNCTION(BlueprintPure, Category = "NPCAvoidance|ImpactReaction")
	UAnimMontage* SelectAvoidanceImpactedAnimMontage();
	
	//播放撞击声
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance|ImpactReaction", meta = (HideSelfPin = "true"))
	void PlayAvoidanceImpactSound();
	
	//播放玩家的Hit动画蒙太奇
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance|ImpactReaction", meta = (HideSelfPin = "true"))
	void PlayPlayerAvoidanceHitMontage(USkeletalMeshComponent* Mesh);

	//计算玩家相对于NPC的角度信息
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance|ImpactReaction", meta = (HideSelfPin = "true"))
	float GetAngleRelativeToNPC();

	//判断玩家属于NPC的哪个方向
	//根据相对于NPC的角度信息判断
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance|ImpactReaction", meta = (HideSelfPin = "true"))
	EDirectionRelativeToNPC GetDirectionRelativeToNPC();
	
#pragma endregion
	
#pragma region NPC避障节点和碰撞交互节点
	//NPC避障
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void UpdateObstacleAvoidance(FVector2D IA_Move, UCapsuleComponent* PlayerCapsule);
	
	//NPC碰撞交互
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance|HitEvent", meta = (HideSelfPin = "true"))
	void NPCImpactReaction(AActor* OtherActor, AActor* SelfRef, UMoverComponent* CharactorMover, USkeletalMeshComponent* PlayerMesh);

	//计算让NPC朝向玩家的目标位置
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance|HitEvent", meta = (HideSelfPin = "true"))
	void StartNPCFacelayer(AActor* NPCReference, float RotateAlpha);
	
#pragma endregion
	
};
