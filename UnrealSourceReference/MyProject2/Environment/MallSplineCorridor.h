#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MallSplineCorridor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;
class USplineComponent;
class UStaticMesh;

/**
 * Spline-driven Mall Hallway Concourse ("H" legs and crossbars).
 * Implements the unsettling real-world mall phenomenon of subtle random direction changes
 * (1.5-4.0 deg yaw kinks) and subtle random floor incline undulations (0.8-2.2 deg pitch)
 * across expandable modular bays, complete with floor expansion joints, dropped ceiling bulkheads,
 * security camera domes, and future-expandable shutter end-caps.
 */
UCLASS(BlueprintType, Blueprintable)
class MYPROJECT2_API AMallSplineCorridor : public AActor
{
	GENERATED_BODY()

public:
	AMallSplineCorridor();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "Mall|SplineCorridor")
	void RebuildSplineCorridor();

	/** Returns the world transform at the center of a given bay on the Left (-Y) or Right (+Y) storefront wall, matching the local floor incline and yaw bend. */
	UFUNCTION(BlueprintPure, Category = "Mall|SplineCorridor")
	FTransform GetStorefrontBayWorldTransform(int32 BayIndex, bool bRightSide) const;

	/** Returns the world transform flush against the corridor ceiling at the center of a given bay, matching the local spline incline and yaw bend. */
	UFUNCTION(BlueprintPure, Category = "Mall|SplineCorridor")
	FTransform GetBayCeilingMountWorldTransform(int32 BayIndex) const;

	/** Returns the world transform at the end of the corridor spline for snapping future expansion wings. */
	UFUNCTION(BlueprintPure, Category = "Mall|SplineCorridor")
	FTransform GetCorridorEndExpansionTransform() const;

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|SplineCorridor")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|SplineCorridor")
	TObjectPtr<USplineComponent> CorridorSpline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|SplineCorridor")
	TObjectPtr<UProceduralMeshComponent> CorridorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|SplineCorridor")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> CeilingLightInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|SplineCorridor")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SecurityCameraInstances;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|SplineCorridor")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShutterCapInstances;

	/** When true, populates CorridorSpline from NumBays, BayLengthCm, and the unsettling Yaw/Incline drift seed. Set false to hand-edit spline points freely in the viewport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout")
	bool bUseProceduralUnsettlingDrift = true;

	/** Number of modular hallway bays along this H-leg (increase anytime to extend the hallway). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout", meta = (ClampMin = "1", ClampMax = "32"))
	int32 NumBays = 4;

	/** Length of each hallway bay in cm (default 1000 cm = 10 m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout", meta = (ClampMin = "400.0"))
	float BayLengthCm = 1000.0f;

	/** Clear walkable width of the hallway in cm (default 900 cm = 9 m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout", meta = (ClampMin = "300.0"))
	float CorridorWidthCm = 900.0f;

	/** Clear ceiling height of the hallway in cm (default 800 cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout", meta = (ClampMin = "350.0"))
	float CorridorHeightCm = 800.0f;

	/**Deterministic seed for subtle direction & floor incline variations along this hallway leg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|UnsettlingDrift")
	int32 UnsettlingDriftSeed = 1337;

	/** Maximum subtle horizontal direction change (degrees) per bay transition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|UnsettlingDrift", meta = (ClampMin = "0.0", ClampMax = "12.0"))
	float MaxYawDriftDegrees = 3.2f;

	/** Maximum subtle floor incline pitch change (degrees) per bay transition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|UnsettlingDrift", meta = (ClampMin = "0.0", ClampMax = "6.0"))
	float MaxInclinePitchDegrees = 1.65f;

	/** Leave open portals on the left/right walls of each bay so enterable AMallRetailStoreUnit shops can connect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout")
	bool bOpenStorefrontPortals = true;

	/** When non-empty, ONLY bay indices in this array open storefront portals; all other bays are sealed with solid 3D walls & shutters so players can never walk out of an unplugged bay! */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout")
	TArray<int32> OpenPortalBayIndices;

	/** Optional override for Left (-Right) wall open portal bay indices. If empty, falls back to OpenPortalBayIndices. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout")
	TArray<int32> OpenLeftPortalBayIndices;

	/** Optional override for Right (+Right) wall open portal bay indices. If empty, falls back to OpenPortalBayIndices. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout")
	TArray<int32> OpenRightPortalBayIndices;

	/** Whether to spawn static unlit HISM ceiling track bars in each bay (default false when using interactive AMallLightFixture actors). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout")
	bool bSpawnInstancedTrackLights = false;

	/** Width of each storefront entrance opening in cm when bOpenStorefrontPortals is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout", meta = (ClampMin = "200.0"))
	float StorefrontPortalWidthCm = 360.0f;

	/** Places a removable corrugated security shutter expansion cap at the far end of the hallway leg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout")
	bool bCapFarEndForFutureExpansion = true;

	/** When true and CorridorWidthCm > StartPortalWidthCm (or CorridorHeightCm > StartPortalHeightCm), builds solid 3D shoulder & header walls at the start of the hall (P0) so wide Grand Biome Halls mate 100% watertight with the 900x800cm Atrium portal! */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout")
	bool bEncloseStartWallAroundPortal = true;

	/** Width of the entrance portal at P0 when bEncloseStartWallAroundPortal is true (default 900 cm to match Atrium portals). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout", meta = (ClampMin = "200.0"))
	float StartPortalWidthCm = 900.0f;

	/** Height of the entrance portal at P0 when bEncloseStartWallAroundPortal is true (default 800 cm to match Atrium portals). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout", meta = (ClampMin = "250.0"))
	float StartPortalHeightCm = 800.0f;

	/** When > 0 and bCapFarEndForFutureExpansion is true, leaves a framed center doorway of this width at the far end (PEnd) for a docked Climax Landmark Store or Freight Airlock while sealing the rest of the wide end wall in solid 3D concrete. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout", meta = (ClampMin = "0.0"))
	float EndPortalWidthCm = 0.0f;

	/** Height of the framed center doorway at the far end when EndPortalWidthCm > 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout", meta = (ClampMin = "220.0"))
	float EndPortalHeightCm = 420.0f;

	/** Builds a guarded central floor opening down to the floor below (for upper-floor H-legs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Layout")
	bool bIsUpperFloorGallery = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Materials")
	TObjectPtr<UMaterialInterface> FloorMarbleMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Materials")
	TObjectPtr<UMaterialInterface> WallPlasterMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Materials")
	TObjectPtr<UMaterialInterface> MetalJointMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Materials")
	TObjectPtr<UMaterialInterface> NeonCoveMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Meshes")
	TObjectPtr<UStaticMesh> CeilingTrackLightMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Meshes")
	TObjectPtr<UStaticMesh> SecurityCameraDomeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|SplineCorridor|Meshes")
	TObjectPtr<UStaticMesh> ClosedRollerShutterMesh;
};
