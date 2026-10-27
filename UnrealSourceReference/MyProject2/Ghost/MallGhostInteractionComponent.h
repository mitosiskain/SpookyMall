#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "MallGhostInteractionComponent.generated.h"

class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EMallGhostMode : uint8
{
	InvisibleDisturbance UMETA(DisplayName="Invisible disturbance"),
	VisibleHunt UMETA(DisplayName="Visible hunt"),
	VisibleWarning UMETA(DisplayName="Visible warning")
};

UENUM(BlueprintType)
enum class EMallGhostCyclePhase : uint8
{
	Activity,
	Warning,
	Hunting,
	Cooldown
};

/** Server-owned harmless disturbances, separate from the Blueprint's confined hunt movement. */
UCLASS(ClassGroup=(Mall), meta=(BlueprintSpawnableComponent))
class MYPROJECT2_API UMallGhostInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMallGhostInteractionComponent();

	/** Fail closed while the subsystem has not yet attached its component. */
	UFUNCTION(BlueprintPure, Category="Mall|Ghost")
	static bool CanGhostHunt(AActor* Ghost);

	/** Full target bounds must fit the ghost's store and ground-floor interaction band. */
	UFUNCTION(BlueprintPure, Category="Mall|Ghost")
	static bool IsActorInGhostStore(const AActor* Ghost, const AActor* Other);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost")
	bool SetGhostMode(EMallGhostMode NewMode);

	UFUNCTION(BlueprintPure, Category="Mall|Ghost")
	EMallGhostMode GetGhostMode() const { return GhostMode; }

	UFUNCTION(BlueprintPure, Category="Mall|Ghost")
	bool IsInvisible() const { return GhostMode == EMallGhostMode::InvisibleDisturbance; }

	/** Uses the synchronized server clock on clients; zero when the cycle is disarmed or manual. */
	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Hunt Cycle")
	float GetPhaseTimeRemaining() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Hunt Cycle")
	bool bAutomaticHunts = true;

	/** A new visitor receives a complete harmless activity period before the first warning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Hunt Cycle", meta=(ClampMin="0.25", ClampMax="600.0", Units="s"))
	float InitialActivityMinSeconds = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Hunt Cycle", meta=(ClampMin="0.25", ClampMax="600.0", Units="s"))
	float InitialActivityMaxSeconds = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Hunt Cycle", meta=(ClampMin="0.25", ClampMax="600.0", Units="s"))
	float HuntDurationSeconds = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Hunt Cycle", meta=(ClampMin="0.25", ClampMax="600.0", Units="s"))
	float CooldownMinSeconds = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Hunt Cycle", meta=(ClampMin="0.25", ClampMax="600.0", Units="s"))
	float CooldownMaxSeconds = 60.0f;

	/** Visible but harmless warning before an automatic hunt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Hunt Cycle", meta=(ClampMin="0.25", ClampMax="600.0", Units="s"))
	float WarningDurationSeconds = 3.0f;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Hunt Cycle")
	EMallGhostCyclePhase CyclePhase = EMallGhostCyclePhase::Activity;

	/** Absolute server-world time; zero means the first-visitor grace period is not armed. */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Hunt Cycle")
	double PhaseEndsServerTime = 0.0;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Hunt Cycle")
	int32 HuntCount = 0;

	/** Does not require a nearby player; the automatic scheduler separately requires one. */
	UFUNCTION(BlueprintPure, Category="Mall|Ghost")
	bool CanPerformDisturbance() const;

	/** Explicit request only: selects an unwritten, placed-open book. Never called by the automatic scheduler. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost")
	bool DisturbNearestBook();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost")
	bool MoveNearestProp();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost")
	bool ThrowNearestProp();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost")
	void PauseDisturbances(float Seconds = 3.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost")
	bool bAutomaticDisturbances = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost", meta=(ClampMin="1.0", ClampMax="120.0", Units="s"))
	float DisturbanceIntervalSeconds = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost", meta=(ClampMin="50.0", ClampMax="2000.0", Units="cm"))
	float InteractionRangeCm = 1000.0f;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost")
	int32 DisturbanceCount = 0;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost")
	FName LastDisturbance;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost")
	double LastDisturbanceServerTime = -1.0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
	void OnRep_GhostMode();
	UFUNCTION()
	void OnRep_CartCollisionPolicy();

private:
	friend class FMallGhostHuntCycleNetworkTest;

	UPROPERTY(ReplicatedUsing=OnRep_GhostMode)
	EMallGhostMode GhostMode = EMallGhostMode::InvisibleDisturbance;
	UPROPERTY(ReplicatedUsing=OnRep_CartCollisionPolicy)
	bool bIgnoresCartPhysics = false;
	void ApplyCartCollisionPolicy();
	void RefreshCartCollisionPolicy();
	TMap<TWeakObjectPtr<UPrimitiveComponent>, uint8> OriginalCartCollisionResponses;

	void ApplyGhostMode(EMallGhostMode NewMode);
	void UpdateHuntCycle();
	void TransitionCycle(EMallGhostCyclePhase NewPhase, double DurationSeconds);
	bool HasEligibleHuntPlayerInStore() const;
	void ApplyVisualVisibility();
	void StopHunt();
	bool HasEligiblePlayerInStore() const;
	bool DisturbNearestProp(bool bThrow);
	void RecordDisturbance(FName Kind);

	struct FOriginalVisualVisibility
	{
		bool bHiddenInGame = false;
		bool bCastHiddenShadow = false;
	};
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FOriginalVisualVisibility> OriginalVisualVisibility;
	double DisturbancePausedUntil = 0.0;
	double NextDisturbanceTime = 0.0;
	uint8 AutomaticActionIndex = 0;
	bool bCycleOwnsMode = false;
	int32 LastObservedCatchCount = 0;
};

/** Attaches ghost gameplay independently of microphone or voice settings. */
UCLASS()
class MYPROJECT2_API UMallGhostInteractionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;

private:
	void AttachComponents();
	double NextRefreshTime = 0.0;
};
