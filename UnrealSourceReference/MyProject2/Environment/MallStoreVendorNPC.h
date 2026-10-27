#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MallStoreVendorNPC.generated.h"

class APawn;
class APlayerController;
class UBoxComponent;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EMallVendorItemType : uint8
{
	EMFReader UMETA(DisplayName = "5-LED EMF Reader ($35)"),
	UVLight UMETA(DisplayName = "UV Blacklight Torch ($40)"),
	Thermometer UMETA(DisplayName = "Digital IR Thermometer ($40)"),
	MotionSensor UMETA(DisplayName = "IR Motion & Presence Sensor ($45)"),
	PhotoCamera UMETA(DisplayName = "Paranormal Evidence Photo Camera ($50)"),
	SpiritBox UMETA(DisplayName = "Frequency Spirit Box Radio ($55)"),
	GhostWritingBook UMETA(DisplayName = "Occult Ghost Writing Ledger ($35)"),
	Flashlight UMETA(DisplayName = "Tactical Belt Flashlight ($20)"),
	EscalatorFuse UMETA(DisplayName = "Heavy-Duty Escalator Relay Fuse ($60)"),
	WarmRation UMETA(DisplayName = "Hot Chongqing Broth & Ration Bowl ($10)")
};

/**
 * Interactive Mall Store Vendor NPC stationed behind store counters.
 * Allows trapped survivors to browse a store's equipment catalog (press E)
 * and purchase/dispense Ghost-Hunting Tools, Escalator Fuses, or Hot Rations (press F)
 * directly into their 4-slot hotbar or onto the counter for Cart loading.
 */
UCLASS(BlueprintType, Blueprintable)
class MYPROJECT2_API AMallStoreVendorNPC : public AActor
{
	GENERATED_BODY()

public:
	AMallStoreVendorNPC();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|Vendor")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|Vendor")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|Vendor")
	TObjectPtr<UStaticMeshComponent> VendorBodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|Vendor")
	TObjectPtr<UStaticMeshComponent> CounterSampleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|Vendor")
	TObjectPtr<UTextRenderComponent> VendorNameText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|Vendor")
	TObjectPtr<UTextRenderComponent> CatalogDisplayText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|Vendor")
	TObjectPtr<UPointLightComponent> CounterLampLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|Vendor")
	FString VendorName = TEXT("QUARTERMASTER CHEN");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|Vendor")
	FString StoreTitle = TEXT("FLASH & PHANTOM SURPLUS");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|Vendor")
	bool bUseFemaleFigure = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|Vendor")
	TArray<EMallVendorItemType> CatalogItems;

	UPROPERTY(ReplicatedUsing = OnRep_VendorState, EditAnywhere, BlueprintReadWrite, Category = "Mall|Vendor")
	int32 SelectedCatalogIndex = 0;

	UPROPERTY(ReplicatedUsing = OnRep_VendorState, BlueprintReadOnly, Category = "Mall|Vendor")
	int32 TotalItemsDispensed = 0;

	UFUNCTION(BlueprintCallable, Category = "Mall|Vendor")
	void RebuildVendorVisuals();

	UFUNCTION(BlueprintPure, Category = "Mall|Vendor")
	EMallVendorItemType GetCurrentCatalogItem() const;

	UFUNCTION(BlueprintPure, Category = "Mall|Vendor")
	static FString GetItemDisplayName(EMallVendorItemType ItemType);

	UFUNCTION(BlueprintPure, Category = "Mall|Vendor")
	FString GetHUDPrompt() const;

	/** Cycles to the next item in the Vendor's catalog (triggered via E). */
	UFUNCTION(BlueprintCallable, Category = "Mall|Vendor")
	bool TryCycleCatalogItem(APlayerController* Player, FString& OutFeedback);

	/** Dispenses and equips (or places on counter) the currently selected catalog item (triggered via F). */
	UFUNCTION(BlueprintCallable, Category = "Mall|Vendor")
	bool TryPurchaseOrDispense(APlayerController* Player, FString& OutFeedback);

	static AMallStoreVendorNPC* FindVendorInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_VendorState();
};
