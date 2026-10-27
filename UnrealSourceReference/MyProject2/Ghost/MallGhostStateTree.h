#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeTaskBase.h"
#include "MallGhostStateTree.generated.h"

class AAIController;
class AActor;
class ACharacter;

/**
 * Native C++ implementation of BP_StoreGhost's confinement, route validation,
 * pursuit, catch, and StateTree asset construction.
 */
UCLASS()
class MYPROJECT2_API UMallGhostStateTreeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Initializes movement speed, controller, home location, and initial safe-center telemetry on BP_StoreGhost. */
	UFUNCTION(BlueprintCallable, Category = "Mall|Ghost|AI")
	static void InitializeGhostAI(AActor* Ghost);

	/** Validates whether a proposed capsule center is inside allowed bounds/domains, unobstructed, and optionally on navmesh. */
	UFUNCTION(BlueprintCallable, Category = "Mall|Ghost|AI")
	static void CheckSafeCenter(AActor* Ghost, FVector Center, bool bRequireNav, bool& bSafe, FVector& NavFeet);

	/** Stops active AI/character movement and clears the current validated route waypoints. */
	UFUNCTION(BlueprintCallable, Category = "Mall|Ghost|AI")
	static void ClearRoute(AActor* Ghost);

	/** Clears the current route, increments PathRejectCount, records Reason, and schedules replanning backoff. */
	UFUNCTION(BlueprintCallable, Category = "Mall|Ghost|AI")
	static void RejectRoute(AActor* Ghost, const FString& Reason);

	/** Enforces physical/cart confinement, recovers out-of-bounds positions, and updates TargetPlayer and bPlayerEligible. */
	UFUNCTION(BlueprintCallable, Category = "Mall|Ghost|AI")
	static void UpdateGuardAndEligibility(AActor* Ghost);

	/** Synchronously plans and capsule-sweeps a complete navigation route to RequestedGoal. */
	UFUNCTION(BlueprintCallable, Category = "Mall|Ghost|AI")
	static void BuildValidatedRoute(AActor* Ghost);

	/** Advances along CurrentValidWaypoints, checking cart/light protection and stall timeouts each frame. */
	UFUNCTION(BlueprintCallable, Category = "Mall|Ghost|AI")
	static void FollowValidatedRoute(AActor* Ghost);

	/** Checks catch preconditions, line of sight, and home safety; resets ghost to HomeLocation and enters Cooldown on catch. */
	UFUNCTION(BlueprintCallable, Category = "Mall|Ghost|AI")
	static void TryCatchPlayer(AActor* Ghost);

	/** Full single-step tick of the ghost AI state machine (used when ticking without an active StateTreeAIComponent). */
	UFUNCTION(BlueprintCallable, Category = "Mall|Ghost|AI")
	static void RuntimeStateTick(AActor* Ghost);

	/** Runs RuntimeStateTick only if the ghost's AIController does not already have an active running StateTreeAIComponent. */
	UFUNCTION(BlueprintCallable, Category = "Mall|Ghost|AI")
	static void TickGhostWithStateTreeFallback(AActor* Ghost);

	/** Builds/updates and compiles /Game/Environment/Mall/Blueprints/ST_StoreGhost and wires BP_StoreGhostController. */
	UFUNCTION(BlueprintCallable, Category = "Mall|Ghost|StateTree")
	static bool BuildStoreGhostStateTreeAsset();
};

USTRUCT()
struct FMallGhostStateTreeEvaluatorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<ACharacter> GhostCharacter = nullptr;

	UPROPERTY(EditAnywhere, Category = "Output")
	TObjectPtr<ACharacter> TargetPlayer = nullptr;

	UPROPERTY(EditAnywhere, Category = "Output")
	bool bCanHunt = false;

	UPROPERTY(EditAnywhere, Category = "Output")
	bool bInCooldown = false;

	UPROPERTY(EditAnywhere, Category = "Output")
	bool bPlayerEligible = false;

	UPROPERTY(EditAnywhere, Category = "Output")
	bool bNeedsReturnHome = false;

	UPROPERTY(EditAnywhere, Category = "Output")
	FString PrototypeState;
};

/**
 * StateTree Evaluator that runs BP_StoreGhost's confinement guard and target eligibility sensor each tick.
 */
USTRUCT(meta = (DisplayName = "Mall Ghost Guard & Sensor Evaluator", Category = "Mall|Ghost"))
struct MYPROJECT2_API FMallGhostStateTreeEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMallGhostStateTreeEvaluatorInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

USTRUCT()
struct FMallGhostConditionInstanceData
{
	GENERATED_BODY()
};

/**
 * Condition: True when UMallGhostInteractionComponent::CanGhostHunt(Ghost) is true (or inverted).
 */
USTRUCT(meta = (DisplayName = "Mall Ghost Can Hunt", Category = "Mall|Ghost"))
struct MYPROJECT2_API FMallGhostCanHuntCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMallGhostConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;
};

/**
 * Condition: True when the ghost is currently in post-catch or voice cooldown (GameTime < CooldownUntil).
 */
USTRUCT(meta = (DisplayName = "Mall Ghost In Cooldown", Category = "Mall|Ghost"))
struct MYPROJECT2_API FMallGhostInCooldownCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMallGhostConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;
};

/**
 * Condition: True when bPlayerEligible is true on the store ghost.
 */
USTRUCT(meta = (DisplayName = "Mall Ghost Player Eligible", Category = "Mall|Ghost"))
struct MYPROJECT2_API FMallGhostPlayerEligibleCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMallGhostConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;
};

/**
 * Condition: True when the store ghost is > 25 cm (2D) from its HomeLocation.
 */
USTRUCT(meta = (DisplayName = "Mall Ghost Needs Return Home", Category = "Mall|Ghost"))
struct MYPROJECT2_API FMallGhostNeedsReturnHomeCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMallGhostConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bInvert = false;
};

USTRUCT()
struct FMallGhostTaskInstanceData
{
	GENERATED_BODY()
};

/**
 * StateTree Task: Maintains dormant / invisible disturbance / visible warning state while hunting is not active.
 */
USTRUCT(meta = (DisplayName = "Mall Ghost Dormant Or Warning Task", Category = "Mall|Ghost"))
struct MYPROJECT2_API FMallGhostDormantTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMallGhostTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/**
 * StateTree Task: Holds the ghost in Cooldown state and clears its route until CooldownUntil expires.
 */
USTRUCT(meta = (DisplayName = "Mall Ghost Cooldown Task", Category = "Mall|Ghost"))
struct MYPROJECT2_API FMallGhostCooldownTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMallGhostTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/**
 * StateTree Task: Plans and follows a validated route to TargetPlayer and attempts a catch within range.
 */
USTRUCT(meta = (DisplayName = "Mall Ghost Chase & Catch Task", Category = "Mall|Ghost"))
struct MYPROJECT2_API FMallGhostChaseTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMallGhostTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/**
 * StateTree Task: Plans and follows a validated route back to HomeNavPoint when no eligible player is present.
 */
USTRUCT(meta = (DisplayName = "Mall Ghost Return Home Task", Category = "Mall|Ghost"))
struct MYPROJECT2_API FMallGhostReturnHomeTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMallGhostTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

/**
 * StateTree Task: Holds the ghost idle at its home position while awaiting an eligible target.
 */
USTRUCT(meta = (DisplayName = "Mall Ghost Idle At Home Task", Category = "Mall|Ghost"))
struct MYPROJECT2_API FMallGhostIdleTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FMallGhostTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
