#pragma once

#include "CoreMinimal.h"
#include "Equipment/MallCartItem.h"
#include "GameFramework/Actor.h"
#include "MallGhostEquipment.generated.h"

class AMallPushCart;
class AMallRetailStoreUnit;
class APawn;
class APlayerController;
class UAudioComponent;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
class USpotLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UTexture2D;

/**
 * Shared server-authoritative base class for handheld and placeable ghost-hunting equipment.
 * Supports 4-slot hotbar integration (BP_InventoryComponent), AMallPushCart rack storage (IMallCartItem),
 * surface placement, dropping, primary action (LMB / R), and secondary mode toggle (E).
 */
UCLASS(Abstract, Blueprintable)
class MYPROJECT2_API AMallHandheldItem : public AActor, public IMallCartItem
{
	GENERATED_BODY()

public:
	AMallHandheldItem();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Equipment")
	TObjectPtr<USceneComponent> ItemRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Equipment")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Equipment")
	TObjectPtr<UStaticMeshComponent> DeviceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Equipment")
	TObjectPtr<UStaticMeshComponent> GripMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Equipment")
	TObjectPtr<UStaticMeshComponent> ScreenBackdropMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Equipment")
	TObjectPtr<UTextRenderComponent> ReadoutText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Equipment")
	TObjectPtr<UPointLightComponent> StatusLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Equipment")
	TObjectPtr<UAudioComponent> DeviceAudio;

	/** Display name shown in the 4-slot hotbar and HUD interaction prompts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Equipment")
	FText DisplayName = FText::FromString(TEXT("Ghost Equipment"));

	/** Icon displayed in the player's 4-slot hotbar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Equipment")
	TObjectPtr<UTexture2D> Icon;

	/** Shop/inventory value reported to BP_InventoryComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Equipment", meta=(ClampMin="0.0", ClampMax="1000.0"))
	float ItemValue = 25.0f;

	/** Relative viewmodel offset from the first-person eye camera when carried and selected by a player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Equipment")
	FVector CarriedViewOffset = FVector(38.0, 16.0, -13.0);

	/** Relative rotation offset when carried and selected by a player in first-person view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Equipment")
	FRotator CarriedViewRotationOffset = FRotator(12.0, 180.0, 0.0);

	/** Half-extent used to validate level tabletop or floor support when placing with F or dropping with G. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Equipment")
	FVector PlacementHalfExtent = FVector(10.0, 8.0, 8.0);

	/** Whether players can trigger primary/secondary use while aiming at this item placed in the world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Equipment")
	bool bCanUseWhilePlaced = true;

	UPROPERTY(ReplicatedUsing=OnRep_ItemState, BlueprintReadOnly, Category="Mall|Equipment")
	TObjectPtr<APawn> Carrier;

	UPROPERTY(ReplicatedUsing=OnRep_ItemState, BlueprintReadOnly, Category="Mall|Equipment")
	TObjectPtr<AMallPushCart> StoredOnCart;

	UPROPERTY(ReplicatedUsing=OnRep_ItemState, BlueprintReadOnly, Category="Mall|Equipment")
	int32 CarriedHotbarSlot = INDEX_NONE;

	UPROPERTY(ReplicatedUsing=OnRep_ItemState, BlueprintReadOnly, Category="Mall|Equipment")
	int32 CarrierSelectedSlot = INDEX_NONE;

	UPROPERTY(ReplicatedUsing=OnRep_ItemState, BlueprintReadOnly, Category="Mall|Equipment")
	bool bPowered = false;

	UFUNCTION(BlueprintPure, Category="Mall|Equipment")
	APawn* GetCarrier() const;

	UFUNCTION(BlueprintPure, Category="Mall|Equipment")
	bool IsPowered() const;

	UFUNCTION(BlueprintPure, Category="Mall|Equipment")
	bool IsSelectedByCarrier() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Equipment")
	void SetCarrierSelectedSlot(int32 NewSelectedSlot);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Equipment")
	bool CanPickup(APlayerController* Player) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Equipment")
	virtual bool TryPickup(APlayerController* Player);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Equipment")
	virtual bool TryPlace(APlayerController* Player, const FTransform& TargetTransform);

	/** Primary action (LMB or R). By default toggles power; overridden by Photo Camera to snap a photo. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Equipment")
	virtual bool TryPrimaryUse(APlayerController* Player);

	/** Secondary action (E when not aiming at a cart/charger). Cycles device mode, units, or mute state. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Equipment")
	virtual bool TrySecondaryUse(APlayerController* Player);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Equipment")
	virtual bool TrySetPowered(APlayerController* Player, bool bNewPowered);

	UFUNCTION(BlueprintPure, Category="Mall|Equipment")
	virtual FString GetHUDPrompt(bool bIsHeld) const;

	UFUNCTION(BlueprintPure, Category="Mall|Equipment")
	virtual FString GetHUDDetail() const;

	UFUNCTION(BlueprintPure, Category="Mall|Equipment")
	virtual FString GetPickupFeedback() const;

	UFUNCTION(BlueprintPure, Category="Mall|Equipment")
	virtual FString GetPlaceFeedback() const;

	UFUNCTION(BlueprintPure, Category="Mall|Equipment")
	virtual FString GetPrimaryUseFeedback() const;

	UFUNCTION(BlueprintPure, Category="Mall|Equipment")
	virtual FString GetSecondaryUseFeedback() const;

	virtual APawn* GetCartItemCarrier_Implementation() const override;
	virtual AMallPushCart* GetCartStorage_Implementation() const override;
	virtual bool TryStoreOnCart_Implementation(APlayerController* Player, AMallPushCart* Cart, const FTransform& SlotTransform) override;
	virtual bool TryTakeFromCart_Implementation(APlayerController* Player, AMallPushCart* Cart) override;
	virtual void ReleaseFromCart_Implementation(AMallPushCart* Cart, const FTransform& FallbackWorldTransform) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	virtual void OnRep_ItemState();

	virtual void UpdateDeviceVisuals();
	virtual void TickServerDetection(float DeltaSeconds);
	virtual void TickClientEffects(float DeltaSeconds);

	bool CanPlayerControlDevice(APlayerController* Player) const;
	void GetActiveAimRay(FVector& OutOrigin, FVector& OutDirection) const;
	void ApplyCarrierPresentation();
	void SyncHotbarInventory();
	void FollowCarrier();
	void RecoverWorldPlacement();

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> SharedAttenuation;

	FTransform SafeWorldTransform;
	bool bHasSafeWorldTransform = false;
	TWeakObjectPtr<APawn> LastHotbarCarrier;
	int32 LastHotbarSlot = INDEX_NONE;
};

/**
 * 5-LED Electromagnetic Field (EMF) Reader.
 * Detects ghost proximity, ghost phase (Warning/Hunting), disturbed props, ghost writing, and EMF-5 evidence.
 */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallEMFReader : public AMallHandheldItem
{
	GENERATED_BODY()

public:
	AMallEMFReader();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|EMF")
	TArray<TObjectPtr<UStaticMeshComponent>> LEDMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> LEDMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|EMF")
	TObjectPtr<USoundBase> BeepSound;

	/** Outer radius where an active store ghost triggers Level 2 EMF. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|EMF", meta=(ClampMin="200.0", ClampMax="3000.0"))
	float MaxDetectionRangeCm = 1200.0f;

	/** Medium radius where a ghost or disturbance triggers Level 3 EMF. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|EMF", meta=(ClampMin="150.0", ClampMax="2000.0"))
	float MediumRangeCm = 650.0f;

	/** Close radius where a ghost triggers Level 4 EMF. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|EMF", meta=(ClampMin="100.0", ClampMax="1200.0"))
	float CloseRangeCm = 360.0f;

	/** Close radius where an EMF-5 ghost or fresh disturbance triggers Level 5 EMF evidence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|EMF", meta=(ClampMin="80.0", ClampMax="800.0"))
	float EMF5RangeCm = 260.0f;

	/** Seconds after a ghost disturbance (write/move/throw) during which residual EMF spikes remain detectable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|EMF", meta=(ClampMin="1.0", ClampMax="60.0"))
	float DisturbanceMemorySeconds = 10.0f;

	/** Current EMF reading (0 = off, 1 = baseline, 2..5 = activity/evidence). */
	UPROPERTY(ReplicatedUsing=OnRep_EMFState, BlueprintReadOnly, Category="Mall|EMF")
	int32 CurrentEMFLevel = 0;

	/** Highest EMF level recorded while powered on. */
	UPROPERTY(ReplicatedUsing=OnRep_EMFState, BlueprintReadOnly, Category="Mall|EMF")
	int32 PeakEMFLevel = 0;

	/** Latched true once Level 5 EMF evidence has been detected. */
	UPROPERTY(ReplicatedUsing=OnRep_EMFState, BlueprintReadOnly, Category="Mall|EMF")
	bool bEMF5Detected = false;

	/** Whether audible Geiger/EMF beeps are muted (toggled via E). */
	UPROPERTY(ReplicatedUsing=OnRep_EMFState, BlueprintReadOnly, Category="Mall|EMF")
	bool bAudioMuted = false;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|EMF")
	int32 EvaluateEMFLevel() const;

	virtual bool TrySetPowered(APlayerController* Player, bool bNewPowered) override;
	virtual bool TrySecondaryUse(APlayerController* Player) override;
	virtual FString GetHUDPrompt(bool bIsHeld) const override;
	virtual FString GetHUDDetail() const override;
	virtual FString GetPickupFeedback() const override;
	virtual FString GetPrimaryUseFeedback() const override;
	virtual FString GetSecondaryUseFeedback() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_EMFState();

	virtual void UpdateDeviceVisuals() override;
	virtual void TickServerDetection(float DeltaSeconds) override;
	virtual void TickClientEffects(float DeltaSeconds) override;

private:
	double NextBeepTime = 0.0;
};

/**
 * Ultraviolet (UV) Evidence Print (Footprint, Handprint, or Ectoplasmic Residue).
 * Remains invisible to the naked eye and normal flashlights, glowing fluorescent green when illuminated by AMallUVLight.
 */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallUVEvidence : public AActor
{
	GENERATED_BODY()

public:
	AMallUVEvidence();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|UV")
	TObjectPtr<USceneComponent> EvidenceRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|UV")
	TObjectPtr<UStaticMeshComponent> EvidenceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|UV")
	TObjectPtr<UTextRenderComponent> EvidenceLabel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|UV")
	TObjectPtr<UPointLightComponent> GlowLight;

	/** Descriptive label shown when revealed under UV light (e.g. "UV Handprint", "UV Footprint"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|UV")
	FString PrintTitle = TEXT("UV Footprint");

	/** Phosphorescent afterglow duration in seconds after the UV beam moves off the print. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|UV", meta=(ClampMin="0.2", ClampMax="15.0"))
	float AfterglowDurationSeconds = 2.5f;

	UPROPERTY(ReplicatedUsing=OnRep_UVState, BlueprintReadOnly, Category="Mall|UV")
	bool bIlluminatedByUV = false;

	UPROPERTY(ReplicatedUsing=OnRep_UVState, BlueprintReadOnly, Category="Mall|UV")
	bool bEverDiscovered = false;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|UV")
	void NotifyUVIlluminated();

	UFUNCTION(BlueprintPure, Category="Mall|UV")
	bool IsCurrentlyGlowing() const { return bIlluminatedByUV; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_UVState();

	void RefreshVisibility();

private:
	double IlluminatedUntilServerTime = 0.0;
};

/**
 * Handheld / Placeable Ultraviolet (UV) Blacklight.
 * Projects a deep-violet spotlight cone that reveals hidden AMallUVEvidence prints and spawns footprints/handprints
 * when the store ghost moves or disturbs props.
 */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallUVLight : public AMallHandheldItem
{
	GENERATED_BODY()

public:
	AMallUVLight();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|UV")
	TObjectPtr<USpotLightComponent> UVSpotLight;

	/** Focused long-range beam length in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|UV", meta=(ClampMin="200.0", ClampMax="2500.0"))
	float FocusedRangeCm = 950.0f;

	/** Focused beam outer cone half-angle in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|UV", meta=(ClampMin="8.0", ClampMax="60.0"))
	float FocusedHalfAngleDegrees = 22.0f;

	/** Wide flood beam length in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|UV", meta=(ClampMin="150.0", ClampMax="1800.0"))
	float WideFloodRangeCm = 620.0f;

	/** Wide flood outer cone half-angle in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|UV", meta=(ClampMin="15.0", ClampMax="75.0"))
	float WideFloodHalfAngleDegrees = 40.0f;

	/** Maximum dynamically spawned UV prints per haunted store. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|UV", meta=(ClampMin="1", ClampMax="16"))
	int32 MaxDynamicPrintsPerStore = 6;

	/** False = Focused Long-Range Beam, True = Wide Flood Beam (toggled via E). */
	UPROPERTY(ReplicatedUsing=OnRep_UVLightState, BlueprintReadOnly, Category="Mall|UV")
	bool bWideFloodMode = false;

	/** Count of UV evidence prints currently illuminated by this blacklight. */
	UPROPERTY(ReplicatedUsing=OnRep_UVLightState, BlueprintReadOnly, Category="Mall|UV")
	int32 IlluminatedPrintCount = 0;

	/** Total unique UV prints revealed by this blacklight. */
	UPROPERTY(ReplicatedUsing=OnRep_UVLightState, BlueprintReadOnly, Category="Mall|UV")
	int32 TotalPrintsRevealed = 0;

	UFUNCTION(BlueprintPure, Category="Mall|UV")
	float GetActiveBeamRangeCm() const { return bWideFloodMode ? WideFloodRangeCm : FocusedRangeCm; }

	UFUNCTION(BlueprintPure, Category="Mall|UV")
	float GetActiveBeamHalfAngleDegrees() const { return bWideFloodMode ? WideFloodHalfAngleDegrees : FocusedHalfAngleDegrees; }

	virtual bool TrySecondaryUse(APlayerController* Player) override;
	virtual FString GetHUDPrompt(bool bIsHeld) const override;
	virtual FString GetHUDDetail() const override;
	virtual FString GetPickupFeedback() const override;
	virtual FString GetPrimaryUseFeedback() const override;
	virtual FString GetSecondaryUseFeedback() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_UVLightState();

	virtual void UpdateDeviceVisuals() override;
	virtual void TickServerDetection(float DeltaSeconds) override;

private:
	void TrySpawnDynamicGhostPrints();
	TSet<TWeakObjectPtr<AMallUVEvidence>> DiscoveredSet;
	int32 LastObservedGhostDisturbanceCount = 0;
	double NextFootprintSpawnTime = 0.0;
};

/**
 * Handheld / Placeable Thermal & Cold-Spot Scanner (Digital IR Thermometer).
 * Reads normal ambient mall temperatures (~20.5 C), cold spots inside haunted stores (2-7 C),
 * and sub-zero Freezing Temperatures (< 0.0 C down to -8.5 C) near the ghost or recent disturbances.
 */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallThermometer : public AMallHandheldItem
{
	GENERATED_BODY()

public:
	AMallThermometer();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Thermometer")
	TObjectPtr<UStaticMeshComponent> ThermalBarMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ThermalBarMID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Thermometer")
	TObjectPtr<USoundBase> FreezeAlertSound;

	/** Normal mall baseline temperature in Celsius outside haunted stores. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Thermometer")
	float BaselineTemperatureCelsius = 20.5f;

	/** Ambient temperature in Celsius inside an active haunted store. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Thermometer")
	float HauntedStoreAmbientCelsius = 5.5f;

	/** Minimum sub-zero temperature in Celsius when aimed at or standing near a freezing ghost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Thermometer")
	float FreezingGhostTemperatureCelsius = -7.8f;

	/** Maximum infrared scan range in centimeters for detecting aimed cold spots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Thermometer", meta=(ClampMin="150.0", ClampMax="2500.0"))
	float IRScanRangeCm = 850.0f;

	/** Distance in centimeters within which the ghost causes freezing readings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Thermometer", meta=(ClampMin="100.0", ClampMax="1200.0"))
	float FreezingProximityCm = 420.0f;

	UPROPERTY(ReplicatedUsing=OnRep_ThermalState, BlueprintReadOnly, Category="Mall|Thermometer")
	float CurrentTemperatureCelsius = 20.5f;

	UPROPERTY(ReplicatedUsing=OnRep_ThermalState, BlueprintReadOnly, Category="Mall|Thermometer")
	float LowestRecordedCelsius = 20.5f;

	UPROPERTY(ReplicatedUsing=OnRep_ThermalState, BlueprintReadOnly, Category="Mall|Thermometer")
	bool bFreezingDetected = false;

	/** False = Celsius (C), True = Fahrenheit (F). Toggled via E. */
	UPROPERTY(ReplicatedUsing=OnRep_ThermalState, BlueprintReadOnly, Category="Mall|Thermometer")
	bool bUseFahrenheit = false;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Thermometer")
	float EvaluateTemperatureCelsius() const;

	UFUNCTION(BlueprintPure, Category="Mall|Thermometer")
	FString FormatTemperature(float TempCelsius) const;

	virtual bool TrySetPowered(APlayerController* Player, bool bNewPowered) override;
	virtual bool TrySecondaryUse(APlayerController* Player) override;
	virtual FString GetHUDPrompt(bool bIsHeld) const override;
	virtual FString GetHUDDetail() const override;
	virtual FString GetPickupFeedback() const override;
	virtual FString GetPrimaryUseFeedback() const override;
	virtual FString GetSecondaryUseFeedback() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_ThermalState();

	virtual void UpdateDeviceVisuals() override;
	virtual void TickServerDetection(float DeltaSeconds) override;
};

/**
 * Placeable / Handheld Infrared Motion & Presence Sensor.
 * Detects store ghosts passing through its forward detection cone even while the ghost is invisible (InvisibleDisturbance).
 */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallMotionSensor : public AMallHandheldItem
{
	GENERATED_BODY()

public:
	AMallMotionSensor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|MotionSensor")
	TObjectPtr<USoundBase> AlertChimeSound;

	/** Forward IR beam detection range in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|MotionSensor", meta=(ClampMin="100.0", ClampMax="1500.0"))
	float DetectionRangeCm = 480.0f;

	/** Half-angle in degrees of the forward IR detection cone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|MotionSensor", meta=(ClampMin="15.0", ClampMax="180.0"))
	float DetectionHalfAngleDegrees = 60.0f;

	/** Omni-directional close proximity radius in centimeters around the sensor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|MotionSensor", meta=(ClampMin="50.0", ClampMax="600.0"))
	float OmniProximityRadiusCm = 200.0f;

	UPROPERTY(ReplicatedUsing=OnRep_MotionState, BlueprintReadOnly, Category="Mall|MotionSensor")
	bool bMotionDetected = false;

	UPROPERTY(ReplicatedUsing=OnRep_MotionState, BlueprintReadOnly, Category="Mall|MotionSensor")
	int32 TriggerCount = 0;

	UPROPERTY(ReplicatedUsing=OnRep_MotionState, BlueprintReadOnly, Category="Mall|MotionSensor")
	bool bSilentAlarm = false;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|MotionSensor")
	bool EvaluateGhostMotion() const;

	virtual bool TrySecondaryUse(APlayerController* Player) override;
	virtual FString GetHUDPrompt(bool bIsHeld) const override;
	virtual FString GetHUDDetail() const override;
	virtual FString GetPickupFeedback() const override;
	virtual FString GetPlaceFeedback() const override;
	virtual FString GetPrimaryUseFeedback() const override;
	virtual FString GetSecondaryUseFeedback() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_MotionState();

	virtual void UpdateDeviceVisuals() override;
	virtual void TickServerDetection(float DeltaSeconds) override;

private:
	double LastTriggerServerTime = -100.0;
};

/**
 * Paranormal Evidence Photo Camera.
 * Fires a xenon flash burst on primary use (LMB / R) and evaluates line-of-sight targets in the camera frustum,
 * scoring photos from 0 to 5 stars (Visible Ghost, UV Footprint, Ectoplasmic Silhouette, Ghost Writing, High EMF, Disturbed Prop).
 */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallPhotoCamera : public AMallHandheldItem
{
	GENERATED_BODY()

public:
	AMallPhotoCamera();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|PhotoCamera")
	TObjectPtr<UPointLightComponent> XenonFlashLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|PhotoCamera")
	TObjectPtr<USoundBase> ShutterSound;

	/** Maximum range in centimeters for capturing paranormal evidence in a photo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|PhotoCamera", meta=(ClampMin="200.0", ClampMax="3000.0"))
	float MaxPhotoRangeCm = 1200.0f;

	/** Half-angle in degrees of the camera lens frustum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|PhotoCamera", meta=(ClampMin="10.0", ClampMax="75.0"))
	float ViewConeHalfAngleDegrees = 35.0f;

	/** Maximum exposures available in the camera roll. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|PhotoCamera", meta=(ClampMin="1", ClampMax="50"))
	int32 MaxPhotos = 10;

	/** Minimum recharge time in seconds between flash photos. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|PhotoCamera", meta=(ClampMin="0.2", ClampMax="10.0"))
	float FlashCooldownSeconds = 1.0f;

	/** Duration in seconds that a close flash photo pauses store ghost disturbances. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|PhotoCamera", meta=(ClampMin="0.0", ClampMax="15.0"))
	float GhostFlashPauseSeconds = 4.0f;

	UPROPERTY(ReplicatedUsing=OnRep_CameraState, BlueprintReadOnly, Category="Mall|PhotoCamera")
	int32 PhotosTaken = 0;

	UPROPERTY(ReplicatedUsing=OnRep_CameraState, BlueprintReadOnly, Category="Mall|PhotoCamera")
	int32 LastPhotoStars = 0;

	UPROPERTY(ReplicatedUsing=OnRep_CameraState, BlueprintReadOnly, Category="Mall|PhotoCamera")
	int32 LastPhotoReward = 0;

	UPROPERTY(ReplicatedUsing=OnRep_CameraState, BlueprintReadOnly, Category="Mall|PhotoCamera")
	int32 TotalPhotoValue = 0;

	UPROPERTY(ReplicatedUsing=OnRep_CameraState, BlueprintReadOnly, Category="Mall|PhotoCamera")
	FString LastPhotoSubject;

	UPROPERTY(ReplicatedUsing=OnRep_CameraState, BlueprintReadOnly, Category="Mall|PhotoCamera")
	bool bReviewLogMode = false;

	UPROPERTY(ReplicatedUsing=OnRep_CameraState, BlueprintReadOnly, Category="Mall|PhotoCamera")
	bool bFlashBurstActive = false;

	/** Snaps a photo along the active aim ray and evaluates paranormal subjects in view. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|PhotoCamera")
	bool TryCapturePhoto(APlayerController* Player);

	virtual bool TryPrimaryUse(APlayerController* Player) override;
	virtual bool TrySecondaryUse(APlayerController* Player) override;
	virtual FString GetHUDPrompt(bool bIsHeld) const override;
	virtual FString GetHUDDetail() const override;
	virtual FString GetPickupFeedback() const override;
	virtual FString GetPrimaryUseFeedback() const override;
	virtual FString GetSecondaryUseFeedback() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_CameraState();

	virtual void UpdateDeviceVisuals() override;
	virtual void TickServerDetection(float DeltaSeconds) override;

private:
	void ScoreBestSubjectInFrame(const FVector& Origin, const FVector& Direction, int32& OutStars, int32& OutReward, FString& OutSubject, AActor*& OutMatchedGhost) const;
	bool HasCameraLineOfSight(const FVector& Origin, const FVector& TargetLocation, const AActor* AllowedTarget) const;
	double NextShutterAllowedTime = 0.0;
	double FlashEndsServerTime = 0.0;
};

/**
 * Physical 3D Mall Light Fixture Actor.
 * Replaces bare invisible PointLight actors across the mall so every light source originates from a visible,
 * raycast-aimable 3D luminaire housing & diffuser bulb with smooth "gray-space" falloff and remote-gun control.
 */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallLightFixture : public AActor
{
	GENERATED_BODY()

public:
	AMallLightFixture();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|LightFixture")
	TObjectPtr<USceneComponent> FixtureRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|LightFixture")
	TObjectPtr<UBoxComponent> AimBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|LightFixture")
	TObjectPtr<UStaticMeshComponent> FixtureHousingMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|LightFixture")
	TObjectPtr<UStaticMeshComponent> GlowingBulbMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|LightFixture")
	TObjectPtr<UStaticMeshComponent> SuspensionStemLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|LightFixture")
	TObjectPtr<UStaticMeshComponent> SuspensionStemRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|LightFixture")
	TObjectPtr<UStaticMeshComponent> CeilingCanopyMesh;

	/** Primary directional architectural spotlight cone (prevents omnidirectional circular wall blobs). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|LightFixture")
	TObjectPtr<USpotLightComponent> FixtureSpotLight;

	/** Subtle 6% local housing/canopy ambient bounce fill. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|LightFixture")
	TObjectPtr<UPointLightComponent> FixturePointLight;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BulbMID;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CachedTrackMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CachedCylinderMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CachedCubeMesh;

	/** Human-readable fixture name shown on the Debugging Light Remote Gun readout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|LightFixture")
	FString FixtureLabel = TEXT("Mall Ceiling Light Fixture");

	/** Baseline lumen output when powered at 100% (Well-Lit mode). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|LightFixture", meta=(ClampMin="10.0", ClampMax="10000.0"))
	float DefaultIntensityLumens = 420.0f;

	/** Smooth attenuation radius in centimeters (creates gradual gray-space falloff between lit and dark areas). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|LightFixture", meta=(ClampMin="150.0", ClampMax="5000.0"))
	float DefaultAttenuationRadius = 1250.0f;

	/** Light color tint for both the directional spotlight and the 3D diffuser bulb mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|LightFixture")
	FLinearColor DefaultLightColor = FLinearColor(1.0f, 0.94f, 0.86f);

	/** Mounting mode: 0 = Flush Ceiling Track, 1 = Suspended Pendant with Steel Conduit Stems, 2 = Grounded Landscape/Pool Bollard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|LightFixture")
	uint8 MountType = 0;

	/** Vertical suspension drop distance from ceiling slab in cm (when > 8 cm, dual steel conduit stems connect the fixture up to the ceiling). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|LightFixture", meta=(ClampMin="0.0", ClampMax="1200.0"))
	float CeilingDropCm = 0.0f;

	/** Relative aim rotation of the directional spotlight cone (default -90 pitch straight down; can angle toward storefronts/trees/pools). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|LightFixture")
	FRotator SpotAimRotation = FRotator(-90.0f, 0.0f, 0.0f);

	/** Inner beam cone angle in degrees for architectural spotlighting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|LightFixture", meta=(ClampMin="5.0", ClampMax="80.0"))
	float SpotInnerConeAngle = 28.0f;

	/** Outer beam cone angle in degrees for smooth gray-space penumbra transition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|LightFixture", meta=(ClampMin="15.0", ClampMax="88.0"))
	float SpotOuterConeAngle = 60.0f;

	/** Whether this light fixture is currently switched ON. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|LightFixture")
	bool bLightPowered = true;

	/** Brightness level: 2 = 100% Well-Lit, 1 = 45% Sorta-Lit Gray Space, 0 = OFF. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_LightState, Category="Mall|LightFixture")
	uint8 BrightnessMode = 2;

	UFUNCTION(BlueprintCallable, Category="Mall|LightFixture")
	void ConfigureFixture(const FString& InLabel, float InIntensityLumens, float InAttenuationRadius, FLinearColor InColor, bool bInPowered = true);

	UFUNCTION(BlueprintCallable, Category="Mall|LightFixture")
	void ConfigureArchitecturalFixture(
		const FString& InLabel,
		float InIntensityLumens,
		float InAttenuationRadius,
		FLinearColor InColor,
		uint8 InMountType,
		float InCeilingDropCm,
		FRotator InSpotAimRotation,
		float InInnerConeAngle = 28.0f,
		float InOuterConeAngle = 60.0f,
		bool bInPowered = true);

	UFUNCTION(BlueprintCallable, Category="Mall|LightFixture")
	void SetLightPowered(bool bNewPowered, uint8 NewBrightnessMode = 2);

	UFUNCTION(BlueprintCallable, Category="Mall|LightFixture")
	bool ToggleLight();

	UFUNCTION(BlueprintCallable, Category="Mall|LightFixture")
	uint8 CycleBrightnessMode();

	UFUNCTION(BlueprintPure, Category="Mall|LightFixture")
	FString GetFixtureStatusText() const;

	UFUNCTION(BlueprintCallable, Category="Mall|LightFixture")
	void UpdateFixtureVisuals();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_LightState();
};

/**
 * Handheld Debugging Light Remote Gun.
 * Can be aimed at any physical 3D light fixture in the mall (AMallLightFixture or AMallRetailStoreUnit ceiling light)
 * to toggle lights ON/OFF (LMB / R) or cycle brightness (100% Well-Lit -> 45% Gray-Space Dim -> OFF) with E.
 */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallLightRemoteGun : public AMallHandheldItem
{
	GENERATED_BODY()

public:
	AMallLightRemoteGun();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|LightRemoteGun")
	TObjectPtr<UStaticMeshComponent> BarrelMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|LightRemoteGun")
	TObjectPtr<USpotLightComponent> LaserPointerLight;

	/** Maximum raycast & sweep range in centimeters for targeting ceiling/wall light fixtures. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|LightRemoteGun", meta=(ClampMin="500.0", ClampMax="12000.0"))
	float MaxRemoteRangeCm = 6000.0f;

	UPROPERTY(ReplicatedUsing=OnRep_RemoteGunState, BlueprintReadOnly, Category="Mall|LightRemoteGun")
	FString AimedTargetTitle;

	UPROPERTY(ReplicatedUsing=OnRep_RemoteGunState, BlueprintReadOnly, Category="Mall|LightRemoteGun")
	bool bAimedTargetPowered = false;

	UPROPERTY(ReplicatedUsing=OnRep_RemoteGunState, BlueprintReadOnly, Category="Mall|LightRemoteGun")
	uint8 AimedTargetBrightnessMode = 0;

	UPROPERTY(ReplicatedUsing=OnRep_RemoteGunState, BlueprintReadOnly, Category="Mall|LightRemoteGun")
	FString LastActionFeedback;

	UPROPERTY(ReplicatedUsing=OnRep_RemoteGunState, BlueprintReadOnly, Category="Mall|LightRemoteGun")
	int32 TotalLightsToggled = 0;

	/** Resolves the light fixture or store ceiling light aimed at along Origin + Direction. */
	bool FindAimedLightTarget(
		const FVector& Origin,
		const FVector& Direction,
		AMallLightFixture*& OutFixture,
		AMallRetailStoreUnit*& OutStoreUnit,
		FString& OutTargetName,
		bool& bOutIsOn,
		uint8& OutBrightnessMode) const;

	virtual bool TryPrimaryUse(APlayerController* Player) override;
	virtual bool TrySecondaryUse(APlayerController* Player) override;
	virtual FString GetHUDPrompt(bool bIsHeld) const override;
	virtual FString GetHUDDetail() const override;
	virtual FString GetPickupFeedback() const override;
	virtual FString GetPrimaryUseFeedback() const override;
	virtual FString GetSecondaryUseFeedback() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_RemoteGunState();

	virtual void UpdateDeviceVisuals() override;
	virtual void TickServerDetection(float DeltaSeconds) override;
};
