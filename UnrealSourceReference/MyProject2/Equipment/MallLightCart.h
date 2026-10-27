#pragma once

#include "CoreMinimal.h"
#include "Equipment/MallPushCart.h"
#include "MallLightCart.generated.h"

class AMallCartCharger;
class USpotLightComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UTextRenderComponent;

/** A server-powered mobile refuge. The rendered spotlight and protection use the same finite cone. */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallLightCart : public AMallPushCart
{
	GENERATED_BODY()

public:
	AMallLightCart();

	virtual bool TryUse(APlayerController* Player) override;
	virtual FString GetCartStatus() const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart|Light")
	bool SetPowerRequested(bool bRequested);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart|Charging")
	bool ConnectToCharger(AMallCartCharger* Charger);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart|Charging")
	void DisconnectCharger();

	UFUNCTION(BlueprintPure, Category="Mall|Cart|Light")
	bool IsSafetyActive() const;

	/** Ghost exclusion uses any intersection with its full collision capsule. */
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Light")
	bool IsCapsuleProtected(FVector Centre, float Radius, float HalfHeight) const;

	/** A player is safe only if the complete capsule fits the illuminated cone. */
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Light")
	bool IsCapsuleFullyProtected(FVector Centre, float Radius, float HalfHeight) const;

	static bool ConeIntersectsCapsule(FVector Apex, FVector Direction, float Length, float HalfAngleDegrees,
		FVector Centre, float Radius, float HalfHeight);
	static bool ConeContainsCapsule(FVector Apex, FVector Direction, float Length, float HalfAngleDegrees,
		FVector Centre, float Radius, float HalfHeight);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Battery", meta=(ClampMin="1.0", Units="s"))
	float RuntimeSeconds = 900.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Battery", meta=(ClampMin="1.0", Units="s"))
	float RechargeSeconds = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|Cart|Battery", meta=(ClampMin="0.0", ClampMax="1.0"))
	float BatteryFraction = 1.0f;
	UPROPERTY(ReplicatedUsing=OnRep_LightState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Light")
	bool bPowerRequested = true;
	UPROPERTY(ReplicatedUsing=OnRep_LightState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Charging")
	TObjectPtr<AMallCartCharger> CurrentCharger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|Cart|Light")
	bool bSafetyWhilePushing = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|Cart|Light", meta=(ClampMin="150.0", ClampMax="1000.0", Units="cm"))
	float LampHeightCm = 320.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|Cart|Light", meta=(ClampMin="10.0", ClampMax="75.0"))
	float ConeHalfAngleDegrees = 55.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|Cart|Light", meta=(ClampMin="150.0", ClampMax="2000.0", Units="cm"))
	float ConeLengthCm = 650.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|Cart|Light", meta=(ClampMin="0.0"))
	float LampIntensityLumens = 18000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|Cart|Appearance")
	TObjectPtr<UStaticMesh> LampHeadMesh;

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void OnPushStateChanged() override;

private:
	UFUNCTION()
	void OnRep_LightState();
	void RefreshLightPresentation();
	bool HasClearLightPath(FVector Target) const;
	void GetSafeCone(FVector& Apex, FVector& Direction, float& Length, float& Angle) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Light", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpotLightComponent> SafetySpotlight;
	UPROPERTY(VisibleAnywhere, Category="Mall|Cart|Light")
	TObjectPtr<UStaticMeshComponent> LampMast;
	UPROPERTY(VisibleAnywhere, Category="Mall|Cart|Light")
	TObjectPtr<UStaticMeshComponent> LampHead;
	UPROPERTY(VisibleAnywhere, Category="Mall|Cart|Charging")
	TObjectPtr<UStaticMeshComponent> ChargingCable;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> FallbackLampHeadMesh;
	UPROPERTY(VisibleAnywhere, Category="Mall|Cart|Light")
	TObjectPtr<UTextRenderComponent> BatteryDisplay;
};
