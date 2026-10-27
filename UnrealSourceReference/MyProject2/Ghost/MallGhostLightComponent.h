#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MallGhostLightComponent.generated.h"

class ALight;
class ULocalLightComponent;

UENUM(BlueprintType)
enum class EMallGhostLightMode : uint8
{
	Off,
	Warning,
	Chasing
};

/** One coherent server-selected effect, including the explicit lights each client should use. */
USTRUCT()
struct FMallGhostLightState
{
	GENERATED_BODY()

	UPROPERTY()
	EMallGhostLightMode Mode = EMallGhostLightMode::Off;

	UPROPERTY()
	int32 Seed = 0;

	UPROPERTY()
	double EpochServerTime = 0.0;

	UPROPERTY()
	float MinimumBrightness = 1.0f;

	UPROPERTY()
	float Rate = 2.3f;

	UPROPERTY()
	TArray<TObjectPtr<ALight>> Targets;
};

/** Temporary intensity-only flicker for explicitly tagged, authored store lights. */
UCLASS(ClassGroup=(Mall), meta=(BlueprintSpawnableComponent))
class MYPROJECT2_API UMallGhostLightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMallGhostLightComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Lights")
	bool bEnableFlicker = true;

	/** Opt in only the intended store's ALight actors with this tag. None selects nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Lights")
	FName TargetLightTag = NAME_None;

	/** Fraction of each light's original intensity at the deepest warning dip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Lights", meta=(ClampMin="0.0", ClampMax="1.0"))
	float WarningMinimumBrightness = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Lights", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ChaseMinimumBrightness = 0.0f;

	/** Brightness steps per second, with variable peaks and short held dips; intensity only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Lights", meta=(ClampMin="0.25", ClampMax="3.0"))
	float FlickerRate = 2.3f;

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Lights")
	EMallGhostLightMode GetFlickerMode() const { return FlickerState.Mode; }

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Lights")
	int32 GetTargetLightCount() const { return TargetLightCount; }

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Lights")
	int32 TargetLightCount = 0;

	/** Local presentation diagnostic, also evaluated without rendering on a dedicated server. */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Lights")
	float AppliedIntensityMultiplier = 1.0f;

	virtual void Deactivate() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnregister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	friend class FMallGhostLightNetworkTest;

	UPROPERTY(ReplicatedUsing=OnRep_FlickerState)
	FMallGhostLightState FlickerState;

	UFUNCTION()
	void OnRep_FlickerState();

	EMallGhostLightMode DetermineMode() const;
	bool IsEligibleLight(const ALight* Light) const;
	void UpdateAuthoritativeState();
	void ApplyFlicker();
	void RestoreLights();
	void ClearAuthoritativeState();
	static float EvaluateMultiplier(const FMallGhostLightState& State, double ElapsedSeconds);

	TArray<TWeakObjectPtr<ALight>> SelectedLights;
	TMap<TWeakObjectPtr<ULocalLightComponent>, float> OriginalIntensities;
	EMallGhostLightMode LastRequestedMode = EMallGhostLightMode::Off;
	FName LastTargetTag = NAME_None;
	double NextTargetRefreshTime = 0.0;
};
