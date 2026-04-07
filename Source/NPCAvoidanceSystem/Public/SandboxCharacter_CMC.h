// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SandboxCharacter_CMC.generated.h"

UCLASS()
class NPCAVOIDANCESYSTEM_API ASandboxCharacter_CMC : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ASandboxCharacter_CMC();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	//播放NPC受击动画蒙太奇事件
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NPCAvoidance")
	void PlayNPCImpactedMontage(UAnimMontage* MontageToPlay);
	virtual void PlayNPCImpactedMontage_Implementation(UAnimMontage* MontageToPlay);

	//NPC跟随事件
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NPCAvoidance")
	void StartNPCFollowing();
	virtual void StartNPCFollowing_Implementation();
	
};
