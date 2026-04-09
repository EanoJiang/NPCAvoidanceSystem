#pragma once

#include "CoreMinimal.h"
#include "DirectionRelative.h"
#include "HitStructure.generated.h"

//结构体：碰撞交互蒙太奇选择需要用到的结构体
USTRUCT(BlueprintType)
struct NPCAVOIDANCESYSTEM_API FHitStructure
{
	GENERATED_BODY()
	
	// 玩家撞击的方向
	UPROPERTY(BlueprintReadWrite, Category="HitStructure")
	EDirectionRelative PlayerHitDirection;

	// 受击时候的玩家速度
	UPROPERTY(BlueprintReadWrite, Category="HitStructure")
	float PlayerSpeedAtImpact;
};