#include "Environment/MallCurvedAtriumRing.h"
#include "ProceduralMeshComponent.h"
#include "Components/SplineComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"

namespace
{
	struct FMallProcSectionData
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
		TArray<FProcMeshTangent> Tangents;

		void Reset()
		{
			Vertices.Reset();
			Triangles.Reset();
			Normals.Reset();
			UVs.Reset();
			Colors.Reset();
			Tangents.Reset();
		}

		void AddQuad(
			const FVector& V0,
			const FVector& V1,
			const FVector& V2,
			const FVector& V3,
			const FVector& N0,
			const FVector& N1,
			const FVector& N2,
			const FVector& V3Normal,
			const FVector2D& UV0,
			const FVector2D& UV1,
			const FVector2D& UV2,
			const FVector2D& UV3,
			const FVector& DesiredFaceNormal)
		{
			const int32 BaseIdx = Vertices.Num();
			Vertices.Add(V0);
			Vertices.Add(V1);
			Vertices.Add(V2);
			Vertices.Add(V3);

			Normals.Add(N0.GetSafeNormal());
			Normals.Add(N1.GetSafeNormal());
			Normals.Add(N2.GetSafeNormal());
			Normals.Add(V3Normal.GetSafeNormal());

			UVs.Add(UV0);
			UVs.Add(UV1);
			UVs.Add(UV2);
			UVs.Add(UV3);

			Colors.Add(FLinearColor::White);
			Colors.Add(FLinearColor::White);
			Colors.Add(FLinearColor::White);
			Colors.Add(FLinearColor::White);

			const FVector EdgeTangent = (V1 - V0).GetSafeNormal();
			const FProcMeshTangent Tangent(EdgeTangent.IsNearlyZero() ? FVector::ForwardVector : EdgeTangent, false);
			Tangents.Add(Tangent);
			Tangents.Add(Tangent);
			Tangents.Add(Tangent);
			Tangents.Add(Tangent);

			auto EmitTriIfNonDegenerate = [&](int32 I0, int32 I1, int32 I2)
			{
				const FVector& A = Vertices[I0];
				const FVector& B = Vertices[I1];
				const FVector& C = Vertices[I2];
				if (FVector::CrossProduct(B - A, C - A).SizeSquared() > 1.0f)
				{
					Triangles.Add(I0);
					Triangles.Add(I1);
					Triangles.Add(I2);
				}
			};

			const FVector Cross = FVector::CrossProduct(V2 - V0, V1 - V0);
			if (FVector::DotProduct(Cross, DesiredFaceNormal) >= 0.0f)
			{
				EmitTriIfNonDegenerate(BaseIdx + 0, BaseIdx + 1, BaseIdx + 2);
				EmitTriIfNonDegenerate(BaseIdx + 0, BaseIdx + 2, BaseIdx + 3);
			}
			else
			{
				EmitTriIfNonDegenerate(BaseIdx + 0, BaseIdx + 2, BaseIdx + 1);
				EmitTriIfNonDegenerate(BaseIdx + 0, BaseIdx + 3, BaseIdx + 2);
			}
		}

		void AddFlatQuad(
			const FVector& V0,
			const FVector& V1,
			const FVector& V2,
			const FVector& V3,
			const FVector& FaceNormal,
			float TileScaleCm)
		{
			const float InvScale = 1.0f / FMath::Max(TileScaleCm, 10.0f);
			auto ComputeUV = [&](const FVector& P) -> FVector2D
			{
				if (FMath::Abs(FaceNormal.Z) > 0.7f)
				{
					return FVector2D(P.X * InvScale, P.Y * InvScale);
				}
				return FVector2D((P.X + P.Y) * InvScale, P.Z * InvScale);
			};
			AddQuad(V0, V1, V2, V3, FaceNormal, FaceNormal, FaceNormal, FaceNormal,
				ComputeUV(V0), ComputeUV(V1), ComputeUV(V2), ComputeUV(V3), FaceNormal);
		}

		void AddDoubleSidedQuad(
			const FVector& V0,
			const FVector& V1,
			const FVector& V2,
			const FVector& V3,
			const FVector& PrimaryNormal,
			float TileScaleCm)
		{
			AddFlatQuad(V0, V1, V2, V3, PrimaryNormal, TileScaleCm);
			AddFlatQuad(V0, V3, V2, V1, -PrimaryNormal, TileScaleCm);
		}

		void AddSolidBox(const FVector& MinPt, const FVector& MaxPt, float TileScaleCm = 150.0f)
		{
			const FVector B00(MinPt.X, MinPt.Y, MinPt.Z);
			const FVector B10(MaxPt.X, MinPt.Y, MinPt.Z);
			const FVector B11(MaxPt.X, MaxPt.Y, MinPt.Z);
			const FVector B01(MinPt.X, MaxPt.Y, MinPt.Z);

			const FVector T00(MinPt.X, MinPt.Y, MaxPt.Z);
			const FVector T10(MaxPt.X, MinPt.Y, MaxPt.Z);
			const FVector T11(MaxPt.X, MaxPt.Y, MaxPt.Z);
			const FVector T01(MinPt.X, MaxPt.Y, MaxPt.Z);

			AddFlatQuad(T00, T10, T11, T01, FVector::UpVector, TileScaleCm);
			AddFlatQuad(B00, B01, B11, B10, FVector::DownVector, TileScaleCm);
			AddFlatQuad(B00, B10, T10, T00, FVector(0.0f, -1.0f, 0.0f), TileScaleCm);
			AddFlatQuad(B01, T01, T11, B11, FVector(0.0f, 1.0f, 0.0f), TileScaleCm);
			AddFlatQuad(B00, T00, T01, B01, FVector(-1.0f, 0.0f, 0.0f), TileScaleCm);
			AddFlatQuad(B10, B11, T11, T10, FVector(1.0f, 0.0f, 0.0f), TileScaleCm);
		}

		void AddSolidBeamSegment(const FVector& P0, const FVector& P1, float HalfWidth, float HalfHeight, float TileScaleCm = 120.0f)
		{
			const FVector Dir = (P1 - P0).GetSafeNormal();
			if (Dir.IsNearlyZero())
			{
				return;
			}
			FVector Right = FVector::CrossProduct(FVector::UpVector, Dir).GetSafeNormal();
			if (Right.IsNearlyZero())
			{
				Right = FVector::RightVector;
			}
			const FVector Up = FVector::CrossProduct(Dir, Right).GetSafeNormal();

			const FVector A0 = P0 - Right * HalfWidth - Up * HalfHeight;
			const FVector A1 = P0 + Right * HalfWidth - Up * HalfHeight;
			const FVector A2 = P0 + Right * HalfWidth + Up * HalfHeight;
			const FVector A3 = P0 - Right * HalfWidth + Up * HalfHeight;

			const FVector B0 = P1 - Right * HalfWidth - Up * HalfHeight;
			const FVector B1 = P1 + Right * HalfWidth - Up * HalfHeight;
			const FVector B2 = P1 + Right * HalfWidth + Up * HalfHeight;
			const FVector B3 = P1 - Right * HalfWidth + Up * HalfHeight;

			AddFlatQuad(A3, A2, B2, B3, Up, TileScaleCm);
			AddFlatQuad(A0, B0, B1, A1, -Up, TileScaleCm);
			AddFlatQuad(A1, B1, B2, A2, Right, TileScaleCm);
			AddFlatQuad(A0, A3, B3, B0, -Right, TileScaleCm);
			AddFlatQuad(A0, A1, A2, A3, -Dir, TileScaleCm);
			AddFlatQuad(B0, B3, B2, B1, Dir, TileScaleCm);
		}
	};

	FVector ProjectDirectionToRectangle(float AngleRad, float HalfExtentX, float HalfExtentY)
	{
		const float CosA = FMath::Cos(AngleRad);
		const float SinA = FMath::Sin(AngleRad);
		const float Tx = FMath::Abs(CosA) > 1e-4f ? (HalfExtentX / FMath::Abs(CosA)) : BIG_NUMBER;
		const float Ty = FMath::Abs(SinA) > 1e-4f ? (HalfExtentY / FMath::Abs(SinA)) : BIG_NUMBER;
		const float T = FMath::Min(Tx, Ty);
		return FVector(CosA * T, SinA * T, 0.0f);
	}
}

AMallCurvedAtriumRing::AMallCurvedAtriumRing()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InnerOvalGallerySpline = CreateDefaultSubobject<USplineComponent>(TEXT("InnerOvalGallerySpline"));
	InnerOvalGallerySpline->SetupAttachment(SceneRoot);
	InnerOvalGallerySpline->SetClosedLoop(true);

	AtriumRingSpline = CreateDefaultSubobject<USplineComponent>(TEXT("AtriumRingSpline"));
	AtriumRingSpline->SetupAttachment(SceneRoot);
	AtriumRingSpline->SetClosedLoop(true);

	ArchitectureMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ArchitectureMesh"));
	ArchitectureMesh->SetupAttachment(SceneRoot);
	ArchitectureMesh->bUseComplexAsSimpleCollision = true;
	ArchitectureMesh->SetCollisionProfileName(TEXT("BlockAll"));

	// Default Escalator Landing portals ONLY on Floor 3 (TargetUpperFloorIndex = 2) at SW (+142 deg) and SE (+38 deg)
	// where the Floor 2 -> Floor 3 and 2-Story Vertigo Express escalators actually land at Y = +1460, Z = 1700!
	// Floor 2 (FloorIdx = 1) escalators land on the West/East Cardinal Bridges (Y = -200), keeping Floor 2's outer rim 100% guarded!
	for (float AngleDeg : { 142.0f, 38.0f })
	{
		FMallAngularPortalSpan EscPortal;
		EscPortal.CenterAngleDegrees = AngleDeg;
		EscPortal.HalfAngleSpanDegrees = 12.0f;
		EscPortal.TargetUpperFloorIndex = 2;
		BalustradePortals.Add(EscPortal);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarbleMat(TEXT("/Game/Environment/Mall/Materials/M_MallFloorMarble.M_MallFloorMarble"));
	if (MarbleMat.Succeeded())
	{
		FloorMarbleMaterial = MarbleMat.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlasterMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_carcass.M_MallRetail_carcass"));
	if (PlasterMat.Succeeded())
	{
		PlasterFasciaMaterial = PlasterMat.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlassMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_glass.M_MallRetail_glass"));
	if (GlassMat.Succeeded())
	{
		GlassBalustradeMaterial = GlassMat.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MetalMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_metal.M_MallRetail_metal"));
	if (MetalMat.Succeeded())
	{
		BrushedMetalMaterial = MetalMat.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> AccentMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_accent.M_MallRetail_accent"));
	if (AccentMat.Succeeded())
	{
		AccentMedallionMaterial = AccentMat.Object;
	}
}

void AMallCurvedAtriumRing::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildCurvedArchitecture();
}

void AMallCurvedAtriumRing::BeginPlay()
{
	Super::BeginPlay();
	if (ArchitectureMesh && ArchitectureMesh->GetNumSections() == 0)
	{
		RebuildCurvedArchitecture();
	}
}

FVector AMallCurvedAtriumRing::EvaluateEllipsePoint(float RadiusX, float RadiusY, float AngleRadians, float ZElevation) const
{
	return FVector(
		FMath::Max(RadiusX, 50.0f) * FMath::Cos(AngleRadians),
		FMath::Max(RadiusY, 50.0f) * FMath::Sin(AngleRadians),
		ZElevation);
}

FVector AMallCurvedAtriumRing::EvaluateAtriumEllipsePoint(float AngleRadians, float ZElevation, float RadialOffset) const
{
	return EvaluateEllipsePoint(AtriumRadiusX + RadialOffset, AtriumRadiusY + RadialOffset, AngleRadians, ZElevation);
}

FVector AMallCurvedAtriumRing::EvaluateAtriumInwardNormal(float AngleRadians) const
{
	const FVector RawInward(-AtriumRadiusY * FMath::Cos(AngleRadians), -AtriumRadiusX * FMath::Sin(AngleRadians), 0.0f);
	return RawInward.GetSafeNormal();
}

float AMallCurvedAtriumRing::GetUpperFloorElevationZ(int32 UpperFloorIndex) const
{
	return static_cast<float>(FMath::Max(1, UpperFloorIndex)) * FloorToFloorHeightCm;
}

bool AMallCurvedAtriumRing::IsAngleInsideCardinalBridge(float AngleDegrees) const
{
	for (float Cardinal : { 0.0f, 90.0f, 180.0f, -90.0f })
	{
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(AngleDegrees, Cardinal)) <= CardinalBridgeHalfAngleDeg)
		{
			return true;
		}
	}
	return false;
}

bool AMallCurvedAtriumRing::IsAngleInsideOuterPortal(float AngleDegrees, int32 UpperFloorIndex) const
{
	if (IsAngleInsideCardinalBridge(AngleDegrees))
	{
		return true;
	}
	for (const FMallAngularPortalSpan& Portal : BalustradePortals)
	{
		if (Portal.TargetUpperFloorIndex != 0 && Portal.TargetUpperFloorIndex != UpperFloorIndex)
		{
			continue;
		}
		const float Delta = FMath::Abs(FMath::FindDeltaAngleDegrees(AngleDegrees, Portal.CenterAngleDegrees));
		if (Delta <= Portal.HalfAngleSpanDegrees)
		{
			return true;
		}
	}
	return false;
}

void AMallCurvedAtriumRing::RebuildCurvedArchitecture()
{
	if (!ArchitectureMesh)
	{
		return;
	}

	const int32 NumSegs = FMath::Clamp(CurveSegments, 48, 256);
	const int32 FloorCount = FMath::Clamp(NumUpperFloors, 1, 6);
	const float StepRad = (2.0f * PI) / static_cast<float>(NumSegs);

	// 1. Update both closed elliptical splines in the Editor so designers can see/snap to the Inner Oval & Outer Atrium Ring
	const float FirstUpperZ = GetUpperFloorElevationZ(1);
	if (InnerOvalGallerySpline)
	{
		InnerOvalGallerySpline->ClearSplinePoints(false);
		for (int32 i = 0; i < 16; ++i)
		{
			const float Angle = (2.0f * PI * static_cast<float>(i)) / 16.0f;
			InnerOvalGallerySpline->AddSplinePoint(
				EvaluateEllipsePoint(OvalIslandRadiusX, OvalIslandRadiusY, Angle, FirstUpperZ),
				ESplineCoordinateSpace::Local, false);
		}
		InnerOvalGallerySpline->SetClosedLoop(true, true);
	}
	if (AtriumRingSpline)
	{
		AtriumRingSpline->ClearSplinePoints(false);
		for (int32 i = 0; i < 16; ++i)
		{
			const float Angle = (2.0f * PI * static_cast<float>(i)) / 16.0f;
			AtriumRingSpline->AddSplinePoint(
				EvaluateAtriumEllipsePoint(Angle, FirstUpperZ, 0.0f),
				ESplineCoordinateSpace::Local, false);
		}
		AtriumRingSpline->SetClosedLoop(true, true);
	}

	ArchitectureMesh->ClearAllMeshSections();

	FMallProcSectionData MarbleSection;
	FMallProcSectionData PlasterSection;
	FMallProcSectionData GlassSection;
	FMallProcSectionData MetalSection;
	FMallProcSectionData AccentSection;

	// Solid 3D Curved Balustrade Segment:
	// - Solid Brushed-Metal Bottom Shoe Curb (Z = BaseZ .. BaseZ + 12cm)
	// - Solid 3D Thick Glass Panel (front, back, top, bottom faces)
	// - Solid Brushed-Metal Top Handrail Box (Z = RailTopZ - 8cm .. RailTopZ)
	// - Solid Vertical Brushed-Metal Mullion Post at segment boundary
	auto AddCurvedBalustradeSegment = [&](
		float R_X, float R_Y, float A0, float A1, float BaseZ, bool bFaceInward, int32 SegIndex)
	{
		const float CurbTopZ = BaseZ + 12.0f;
		const float GlassBottomZ = CurbTopZ;
		const float GlassTopZ = BaseZ + BalustradeHeightCm - 8.0f;
		const float RailTopZ = BaseZ + BalustradeHeightCm;

		const FVector Inward0 = FVector(-R_Y * FMath::Cos(A0), -R_X * FMath::Sin(A0), 0.0f).GetSafeNormal();
		const FVector Inward1 = FVector(-R_Y * FMath::Cos(A1), -R_X * FMath::Sin(A1), 0.0f).GetSafeNormal();
		const FVector Outward0 = -Inward0;
		const FVector Outward1 = -Inward1;
		const FVector AvgInward = (Inward0 + Inward1).GetSafeNormal();
		const FVector AvgOutward = -AvgInward;

		const float OffsetSign = bFaceInward ? 1.0f : -1.0f;
		const float RInX = R_X + OffsetSign * 8.0f;
		const float RInY = R_Y + OffsetSign * 8.0f;
		const float ROutX = R_X + OffsetSign * 18.0f;
		const float ROutY = R_Y + OffsetSign * 18.0f;

		// 1. Solid Metal Bottom Shoe Curb (visible from both sides!)
		const FVector CInBot0 = EvaluateEllipsePoint(RInX, RInY, A0, BaseZ);
		const FVector CInBot1 = EvaluateEllipsePoint(RInX, RInY, A1, BaseZ);
		const FVector CInTop0 = EvaluateEllipsePoint(RInX, RInY, A0, CurbTopZ);
		const FVector CInTop1 = EvaluateEllipsePoint(RInX, RInY, A1, CurbTopZ);
		const FVector COutBot0 = EvaluateEllipsePoint(ROutX, ROutY, A0, BaseZ);
		const FVector COutBot1 = EvaluateEllipsePoint(ROutX, ROutY, A1, BaseZ);
		const FVector COutTop0 = EvaluateEllipsePoint(ROutX, ROutY, A0, CurbTopZ);
		const FVector COutTop1 = EvaluateEllipsePoint(ROutX, ROutY, A1, CurbTopZ);

		MetalSection.AddFlatQuad(CInBot0, CInBot1, CInTop1, CInTop0, bFaceInward ? AvgInward : AvgOutward, 100.0f);
		MetalSection.AddFlatQuad(COutBot0, COutTop0, COutTop1, COutBot1, bFaceInward ? AvgOutward : AvgInward, 100.0f);
		MetalSection.AddFlatQuad(CInTop0, COutTop0, COutTop1, CInTop1, FVector::UpVector, 100.0f);

		// 2. Solid 3D Thick Glass Pane (both sides + top/bottom edges)
		const FVector GIn0 = EvaluateEllipsePoint(R_X + OffsetSign * 10.0f, R_Y + OffsetSign * 10.0f, A0, GlassBottomZ);
		const FVector GIn1 = EvaluateEllipsePoint(R_X + OffsetSign * 10.0f, R_Y + OffsetSign * 10.0f, A1, GlassBottomZ);
		const FVector GInTop0 = EvaluateEllipsePoint(R_X + OffsetSign * 10.0f, R_Y + OffsetSign * 10.0f, A0, GlassTopZ);
		const FVector GInTop1 = EvaluateEllipsePoint(R_X + OffsetSign * 10.0f, R_Y + OffsetSign * 10.0f, A1, GlassTopZ);

		const FVector GOut0 = EvaluateEllipsePoint(R_X + OffsetSign * 16.0f, R_Y + OffsetSign * 16.0f, A0, GlassBottomZ);
		const FVector GOut1 = EvaluateEllipsePoint(R_X + OffsetSign * 16.0f, R_Y + OffsetSign * 16.0f, A1, GlassBottomZ);
		const FVector GOutTop0 = EvaluateEllipsePoint(R_X + OffsetSign * 16.0f, R_Y + OffsetSign * 16.0f, A0, GlassTopZ);
		const FVector GOutTop1 = EvaluateEllipsePoint(R_X + OffsetSign * 16.0f, R_Y + OffsetSign * 16.0f, A1, GlassTopZ);

		GlassSection.AddDoubleSidedQuad(GIn0, GIn1, GInTop1, GInTop0, AvgInward, 120.0f);
		GlassSection.AddDoubleSidedQuad(GOut0, GOut1, GOutTop1, GOutTop0, AvgOutward, 120.0f);

		// 3. Solid 3D Brushed-Steel Top Handrail Box (4 faces: top, bottom, inner, outer)
		const FVector RInBot0 = EvaluateEllipsePoint(RInX - OffsetSign * 2.0f, RInY - OffsetSign * 2.0f, A0, GlassTopZ);
		const FVector RInBot1 = EvaluateEllipsePoint(RInX - OffsetSign * 2.0f, RInY - OffsetSign * 2.0f, A1, GlassTopZ);
		const FVector RInTop0 = EvaluateEllipsePoint(RInX - OffsetSign * 2.0f, RInY - OffsetSign * 2.0f, A0, RailTopZ);
		const FVector RInTop1 = EvaluateEllipsePoint(RInX - OffsetSign * 2.0f, RInY - OffsetSign * 2.0f, A1, RailTopZ);
		const FVector ROutBot0 = EvaluateEllipsePoint(ROutX + OffsetSign * 2.0f, ROutY + OffsetSign * 2.0f, A0, GlassTopZ);
		const FVector ROutBot1 = EvaluateEllipsePoint(ROutX + OffsetSign * 2.0f, ROutY + OffsetSign * 2.0f, A1, GlassTopZ);
		const FVector ROutTop0 = EvaluateEllipsePoint(ROutX + OffsetSign * 2.0f, ROutY + OffsetSign * 2.0f, A0, RailTopZ);
		const FVector ROutTop1 = EvaluateEllipsePoint(ROutX + OffsetSign * 2.0f, ROutY + OffsetSign * 2.0f, A1, RailTopZ);

		MetalSection.AddFlatQuad(RInTop0, ROutTop0, ROutTop1, RInTop1, FVector::UpVector, 100.0f);
		MetalSection.AddFlatQuad(RInBot0, RInBot1, ROutBot1, ROutBot0, FVector::DownVector, 100.0f);
		MetalSection.AddFlatQuad(RInBot0, RInBot1, RInTop1, RInTop0, bFaceInward ? AvgInward : AvgOutward, 100.0f);
		MetalSection.AddFlatQuad(ROutBot0, ROutTop0, ROutTop1, ROutBot1, bFaceInward ? AvgOutward : AvgInward, 100.0f);

		// 4. Vertical Brushed-Steel Balustrade Stanchion Post every 2 segments so railing is clearly visible from all angles
		if (SegIndex % 2 == 0)
		{
			const FVector PostPos = EvaluateEllipsePoint(R_X + OffsetSign * 13.0f, R_Y + OffsetSign * 13.0f, A0, BaseZ);
			MetalSection.AddSolidBeamSegment(PostPos, PostPos + FVector(0, 0, BalustradeHeightCm), 4.5f, 4.5f, 80.0f);
		}
	};

	const float RoofBaseZ = static_cast<float>(FloorCount + 1) * FloorToFloorHeightCm;
	const float DomeMidZ = RoofBaseZ + BiodomeApexRiseCm * 0.58f;
	const float DomeApexZ = RoofBaseZ + BiodomeApexRiseCm;

	for (int32 i = 0; i < NumSegs; ++i)
	{
		const float A0 = static_cast<float>(i) * StepRad;
		const float A1 = static_cast<float>(i + 1) * StepRad;
		const float MidAngleDeg = FMath::RadiansToDegrees((A0 + A1) * 0.5f);

		const FVector InwardN0 = EvaluateAtriumInwardNormal(A0);
		const FVector InwardN1 = EvaluateAtriumInwardNormal(A1);
		const FVector OutwardN0 = -InwardN0;
		const FVector OutwardN1 = -InwardN1;
		const FVector AvgInward = (InwardN0 + InwardN1).GetSafeNormal();
		const FVector AvgOutward = -AvgInward;

		const float UCoord0 = static_cast<float>(i) / static_cast<float>(NumSegs) * 16.0f;
		const float UCoord1 = static_cast<float>(i + 1) / static_cast<float>(NumSegs) * 16.0f;

		// ====================================================================
		// 1. GROUND FLOOR (Z = 0): Fountain Plaza + Geothermal Hot Springs Basins
		// ====================================================================
		const FVector G_Center(0.0f, 0.0f, 0.0f);
		const FVector G_FountainRing0 = EvaluateEllipsePoint(380.0f, 520.0f, A0, 0.0f);
		const FVector G_FountainRing1 = EvaluateEllipsePoint(380.0f, 520.0f, A1, 0.0f);
		const FVector G_ChannelIn0 = EvaluateEllipsePoint(520.0f, 760.0f, A0, 0.0f);
		const FVector G_ChannelIn1 = EvaluateEllipsePoint(520.0f, 760.0f, A1, 0.0f);
		const FVector G_ChannelOut0 = EvaluateEllipsePoint(880.0f, 1580.0f, A0, 0.0f);
		const FVector G_ChannelOut1 = EvaluateEllipsePoint(880.0f, 1580.0f, A1, 0.0f);
		const FVector G_OvalEdge0 = EvaluateEllipsePoint(OvalIslandRadiusX, OvalIslandRadiusY, A0, 0.0f);
		const FVector G_OvalEdge1 = EvaluateEllipsePoint(OvalIslandRadiusX, OvalIslandRadiusY, A1, 0.0f);
		const FVector G_Atrium0 = EvaluateAtriumEllipsePoint(A0, 0.0f, 0.0f);
		const FVector G_Atrium1 = EvaluateAtriumEllipsePoint(A1, 0.0f, 0.0f);
		const FVector G_Ring0 = EvaluateEllipsePoint(ConcourseRingRadiusX, ConcourseRingRadiusY, A0, 0.0f);
		const FVector G_Ring1 = EvaluateEllipsePoint(ConcourseRingRadiusX, ConcourseRingRadiusY, A1, 0.0f);
		const FVector G_Outer0 = ProjectDirectionToRectangle(A0, OuterExtentX, OuterExtentY);
		const FVector G_Outer1 = ProjectDirectionToRectangle(A1, OuterExtentX, OuterExtentY);

		MarbleSection.AddFlatQuad(G_Center, G_FountainRing0, G_FountainRing1, G_Center, FVector::UpVector, MarbleTileSizeCm);
		MarbleSection.AddFlatQuad(G_FountainRing0, G_ChannelIn0, G_ChannelIn1, G_FountainRing1, FVector::UpVector, MarbleTileSizeCm);
		MarbleSection.AddFlatQuad(G_ChannelIn0, G_ChannelOut0, G_ChannelOut1, G_ChannelIn1, FVector::UpVector, MarbleTileSizeCm);
		MarbleSection.AddFlatQuad(G_ChannelOut0, G_OvalEdge0, G_OvalEdge1, G_ChannelOut1, FVector::UpVector, MarbleTileSizeCm);
		MarbleSection.AddFlatQuad(G_OvalEdge0, G_Atrium0, G_Atrium1, G_OvalEdge1, FVector::UpVector, MarbleTileSizeCm);
		MarbleSection.AddFlatQuad(G_Atrium0, G_Ring0, G_Ring1, G_Atrium1, FVector::UpVector, MarbleTileSizeCm);
		MarbleSection.AddFlatQuad(G_Ring0, G_Outer0, G_Outer1, G_Ring1, FVector::UpVector, MarbleTileSizeCm);

		// Solid Underside Slab for Ground Floor (Z = -SlabThicknessCm) so nothing is single-sided from below
		PlasterSection.AddFlatQuad(
			G_Center - FVector(0, 0, SlabThicknessCm),
			G_Outer1 - FVector(0, 0, SlabThicknessCm),
			G_Outer0 - FVector(0, 0, SlabThicknessCm),
			G_Center - FVector(0, 0, SlabThicknessCm),
			FVector::DownVector, MarbleTileSizeCm);

		// Ground-floor 4-quadrant Geothermal Hot Springs & Arboretum Water Basins (leaving wide walkways at 0, 90, 180, -90 deg)
		bool bInGroundWalkway = false;
		for (float WalkAngle : { 0.0f, 90.0f, 180.0f, -90.0f })
		{
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(MidAngleDeg, WalkAngle)) <= 14.0f)
			{
				bInGroundWalkway = true;
				break;
			}
		}
		if (!bInGroundWalkway)
		{
			const float CurbH = 38.0f;
			const float WaterH = 28.0f;
			// SOLID 3D Inner & Outer Hot-Spring Stone Curb Walls (22cm thick with inner, outer, and top ledge faces!)
			const FVector InCurbOuter0 = EvaluateEllipsePoint(542.0f, 782.0f, A0, 0.0f);
			const FVector InCurbOuter1 = EvaluateEllipsePoint(542.0f, 782.0f, A1, 0.0f);
			PlasterSection.AddFlatQuad(G_ChannelIn0, G_ChannelIn1, G_ChannelIn1 + FVector(0, 0, CurbH), G_ChannelIn0 + FVector(0, 0, CurbH), AvgInward, 100.0f);
			PlasterSection.AddFlatQuad(InCurbOuter0, InCurbOuter0 + FVector(0, 0, CurbH), InCurbOuter1 + FVector(0, 0, CurbH), InCurbOuter1, AvgOutward, 100.0f);
			MarbleSection.AddFlatQuad(G_ChannelIn0 + FVector(0, 0, CurbH), InCurbOuter0 + FVector(0, 0, CurbH), InCurbOuter1 + FVector(0, 0, CurbH), G_ChannelIn1 + FVector(0, 0, CurbH), FVector::UpVector, 100.0f);

			const FVector OutCurbInner0 = EvaluateEllipsePoint(858.0f, 1558.0f, A0, 0.0f);
			const FVector OutCurbInner1 = EvaluateEllipsePoint(858.0f, 1558.0f, A1, 0.0f);
			PlasterSection.AddFlatQuad(G_ChannelOut0, G_ChannelOut0 + FVector(0, 0, CurbH), G_ChannelOut1 + FVector(0, 0, CurbH), G_ChannelOut1, AvgOutward, 100.0f);
			PlasterSection.AddFlatQuad(OutCurbInner0, OutCurbInner1, OutCurbInner1 + FVector(0, 0, CurbH), OutCurbInner0 + FVector(0, 0, CurbH), AvgInward, 100.0f);
			MarbleSection.AddFlatQuad(OutCurbInner0 + FVector(0, 0, CurbH), G_ChannelOut0 + FVector(0, 0, CurbH), G_ChannelOut1 + FVector(0, 0, CurbH), OutCurbInner1 + FVector(0, 0, CurbH), FVector::UpVector, 100.0f);

			// Double-sided Geothermal Hot-Spring Water Surface + Glowing Biotic Mineral Rim Strip
			GlassSection.AddDoubleSidedQuad(
				InCurbOuter0 + FVector(0, 0, WaterH),
				OutCurbInner0 + FVector(0, 0, WaterH),
				OutCurbInner1 + FVector(0, 0, WaterH),
				InCurbOuter1 + FVector(0, 0, WaterH),
				FVector::UpVector, 150.0f);
			AccentSection.AddDoubleSidedQuad(
				InCurbOuter0 + FVector(0, 0, WaterH - 4.0f),
				InCurbOuter0 + (OutCurbInner0 - InCurbOuter0) * 0.15f + FVector(0, 0, WaterH - 4.0f),
				InCurbOuter1 + (OutCurbInner1 - InCurbOuter1) * 0.15f + FVector(0, 0, WaterH - 4.0f),
				InCurbOuter1 + FVector(0, 0, WaterH - 4.0f),
				FVector::UpVector, 100.0f);
		}

		// ====================================================================
		// 2. MULTI-FLOOR UPPER GALLERIES & CENTRAL OVAL SHOPPING ISLAND
		// ====================================================================
		for (int32 FloorIdx = 1; FloorIdx <= FloorCount; ++FloorIdx)
		{
			const float FloorZ = GetUpperFloorElevationZ(FloorIdx);
			const float UndersideZ = FloorZ - SlabThicknessCm;

			// A) CENTRAL OVAL SHOPPING & ARBORETUM ISLAND (Radius 0 .. OvalIslandRadiusX/Y)
			// Leave two large elliptical "Soul Tree" & Waterfall Biotope Wells at the North & South foci of the Oval Island
			// (inner radius 0 .. 0.76 * OvalIslandRadius when |Sin(Angle)| > 0.22) while keeping a wide walkable ring around
			// the outer perimeter (0.76 .. 1.0) and a solid East-West cross-promenade across the center!
			const float WellInnerFrac = 0.76f;
			const FVector O_InnerTop0 = EvaluateEllipsePoint(OvalIslandRadiusX * WellInnerFrac, OvalIslandRadiusY * WellInnerFrac, A0, FloorZ);
			const FVector O_InnerTop1 = EvaluateEllipsePoint(OvalIslandRadiusX * WellInnerFrac, OvalIslandRadiusY * WellInnerFrac, A1, FloorZ);
			const FVector O_InnerBot0 = EvaluateEllipsePoint(OvalIslandRadiusX * WellInnerFrac, OvalIslandRadiusY * WellInnerFrac, A0, UndersideZ);
			const FVector O_InnerBot1 = EvaluateEllipsePoint(OvalIslandRadiusX * WellInnerFrac, OvalIslandRadiusY * WellInnerFrac, A1, UndersideZ);
			const FVector O_CenterTop0(0.0f, O_InnerTop0.Y, FloorZ);
			const FVector O_CenterTop1(0.0f, O_InnerTop1.Y, FloorZ);
			const FVector O_CenterBot0(0.0f, O_InnerBot0.Y, UndersideZ);
			const FVector O_CenterBot1(0.0f, O_InnerBot1.Y, UndersideZ);
			const FVector O_EdgeTop0 = EvaluateEllipsePoint(OvalIslandRadiusX, OvalIslandRadiusY, A0, FloorZ);
			const FVector O_EdgeTop1 = EvaluateEllipsePoint(OvalIslandRadiusX, OvalIslandRadiusY, A1, FloorZ);
			const FVector O_EdgeBot0 = EvaluateEllipsePoint(OvalIslandRadiusX, OvalIslandRadiusY, A0, UndersideZ);
			const FVector O_EdgeBot1 = EvaluateEllipsePoint(OvalIslandRadiusX, OvalIslandRadiusY, A1, UndersideZ);

			const bool bIsInNorthOrSouthBiotopeWell = (FMath::Abs(FMath::Sin( FMath::DegreesToRadians(MidAngleDeg) )) > 0.22f);

			// Outer walkable ring of the Central Oval Island (0.76 .. 1.0) is always solid
			MarbleSection.AddFlatQuad(O_InnerTop0, O_EdgeTop0, O_EdgeTop1, O_InnerTop1, FVector::UpVector, MarbleTileSizeCm);
			PlasterSection.AddFlatQuad(O_InnerBot0, O_InnerBot1, O_EdgeBot1, O_EdgeBot0, FVector::DownVector, MarbleTileSizeCm);

			if (!bIsInNorthOrSouthBiotopeWell)
			{
				// Solid full-width rectangular East-West central cross-deck between West & East Bridges
				MarbleSection.AddFlatQuad(O_CenterTop0, O_InnerTop0, O_InnerTop1, O_CenterTop1, FVector::UpVector, MarbleTileSizeCm);
				PlasterSection.AddFlatQuad(O_CenterBot0, O_InnerBot1, O_InnerBot0, O_CenterBot1, FVector::DownVector, MarbleTileSizeCm);
			}
			else
			{
				// Open North/South Soul-Tree & Waterfall Biotope Well: add inner well fascia & safety glass balustrade
				PlasterSection.AddQuad(
					O_InnerBot0, O_InnerBot1, O_InnerTop1, O_InnerTop0,
					InwardN0, InwardN1, InwardN1, InwardN0,
					FVector2D(UCoord0, 0), FVector2D(UCoord1, 0), FVector2D(UCoord1, 1), FVector2D(UCoord0, 1), AvgInward);
				AddCurvedBalustradeSegment(OvalIslandRadiusX * WellInnerFrac, OvalIslandRadiusY * WellInnerFrac, A0, A1, FloorZ, true, i);
			}

			// Curved Outer Fascia of Central Oval Island + Biotic-Cyan Neon Cove Strip
			PlasterSection.AddQuad(
				O_EdgeBot0, O_EdgeBot1, O_EdgeTop1, O_EdgeTop0,
				OutwardN0, OutwardN1, OutwardN1, OutwardN0,
				FVector2D(UCoord0, 0), FVector2D(UCoord1, 0), FVector2D(UCoord1, 1), FVector2D(UCoord0, 1), AvgOutward);
			AccentSection.AddQuad(
				O_EdgeBot0 + OutwardN0 * 1.5f, O_EdgeBot1 + OutwardN1 * 1.5f,
				O_EdgeBot1 + OutwardN1 * 1.5f + FVector(0, 0, 8.0f), O_EdgeBot0 + OutwardN0 * 1.5f + FVector(0, 0, 8.0f),
				OutwardN0, OutwardN1, OutwardN1, OutwardN0,
				FVector2D(UCoord0, 0), FVector2D(UCoord1, 0), FVector2D(UCoord1, 1), FVector2D(UCoord0, 1), AvgOutward);

			// Central Oval Outer Balustrade (except where the 4 Cardinal Bridges connect!)
			if (!IsAngleInsideCardinalBridge(MidAngleDeg))
			{
				AddCurvedBalustradeSegment(OvalIslandRadiusX, OvalIslandRadiusY, A0, A1, FloorZ, false, i);
			}

			// B) 4 CARDINAL CLEAR-SPAN "OASIS WALK" BRIDGES (Connecting Central Oval Island across the 7.5m Atrium Chasm)
			if (IsAngleInsideCardinalBridge(MidAngleDeg))
			{
				const bool bIsEastBridge = (FMath::Cos(A0) > 0.7f);
				const bool bIsEastOrWestBridge = (FMath::Abs(FMath::Cos(A0)) > 0.7f);
				// On Floor 2 (FloorIdx == 1) East Bridge, stop outer bridge at t=0.62 (X=+1415) so the 2-Story Vertigo Express Escalator at X=+1560 passes cleanly to Floor 3!
				const float BridgeSpanFraction = (bIsEastBridge && FloorIdx == 1) ? 0.62f : 1.0f;

				const FVector FullA_Top0 = EvaluateAtriumEllipsePoint(A0, FloorZ, 0.0f);
				const FVector FullA_Top1 = EvaluateAtriumEllipsePoint(A1, FloorZ, 0.0f);
				const FVector FullA_Bot0 = EvaluateAtriumEllipsePoint(A0, UndersideZ, 0.0f);
				const FVector FullA_Bot1 = EvaluateAtriumEllipsePoint(A1, UndersideZ, 0.0f);

				const FVector A_EdgeTop0 = FMath::Lerp(O_EdgeTop0, FullA_Top0, BridgeSpanFraction);
				const FVector A_EdgeTop1 = FMath::Lerp(O_EdgeTop1, FullA_Top1, BridgeSpanFraction);
				const FVector A_EdgeBot0 = FMath::Lerp(O_EdgeBot0, FullA_Bot0, BridgeSpanFraction);
				const FVector A_EdgeBot1 = FMath::Lerp(O_EdgeBot1, FullA_Bot1, BridgeSpanFraction);

				// Bridge walkable top & plaster underside
				MarbleSection.AddFlatQuad(O_EdgeTop0, A_EdgeTop0, A_EdgeTop1, O_EdgeTop1, FVector::UpVector, MarbleTileSizeCm);
				PlasterSection.AddFlatQuad(O_EdgeBot0, A_EdgeBot1, A_EdgeBot0, O_EdgeBot1, FVector::DownVector, MarbleTileSizeCm);
				PlasterSection.AddFlatQuad(O_EdgeBot0, O_EdgeBot1, A_EdgeBot1, A_EdgeBot0, FVector::DownVector, MarbleTileSizeCm);

				const float PrevAngleDeg = FMath::RadiansToDegrees(A0 - StepRad * 0.5f);
				const float NextAngleDeg = FMath::RadiansToDegrees(A1 + StepRad * 0.5f);
				if (!IsAngleInsideCardinalBridge(PrevAngleDeg))
				{
					const FVector SideNormal = FVector::CrossProduct(A_EdgeTop0 - O_EdgeTop0, FVector::UpVector).GetSafeNormal();
					PlasterSection.AddDoubleSidedQuad(O_EdgeBot0, A_EdgeBot0, A_EdgeTop0, O_EdgeTop0, SideNormal, 100.0f);
					if (!bIsEastOrWestBridge)
					{
						MetalSection.AddSolidBeamSegment(O_EdgeTop0 + FVector(0, 0, BalustradeHeightCm), A_EdgeTop0 + FVector(0, 0, BalustradeHeightCm), 6.0f, 4.0f, 100.0f);
						MetalSection.AddSolidBeamSegment(O_EdgeTop0 + FVector(0, 0, 6.0f), A_EdgeTop0 + FVector(0, 0, 6.0f), 6.0f, 6.0f, 100.0f);
						GlassSection.AddDoubleSidedQuad(
							O_EdgeTop0 + FVector(0, 0, 12.0f), A_EdgeTop0 + FVector(0, 0, 12.0f),
							A_EdgeTop0 + FVector(0, 0, BalustradeHeightCm - 6.0f), O_EdgeTop0 + FVector(0, 0, BalustradeHeightCm - 6.0f),
							SideNormal, 100.0f);
					}
				}
				if (!IsAngleInsideCardinalBridge(NextAngleDeg))
				{
					const FVector SideNormal = FVector::CrossProduct(FVector::UpVector, A_EdgeTop1 - O_EdgeTop1).GetSafeNormal();
					PlasterSection.AddDoubleSidedQuad(O_EdgeBot1, A_EdgeBot1, A_EdgeTop1, O_EdgeTop1, SideNormal, 100.0f);
					if (!bIsEastOrWestBridge)
					{
						MetalSection.AddSolidBeamSegment(O_EdgeTop1 + FVector(0, 0, BalustradeHeightCm), A_EdgeTop1 + FVector(0, 0, BalustradeHeightCm), 6.0f, 4.0f, 100.0f);
						MetalSection.AddSolidBeamSegment(O_EdgeTop1 + FVector(0, 0, 6.0f), A_EdgeTop1 + FVector(0, 0, 6.0f), 6.0f, 6.0f, 100.0f);
						GlassSection.AddDoubleSidedQuad(
							O_EdgeTop1 + FVector(0, 0, 12.0f), A_EdgeTop1 + FVector(0, 0, 12.0f),
							A_EdgeTop1 + FVector(0, 0, BalustradeHeightCm - 6.0f), O_EdgeTop1 + FVector(0, 0, BalustradeHeightCm - 6.0f),
							SideNormal, 100.0f);
					}
				}
			}

			// C) OUTER CONCOURSE GALLERY SLAB (From AtriumRadius out to OuterExtentX/Y)
			const FVector U_AtriumTop0 = EvaluateAtriumEllipsePoint(A0, FloorZ, 0.0f);
			const FVector U_AtriumTop1 = EvaluateAtriumEllipsePoint(A1, FloorZ, 0.0f);
			const FVector U_RingTop0 = EvaluateEllipsePoint(ConcourseRingRadiusX, ConcourseRingRadiusY, A0, FloorZ);
			const FVector U_RingTop1 = EvaluateEllipsePoint(ConcourseRingRadiusX, ConcourseRingRadiusY, A1, FloorZ);
			const FVector U_OuterTop0 = G_Outer0 + FVector(0.0f, 0.0f, FloorZ);
			const FVector U_OuterTop1 = G_Outer1 + FVector(0.0f, 0.0f, FloorZ);

			const FVector U_AtriumBot0 = EvaluateAtriumEllipsePoint(A0, UndersideZ, 0.0f);
			const FVector U_AtriumBot1 = EvaluateAtriumEllipsePoint(A1, UndersideZ, 0.0f);
			const FVector U_RingBot0 = EvaluateEllipsePoint(ConcourseRingRadiusX, ConcourseRingRadiusY, A0, UndersideZ);
			const FVector U_RingBot1 = EvaluateEllipsePoint(ConcourseRingRadiusX, ConcourseRingRadiusY, A1, UndersideZ);
			const FVector U_OuterBot0 = G_Outer0 + FVector(0.0f, 0.0f, UndersideZ);
			const FVector U_OuterBot1 = G_Outer1 + FVector(0.0f, 0.0f, UndersideZ);

			MarbleSection.AddFlatQuad(U_AtriumTop0, U_RingTop0, U_RingTop1, U_AtriumTop1, FVector::UpVector, MarbleTileSizeCm);
			MarbleSection.AddFlatQuad(U_RingTop0, U_OuterTop0, U_OuterTop1, U_RingTop1, FVector::UpVector, MarbleTileSizeCm);

			PlasterSection.AddFlatQuad(U_AtriumBot0, U_AtriumBot1, U_RingBot1, U_RingBot0, FVector::DownVector, MarbleTileSizeCm);
			PlasterSection.AddFlatQuad(U_RingBot0, U_RingBot1, U_OuterBot1, U_OuterBot0, FVector::DownVector, MarbleTileSizeCm);

			// Smooth Curved Elliptical Atrium Fascia + Recessed Neon Cove Strip
			PlasterSection.AddQuad(
				U_AtriumBot0, U_AtriumBot1, U_AtriumTop1, U_AtriumTop0,
				InwardN0, InwardN1, InwardN1, InwardN0,
				FVector2D(UCoord0, 0.0f), FVector2D(UCoord1, 0.0f), FVector2D(UCoord1, 1.0f), FVector2D(UCoord0, 1.0f),
				AvgInward);
			AccentSection.AddQuad(
				U_AtriumBot0 + InwardN0 * 1.5f, U_AtriumBot1 + InwardN1 * 1.5f,
				U_AtriumBot1 + InwardN1 * 1.5f + FVector(0, 0, 10.0f), U_AtriumBot0 + InwardN0 * 1.5f + FVector(0, 0, 10.0f),
				InwardN0, InwardN1, InwardN1, InwardN0,
				FVector2D(UCoord0, 0.0f), FVector2D(UCoord1, 0.0f), FVector2D(UCoord1, 1.0f), FVector2D(UCoord0, 1.0f),
				AvgInward);

			// Outer Atrium Balustrade (open at Cardinal Bridges & Escalator Landings)
			if (!IsAngleInsideOuterPortal(MidAngleDeg, FloorIdx))
			{
				AddCurvedBalustradeSegment(AtriumRadiusX, AtriumRadiusY, A0, A1, FloorZ, true, i);
			}
		}

		// ====================================================================
		// 3. VAULTED SPACESHIP / BIODOME SKYLIGHT CANOPY & SOLID 3D ARCH RIBS
		// ====================================================================
		const FVector R_Outer0 = G_Outer0 + FVector(0.0f, 0.0f, RoofBaseZ);
		const FVector R_Outer1 = G_Outer1 + FVector(0.0f, 0.0f, RoofBaseZ);
		const FVector R_Ring0 = EvaluateEllipsePoint(ConcourseRingRadiusX, ConcourseRingRadiusY, A0, RoofBaseZ);
		const FVector R_Ring1 = EvaluateEllipsePoint(ConcourseRingRadiusX, ConcourseRingRadiusY, A1, RoofBaseZ);
		const FVector R_Mid0 = EvaluateEllipsePoint(AtriumRadiusX * 0.72f, AtriumRadiusY * 0.72f, A0, DomeMidZ);
		const FVector R_Mid1 = EvaluateEllipsePoint(AtriumRadiusX * 0.72f, AtriumRadiusY * 0.72f, A1, DomeMidZ);
		const FVector R_Oculus0 = EvaluateEllipsePoint(OvalIslandRadiusX * 0.45f, OvalIslandRadiusY * 0.45f, A0, DomeApexZ);
		const FVector R_Oculus1 = EvaluateEllipsePoint(OvalIslandRadiusX * 0.45f, OvalIslandRadiusY * 0.45f, A1, DomeApexZ);

		// Outer solid double-sided ceiling soffit (blocks directional sunlight outside the central Atrium Ring!)
		PlasterSection.AddDoubleSidedQuad(R_Ring0, R_Outer0, R_Outer1, R_Ring1, FVector::DownVector, MarbleTileSizeCm);
		PlasterSection.AddDoubleSidedQuad(
			R_Ring0 + FVector(0, 0, SlabThicknessCm),
			R_Outer0 + FVector(0, 0, SlabThicknessCm),
			R_Outer1 + FVector(0, 0, SlabThicknessCm),
			R_Ring1 + FVector(0, 0, SlabThicknessCm),
			FVector::UpVector, MarbleTileSizeCm);

		// Sunlit Vaulted Biodome Skylight Tiers directly over the Central Atrium & Ghost-Hunting Shop Galleries
		GlassSection.AddDoubleSidedQuad(R_Mid0, R_Ring0, R_Ring1, R_Mid1, FVector::DownVector, 250.0f);
		GlassSection.AddDoubleSidedQuad(R_Oculus0, R_Mid0, R_Mid1, R_Oculus1, FVector::DownVector, 250.0f);

		// Glowing Biotic-Cyan Skylight Oculus Ring overhead
		AccentSection.AddDoubleSidedQuad(
			EvaluateEllipsePoint(0.0f, 0.0f, A0, DomeApexZ + 10.0f),
			R_Oculus0, R_Oculus1,
			EvaluateEllipsePoint(0.0f, 0.0f, A1, DomeApexZ + 10.0f),
			FVector::DownVector, 200.0f);

		// SOLID 3D Spaceship Structural Arch Beams in the Upper Dome Vault (every 6 segments = 16 solid 3D ribs)
		// Never slices across walkways or creates planar walls!
		if (i % (NumSegs / 16) == 0)
		{
			MetalSection.AddSolidBeamSegment(R_Ring0, R_Mid0, 18.0f, 22.0f, 120.0f);
			MetalSection.AddSolidBeamSegment(R_Mid0, R_Oculus0, 14.0f, 18.0f, 120.0f);
		}
	}

	// ====================================================================
	// 4. SQUARED-OFF ESCALATOR LANDING APRONS & SAFETY BALUSTRADES (Zero Gap / Zero Drop-Off!)
	// ====================================================================
	auto AddStraightBalustradeWall = [&](const FVector& P0, const FVector& P1, float BaseZ)
	{
		const FVector MinPt(
			FMath::Min(P0.X, P1.X) - 6.0f,
			FMath::Min(P0.Y, P1.Y) - 6.0f,
			BaseZ);
		const FVector MaxPt(
			FMath::Max(P0.X, P1.X) + 6.0f,
			FMath::Max(P0.Y, P1.Y) + 6.0f,
			BaseZ + BalustradeHeightCm);
		MetalSection.AddSolidBox(FVector(MinPt.X, MinPt.Y, BaseZ), FVector(MaxPt.X, MaxPt.Y, BaseZ + 12.0f), 100.0f);
		GlassSection.AddSolidBox(FVector(MinPt.X + 2.0f, MinPt.Y + 2.0f, BaseZ + 12.0f), FVector(MaxPt.X - 2.0f, MaxPt.Y - 2.0f, BaseZ + BalustradeHeightCm - 8.0f), 120.0f);
		MetalSection.AddSolidBox(FVector(MinPt.X, MinPt.Y, BaseZ + BalustradeHeightCm - 8.0f), FVector(MaxPt.X, MaxPt.Y, BaseZ + BalustradeHeightCm), 100.0f);
	};

	for (int32 FloorIdx = 1; FloorIdx <= FloorCount; ++FloorIdx)
	{
		const float FloorZ = GetUpperFloorElevationZ(FloorIdx);
		const float BotZ = FloorZ - SlabThicknessCm;

		// West & East Central Bridge Aprons (Y = -340 .. +340) + Central Oval Cross-Deck Guardrails
		MarbleSection.AddSolidBox(FVector(-1740.0f, -340.0f, BotZ), FVector(-920.0f, 340.0f, FloorZ), MarbleTileSizeCm);
		const float EastBridgeMaxX = (FloorIdx == 1) ? 1450.0f : 1740.0f;
		MarbleSection.AddSolidBox(FVector(920.0f, -340.0f, BotZ), FVector(EastBridgeMaxX, 340.0f, FloorZ), MarbleTileSizeCm);
		// Guard the North & South well edges of the Central Oval East-West cross-deck (X = -715 .. +715, Y = +-325)
		AddStraightBalustradeWall(FVector(-715.0f, -325.0f, 0.0f), FVector(715.0f, -325.0f, 0.0f), FloorZ);
		AddStraightBalustradeWall(FVector(-715.0f, 325.0f, 0.0f), FVector(715.0f, 325.0f, 0.0f), FloorZ);

		if (FloorIdx == 1)
		{
			// On Floor 2 (Z=850), guard the inner bridge edges (X = -1140..-920 and +920..+1140) and the East Bridge outer slot edge (X = +1450)
			AddStraightBalustradeWall(FVector(-1140.0f, -340.0f, 0.0f), FVector(-920.0f, -340.0f, 0.0f), FloorZ);
			AddStraightBalustradeWall(FVector(-1140.0f, 340.0f, 0.0f), FVector(-920.0f, 340.0f, 0.0f), FloorZ);
			AddStraightBalustradeWall(FVector(920.0f, -340.0f, 0.0f), FVector(1140.0f, -340.0f, 0.0f), FloorZ);
			AddStraightBalustradeWall(FVector(920.0f, 340.0f, 0.0f), FVector(1140.0f, 340.0f, 0.0f), FloorZ);
			// Guard the outer East edge of the Floor 2 East Bridge at X = +1450 where the 2-Story Vertigo Express passes by at X = +1560!
			AddStraightBalustradeWall(FVector(1445.0f, -340.0f, 0.0f), FVector(1445.0f, 340.0f, 0.0f), FloorZ);
		}
		else if (FloorIdx == 2)
		{
			// On Floor 3 (Z=1700), no escalators dock at the Central Bridges (Y = +-340), so guard the entire North & South sides of both bridges!
			AddStraightBalustradeWall(FVector(-1700.0f, -340.0f, 0.0f), FVector(-920.0f, -340.0f, 0.0f), FloorZ);
			AddStraightBalustradeWall(FVector(-1700.0f, 340.0f, 0.0f), FVector(-920.0f, 340.0f, 0.0f), FloorZ);
			AddStraightBalustradeWall(FVector(920.0f, -340.0f, 0.0f), FVector(1700.0f, -340.0f, 0.0f), FloorZ);
			AddStraightBalustradeWall(FVector(920.0f, 340.0f, 0.0f), FVector(1700.0f, 340.0f, 0.0f), FloorZ);

			// Floor 3 SW & SE Escalator Upper Landing Aprons (Y = +1300 .. +1620, where F2->F3 and Vertigo Express escalators arrive at Y = +1460)
			MarbleSection.AddSolidBox(FVector(-1740.0f, 1300.0f, BotZ), FVector(-1080.0f, 1620.0f, FloorZ), MarbleTileSizeCm);
			MarbleSection.AddSolidBox(FVector(1080.0f, 1300.0f, BotZ), FVector(1740.0f, 1620.0f, FloorZ), MarbleTileSizeCm);

			// Guard the inner chasm edges (X = -1080 and X = +1080) of the Floor 3 Escalator Landing Aprons
			AddStraightBalustradeWall(FVector(-1085.0f, 1300.0f, 0.0f), FVector(-1085.0f, 1620.0f, 0.0f), FloorZ);
			AddStraightBalustradeWall(FVector(1085.0f, 1300.0f, 0.0f), FVector(1085.0f, 1620.0f, 0.0f), FloorZ);
		}
	}

	// ====================================================================
	// 5. CHONGQING 'THE RING' (MU GUANG FOREST) MONUMENTAL ARBORETUM:
	//    - 3 Colossal Multi-Story 'Soul Tree' Biophilic Columns
	//    - 4 Anti-Gravity 'Floating Forest' Suspended Planter Spheres
	//    - 20-Meter Vertical Waterfall & Cantilevered Trough -> Hot Springs
	// ====================================================================
	if (bBuildMonumentalArboretumAndHotSprings)
	{
		auto AddCylindricalPlanterDrum = [&](const FVector& Center, float Radius, float Height, float TrunkRadius, float TrunkHeight)
		{
			const int32 DrumSegs = 24;
			const float DStep = (2.0f * PI) / static_cast<float>(DrumSegs);
			for (int32 s = 0; s < DrumSegs; ++s)
			{
				const float CA0 = s * DStep;
				const float CA1 = (s + 1) * DStep;
				const FVector Dir0(FMath::Cos(CA0), FMath::Sin(CA0), 0.0f);
				const FVector Dir1(FMath::Cos(CA1), FMath::Sin(CA1), 0.0f);
				const FVector AvgDir = (Dir0 + Dir1).GetSafeNormal();

				const FVector B0 = Center + Dir0 * Radius;
				const FVector B1 = Center + Dir1 * Radius;
				const FVector T0 = B0 + FVector(0, 0, Height);
				const FVector T1 = B1 + FVector(0, 0, Height);

				MetalSection.AddFlatQuad(B0, B1, T1, T0, AvgDir, 100.0f);
				MarbleSection.AddFlatQuad(Center + FVector(0, 0, Height), T0, T1, Center + FVector(0, 0, Height), FVector::UpVector, 120.0f);
				PlasterSection.AddFlatQuad(Center, B1, B0, Center, FVector::DownVector, 120.0f);
				AccentSection.AddFlatQuad(B0 + AvgDir * 1.5f + FVector(0, 0, 8.0f), B1 + AvgDir * 1.5f + FVector(0, 0, 8.0f), B1 + AvgDir * 1.5f + FVector(0, 0, 16.0f), B0 + AvgDir * 1.5f + FVector(0, 0, 16.0f), AvgDir, 80.0f);

				if (TrunkRadius > 10.0f && TrunkHeight > 10.0f)
				{
					const FVector TB0 = Center + Dir0 * TrunkRadius;
					const FVector TB1 = Center + Dir1 * TrunkRadius;
					const FVector TT0 = TB0 + FVector(0, 0, TrunkHeight);
					const FVector TT1 = TB1 + FVector(0, 0, TrunkHeight);
					PlasterSection.AddFlatQuad(TB0, TB1, TT1, TT0, AvgDir, 100.0f);
				}
			}
		};

		// A) 2 Colossal North & South "Soul Tree" Biophilic Columns rising from Ground Hot Springs through Floor 2 & Floor 3
		for (float FocusY : { -1120.0f, 1120.0f })
		{
			AddCylindricalPlanterDrum(FVector(0.0f, FocusY, 0.0f), 260.0f, 75.0f, 68.0f, 2150.0f);
			AddCylindricalPlanterDrum(FVector(0.0f, FocusY, 440.0f), 210.0f, 52.0f, 0.0f, 0.0f);
			AddCylindricalPlanterDrum(FVector(0.0f, FocusY, 1260.0f), 195.0f, 48.0f, 0.0f, 0.0f);
		}

		// B) 4 Biophilic "Floating Forest" Terrace Planter Pods in the Open Atrium Chasm (with Ground Column + Dome Suspension Cables!)
		struct FSuspendedBioPodSpec
		{
			FVector Center;
			float Radius;
		};
		const FSuspendedBioPodSpec BioPods[] = {
			{ FVector(-1280.0f, -920.0f, 620.0f), 210.0f },
			{ FVector(1280.0f, -920.0f, 1140.0f), 220.0f },
			{ FVector(-1280.0f, 920.0f, 1320.0f), 210.0f },
			{ FVector(1280.0f, 920.0f, 660.0f), 205.0f },
		};
		for (const FSuspendedBioPodSpec& Pod : BioPods)
		{
			// Grounded architectural base plinth & vertical biophilic column rising from Z = 0 to the pod!
			AddCylindricalPlanterDrum(FVector(Pod.Center.X, Pod.Center.Y, 0.0f), 96.0f, 38.0f, 36.0f, Pod.Center.Z);
			AddCylindricalPlanterDrum(Pod.Center, Pod.Radius, 58.0f, 0.0f, 0.0f);
			AddCylindricalPlanterDrum(Pod.Center - FVector(0, 0, 34.0f), Pod.Radius * 0.68f, 34.0f, 0.0f, 0.0f);
			// 3 Steel Suspension Cables anchored all the way up to the Spaceship Biodome Roof!
			for (float CableAngDeg : { 30.0f, 150.0f, 270.0f })
			{
				const float Rad = FMath::DegreesToRadians(CableAngDeg);
				const FVector RimPt = Pod.Center + FVector(FMath::Cos(Rad) * (Pod.Radius - 18.0f), FMath::Sin(Rad) * (Pod.Radius - 18.0f), 58.0f);
				const FVector RoofPt(RimPt.X, RimPt.Y, DomeMidZ + 60.0f);
				MetalSection.AddSolidBeamSegment(RimPt, RoofPt, 3.5f, 3.5f, 100.0f);
			}
		}

		// C) 20-Meter Aerial Waterfall Column & Cantilevered Water Trough at North Focus (Y = -1520, Z = 0 .. 1700)
		// Cantilevered Brushed-Steel Water Chute on Floor 3 (Z = 1700)
		MetalSection.AddSolidBox(FVector(-150.0f, -1680.0f, 1665.0f), FVector(150.0f, -1380.0f, 1715.0f), 100.0f);
		// Solid 3D Translucent Cascading Waterfall Sheet (Z = 24 .. 1690) + Glowing Biotic Mist Core
		GlassSection.AddSolidBox(FVector(-120.0f, -1440.0f, 24.0f), FVector(120.0f, -1412.0f, 1690.0f), 160.0f);
		AccentSection.AddDoubleSidedQuad(
			FVector(-95.0f, -1426.0f, 28.0f), FVector(95.0f, -1426.0f, 28.0f),
			FVector(95.0f, -1426.0f, 1685.0f), FVector(-95.0f, -1426.0f, 1685.0f),
			FVector(0.0f, 1.0f, 0.0f), 120.0f);
	}

	// ====================================================================
	// 6. SOLID 3D OUTER PERIMETER ENCLOSURE WITH H-HALLWAY & CENTRAL SPINE PORTALS
	// ====================================================================
	if (bEncloseOuterPerimeterWithHallwayPortals)
	{
		const float WallThick = 28.0f;
		// North (Y = -OuterExtentY) & South (Y = +OuterExtentY) Perimeter Walls:
		// Leave 900cm-wide open concourse portals at:
		//   - West H-Leg: X = [-2750 .. -1850] (centered at X = -2300)
		//   - 3rd Central Spine (Food Court & Hot Springs Grotto): X = [-450 .. +450] (centered at X = 0)
		//   - East H-Leg: X = [+1850 .. +2750] (centered at X = +2300)
		for (float SignY : { -1.0f, 1.0f })
		{
			const float YMin = (SignY < 0.0f) ? (-OuterExtentY - WallThick) : OuterExtentY;
			const float YMax = YMin + WallThick;

			// Solid Wall Piers between the 3 North/South Concourse Portals (full height Z = 0 .. RoofBaseZ)
			PlasterSection.AddSolidBox(FVector(-OuterExtentX - WallThick, YMin, 0.0f), FVector(-2750.0f, YMax, RoofBaseZ), 200.0f);
			PlasterSection.AddSolidBox(FVector(-1850.0f, YMin, 0.0f), FVector(-450.0f, YMax, RoofBaseZ), 200.0f);
			PlasterSection.AddSolidBox(FVector(450.0f, YMin, 0.0f), FVector(1850.0f, YMax, RoofBaseZ), 200.0f);
			PlasterSection.AddSolidBox(FVector(2750.0f, YMin, 0.0f), FVector(OuterExtentX + WallThick, YMax, RoofBaseZ), 200.0f);

			// Solid Upper Spandrel Headers above the West & East H-Leg Portals (Floor 2 slab spandrel Z = 800..850, and Floor 3 enclosure Z = 1650..RoofBaseZ)
			for (float PortalCenterX : { -2300.0f, 2300.0f })
			{
				PlasterSection.AddSolidBox(FVector(PortalCenterX - 450.0f, YMin, 795.0f), FVector(PortalCenterX + 450.0f, YMax, 852.0f), 200.0f);
				PlasterSection.AddSolidBox(FVector(PortalCenterX - 450.0f, YMin, 1645.0f), FVector(PortalCenterX + 450.0f, YMax, RoofBaseZ), 200.0f);
			}

			// Central Spine Portal (X = -450 .. +450): Open on Ground Floor (Z = 0 .. 800) for North Food Court & South Bathhouse/Sauna Biome Halls;
			// completely sealed on Floor 2 & Floor 3 (Z = 795 .. RoofBaseZ) so players on upper floors can never jump onto the hall roofs
			PlasterSection.AddSolidBox(FVector(-450.0f, YMin, 795.0f), FVector(450.0f, YMax, RoofBaseZ), 200.0f);
		}

		// West (X = -OuterExtentX) & East (X = +OuterExtentX) Perimeter Walls:
		// Leave open 960cm-wide Ground-Floor Storefront Bays at Y = [-1480 .. -520], [-480 .. +480], [+520 .. +1480] (Z = 0 .. 740)
		// for the 6 Sunlit Ghost-Hunting Tool & Cart Base-Camp Shops, and seal Z = 740 .. RoofBaseZ so Floor 2 & Floor 3 are 100% enclosed!
		for (float SignX : { -1.0f, 1.0f })
		{
			const float XMin = (SignX < 0.0f) ? (-OuterExtentX - WallThick) : OuterExtentX;
			const float XMax = XMin + WallThick;

			PlasterSection.AddSolidBox(FVector(XMin, -OuterExtentY, 0.0f), FVector(XMax, -1480.0f, RoofBaseZ), 200.0f);
			PlasterSection.AddSolidBox(FVector(XMin, -520.0f, 0.0f), FVector(XMax, -480.0f, RoofBaseZ), 200.0f);
			PlasterSection.AddSolidBox(FVector(XMin, 480.0f, 0.0f), FVector(XMax, 520.0f, RoofBaseZ), 200.0f);
			PlasterSection.AddSolidBox(FVector(XMin, 1480.0f, 0.0f), FVector(XMax, OuterExtentY, RoofBaseZ), 200.0f);

			// Solid enclosure across Floor 2 (Z = 640 .. 1700) above the Ground-Floor Sunlit Base-Camp Storefront Bays
			PlasterSection.AddSolidBox(FVector(XMin, -1480.0f, 640.0f), FVector(XMax, 1480.0f, 1700.0f), 200.0f);
			// On Floor 3 (Z = 1700 .. 2340), leave Y = [-1480 .. -520] and [+520 .. +1480] open for the 4 Penthouse Anchor Stores,
			// while enclosing the central bay Y = [-520 .. +520] and the upper roof transom Z = 2340 .. RoofBaseZ!
			PlasterSection.AddSolidBox(FVector(XMin, -520.0f, 1700.0f), FVector(XMax, 520.0f, RoofBaseZ), 200.0f);
			PlasterSection.AddSolidBox(FVector(XMin, -1480.0f, 2340.0f), FVector(XMax, 1480.0f, RoofBaseZ), 200.0f);
		}
	}

	auto CommitSection = [&](int32 SectionIdx, const FMallProcSectionData& Data, UMaterialInterface* Mat, bool bCollision)
	{
		if (Data.Vertices.Num() > 0 && Data.Triangles.Num() > 0)
		{
			ArchitectureMesh->CreateMeshSection_LinearColor(
				SectionIdx, Data.Vertices, Data.Triangles, Data.Normals, Data.UVs, Data.Colors, Data.Tangents, bCollision);
			if (Mat)
			{
				ArchitectureMesh->SetMaterial(SectionIdx, Mat);
			}
		}
	};

	CommitSection(0, MarbleSection, FloorMarbleMaterial, true);
	CommitSection(1, PlasterSection, PlasterFasciaMaterial, true);
	CommitSection(2, GlassSection, GlassBalustradeMaterial, true);
	CommitSection(3, MetalSection, BrushedMetalMaterial, true);
	CommitSection(4, AccentSection, AccentMedallionMaterial, false);
}
