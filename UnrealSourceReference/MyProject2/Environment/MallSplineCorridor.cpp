#include "Environment/MallSplineCorridor.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	struct FSplineCorridorSection
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
		TArray<FProcMeshTangent> Tangents;

		void AddQuad(const FVector& V0, const FVector& V1, const FVector& V2, const FVector& V3, const FVector& FaceNormal, float UVScale = 200.0f)
		{
			const int32 Base = Vertices.Num();
			Vertices.Add(V0);
			Vertices.Add(V1);
			Vertices.Add(V2);
			Vertices.Add(V3);

			const FVector SafeN = FaceNormal.GetSafeNormal();
			Normals.Add(SafeN);
			Normals.Add(SafeN);
			Normals.Add(SafeN);
			Normals.Add(SafeN);

			const float Inv = 1.0f / FMath::Max(UVScale, 10.0f);
			auto CalcUV = [&](const FVector& P)
			{
				if (FMath::Abs(SafeN.Z) > 0.7f)
				{
					return FVector2D(P.X * Inv, P.Y * Inv);
				}
				return FVector2D((P.X + P.Y) * Inv, P.Z * Inv);
			};
			UVs.Add(CalcUV(V0));
			UVs.Add(CalcUV(V1));
			UVs.Add(CalcUV(V2));
			UVs.Add(CalcUV(V3));

			Colors.Add(FLinearColor::White);
			Colors.Add(FLinearColor::White);
			Colors.Add(FLinearColor::White);
			Colors.Add(FLinearColor::White);

			const FVector Edge = (V1 - V0).GetSafeNormal();
			const FProcMeshTangent Tan(Edge.IsNearlyZero() ? FVector::ForwardVector : Edge, false);
			Tangents.Add(Tan);
			Tangents.Add(Tan);
			Tangents.Add(Tan);
			Tangents.Add(Tan);

			const FVector Cross = FVector::CrossProduct(V2 - V0, V1 - V0);
			if (FVector::DotProduct(Cross, SafeN) >= 0.0f)
			{
				Triangles.Add(Base + 0); Triangles.Add(Base + 1); Triangles.Add(Base + 2);
				Triangles.Add(Base + 0); Triangles.Add(Base + 2); Triangles.Add(Base + 3);
			}
			else
			{
				Triangles.Add(Base + 0); Triangles.Add(Base + 2); Triangles.Add(Base + 1);
				Triangles.Add(Base + 0); Triangles.Add(Base + 3); Triangles.Add(Base + 2);
			}
		}

		void AddThickWallQuad(
			const FVector& V0,
			const FVector& V1,
			const FVector& V2,
			const FVector& V3,
			const FVector& InwardNormal,
			float ThicknessCm,
			float UVScale = 200.0f)
		{
			const FVector OutOffset = -InwardNormal.GetSafeNormal() * ThicknessCm;
			const FVector O0 = V0 + OutOffset;
			const FVector O1 = V1 + OutOffset;
			const FVector O2 = V2 + OutOffset;
			const FVector O3 = V3 + OutOffset;

			// Inner face, outer face, and 4 perimeter edge caps (100% closed 3D manifold!)
			AddQuad(V0, V1, V2, V3, InwardNormal, UVScale);
			AddQuad(O1, O0, O3, O2, -InwardNormal, UVScale);

			const FVector EdgeRight = (V1 - V0).GetSafeNormal();
			const FVector EdgeUp = (V3 - V0).GetSafeNormal();
			AddQuad(V3, V2, O2, O3, EdgeUp, UVScale);
			AddQuad(O0, O1, V1, V0, -EdgeUp, UVScale);
			AddQuad(V1, O1, O2, V2, EdgeRight, UVScale);
			AddQuad(O0, V0, V3, O3, -EdgeRight, UVScale);
		}
	};
}

AMallSplineCorridor::AMallSplineCorridor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CorridorSpline = CreateDefaultSubobject<USplineComponent>(TEXT("CorridorSpline"));
	CorridorSpline->SetupAttachment(SceneRoot);

	CorridorMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CorridorMesh"));
	CorridorMesh->SetupAttachment(SceneRoot);
	CorridorMesh->bUseComplexAsSimpleCollision = true;
	CorridorMesh->SetCollisionProfileName(TEXT("BlockAll"));

	CeilingLightInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CeilingLightInstances"));
	CeilingLightInstances->SetupAttachment(SceneRoot);
	CeilingLightInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SecurityCameraInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("SecurityCameraInstances"));
	SecurityCameraInstances->SetupAttachment(SceneRoot);
	SecurityCameraInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ShutterCapInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ShutterCapInstances"));
	ShutterCapInstances->SetupAttachment(SceneRoot);
	ShutterCapInstances->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarbleMat(TEXT("/Game/Environment/Mall/Materials/M_MallFloorMarble.M_MallFloorMarble"));
	if (MarbleMat.Succeeded())
	{
		FloorMarbleMaterial = MarbleMat.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlasterMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_carcass.M_MallRetail_carcass"));
	if (PlasterMat.Succeeded())
	{
		WallPlasterMaterial = PlasterMat.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MetalMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_metal.M_MallRetail_metal"));
	if (MetalMat.Succeeded())
	{
		MetalJointMaterial = MetalMat.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> AccentMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_accent.M_MallRetail_accent"));
	if (AccentMat.Succeeded())
	{
		NeonCoveMaterial = AccentMat.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TrackLightFinder(TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25354_CeilingTrackLightingBar.SM_Retail_25354_CeilingTrackLightingBar"));
	if (TrackLightFinder.Succeeded())
	{
		CeilingTrackLightMesh = TrackLightFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CamDomeFinder(TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25337_CeilingCameraDome.SM_Retail_25337_CeilingCameraDome"));
	if (CamDomeFinder.Succeeded())
	{
		SecurityCameraDomeMesh = CamDomeFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShutterFinder(TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25344_RollerShutterClosed.SM_Retail_25344_RollerShutterClosed"));
	if (ShutterFinder.Succeeded())
	{
		ClosedRollerShutterMesh = ShutterFinder.Object;
	}
}

void AMallSplineCorridor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildSplineCorridor();
}

void AMallSplineCorridor::BeginPlay()
{
	Super::BeginPlay();
	if (CorridorMesh && CorridorMesh->GetNumSections() == 0)
	{
		RebuildSplineCorridor();
	}
}

FTransform AMallSplineCorridor::GetStorefrontBayWorldTransform(int32 BayIndex, bool bRightSide) const
{
	if (!CorridorSpline || CorridorSpline->GetNumberOfSplinePoints() < 2)
	{
		return GetActorTransform();
	}
	const int32 ClampedBay = FMath::Clamp(BayIndex, 0, CorridorSpline->GetNumberOfSplinePoints() - 2);
	const FVector P0 = CorridorSpline->GetLocationAtSplinePoint(ClampedBay, ESplineCoordinateSpace::Local);
	const FVector P1 = CorridorSpline->GetLocationAtSplinePoint(ClampedBay + 1, ESplineCoordinateSpace::Local);
	const FVector Mid = (P0 + P1) * 0.5f;
	const FVector Forward = (P1 - P0).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const float SideSign = bRightSide ? 1.0f : -1.0f;
	const FVector WallLoc = Mid + Right * (SideSign * CorridorWidthCm * 0.5f);
	const FVector InwardDir = -Right * SideSign;
	const FRotator LocalRot = InwardDir.Rotation();
	return FTransform(LocalRot, WallLoc) * GetActorTransform();
}

FTransform AMallSplineCorridor::GetBayCeilingMountWorldTransform(int32 BayIndex) const
{
	if (!CorridorSpline || CorridorSpline->GetNumberOfSplinePoints() < 2)
	{
		return GetActorTransform();
	}
	const int32 ClampedBay = FMath::Clamp(BayIndex, 0, CorridorSpline->GetNumberOfSplinePoints() - 2);
	const FVector P0 = CorridorSpline->GetLocationAtSplinePoint(ClampedBay, ESplineCoordinateSpace::Local);
	const FVector P1 = CorridorSpline->GetLocationAtSplinePoint(ClampedBay + 1, ESplineCoordinateSpace::Local);
	const FVector Mid = (P0 + P1) * 0.5f;
	const FVector Forward = (P1 - P0).GetSafeNormal();
	const float Height = FMath::Max(CorridorHeightCm, 350.0f);
	const FVector CeilingLoc = Mid + FVector(0.0f, 0.0f, Height - 5.0f);
	return FTransform(Forward.Rotation(), CeilingLoc) * GetActorTransform();
}

FTransform AMallSplineCorridor::GetCorridorEndExpansionTransform() const
{
	if (!CorridorSpline || CorridorSpline->GetNumberOfSplinePoints() < 2)
	{
		return GetActorTransform();
	}
	const int32 LastIdx = CorridorSpline->GetNumberOfSplinePoints() - 1;
	const FVector EndLoc = CorridorSpline->GetLocationAtSplinePoint(LastIdx, ESplineCoordinateSpace::Local);
	const FVector PrevLoc = CorridorSpline->GetLocationAtSplinePoint(LastIdx - 1, ESplineCoordinateSpace::Local);
	const FVector Forward = (EndLoc - PrevLoc).GetSafeNormal();
	return FTransform(Forward.Rotation(), EndLoc) * GetActorTransform();
}

void AMallSplineCorridor::RebuildSplineCorridor()
{
	if (!CorridorSpline || !CorridorMesh)
	{
		return;
	}

	const int32 SafeBays = FMath::Clamp(NumBays, 1, 32);
	const float HalfW = FMath::Max(CorridorWidthCm * 0.5f, 150.0f);
	const float Height = FMath::Max(CorridorHeightCm, 350.0f);
	const float SlabThick = 50.0f;
	const float WallThick = 28.0f;

	// 1. Populate spline points with subtle unsettling Yaw bends and Floor Incline undulations
	if (bUseProceduralUnsettlingDrift)
	{
		CorridorSpline->ClearSplinePoints(false);
		FRandomStream Rng(UnsettlingDriftSeed);
		FVector CurrentPos = FVector::ZeroVector;
		float CumulativeYawDeg = 0.0f;

		CorridorSpline->AddSplinePoint(CurrentPos, ESplineCoordinateSpace::Local, false);
		CorridorSpline->SetSplinePointType(0, ESplinePointType::Linear, false);

		for (int32 b = 0; b < SafeBays; ++b)
		{
			const float DeltaYaw = (b == 0) ? 0.0f : Rng.FRandRange(-MaxYawDriftDegrees, MaxYawDriftDegrees);
			const float PitchDeg = (b == 0) ? 0.0f : Rng.FRandRange(-MaxInclinePitchDegrees, MaxInclinePitchDegrees);
			CumulativeYawDeg = FMath::Clamp(CumulativeYawDeg + DeltaYaw, -14.0f, 14.0f);

			const FRotator SegRot(PitchDeg, CumulativeYawDeg, 0.0f);
			CurrentPos += SegRot.Vector() * BayLengthCm;

			CorridorSpline->AddSplinePoint(CurrentPos, ESplineCoordinateSpace::Local, false);
			CorridorSpline->SetSplinePointType(b + 1, ESplinePointType::Linear, false);
		}
		CorridorSpline->UpdateSpline();
	}

	CorridorMesh->ClearAllMeshSections();
	CeilingLightInstances->ClearInstances();
	SecurityCameraInstances->ClearInstances();
	ShutterCapInstances->ClearInstances();

	if (CeilingTrackLightMesh)
	{
		CeilingLightInstances->SetStaticMesh(CeilingTrackLightMesh);
	}
	if (SecurityCameraDomeMesh)
	{
		SecurityCameraInstances->SetStaticMesh(SecurityCameraDomeMesh);
	}
	if (ClosedRollerShutterMesh)
	{
		ShutterCapInstances->SetStaticMesh(ClosedRollerShutterMesh);
	}

	FSplineCorridorSection FloorSection;
	FSplineCorridorSection WallCeilingSection;
	FSplineCorridorSection MetalJointSection;
	FSplineCorridorSection NeonCoveSection;

	const int32 PointCount = CorridorSpline->GetNumberOfSplinePoints();
	for (int32 b = 0; b < PointCount - 1; ++b)
	{
		const FVector P0 = CorridorSpline->GetLocationAtSplinePoint(b, ESplineCoordinateSpace::Local);
		const FVector P1 = CorridorSpline->GetLocationAtSplinePoint(b + 1, ESplineCoordinateSpace::Local);
		const FVector SegVec = P1 - P0;
		const float SegLen = SegVec.Size();
		if (SegLen < 10.0f)
		{
			continue;
		}
		const FVector Forward = SegVec / SegLen;
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
		const FVector FloorNormal = FVector::CrossProduct(Forward, Right).GetSafeNormal();

		const FVector F0L = P0 - Right * HalfW;
		const FVector F0R = P0 + Right * HalfW;
		const FVector F1L = P1 - Right * HalfW;
		const FVector F1R = P1 + Right * HalfW;

		const FVector B0L = F0L - FVector(0, 0, SlabThick);
		const FVector B0R = F0R - FVector(0, 0, SlabThick);
		const FVector B1L = F1L - FVector(0, 0, SlabThick);
		const FVector B1R = F1R - FVector(0, 0, SlabThick);

		const FVector C0L = F0L + FVector(0, 0, Height);
		const FVector C0R = F0R + FVector(0, 0, Height);
		const FVector C1L = F1L + FVector(0, 0, Height);
		const FVector C1R = F1R + FVector(0, 0, Height);

		const FVector RTop0L = C0L + FVector(0, 0, SlabThick) - Right * WallThick;
		const FVector RTop0R = C0R + FVector(0, 0, SlabThick) + Right * WallThick;
		const FVector RTop1L = C1L + FVector(0, 0, SlabThick) - Right * WallThick;
		const FVector RTop1R = C1R + FVector(0, 0, SlabThick) + Right * WallThick;

		// A) SOLID 3D Walkable Floor Slab (Top, Bottom, and Left/Right Exterior Slab Edges)
		FloorSection.AddQuad(F0L, F1L, F1R, F0R, FloorNormal, 200.0f);
		WallCeilingSection.AddQuad(B0L, B0R, B1R, B1L, -FloorNormal, 200.0f);
		WallCeilingSection.AddQuad(B0L, B1L, F1L, F0L, -Right, 200.0f);
		WallCeilingSection.AddQuad(F0R, F1R, B1R, B0R, Right, 200.0f);

		// B) Expansion Joint Strip across the floor & SOLID 3D Dropped Soffit Bulkhead
		// At Bay 0 entrance (b == 0), drop a deeper Solar Light-Baffle Bulkhead (120cm) so sunlight from the Atrium cannot creep down the hallway!
		const FVector JointEnd0L = F0L + Forward * 18.0f + FVector(0, 0, 0.4f);
		const FVector JointEnd0R = F0R + Forward * 18.0f + FVector(0, 0, 0.4f);
		MetalJointSection.AddQuad(F0L + FVector(0, 0, 0.4f), JointEnd0L, JointEnd0R, F0R + FVector(0, 0, 0.4f), FloorNormal, 100.0f);

		const float BulkheadDrop = (b == 0) ? 120.0f : 52.0f;
		const FVector Beam0L = C0L - FVector(0, 0, BulkheadDrop);
		const FVector Beam0R = C0R - FVector(0, 0, BulkheadDrop);
		const FVector Beam1L = Beam0L + Forward * 40.0f;
		const FVector Beam1R = Beam0R + Forward * 40.0f;
		WallCeilingSection.AddQuad(Beam0L, Beam0R, Beam1R, Beam1L, -FloorNormal, 150.0f);
		WallCeilingSection.AddQuad(C0L, C0R, Beam0R, Beam0L, -Forward, 150.0f);
		WallCeilingSection.AddQuad(Beam1L, Beam1R, C0R + Forward * 40.0f, C0L + Forward * 40.0f, Forward, 150.0f);

		// C) SOLID 3D Acoustic Ceiling + Sun-Blocking Exterior Roof Slab Top + Side Roof Fascias
		WallCeilingSection.AddQuad(C0L, C0R, C1R, C1L, -FloorNormal, 200.0f);
		WallCeilingSection.AddQuad(RTop0L, RTop1L, RTop1R, RTop0R, FloorNormal, 200.0f);
		WallCeilingSection.AddQuad(C0L - Right * WallThick, C1L - Right * WallThick, RTop1L, RTop0L, -Right, 200.0f);
		WallCeilingSection.AddQuad(RTop0R, RTop1R, C1R + Right * WallThick, C0R + Right * WallThick, Right, 200.0f);

		NeonCoveSection.AddQuad(
			C0L + Right * 35.0f - FVector(0, 0, 2.0f),
			C1L + Right * 35.0f - FVector(0, 0, 2.0f),
			C1L + Right * 50.0f - FVector(0, 0, 2.0f),
			C0L + Right * 50.0f - FVector(0, 0, 2.0f),
			-FloorNormal, 100.0f);
		NeonCoveSection.AddQuad(
			C0R - Right * 50.0f - FVector(0, 0, 2.0f),
			C1R - Right * 50.0f - FVector(0, 0, 2.0f),
			C1R - Right * 35.0f - FVector(0, 0, 2.0f),
			C0R - Right * 35.0f - FVector(0, 0, 2.0f),
			-FloorNormal, 100.0f);

		// D) SOLID 3D Left (-Right) and Right (+Right) Storefront Fascia Walls with Open Entrance Portals
		const float PortalHalfLen = FMath::Clamp(StorefrontPortalWidthCm * 0.5f, 100.0f, SegLen * 0.42f);
		const float PortalHeight = FMath::Min(420.0f, Height - 80.0f);
		const float SidePillarLen = (SegLen - PortalHalfLen * 2.0f) * 0.5f;
		const bool bHasSideSpecificPortals = (OpenLeftPortalBayIndices.Num() > 0 || OpenRightPortalBayIndices.Num() > 0);
		const bool bOpenDefault = bOpenStorefrontPortals && (OpenPortalBayIndices.Num() == 0 || OpenPortalBayIndices.Contains(b));
		const bool bOpenLeftBay = bHasSideSpecificPortals ? (bOpenStorefrontPortals && OpenLeftPortalBayIndices.Contains(b)) : bOpenDefault;
		const bool bOpenRightBay = bHasSideSpecificPortals ? (bOpenStorefrontPortals && OpenRightPortalBayIndices.Contains(b)) : bOpenDefault;

		auto BuildSolidSideWall = [&](const FVector& StartBot, const FVector& EndBot, const FVector& InwardNormal, bool bOpenWallPortal)
		{
			if (!bOpenWallPortal)
			{
				WallCeilingSection.AddThickWallQuad(StartBot, EndBot, EndBot + FVector(0, 0, Height), StartBot + FVector(0, 0, Height), InwardNormal, WallThick, 200.0f);
				if (ClosedRollerShutterMesh)
				{
					const FVector MidWall = (StartBot + EndBot) * 0.5f + InwardNormal * 4.0f;
					ShutterCapInstances->AddInstance(FTransform(InwardNormal.Rotation(), MidWall, FVector(1.25f, 1.8f, 1.15f)));
				}
				return;
			}
			const FVector P_Door0 = StartBot + Forward * SidePillarLen;
			const FVector P_Door1 = EndBot - Forward * SidePillarLen;

			// Solid 3D Left & Right wall piers flanking the storefront entrance
			WallCeilingSection.AddThickWallQuad(StartBot, P_Door0, P_Door0 + FVector(0, 0, PortalHeight), StartBot + FVector(0, 0, PortalHeight), InwardNormal, WallThick, 200.0f);
			WallCeilingSection.AddThickWallQuad(P_Door1, EndBot, EndBot + FVector(0, 0, PortalHeight), P_Door1 + FVector(0, 0, PortalHeight), InwardNormal, WallThick, 200.0f);
			// Solid 3D Upper fascia transom above the storefront entrance
			WallCeilingSection.AddThickWallQuad(
				StartBot + FVector(0, 0, PortalHeight),
				EndBot + FVector(0, 0, PortalHeight),
				EndBot + FVector(0, 0, Height),
				StartBot + FVector(0, 0, Height),
				InwardNormal, WallThick, 200.0f);
			// Solid 3D Metallic door frame trim
			MetalJointSection.AddThickWallQuad(
				P_Door0, P_Door0 + Forward * 8.0f,
				P_Door0 + Forward * 8.0f + FVector(0, 0, PortalHeight), P_Door0 + FVector(0, 0, PortalHeight),
				InwardNormal, WallThick + 4.0f, 100.0f);
			MetalJointSection.AddThickWallQuad(
				P_Door1 - Forward * 8.0f, P_Door1,
				P_Door1 + FVector(0, 0, PortalHeight), P_Door1 - Forward * 8.0f + FVector(0, 0, PortalHeight),
				InwardNormal, WallThick + 4.0f, 100.0f);
		};

		BuildSolidSideWall(F0L, F1L, Right, bOpenLeftBay);
		BuildSolidSideWall(F0R, F1R, -Right, bOpenRightBay);

		// E) Instanced Ceiling Track Lights & Ceiling Security Dome Camera at each bay
		const FVector MidCeiling = (C0L + C0R + C1L + C1R) * 0.25f;
		if (bSpawnInstancedTrackLights && CeilingTrackLightMesh)
		{
			CeilingLightInstances->AddInstance(FTransform(Forward.Rotation(), MidCeiling - FVector(0, 0, 12.0f), FVector(1.2f)));
		}
		if (SecurityCameraDomeMesh && (b % 2 == 1))
		{
			SecurityCameraInstances->AddInstance(FTransform(Forward.Rotation(), (C0L + C0R) * 0.5f + Forward * 18.0f - FVector(0, 0, BulkheadDrop + 4.0f), FVector(1.5f)));
		}
	}

	// E2) SOLID 3D Start-Portal Shoulder & Transom Walls at P0 (when CorridorWidthCm > StartPortalWidthCm or Height > StartPortalHeightCm)
	if (bEncloseStartWallAroundPortal && PointCount >= 2)
	{
		const FVector PStart = CorridorSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);
		const FVector PNext = CorridorSpline->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::Local);
		const FVector StartForward = (PNext - PStart).GetSafeNormal();
		const FVector StartRight = FVector::CrossProduct(FVector::UpVector, StartForward).GetSafeNormal();
		const float HalfStartPortalW = FMath::Clamp(StartPortalWidthCm * 0.5f, 100.0f, HalfW);
		const float ClampedStartPortalH = FMath::Clamp(StartPortalHeightCm, 250.0f, Height + SlabThick);

		if (HalfW > HalfStartPortalW + 5.0f)
		{
			const FVector SL_Outer = PStart - StartRight * (HalfW + WallThick);
			const FVector SL_Inner = PStart - StartRight * HalfStartPortalW;
			WallCeilingSection.AddThickWallQuad(
				SL_Outer, SL_Inner,
				SL_Inner + FVector(0, 0, Height + SlabThick), SL_Outer + FVector(0, 0, Height + SlabThick),
				StartForward, WallThick, 200.0f);

			const FVector SR_Inner = PStart + StartRight * HalfStartPortalW;
			const FVector SR_Outer = PStart + StartRight * (HalfW + WallThick);
			WallCeilingSection.AddThickWallQuad(
				SR_Inner, SR_Outer,
				SR_Outer + FVector(0, 0, Height + SlabThick), SR_Inner + FVector(0, 0, Height + SlabThick),
				StartForward, WallThick, 200.0f);
		}
		if (Height + SlabThick > ClampedStartPortalH + 5.0f)
		{
			const FVector ST_L = PStart - StartRight * HalfStartPortalW;
			const FVector ST_R = PStart + StartRight * HalfStartPortalW;
			WallCeilingSection.AddThickWallQuad(
				ST_L + FVector(0, 0, ClampedStartPortalH), ST_R + FVector(0, 0, ClampedStartPortalH),
				ST_R + FVector(0, 0, Height + SlabThick), ST_L + FVector(0, 0, Height + SlabThick),
				StartForward, WallThick, 200.0f);
		}
	}

	// F) SOLID 3D Future-Expandable End Cap at the far end of the H-leg (or Framed Climax Portal when EndPortalWidthCm > 0)
	if (bCapFarEndForFutureExpansion && PointCount >= 2)
	{
		const int32 LastIdx = PointCount - 1;
		const FVector PEnd = CorridorSpline->GetLocationAtSplinePoint(LastIdx, ESplineCoordinateSpace::Local);
		const FVector PPrev = CorridorSpline->GetLocationAtSplinePoint(LastIdx - 1, ESplineCoordinateSpace::Local);
		const FVector Forward = (PEnd - PPrev).GetSafeNormal();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

		const FVector E0L = PEnd - Right * (HalfW + WallThick);
		const FVector E0R = PEnd + Right * (HalfW + WallThick);

		if (EndPortalWidthCm > 10.0f)
		{
			const float HalfEndPortalW = FMath::Clamp(EndPortalWidthCm * 0.5f, 60.0f, HalfW);
			const float ClampedEndPortalH = FMath::Clamp(EndPortalHeightCm, 220.0f, Height);
			const FVector EP_L = PEnd - Right * HalfEndPortalW;
			const FVector EP_R = PEnd + Right * HalfEndPortalW;

			// Left End Pier
			if (HalfW > HalfEndPortalW + 5.0f)
			{
				WallCeilingSection.AddThickWallQuad(
					E0L, EP_L,
					EP_L + FVector(0, 0, Height + SlabThick), E0L + FVector(0, 0, Height + SlabThick),
					-Forward, WallThick, 200.0f);
				// Right End Pier
				WallCeilingSection.AddThickWallQuad(
					EP_R, E0R,
					E0R + FVector(0, 0, Height + SlabThick), EP_R + FVector(0, 0, Height + SlabThick),
					-Forward, WallThick, 200.0f);
			}
			// Upper End Header above Climax Portal
			WallCeilingSection.AddThickWallQuad(
				EP_L + FVector(0, 0, ClampedEndPortalH), EP_R + FVector(0, 0, ClampedEndPortalH),
				EP_R + FVector(0, 0, Height + SlabThick), EP_L + FVector(0, 0, Height + SlabThick),
				-Forward, WallThick, 200.0f);
		}
		else
		{
			// Solid 3D Upper wall above shutter
			WallCeilingSection.AddThickWallQuad(
				E0L + FVector(0, 0, 320.0f), E0R + FVector(0, 0, 320.0f),
				E0R + FVector(0, 0, Height + SlabThick), E0L + FVector(0, 0, Height + SlabThick),
				-Forward, WallThick, 200.0f);
			// Solid 3D Corrugated security shutter barrier across the lower 320 cm
			MetalJointSection.AddThickWallQuad(
				E0L, E0R,
				E0R + FVector(0, 0, 320.0f), E0L + FVector(0, 0, 320.0f),
				-Forward, WallThick, 120.0f);
			if (ClosedRollerShutterMesh)
			{
				for (float OffsetY : { -HalfW * 0.5f, 0.0f, HalfW * 0.5f })
				{
					ShutterCapInstances->AddInstance(FTransform(
						(-Forward).Rotation(),
						PEnd + Right * OffsetY - Forward * 6.0f,
						FVector(1.0f)));
				}
			}
		}
	}

	auto Commit = [&](int32 Idx, const FSplineCorridorSection& Sec, UMaterialInterface* Mat, bool bCollision)
	{
		if (Sec.Vertices.Num() > 0 && Sec.Triangles.Num() > 0)
		{
			CorridorMesh->CreateMeshSection_LinearColor(
				Idx, Sec.Vertices, Sec.Triangles, Sec.Normals, Sec.UVs, Sec.Colors, Sec.Tangents, bCollision);
			if (Mat)
			{
				CorridorMesh->SetMaterial(Idx, Mat);
			}
		}
	};

	Commit(0, FloorSection, FloorMarbleMaterial, true);
	Commit(1, WallCeilingSection, WallPlasterMaterial, true);
	Commit(2, MetalJointSection, MetalJointMaterial, true);
	Commit(3, NeonCoveSection, NeonCoveMaterial, false);
}
