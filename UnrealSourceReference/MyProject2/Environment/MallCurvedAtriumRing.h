#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MallCurvedAtriumRing.generated.h"

class UProceduralMeshComponent;
class USplineComponent;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FMallAngularPortalSpan
{
	GENERATED_BODY()

	/** Center angle in degrees around the elliptical ring (-180..180, where 0=+X East, 90=+Y South, 180=-X West, -90=-Y North). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture")
	float CenterAngleDegrees = 180.0f;

	/** Angular half-width in degrees left open in the curved balustrade for escalator or bridge landings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "1.0", ClampMax = "45.0"))
	float HalfAngleSpanDegrees = 10.5f;

	/** Floor index (1 = second floor Z=850, 2 = third floor Z=1700, 0 = all upper floors). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture")
	int32 TargetUpperFloorIndex = 0;
};

/**
 * Multi-floor Parametric & Spline-Driven Central Oval Shopping Center + Spaceship/Arboretum Atrium.
 * - Ground Floor (Z=0): Fountain Plaza, 4-quadrant curbed Arboretum Water Channels & Planter Islands.
 * - Upper Floors (Z=850, Z=1700, expandable via NumUpperFloors): Central Oval Shopping Island +
 *   Inner Oval Walkway Gallery + Open Elliptical Atrium Void + 4 Cardinal Clear-Span Bridges + Outer Concourse Ring.
 * - Roof Canopy: Vaulted Spaceship Hull Ribs, Geodesic Skylight Oculus, and Biotic-Cyan Neon Cove Strips.
 */
UCLASS(BlueprintType, Blueprintable)
class MYPROJECT2_API AMallCurvedAtriumRing : public AActor
{
	GENERATED_BODY()

public:
	AMallCurvedAtriumRing();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "Mall|CurvedArchitecture")
	void RebuildCurvedArchitecture();

	UFUNCTION(BlueprintPure, Category = "Mall|CurvedArchitecture")
	FVector EvaluateEllipsePoint(float RadiusX, float RadiusY, float AngleRadians, float ZElevation) const;

	UFUNCTION(BlueprintPure, Category = "Mall|CurvedArchitecture")
	FVector EvaluateAtriumEllipsePoint(float AngleRadians, float ZElevation, float RadialOffset = 0.0f) const;

	UFUNCTION(BlueprintPure, Category = "Mall|CurvedArchitecture")
	FVector EvaluateAtriumInwardNormal(float AngleRadians) const;

	UFUNCTION(BlueprintPure, Category = "Mall|CurvedArchitecture")
	float GetUpperFloorElevationZ(int32 UpperFloorIndex) const;

protected:
	virtual void BeginPlay() override;

	bool IsAngleInsideOuterPortal(float AngleDegrees, int32 UpperFloorIndex) const;
	bool IsAngleInsideCardinalBridge(float AngleDegrees) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|CurvedArchitecture")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|CurvedArchitecture")
	TObjectPtr<USplineComponent> InnerOvalGallerySpline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|CurvedArchitecture")
	TObjectPtr<USplineComponent> AtriumRingSpline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mall|CurvedArchitecture")
	TObjectPtr<UProceduralMeshComponent> ArchitectureMesh;

public:
	/** Number of smooth angular subdivisions around the elliptical curves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "48", ClampMax = "256"))
	int32 CurveSegments = 96;

	/** Number of upper floors above the ground floor (2 = Ground + Floor 2 at Z=850 + Floor 3 at Z=1700; expandable up to 6). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "1", ClampMax = "6"))
	int32 NumUpperFloors = 2;

	/** Floor-to-floor vertical spacing in cm (accepted scale: 850 cm = 800 cm clear height + 50 cm structure). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "400.0"))
	float FloorToFloorHeightCm = 850.0f;

	/** Legacy/convenience alias for first upper floor Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture")
	float UpperFloorZ = 850.0f;

	/** Central Oval Shopping Island semi-minor (East-West X) and semi-major (North-South Y) radii on upper floors. Oriented North-South parallel to the H-concourse hallways! */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "500.0"))
	float OvalIslandRadiusX = 950.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "400.0"))
	float OvalIslandRadiusY = 1950.0f;

	/** Outer edge of the Open Elliptical Atrium Chasm (oriented North-South so East-West bridge spans are a compact 7.5m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "1000.0"))
	float AtriumRadiusX = 1700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "800.0"))
	float AtriumRadiusY = 2850.0f;

	/** Intermediate elliptical concourse walkway ring radius before blending into outer H-concourse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "1200.0"))
	float ConcourseRingRadiusX = 2300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "1000.0"))
	float ConcourseRingRadiusY = 3450.0f;

	/** Outer half-extent X/Y of the central hub block (connects to the West/East H-concourses at X=+-2300 and Central Spine at X=0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "2000.0"))
	float OuterExtentX = 2850.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "1800.0"))
	float OuterExtentY = 3850.0f;

	/** Builds Chongqing 'The Ring' (Mu Guang Forest) monumental Soul Tree columns, Floating Forest suspended planter pods, 20m waterfall, and steaming Geothermal Hot Springs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture")
	bool bBuildMonumentalArboretumAndHotSprings = true;

	/** Builds solid 3D exterior perimeter walls with open portals for the 4 H-Hallway legs (X=+-2300) and the 3rd Central Spine (X=0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture")
	bool bEncloseOuterPerimeterWithHallwayPortals = true;

	/** Structural slab thickness in cm (50 cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "20.0"))
	float SlabThicknessCm = 50.0f;

	/** Height of the vaulted Spaceship/Biodome ceiling apex above the highest upper floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "300.0"))
	float BiodomeApexRiseCm = 750.0f;

	/** Height of the curved glass atrium balustrade above each floor in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "80.0"))
	float BalustradeHeightCm = 115.0f;

	/** Angular half-span in degrees for the 4 Cardinal Bridges at 0, 90, 180, -90 degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "3.0", ClampMax = "20.0"))
	float CardinalBridgeHalfAngleDeg = 6.5f;

	/** World-space UV tile size for marble flooring in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture", meta = (ClampMin = "50.0"))
	float MarbleTileSizeCm = 200.0f;

	/** Additional angular openings in the outer balustrade for escalator landings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture")
	TArray<FMallAngularPortalSpan> BalustradePortals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture|Materials")
	TObjectPtr<UMaterialInterface> FloorMarbleMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture|Materials")
	TObjectPtr<UMaterialInterface> PlasterFasciaMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture|Materials")
	TObjectPtr<UMaterialInterface> GlassBalustradeMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture|Materials")
	TObjectPtr<UMaterialInterface> BrushedMetalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mall|CurvedArchitecture|Materials")
	TObjectPtr<UMaterialInterface> AccentMedallionMaterial;
};
