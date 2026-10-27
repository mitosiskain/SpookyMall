#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MallHidingSpot.generated.h"

class APawn;
class APlayerController;
class UBoxComponent;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EMallHidingSpotType : uint8
{
	FittingRoomBooth UMETA(DisplayName = "Enclosed Fitting Room Cubicle Booth"),
	CircularClothingRack UMETA(DisplayName = "Dense Circular Clothing Rack"),
	SaunaWoodenBooth UMETA(DisplayName = "Steamy Cedar Sauna Privacy Booth"),
	SleepPodBunk UMETA(DisplayName = "Curtained Mattress & Sleep-Pod Bunk"),
	ServiceUtilityCloset UMETA(DisplayName = "Back-of-House Janitor & Utility Closet")
};

/**
 * Real-world Mall Hiding Spot Actor.
 * Allows players to duck inside enclosed fitting room cubicles, circular clothing racks,
 * cedar sauna booths, sleep-pod bunks, and BOH utility closets (via F) to break line-of-sight
 * and evade hunting Store Ghosts—unless a hunting ghost directly witnesses the player entering at close range.
 */
UCLASS(BlueprintType, Blueprintable)
class MYPROJECT2_API AMallHidingSpot : public AActor
{
	GENERATED_BODY()

public:
	AMallHidingSpot();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|HidingSpot")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|HidingSpot")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|HidingSpot")
	TObjectPtr<UStaticMeshComponent> EnclosureMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|HidingSpot")
	TObjectPtr<UStaticMeshComponent> SecondaryCoverMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|HidingSpot")
	TObjectPtr<UTextRenderComponent> SpotSignText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|HidingSpot")
	TObjectPtr<UPointLightComponent> IndicatorLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|HidingSpot")
	EMallHidingSpotType HidingSpotType = EMallHidingSpotType::FittingRoomBooth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|HidingSpot")
	FString SpotDisplayName = TEXT("Fitting Room Booth");

	/** Local position where the hidden pawn stands/crouches while inside. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|HidingSpot")
	FVector HiddenLocalOffset = FVector(0.0f, 0.0f, 92.0f);

	/** Local position outside the entrance where the pawn steps out upon exiting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|HidingSpot")
	FVector ExitLocalOffset = FVector(0.0f, -135.0f, 92.0f);

	UPROPERTY(ReplicatedUsing = OnRep_OccupantState, BlueprintReadOnly, Category = "Mall|HidingSpot")
	TObjectPtr<APawn> OccupantPawn;

	/** True when the occupant entered without a hunting ghost having close unobstructed line-of-sight. */
	UPROPERTY(ReplicatedUsing = OnRep_OccupantState, BlueprintReadOnly, Category = "Mall|HidingSpot")
	bool bOccupiedWithoutWitness = false;

	UFUNCTION(BlueprintCallable, Category = "Mall|HidingSpot")
	void RebuildHidingSpotVisuals();

	UFUNCTION(BlueprintCallable, Category = "Mall|HidingSpot")
	bool TryToggleHide(APlayerController* Player, FString& OutFeedback);

	UFUNCTION(BlueprintPure, Category = "Mall|HidingSpot")
	FString GetHUDPrompt(const APawn* ViewerPawn) const;

	static AMallHidingSpot* FindHidingSpotInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction);
	static AMallHidingSpot* FindSpotOccupiedBy(const APawn* Pawn);
	static bool IsPlayerConcealedFromGhost(const AActor* Player, const AActor* Ghost);
	static bool IsPlayerHiddenAndSafe(const AActor* Player);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_OccupantState();

private:
	bool HasHuntingGhostWitness(const APawn* EnteringPawn) const;
};
