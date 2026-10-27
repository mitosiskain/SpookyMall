#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Equipment/MallGhostEquipment.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "MallSpaAndFoodSystem.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EMallFoodBuffType : uint8
{
	None UMETA(DisplayName = "None"),
	SpicyHotPot_SprintSurge UMETA(DisplayName = "Chongqing Hot Pot (+25% Speed & Stamina Surge)"),
	MatchaBoba_SpectralStealth UMETA(DisplayName = "Geothermal Matcha Boba (-30% Ghost Detection)"),
	BentoRation_CartOvercharge UMETA(DisplayName = "Pocket-Dimension Bento (+50% Cart Battery Overcharge)"),
	HimalayanSalt_ThermalWard UMETA(DisplayName = "Himalayan Pink Salt Ward (Ghost Chill Shield & +15% Speed)")
};

UENUM(BlueprintType)
enum class EMallSpaSanctuaryType : uint8
{
	HimalayanPinkSaltBath UMETA(DisplayName = "Himalayan Pink Salt Mineral Bath"),
	JadeGeothermalOnsen UMETA(DisplayName = "Jade Geothermal Onsen Pool"),
	HinokiCedarSauna UMETA(DisplayName = "Hinoki Cedar & Loyly Steam Sauna")
};

/**
 * Tracks Player Health, Stamina, Bio-Thermal Energy (from Food Court rations),
 * Himalayan Pink Salt Ward protection, and active combat/movement buffs.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MYPROJECT2_API UMallPlayerVitalityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMallPlayerVitalityComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Mall|Vitality")
	float Health = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Mall|Vitality")
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Mall|Vitality")
	float Stamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Mall|Vitality")
	float MaxStamina = 100.0f;

	/** Bio-Thermal Energy accumulated from eating Food Court meals; can recharge Light Cart batteries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Mall|Vitality")
	float BioEnergy = 50.0f;

	/** Remaining duration of the Himalayan Pink Salt Bath Ward (shields against ghost chill/drain). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Mall|Vitality")
	float HimalayanSaltWardSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Mall|Vitality")
	EMallFoodBuffType ActiveFoodBuff = EMallFoodBuffType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Mall|Vitality")
	float FoodBuffRemainingSeconds = 0.0f;

	/** True while the player is seated and resting on an AMallRestBench (rapid Stamina recovery in 3rd-person view). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Mall|Vitality")
	bool bIsSeatedResting = false;

	UFUNCTION(BlueprintCallable, Category="Mall|Vitality")
	void ApplySpaHealing(float HealAmount, float StaminaAmount, float SaltWardDurationSeconds);

	UFUNCTION(BlueprintCallable, Category="Mall|Vitality")
	void ApplyFoodConsumption(EMallFoodBuffType BuffType, float HealAmount, float StaminaAmount, float EnergyAmount, float BuffDurationSeconds, float CartBatteryBoostFraction);

	UFUNCTION(BlueprintCallable, Category="Mall|Vitality")
	float ApplyGhostChillDamage(float RawDamage);

	UFUNCTION(BlueprintPure, Category="Mall|Vitality")
	float GetGhostDetectionDistanceMultiplier() const;

	UFUNCTION(BlueprintPure, Category="Mall|Vitality")
	FString GetVitalityStatusSummary() const;

	UFUNCTION(BlueprintPure, Category="Mall|Vitality")
	FString GetVitalityHUDStatus() const { return GetVitalityStatusSummary(); }

	UFUNCTION(BlueprintPure, Category="Mall|Vitality")
	bool HasSaltWard() const { return HimalayanSaltWardSeconds > 0.0f; }

	UFUNCTION(BlueprintPure, Category="Mall|Vitality")
	bool HasFoodBuff() const { return ActiveFoodBuff != EMallFoodBuffType::None && FoodBuffRemainingSeconds > 0.0f; }

	static UMallPlayerVitalityComponent* FindOrCreateForPawn(APawn* Pawn);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SyncPawnBlueprintHealth();
	void UpdateMovementBuffs();
	float BaseWalkSpeed = 600.0f;
	bool bCapturedBaseSpeed = false;
};

/**
 * Interactive & Passive Healing Sanctuary placed in the South Bathhouse (Himalayan Pink Salt Baths,
 * Jade Onsen Pools, and Cedar Saunas). Players stepping into the bath or pressing F heal Health/Stamina,
 * purge Ghost Chill, and gain the Himalayan Pink Salt Ward buff.
 */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallSpaHealingZone : public AActor
{
	GENERATED_BODY()

public:
	AMallSpaHealingZone();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Spa")
	TObjectPtr<USceneComponent> ZoneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Spa")
	TObjectPtr<UBoxComponent> HealingVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Spa")
	TObjectPtr<UStaticMeshComponent> SaltCrystalClusterMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Spa")
	TObjectPtr<UTextRenderComponent> SanctuaryTitleText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Spa")
	TObjectPtr<UPointLightComponent> MineralGlowLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Spa")
	EMallSpaSanctuaryType SanctuaryType = EMallSpaSanctuaryType::HimalayanPinkSaltBath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Spa")
	FString SanctuaryDisplayName = TEXT("Himalayan Pink Salt Healing Bath");

	/** Health restored per second while soaking inside the bath volume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Spa")
	float PassiveHealPerSecond = 18.0f;

	/** Instant health restored when pressing F to take a deep mineral soak. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Spa")
	float InteractiveBurstHeal = 45.0f;

	/** Duration of the Himalayan Salt Ward granted by soaking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Spa")
	float SaltWardDurationSeconds = 60.0f;

	UFUNCTION(BlueprintCallable, Category="Mall|Spa")
	void RebuildSanctuaryVisuals();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Spa")
	bool TryInteractiveSoak(APlayerController* Player, FString& OutFeedback);

	UFUNCTION(BlueprintPure, Category="Mall|Spa")
	FString GetHUDPrompt(const APawn* ViewerPawn) const;

	UFUNCTION(BlueprintPure, Category="Mall|Spa")
	bool IsPawnInsideSanctuary(const APawn* Pawn) const;

	static AMallSpaHealingZone* FindSpaZoneInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction);
	static AMallSpaHealingZone* FindSpaZoneInViewOrOverlapping(const APawn* Pawn, const FVector& Origin, const FVector& Direction)
	{
		return FindSpaZoneInView(Pawn, Origin, Direction);
	}
	static AMallSpaHealingZone* FindSpaZoneContainingPawn(const APawn* Pawn);
	static bool IsPawnInsideAnySanctuary(const APawn* Pawn) { return FindSpaZoneContainingPawn(Pawn) != nullptr; }

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SaltCrystalMID;
};

/**
 * Handheld Food & Energy Ration item from the North Night-Market Food Court (subclasses AMallHandheldItem).
 * Visible in the character's right hand with a 3D bowl/cup model, warm steam glow, and diegetic readout.
 * Pressing LMB/R consumes the ration to restore Stamina/Health, grant combat buffs, and recharge nearby Cart batteries!
 */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallFoodRationItem : public AMallHandheldItem
{
	GENERATED_BODY()

public:
	AMallFoodRationItem();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Food")
	TObjectPtr<UStaticMeshComponent> BowlContentsMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_FoodState, Category="Mall|Food")
	EMallFoodBuffType MealBuffType = EMallFoodBuffType::SpicyHotPot_SprintSurge;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_FoodState, Category="Mall|Food")
	int32 ServingsRemaining = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Food")
	float HealPerServing = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Food")
	float StaminaPerServing = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Food")
	float CartBatteryBoostPerServing = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Food")
	float BuffDurationSeconds = 90.0f;

	UFUNCTION(BlueprintCallable, Category="Mall|Food")
	void ConfigureMealType(EMallFoodBuffType NewType);

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
	void OnRep_FoodState();

	virtual void UpdateDeviceVisuals() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BrothMID;
	FString LastConsumeFeedback;
};

/**
 * Interactive Mall Rest Bench Actor.
 * Players can press [F] or [E] at any bench to sit down, switch to Third-Person Camera View to see their
 * character sitting and resting, and rapidly recover Stamina (+24%/sec) and Health (+8 HP/sec).
 * Pressing [F], [E], [G], [Space], or movement keys stands the player back up and restores First-Person View.
 */
UCLASS(BlueprintType, Blueprintable)
class MYPROJECT2_API AMallRestBench : public AActor
{
	GENERATED_BODY()

public:
	AMallRestBench();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Bench")
	TObjectPtr<USceneComponent> BenchRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Bench")
	TObjectPtr<UStaticMeshComponent> BenchMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Bench")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Bench")
	FString BenchDisplayName = TEXT("Mall Concourse Rest Bench");

	/** Stamina restored per second while seated on this bench. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Bench")
	float SeatedStaminaRegenPerSecond = 24.0f;

	/** Health restored per second while seated on this bench. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Bench")
	float SeatedHealthRegenPerSecond = 8.0f;

	/** Local seat height & offset for the seated character capsule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Bench")
	FVector SeatLocalOffset = FVector(0.0f, 14.0f, 78.0f);

	/** Local stand-up offset in front of the bench. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Bench")
	FVector StandUpLocalOffset = FVector(0.0f, 92.0f, 92.0f);

	UFUNCTION(BlueprintPure, Category="Mall|Bench")
	bool IsOccupied() const { return IsValid(OccupantPawn); }

	UFUNCTION(BlueprintPure, Category="Mall|Bench")
	bool IsOccupiedBy(const APawn* Pawn) const { return IsValid(Pawn) && OccupantPawn == Pawn; }

	UFUNCTION(BlueprintPure, Category="Mall|Bench")
	APawn* GetOccupantPawn() const { return OccupantPawn; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Bench")
	bool TrySitDown(APlayerController* Player, FString& OutFeedback);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Bench")
	bool TryStandUp(APlayerController* Player, FString& OutFeedback);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Bench")
	bool TryToggleSit(APlayerController* Player, FString& OutFeedback);

	UFUNCTION(BlueprintPure, Category="Mall|Bench")
	FString GetHUDPrompt(const APawn* ViewerPawn) const;

	static AMallRestBench* FindBenchInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction);
	static AMallRestBench* FindBenchOccupiedBy(const APawn* Pawn);
	static void ApplySeatedCharacterPosture(APawn* Pawn, bool bSeated);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_OccupantPawn(APawn* PreviousOccupant);

private:
	UPROPERTY(ReplicatedUsing=OnRep_OccupantPawn)
	TObjectPtr<APawn> OccupantPawn;

	FTransform OccupantEntryTransform;
	float OccupantSeatSideSign = 1.0f;
	double SitStartTimeSeconds = 0.0;
};
