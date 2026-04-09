// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ImpactedStructure.h"
#include "Chooser.h"
#include "DirectionRelative.h"
#include "HitStructure.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "SandboxCharacter_CMC.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetSystemLibrary.h"
#include "SandboxCharacter_Mover.generated.h"

UCLASS()
class NPCAVOIDANCESYSTEM_API ASandboxCharacter_Mover : public APawn
{
	GENERATED_BODY()

	
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

	UPROPERTY(BlueprintReadWrite, Category = "ImpactReaction")
	TObjectPtr<ASandboxCharacter_CMC> NPCRef;
	
	UPROPERTY(BlueprintReadWrite, Category = "Mover")
	float PlayerMoverSpeed;

	UPROPERTY(BlueprintReadWrite, Category = "ImpactReaction")
	float PlayerAngleRelativeToNPC;
	UPROPERTY(BlueprintReadWrite, Category = "ImpactReaction")
	float NPCAngleRelativeToPlayer;
	UPROPERTY(BlueprintReadWrite, Category = "ImpactReaction")
	EDirectionRelative PlayerDirectionRelativeToNPC;
	UPROPERTY(BlueprintReadWrite, Category = "ImpactReaction")
	EDirectionRelative NPCDirectionRelativeToPlayer;

	UPROPERTY(BlueprintReadWrite, Category = "ImpactReaction")
	FHitStructure HitStructure;
	UPROPERTY(BlueprintReadWrite, Category = "ImpactReaction")
	FImpactedStructure ImpactedStructure;
	
	UPROPERTY(EditAnywhere, Category = "ImpactReaction|ImpactSound")
	USoundBase* LightImpactSound;
	UPROPERTY(EditAnywhere, Category = "ImpactReaction|ImpactSound")
	USoundBase* HeavyImpactSound;
	
	UPROPERTY(EditAnywhere, Category = "ImpactReaction|HitMontage")
	TObjectPtr<UChooserTable> CT_HitMontage;
	UPROPERTY(EditAnywhere, Category = "ImpactReaction|ImpactedMontage")
	TObjectPtr<UChooserTable> CT_ImpactedMontage;

	UPROPERTY(BlueprintReadWrite, Category = "ImpactReaction|HitMontage")
	UAnimMontage* SelectedHitMontage;
	UPROPERTY(BlueprintReadWrite, Category = "ImpactReaction|ImpactedMontage")
	UAnimMontage* SelectedImpactedMontage;
	
#pragma endregion 

#pragma region 节点：避障和碰撞交互
	
	//NPC避障
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void UpdateObstacleAvoidance(FVector2D IA_Move, UCapsuleComponent* PlayerCapsule);

	//NPC碰撞交互
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance|HitEvent", meta = (HideSelfPin = "true"))
	void NPCImpactReaction(AActor* OtherActor);
	
#pragma endregion
	
#pragma region NPC避障内部逻辑节点
	
	//计算AvoidanceTrace的起点和终点
	void BuildSideAvoidanceTrace(FVector& Start, FVector& End, bool IsRight);

	//距离值映射为α值：将角色与墙壁击中距离转换为转向权重
	float MapDistanceToAlpha(const FVector& TraceImpactPoint, float MinDistance, float MaxDistance,float MinAlpha, float MaxAlpha);

	//SphereTrace检测身体两侧的墙
	void CheckSideWall(bool IsRight, EDrawDebugTrace::Type DrawDebugType = EDrawDebugTrace::ForOneFrame);
	
	//检测右墙
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void CheckRightWall(){CheckSideWall(true, EDrawDebugTrace::None);}
	
	//检测左墙
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void CheckLeftWall(){CheckSideWall(false, EDrawDebugTrace::None);}
	
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
	
	//可以触发碰撞交互的条件
	UFUNCTION(BlueprintPure, Category = "ImpactReaction")
	bool CanTriggerImpactReaction(AActor* Other);
	
	//获取玩家移动速度
	UFUNCTION(BlueprintCallable, Category = "Mover")
	void GetPlayerMoverSpeed();

	//计算相对角度信息
	void GetAngleRelative(AActor* Actor1, AActor* Actor2, float& AngleRelative);
	//计算玩家相对于NPC的角度信息
	UFUNCTION(BlueprintCallable, Category = "ImpactReaction", meta = (HideSelfPin = "true"))
	void GetPlayerAngleRelativeToNPC(){GetAngleRelative(NPCRef, this, PlayerAngleRelativeToNPC);}
	//计算NPC相对于玩家的角度信息
	UFUNCTION(BlueprintCallable, Category = "ImpactReaction", meta = (HideSelfPin = "true"))
	void GetNPCAngleRelativeToPlayer(){GetAngleRelative(this, NPCRef, NPCAngleRelativeToPlayer);}

	//计算相对方向
	void GetDirectionRelative(float& AngleRelative, EDirectionRelative& DirectionRelative);
	//计算玩家相对于NPC的方向
	UFUNCTION(BlueprintCallable, Category = "ImpactReaction", meta = (HideSelfPin = "true"))
	void GetPlayerDirectionRelativeToNPC(){GetDirectionRelative(PlayerAngleRelativeToNPC, PlayerDirectionRelativeToNPC);}
	//计算NPC相对于玩家的方向
	UFUNCTION(BlueprintCallable, Category = "ImpactReaction", meta = (HideSelfPin = "true"))
	void GetNPCDirectionRelativeToPlayer(){GetDirectionRelative(NPCAngleRelativeToPlayer, NPCDirectionRelativeToPlayer);}

	//数据传入FHitStructure结构体
	void SetHitStructure();
	//数据传入FImpactedtructure结构体
	void SetImpactedStructure();
	//选择表选择蒙太奇
	template <typename TStruct>
	UAnimMontage* SelectMontage(UChooserTable* MontageChooserTable, TStruct& StructParams);
	
	//设置碰撞交互蒙太奇资产
	UFUNCTION(BlueprintCallable, Category = "ImpactReaction", meta = (HideSelfPin = "true"))
	void SetImpactReactionMontage();
	
	//播放玩家的Hit动画蒙太奇
	void PlayPlayerHitMontage();
	//播放撞击声
	void PlayAvoidanceImpactSound();
	//播放NPC的Impacted动画蒙太奇
	void PlayNPCImpactedMontage();

	//播放玩家和NPC的碰撞交互蒙太奇
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance|HitEvent", meta = (HideSelfPin = "true"))
	void PlayReactionMontages();
	
#pragma endregion
	
};
