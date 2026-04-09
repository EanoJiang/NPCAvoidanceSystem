// Fill out your copyright notice in the Description page of Project Settings.
#include "SandboxCharacter_CMC.h"


// Sets default values
ASandboxCharacter_CMC::ASandboxCharacter_CMC()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ASandboxCharacter_CMC::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASandboxCharacter_CMC::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ASandboxCharacter_CMC::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}


