#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MallRetailStoreUnit.generated.h"

class UBoxComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USpotLightComponent;
class UProceduralMeshComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EMallStoreArchetype : uint8
{
	GhostHuntingEssentials UMETA(DisplayName = "Ghost-Hunting Essentials & Tool Shop (Sunlit Safe Hub)"),
	LingerieParody UMETA(DisplayName = "Haunted Lingerie & Undergarment Parody Boutique"),
	HosierySpecialty UMETA(DisplayName = "Haunted Hosiery & Stockings Specialty Store"),
	UselessClothing UMETA(DisplayName = "Useless Mall Clothing & Shoe Store (Maze/Territory)"),
	FoodCourtStall UMETA(DisplayName = "Pocket-Dimension Food Court Stall & Counter"),
	DeliveryAirlockBay UMETA(DisplayName = "Automated 'Out-of-Nowhere' Supply Delivery Airlock"),
	ParodyLegoBrickStore UMETA(DisplayName = "Parody LEGO & Modular Brick Store ('BRICKWORKZ')"),
	BathhouseSaunaShowers UMETA(DisplayName = "Mall City 24h Tangquan Sauna, Showers & Locker Complex"),
	SurvivorCivicFacility UMETA(DisplayName = "Mall City Civic Facility (Pharmacy Clinic / DIY Workshop / Sleep Showroom)")
};

/**
 * Self-contained Modular Mall Store Actor.
 * Replaces hundreds of loose Outliner actors with a single clean, Blueprint-configurable actor
 * supporting Lingerie/Hosiery Parody Boutiques, Useless Clothing Stores, and the Ghost-Hunting
 * Essentials Shop (with dedicated Shopkeeper NPC standing space behind the counter and Day/Night shutter control).
 */
UCLASS(BlueprintType, Blueprintable)
class MYPROJECT2_API AMallRetailStoreUnit : public AActor
{
	GENERATED_BODY()

public:
	AMallRetailStoreUnit();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "Mall|StoreUnit")
	void RebuildStoreUnit();

	/** Opens or closes the storefront security roller shutter (used by the Day/Night cycle on the Essentials Shop). */
	UFUNCTION(BlueprintCallable, Category = "Mall|StoreUnit")
	void SetNightShutterClosed(bool bClosed);

	/** Returns the world transform behind the checkout counter where the Shopkeeper NPC stands. */
	UFUNCTION(BlueprintPure, Category = "Mall|StoreUnit")
	FTransform GetShopkeeperCounterTransform() const;

	/** Returns the world transform at the center of the store floor for Ghost Territory home placement. */
	UFUNCTION(BlueprintPure, Category = "Mall|StoreUnit")
	FTransform GetStoreTerritoryCenterTransform() const;

	/** Toggles the shop's interior light fixtures ON / OFF (used by the Debugging Light Remote Gun). */
	UFUNCTION(BlueprintCallable, Category = "Mall|StoreUnit")
	bool ToggleStoreLights();

	/** Cycles the shop's interior light brightness (2 = 100% Well-Lit -> 1 = 45% Gray-Space Dim -> 0 = OFF). */
	UFUNCTION(BlueprintCallable, Category = "Mall|StoreUnit")
	uint8 CycleStoreLightBrightness();

	/** Applies the current bStoreLightPowered and StoreLightBrightnessMode to the light components and 3D fixture bulbs. */
	UFUNCTION(BlueprintCallable, Category = "Mall|StoreUnit")
	void ApplyStoreLightState();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UProceduralMeshComponent> StoreShellMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UTextRenderComponent> FasciaSignText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UTextRenderComponent> UpperFasciaSignText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<USpotLightComponent> StoreInteriorSpotLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<USpotLightComponent> StoreUpperFloorSpotLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UPointLightComponent> StoreInteriorLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UPointLightComponent> StoreUpperFloorLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UStaticMeshComponent> StoreLightFixtureMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UStaticMeshComponent> StoreUpperLightFixtureMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UBoxComponent> StoreLightAimBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UStaticMeshComponent> RollerShutterComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UStaticMeshComponent> CheckoutCounterComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UStaticMeshComponent> POSTerminalComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UStaticMeshComponent> BackPanelComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PrimaryFixtureInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SecondaryFixtureInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> MannequinDisplayInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> WallBayInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShelfStockPrimaryInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShelfStockSecondaryInstances;

	/** Store archetype controlling interior layout, fixtures, lighting tint, and gameplay role. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Config")
	EMallStoreArchetype StoreArchetype = EMallStoreArchetype::UselessClothing;

	/** Unique Store ID tag (e.g. "Oval_F1_01", "NW_F0_02") used by ST_StoreGhost and Cart Doorway Docks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Config")
	FName StoreId = TEXT("Store_01");

	/** Displayed store sign name above the entrance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Config")
	FString StoreSignTitle = TEXT("VICTORIA'S PHANTOMS");

	/** Interior width (Y axis, parallel to corridor) in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Geometry", meta = (ClampMin = "400.0"))
	float StoreWidthCm = 960.0f;

	/** Interior depth (X axis, extending away from corridor entrance at X=0) in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Geometry", meta = (ClampMin = "400.0"))
	float StoreDepthCm = 900.0f;

	/** Clear room height in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Geometry", meta = (ClampMin = "320.0"))
	float StoreHeightCm = 750.0f;

	/** Front entrance doorway width in cm (140 cm matches BP_MallEquipmentCart doorway seal, 280 cm for broad open entrance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Geometry", meta = (ClampMin = "120.0"))
	float FrontEntranceWidthCm = 280.0f;

	/** Leave neighbor store pass-through doorways open on both Left (-Y) and Right (+Y) walls (defaults to false so outer stores never open into the void!). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Geometry")
	bool bOpenNeighborPassThroughDoors = false;

	/** Explicitly open only the Left (-Y) side wall pass-through doorway to an adjacent store. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Geometry")
	bool bOpenLeftNeighborDoor = false;

	/** Explicitly open only the Right (+Y) side wall pass-through doorway to an adjacent store. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Geometry")
	bool bOpenRightNeighborDoor = false;

	/** Leave a rear doorway open at X = StoreDepthCm (used on the Central Oval Shopping Island and Back-of-House Service Loop doors!). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Geometry")
	bool bOpenRearPassThroughDoor = false;

	/** When true, builds a 2-story Anchor Department Store spanning Ground Floor (Z=0) and Floor 2 (Z=UpperFloorHeightCm), with entrances on both floors, an internal mezzanine gallery, and a walkable internal staircase connecting both floors! */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Geometry")
	bool bIsMultiFloorAnchorStore = false;

	/** Floor-to-floor height in cm for the second story when bIsMultiFloorAnchorStore is true (default 850 cm to match Floor 2 concourses). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Geometry", meta = (ClampMin = "450.0"))
	float UpperFloorHeightCm = 850.0f;

	/** Whether the store's interior light casts dynamic shadows (prevents light bleeding in flagship/hero stores). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|State")
	bool bCastInteriorLightShadows = false;

	/** Whether the security roller shutter is currently closed (blocks entrance during Night lock-in on Essentials Shop). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|State")
	bool bNightShutterClosed = false;

	/** Whether the store's interior light fixtures are currently powered on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|State")
	bool bStoreLightPowered = true;

	/** Brightness mode for the store's interior light fixtures: 2 = 100% Well-Lit, 1 = 45% Gray-Space Dim, 0 = OFF. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|State")
	uint8 StoreLightBrightnessMode = 2;

	/** Baseline lumen intensity computed for the store archetype when well-lit (100%). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|StoreUnit|State")
	float BaseStoreLightIntensity = 540.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Materials")
	TObjectPtr<UMaterialInterface> FloorMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Materials")
	TObjectPtr<UMaterialInterface> WallMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Materials")
	TObjectPtr<UMaterialInterface> GlassMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Materials")
	TObjectPtr<UMaterialInterface> MetalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|StoreUnit|Materials")
	TObjectPtr<UMaterialInterface> ShelfDisplayMaterial;
};
