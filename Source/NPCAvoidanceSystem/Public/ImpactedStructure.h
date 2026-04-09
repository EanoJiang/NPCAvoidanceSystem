#pragma once

#include "CoreMinimal.h"
#include "DirectionRelative.h"
#include "ImpactedStructure.generated.h"

//结构体：碰撞交互蒙太奇选择需要用到的结构体
USTRUCT(BlueprintType)
struct NPCAVOIDANCESYSTEM_API FImpactedStructure
{
	GENERATED_BODY()
	
	// NPC受击的方向
	UPROPERTY(BlueprintReadWrite, Category="HitStructure")
	EDirectionRelative NPCImpactedDirection;

	// 受击时候的玩家速度
	UPROPERTY(BlueprintReadWrite, Category="HitStructure")
	float PlayerSpeedAtImpact;
};