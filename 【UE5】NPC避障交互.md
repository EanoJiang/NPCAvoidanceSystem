> 写在前面：
>
> 对于用到Mover的C++代码，需要在Build.cs中添加Mover
>
> ![1775119985908](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203511134-773057692.png)

## NPC避障

![1774945780886](https://img2024.cnblogs.com/blog/3614909/202603/3614909-20260331192943076-721373714.png)

### 内部逻辑节点

Event Tick中新建节点UpdateObstacleAvoidance

![1774949649537](https://img2024.cnblogs.com/blog/3614909/202603/3614909-20260331192943937-1877728997.png)

#### 全局变量

```cpp
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
```

#### 左右墙检查

```cpp
#pragma region CheckSideWall相关函数(不暴露在蓝图)
//计算AvoidanceTrace的起点和终点
void BuildSideAvoidanceTrace(FVector& Start, FVector& End, bool IsRight);

//距离值映射为α值：将角色与墙壁击中距离转换为转向权重
float MapDistanceToAlpha(const FVector& TraceImpactPoint, float MinDistance, float MaxDistance,float MinAlpha, float MaxAlpha);

//SphereTrace检测身体两侧的墙
void CheckSideWall(bool IsRight, EDrawDebugTrace::Type DrawDebugType = EDrawDebugTrace::ForOneFrame);
#pragma endregion
```

```cpp
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
```

##### 右墙检查

```cpp
	//检测右墙
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void CheckRightWall(){CheckSideWall(true);}
```

##### 左墙检查

```cpp
	//检测左墙
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void CheckLeftWall(){CheckSideWall(false);}
```

#### 平滑玩家输入轨迹方向以驱动探测方向偏移量

```cpp
	//平滑玩家输入轨迹方向以驱动探测方向偏移量
	//IA_Move：玩家的移动输入
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void UpdateTraceSideInput(FVector2D IA_Move);
```

```cpp
void ASandboxCharacter_Mover::UpdateTraceSideInput(FVector2D IA_Move)
{
	TraceSideInput = UKismetMathLibrary::FInterpTo(TraceSideInput, IA_Move.X, TickDeltaTime,5);
}
```

#### 根据Avoidance的权重Alpha调整胶囊体大小

```cpp
	//根据Avoidance的权重Alpha调整胶囊体大小
	//越靠近碰撞，胶囊体越大，稍宽的胶囊有助于将NPC或墙体推离，防止穿模。
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void UpdateCapsuleRadiusFromAvoidance(UCapsuleComponent* PlayerCapsule);
```

```cpp
void ASandboxCharacter_Mover::UpdateCapsuleRadiusFromAvoidance(UCapsuleComponent* PlayerCapsule)
{
	float Radius = UKismetMathLibrary::MapRangeClamped(abs(AvoidanceAlphaSigned),1,0,20,30);
    PlayerCapsule->SetCapsuleRadius(Radius);
}
```

#### 当附近没有墙时，重置Avoidance回初始状态

```cpp
	//当附近没有墙时，重置Avoidance回初始状态
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void ResetAvoidanceState(FVector2D IA_Move);
```

```cpp
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
```

### 整合为一个节点

![1775117177836](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203511532-375350169.png)

```cpp
	//NPC避障
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance", meta = (HideSelfPin = "true"))
	void UpdateObstacleAvoidance(FVector2D IA_Move, UCapsuleComponent* PlayerCapsule);
```

```cpp
void ASandboxCharacter_Mover::UpdateObstacleAvoidance(FVector2D IA_Move, UCapsuleComponent* PlayerCapsule)
{
	CheckRightWall();   // then_0 右墙检查
	CheckLeftWall();    // then_1 左墙检查
	UpdateTraceSideInput(IA_Move); // then_2 平滑玩家输入轨迹方向以驱动探测方向偏移量
	UpdateCapsuleRadiusFromAvoidance(PlayerCapsule); // then_3 更新胶囊体半径
	ResetAvoidanceState(IA_Move); // then_4 当附近没有墙时重置规避
}
```

### 混合空间配置

> BS_Avoidance 一维

![1774953605275](https://img2024.cnblogs.com/blog/3614909/202603/3614909-20260331192945135-902236891.png)

### 动画蓝图

#### 与角色蓝图通信

> 用于PropertyAccess获取角色蓝图中计算好的权重参数

![1774953960940](https://img2024.cnblogs.com/blog/3614909/202603/3614909-20260331192945610-1217296210.png)

#### 根据权重值的绝对值应用网格空间加法器

> 叠加BS_Avoidance

![1774954440452](https://img2024.cnblogs.com/blog/3614909/202603/3614909-20260331192946087-1251727032.png)

![1774954797200](https://img2024.cnblogs.com/blog/3614909/202603/3614909-20260331192946507-166164331.png)

ApplyMeshSpaceAdditive节点设置：开启平滑结果

![1775012392210](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203511893-815228306.png)

## 玩家与NPC碰撞交互

### 播放NPC受击动画蒙太奇事件(NPC)

NPC的蓝图类BP_SandboxCharacter_CMC_Child的父类是蓝图类BP_SandboxCharacter_CMC，而BP_SandboxCharacter_CMC的父类是自定义的C++类SandboxCharacter_CMC

在这个C++类中声明好需要调用的事件函数PlayNPCImpactedMontage

```cpp
	//播放NPC受击动画蒙太奇事件
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NPCAvoidance")
	void PlayNPCImpactedMontage(UAnimMontage* MontageToPlay);
	virtual void PlayNPCImpactedMontage_Implementation(UAnimMontage* MontageToPlay);
```

```cpp
void ASandboxCharacter_CMC::PlayNPCImpactedMontage_Implementation(UAnimMontage* MontageToPlay)
{
}
```

这样，就可以在玩家的c++基类中通过ASandboxCharacter_CMC*类型的变量NPCRef点出这个事件

![1775121253563](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203512246-1678727266.png)

然后在NPC的蓝图类中实现这个事件的逻辑即可

![1775119416339](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203512756-1214394058.png)

### 碰撞检测事件(玩家)

![1775117301284](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203513278-701107522.png)

#### 内部逻辑节点

![1775117499623](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203513782-1520433431.png)

##### 全局变量

```cpp
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
```

##### 配置资产

![1775118945696](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203514245-236152666.png)

##### 可以触发NPC反应的条件

```cpp
	//可以触发NPC反应的条件
	UFUNCTION(BlueprintPure, Category = "NPCAvoidance|ImpactReaction")
	bool CanTriggerNPCImpactReaction(AActor* Other);
```

```cpp
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
```

##### 获取玩家与NPC之间的右侧水平距离

```cpp
	//获取玩家与NPC之间的右侧水平距离
	UFUNCTION(BlueprintPure, Category = "NPCAvoidance|ImpactReaction")
	float GetRightOffsetDistance(AActor* NPCReference, float NPCHorizontalOffset, AActor* PlayerReference, float PlayerHorizontalOffset);
```

```cpp
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
```

##### 获取玩家移动速度

```cpp
	//获取玩家移动速度
	UFUNCTION(BlueprintPure, Category = "Mover")
	float GetPlayerMoverSpeed(UMoverComponent* Mover);
```

```cpp
float ASandboxCharacter_Mover::GetPlayerMoverSpeed(UMoverComponent* Mover)
{
	return Mover->GetVelocity().Length();
}
```

##### 根据IsAtNPCRightSide播放相应的随机动画蒙太奇列表

```cpp
	//根据IsAtNPCRightSide播放相应的随机动画蒙太奇列表
	UFUNCTION(BlueprintPure, Category = "NPCAvoidance|ImpactReaction")
	UAnimMontage* SelectAvoidanceImpactedAnimMontage();
```

```cpp
UAnimMontage* ASandboxCharacter_Mover::SelectAvoidanceImpactedAnimMontage()
{
	TArray<UAnimMontage*> SelectedAnimMontages = (IsAtNPCRightSide)? RightAvoidanceImpactedAnimMontageArray: LeftAvoidanceImpactedAnimMontageArray;
	int RandomIndex = UKismetMathLibrary::RandomIntegerInRange(0,SelectedAnimMontages.Num() - 1);
	return SelectedAnimMontages[RandomIndex];
}
```

##### 播放撞击声

```cpp
	//播放撞击声
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance|ImpactReaction", meta = (HideSelfPin = "true"))
	void PlayAvoidanceImpactSound();
```

```cpp
void ASandboxCharacter_Mover::PlayAvoidanceImpactSound()
{
	USoundBase* SelectedSound;
	SelectedSound = (PlayerMoverSpeed > 200)? AvoidanceImpactSound: AvoidanceLightImpactSound;
	UGameplayStatics::PlaySoundAtLocation(this, SelectedSound, ImpactLocation);
}
```

##### 播放玩家的Hit动画蒙太奇

```cpp
	//播放玩家的Hit动画蒙太奇
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance|ImpactReaction", meta = (HideSelfPin = "true"))
	void PlayPlayerAvoidanceHitMontage(USkeletalMeshComponent* Mesh);
```

```cpp
void ASandboxCharacter_Mover::PlayPlayerAvoidanceHitMontage(USkeletalMeshComponent* Mesh)
{
	//选择撞击蒙太奇
	UAnimMontage* SelectedMontage;
	if (IsAtNPCRightSide)
	{
		SelectedMontage = (PlayerMoverSpeed > 200)? AvoidanceHit_R_Montage: AvoidanceLightHit_R_Montage;
	}
	else
	{
		SelectedMontage = (PlayerMoverSpeed > 200)? AvoidanceHit_L_Montage: AvoidanceLightHit_L_Montage;

	}
	//播放
	Mesh->GetAnimInstance()->Montage_Play(SelectedMontage);
}
```

### 蒙太奇资产配置

#### 玩家

玩家AvoidanceHit的蒙太奇需要设置一个上半身的Slot名

> 在AvoidanceHit动作中保持腿部持续运动，正是让系统运行自然流畅的关键——角色会自然地做出碰撞反应并继续行走，整个动作过程不会出现全身性中断。

![1775120187032](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203515270-1641163076.png)

#### NPC

NPC的受击蒙太奇有停下的脚部动作，因此保持默认的Slot

### 动画蓝图

#### 玩家

![1775120482831](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203515729-279018558.png)

![1775120500923](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203516160-534994462.png)

该Slot从Spine_03开始混合，混合深度=1表示仅混合spine_03(含)以下层级

![1775120570914](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203516561-1336452015.png)

#### NPC

NPC的动画蓝图中不需要设置Slot混合，保持默认的Slot即可

![1775120816822](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203516924-976156214.png)

## NPC跟随

同样地，在NPC蓝图类BP_SandboxCharacter_CMC_Child的父类的C++父类中，声明好需要调用的事件

```cpp
	//NPC跟随事件
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NPCAvoidance")
	void StartNPCFollowing();
	virtual void StartNPCFollowing_Implementation();
```

```cpp
void ASandboxCharacter_CMC::StartNPCFollowing_Implementation()
{
}
```

![1775131850816](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203517367-21595885.png)

![1775131821998](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203517814-1769890636.png)

![1775131836262](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402203518257-491753734.png)

## Collision设置

### Player Mesh

![1775242550285](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260404025715615-1900722870.png)

### PlayerCapsule

![1775242581473](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260404025716021-972174038.png)

### NPC Mesh

![1775242609699](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260404025716247-806555066.png)

### NPC Capsule

![1775242627034](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260404025716510-2105230334.png)

## 最终效果

![1775133806887](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402204412731-1064326060.gif)

![1775133824620](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260402204424579-614886344.gif)

## 优化：区分四个方向(玩家相对于NPC)撞击动画

![1775253629775](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260404060111798-1007062021.png)

```cpp
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
	LeftBack		// -180° ~ -90°
};
```

```cpp
	//计算玩家相对于NPC的角度信息
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance|ImpactReaction", meta = (HideSelfPin = "true"))
	float GetAngleRelativeToNPC();

	//判断玩家属于NPC的哪个方向
	//根据相对于NPC的角度信息判断
	UFUNCTION(BlueprintCallable, Category = "NPCAvoidance|ImpactReaction", meta = (HideSelfPin = "true"))
	EDirectionRelativeToNPC GetDirectionRelativeToNPC();
```

```cpp
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
```

修改播放动画蒙太奇的资产选择逻辑：

```cpp
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
```

```cpp
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
```

效果：

![1775253373274](https://img2024.cnblogs.com/blog/3614909/202604/3614909-20260404055618942-1668493652.gif)

## 优化：用结构体+ChooserTable选择Impacted和Hit蒙太奇资产
