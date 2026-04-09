#pragma once
#include "CoreMinimal.h"
#include "DirectionRelative.generated.h"

//枚举：相对方向（8方向）
UENUM(BlueprintType)
enum class EDirectionRelative : uint8
{
	Back       UMETA(DisplayName="后"),			//-105 ~ -75 
	BackLeft   UMETA(DisplayName="左后"),		//-165 ~ -105
	Left       UMETA(DisplayName="左"),			// 165 ~ 180 ∪ -180 ~ -165
	FrontLeft  UMETA(DisplayName="左前"),		// 105 ~ 165
	Front      UMETA(DisplayName="前"),			// 75  ~ 105
	FrontRight UMETA(DisplayName="右前"),	// 15  ~ 75
	Right      UMETA(DisplayName="右"),			// -15 ~ 15
	BackRight  UMETA(DisplayName="右后"),		// -75 ~ -15
};