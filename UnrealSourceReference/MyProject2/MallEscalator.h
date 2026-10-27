#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MallEscalator.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class UPrimitiveComponent;
class UProceduralMeshComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class APlayerController;

UENUM(BlueprintType)
enum class EEscalatorMaintenanceState : uint8
{
	Operational UMETA(DisplayName = "Operational (Moving)"),
	StalledWalkable UMETA(DisplayName = "Stalled (Walkable Stairs)"),
	BrokenBlocked UMETA(DisplayName = "Broken & Blocked (Requires Repair)")
};

/**
 * A configurable escalator. Local +X is uphill, +Z is up, and the lower entrance
 * starts at X=0. Geometry uses centimetres; TravelSpeed is centimetres/second.
 *
 * Supports arbitrary multi-floor heights (including vertigo-inducing high spans) via
 * its procedural truss & glass balustrade generator, as well as Broken/Blocked
 * progression states that require players to install a Relay Fuse or reset a Breaker.
 */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallEscalator : public AActor
{
	GENERATED_BODY()

public:
	AMallEscalator();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Appearance")
	TObjectPtr<UStaticMesh> ShellMeshAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Appearance")
	TObjectPtr<UStaticMesh> StepMeshAsset;

	/** Shell assets should be authored with their lower entrance at local X/Z=0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Appearance")
	FTransform ShellMeshTransform = FTransform::Identity;

	/** Generates a complete steel truss, glass balustrade, and handrail shell for any Rise/Run height when true or when ShellMeshAsset is null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Appearance")
	bool bUseProceduralShell = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Appearance")
	TObjectPtr<UMaterialInterface> StepMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Appearance")
	TObjectPtr<UMaterialInterface> EndPlateMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Appearance")
	TObjectPtr<UMaterialInterface> TrussMetalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Appearance")
	TObjectPtr<UMaterialInterface> BalustradeGlassMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Appearance")
	TObjectPtr<UMaterialInterface> HandrailDarkMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Appearance")
	TObjectPtr<UMaterialInterface> HazardAccentMaterial;

	/** Turn this off if the imported shell already contains entrance/exit plates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Appearance")
	bool ShowEndPlates = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Geometry", meta=(ClampMin="100.0", Units="cm"))
	float Run = 623.54f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Geometry", meta=(ClampMin="0.0", Units="cm"))
	float Rise = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Geometry", meta=(ClampMin="30.0", Units="cm"))
	float LandingLength = 130.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Geometry", meta=(ClampMin="60.0", Units="cm"))
	float BeltWidth = 106.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Geometry", meta=(Units="cm"))
	float BottomDeckHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Geometry", meta=(ClampMin="15.0", Units="cm"))
	float StepSpacing = 40.0f;

	/** Maximum depth; fitted just below the actual loop pitch to prevent flat-plane overlap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Geometry", meta=(ClampMin="15.0", Units="cm"))
	float StepDepth = 40.0f;

	/** Zero derives the riser height from the incline and tread spacing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Geometry", meta=(ClampMin="0.0", Units="cm"))
	float StepHeight = 0.0f;

	/** Nonmoving end sections give the rider a stable place to enter and leave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Geometry", meta=(ClampMin="10.0", Units="cm"))
	float StationaryEndLength = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Motion", meta=(ClampMin="0.0", Units="cm/s"))
	float TravelSpeed = 165.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_MaintenanceOrMotion, Category="Escalator|Motion")
	bool Running = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_MaintenanceOrMotion, Category="Escalator|Motion")
	bool ReverseDirection = false;

	/** Seconds to smoothly start, stop or reverse the belt. Zero is immediate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Motion", meta=(ClampMin="0.0", Units="s"))
	float SpeedChangeTime = 0.5f;

	/** Operational state: Operational (moving), StalledWalkable (stopped stairs), or BrokenBlocked (advancement gate requiring repair). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_MaintenanceOrMotion, Category="Escalator|Maintenance")
	EEscalatorMaintenanceState MaintenanceState = EEscalatorMaintenanceState::Operational;

	/** When true and MaintenanceState is BrokenBlocked, player must carry an Escalator Relay Fuse in their hotbar (or flip a linked Breaker) to repair. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Maintenance")
	bool bRequiresFuseToRepair = true;

	/** Optional circuit tag linking this escalator to an AMallEscalatorBreakerPanel in a hallway or back room. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Escalator|Maintenance")
	FName LinkedBreakerTag = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Escalator|Components")
	TObjectPtr<UStaticMeshComponent> Shell;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Escalator|Components")
	TObjectPtr<UProceduralMeshComponent> ProceduralShell;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Escalator|Components")
	TObjectPtr<UInstancedStaticMeshComponent> Steps;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Escalator|Components")
	TObjectPtr<UBoxComponent> BrokenBlockerBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Escalator|Components")
	TObjectPtr<UPointLightComponent> StatusBeaconLight;

	UFUNCTION(BlueprintCallable, CallInEditor, Category="Escalator|Geometry")
	void RebuildEscalator();

	UFUNCTION(BlueprintCallable, Category="Escalator|Motion")
	void SetRunning(bool NewRunning);

	UFUNCTION(BlueprintCallable, Category="Escalator|Motion")
	void SetReverseDirection(bool NewReverseDirection);

	UFUNCTION(BlueprintCallable, Category="Escalator|Maintenance")
	void SetMaintenanceState(EEscalatorMaintenanceState NewState);

	UFUNCTION(BlueprintCallable, Category="Escalator|Maintenance")
	bool TryRepairEscalator(APlayerController* RepairingPlayer, FString& OutFeedback);

	UFUNCTION(BlueprintPure, Category="Escalator|Maintenance")
	bool IsBrokenBlocked() const { return MaintenanceState == EEscalatorMaintenanceState::BrokenBlocked; }

	UFUNCTION(BlueprintPure, Category="Escalator|Maintenance")
	FVector GetServicePanelWorldLocation() const;

	UFUNCTION(BlueprintPure, Category="Escalator|Motion")
	float GetCurrentBeltSpeed() const { return CurrentBeltSpeed; }

	UFUNCTION(BlueprintPure, Category="Escalator|Geometry")
	float GetTravelLength() const;

	/** Distance runs along the lower landing, incline and upper landing. */
	UFUNCTION(BlueprintPure, Category="Escalator|Geometry")
	FVector GetDeckWorldPoint(float Distance) const;

	UFUNCTION(BlueprintPure, Category="Escalator|Motion")
	FVector GetBeltVelocityAtWorldPosition(FVector WorldPosition) const;

	static AMallEscalator* FindBrokenEscalatorInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_MaintenanceOrMotion();

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> EscalatorRoot;

	UPROPERTY()
	TObjectPtr<UBoxComponent> LowerDeck;

	UPROPERTY()
	TObjectPtr<UBoxComponent> InclineDeck;

	UPROPERTY()
	TObjectPtr<UBoxComponent> UpperDeck;

	UPROPERTY()
	TObjectPtr<UBoxComponent> RiderVolume;

	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> SideGuards;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LowerEndPlate;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> UpperEndPlate;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PlateMesh;

	float CurrentBeltSpeed = 0.0f;
	float PreviousSpeedTarget = 0.0f;
	float SpeedChangeRate = 0.0f;
	double BeltDistance = 0.0;
	int32 TreadCount = 0;
	float ActualTreadSpacing = 40.0f;
	FVector TreadMeshScale = FVector::OneVector;
	FVector TreadMeshPivotOffset = FVector::ZeroVector;

	void RebuildProceduralShell();
	void RefreshMaintenanceState();
	void UpdateTreads();
	void CarryGroundedRiders(float DeltaSeconds);
	bool IsWalkingDeck(const UPrimitiveComponent* Component) const;
	FVector DeckLocalPoint(float Distance) const;
	float DistanceAtLocalX(float LocalX) const;
	float InclineLength() const;
	float SignedTargetSpeed() const;
};
