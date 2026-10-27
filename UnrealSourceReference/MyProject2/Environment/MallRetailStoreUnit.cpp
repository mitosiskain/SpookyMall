#include "Environment/MallRetailStoreUnit.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	struct FStoreShellSection
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

		void AddSolidBox(const FVector& MinPt, const FVector& MaxPt, float UVScale = 200.0f)
		{
			const FVector B00(MinPt.X, MinPt.Y, MinPt.Z);
			const FVector B10(MaxPt.X, MinPt.Y, MinPt.Z);
			const FVector B11(MaxPt.X, MaxPt.Y, MinPt.Z);
			const FVector B01(MinPt.X, MaxPt.Y, MinPt.Z);

			const FVector T00(MinPt.X, MinPt.Y, MaxPt.Z);
			const FVector T10(MaxPt.X, MinPt.Y, MaxPt.Z);
			const FVector T11(MaxPt.X, MaxPt.Y, MaxPt.Z);
			const FVector T01(MinPt.X, MaxPt.Y, MaxPt.Z);

			AddQuad(T00, T10, T11, T01, FVector::UpVector, UVScale);
			AddQuad(B00, B01, B11, B10, FVector::DownVector, UVScale);
			AddQuad(B00, B10, T10, T00, FVector(0.0f, -1.0f, 0.0f), UVScale);
			AddQuad(B01, T01, T11, B11, FVector(0.0f, 1.0f, 0.0f), UVScale);
			AddQuad(B00, T00, T01, B01, FVector(-1.0f, 0.0f, 0.0f), UVScale);
			AddQuad(B10, B11, T11, T10, FVector(1.0f, 0.0f, 0.0f), UVScale);
		}

		void AddTexturedQuad(
			const FVector& V0,
			const FVector& V1,
			const FVector& V2,
			const FVector& V3,
			const FVector& FaceNormal,
			const FVector2D& UV0,
			const FVector2D& UV1,
			const FVector2D& UV2,
			const FVector2D& UV3)
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

			UVs.Add(UV0);
			UVs.Add(UV1);
			UVs.Add(UV2);
			UVs.Add(UV3);

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

		void AddTransformedMerchandiseTier(const FTransform& FixtureTransform, const FVector& LocalMin, const FVector& LocalMax)
		{
			auto Xf = [&](float X, float Y, float Z)
			{
				return FixtureTransform.TransformPosition(FVector(X, Y, Z));
			};
			auto XfN = [&](const FVector& N)
			{
				return FixtureTransform.TransformVectorNoScale(N).GetSafeNormal();
			};

			const FVector B00 = Xf(LocalMin.X, LocalMin.Y, LocalMin.Z);
			const FVector B10 = Xf(LocalMax.X, LocalMin.Y, LocalMin.Z);
			const FVector B11 = Xf(LocalMax.X, LocalMax.Y, LocalMin.Z);
			const FVector B01 = Xf(LocalMin.X, LocalMax.Y, LocalMin.Z);

			const FVector T00 = Xf(LocalMin.X, LocalMin.Y, LocalMax.Z);
			const FVector T10 = Xf(LocalMax.X, LocalMin.Y, LocalMax.Z);
			const FVector T11 = Xf(LocalMax.X, LocalMax.Y, LocalMax.Z);
			const FVector T01 = Xf(LocalMin.X, LocalMax.Y, LocalMax.Z);

			// Front & Back vertical product display faces (mapped 0..1 vertically so the 2-row shelf texture renders crisp upright product boxes/bottles)
			AddTexturedQuad(B00, B10, T10, T00, XfN(FVector(0.0f, -1.0f, 0.0f)),
				FVector2D(0.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 0.0f), FVector2D(0.0f, 0.0f));
			AddTexturedQuad(B11, B01, T01, T11, XfN(FVector(0.0f, 1.0f, 0.0f)),
				FVector2D(0.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 0.0f), FVector2D(0.0f, 0.0f));

			// Left & Right end-cap faces
			AddTexturedQuad(B01, B00, T00, T01, XfN(FVector(-1.0f, 0.0f, 0.0f)),
				FVector2D(0.0f, 1.0f), FVector2D(0.35f, 1.0f), FVector2D(0.35f, 0.0f), FVector2D(0.0f, 0.0f));
			AddTexturedQuad(B10, B11, T11, T10, XfN(FVector(1.0f, 0.0f, 0.0f)),
				FVector2D(0.65f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 0.0f), FVector2D(0.65f, 0.0f));

			// Top & Bottom faces of the merchandise block
			AddTexturedQuad(T00, T10, T11, T01, XfN(FVector::UpVector),
				FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.5f), FVector2D(0.0f, 0.5f));
			AddTexturedQuad(B00, B01, B11, B10, XfN(FVector::DownVector),
				FVector2D(0.0f, 0.5f), FVector2D(1.0f, 0.5f), FVector2D(1.0f, 1.0f), FVector2D(0.0f, 1.0f));

			// Also add an upright double-sided textured merchandise display plane rising slightly above the box spine
			const float MidY = (LocalMin.Y + LocalMax.Y) * 0.5f;
			const float CardTopZ = LocalMax.Z + 8.0f;
			const FVector C0 = Xf(LocalMin.X, MidY, LocalMin.Z);
			const FVector C1 = Xf(LocalMax.X, MidY, LocalMin.Z);
			const FVector C2 = Xf(LocalMax.X, MidY, CardTopZ);
			const FVector C3 = Xf(LocalMin.X, MidY, CardTopZ);
			AddTexturedQuad(C0, C1, C2, C3, XfN(FVector(0.0f, -1.0f, 0.0f)),
				FVector2D(0.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 0.0f), FVector2D(0.0f, 0.0f));
			AddTexturedQuad(C1, C0, C3, C2, XfN(FVector(0.0f, 1.0f, 0.0f)),
				FVector2D(0.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 0.0f), FVector2D(0.0f, 0.0f));
		}
	};

	UStaticMesh* TryLoadRetailMesh(const TCHAR* PrimaryPath, const TCHAR* FallbackPath = nullptr)
	{
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, PrimaryPath))
		{
			return Mesh;
		}
		if (FallbackPath)
		{
			return LoadObject<UStaticMesh>(nullptr, FallbackPath);
		}
		return nullptr;
	}
}

AMallRetailStoreUnit::AMallRetailStoreUnit()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	StoreShellMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("StoreShellMesh"));
	StoreShellMesh->SetupAttachment(SceneRoot);
	StoreShellMesh->bUseComplexAsSimpleCollision = true;
	StoreShellMesh->SetCollisionProfileName(TEXT("BlockAll"));

	FasciaSignText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("FasciaSignText"));
	FasciaSignText->SetupAttachment(SceneRoot);
	FasciaSignText->SetHorizontalAlignment(EHTA_Center);
	FasciaSignText->SetVerticalAlignment(EVRTA_TextCenter);
	FasciaSignText->SetWorldSize(42.0f);
	FasciaSignText->SetTextRenderColor(FColor(235, 245, 255));

	UpperFasciaSignText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("UpperFasciaSignText"));
	UpperFasciaSignText->SetupAttachment(SceneRoot);
	UpperFasciaSignText->SetHorizontalAlignment(EHTA_Center);
	UpperFasciaSignText->SetVerticalAlignment(EVRTA_TextCenter);
	UpperFasciaSignText->SetWorldSize(38.0f);
	UpperFasciaSignText->SetTextRenderColor(FColor(255, 235, 165));
	UpperFasciaSignText->SetHiddenInGame(true);

	StoreInteriorSpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("StoreInteriorSpotLight"));
	StoreInteriorSpotLight->SetupAttachment(SceneRoot);
	StoreInteriorSpotLight->SetIntensityUnits(ELightUnits::Lumens);
	StoreInteriorSpotLight->SetIntensity(580.0f);
	StoreInteriorSpotLight->SetAttenuationRadius(1120.0f);
	StoreInteriorSpotLight->SetInnerConeAngle(36.0f);
	StoreInteriorSpotLight->SetOuterConeAngle(68.0f);
	StoreInteriorSpotLight->SetSourceRadius(14.0f);
	StoreInteriorSpotLight->SetSoftSourceRadius(20.0f);
	StoreInteriorSpotLight->SetSourceLength(24.0f);
	StoreInteriorSpotLight->SetRelativeRotation(FRotator(-86.0f, 0.0f, 0.0f));
	StoreInteriorSpotLight->SetCastShadows(false);
	StoreInteriorSpotLight->MaxDrawDistance = 3800.0f;
	StoreInteriorSpotLight->MaxDistanceFadeRange = 800.0f;

	StoreUpperFloorSpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("StoreUpperFloorSpotLight"));
	StoreUpperFloorSpotLight->SetupAttachment(SceneRoot);
	StoreUpperFloorSpotLight->SetIntensityUnits(ELightUnits::Lumens);
	StoreUpperFloorSpotLight->SetIntensity(0.0f);
	StoreUpperFloorSpotLight->SetAttenuationRadius(1050.0f);
	StoreUpperFloorSpotLight->SetInnerConeAngle(36.0f);
	StoreUpperFloorSpotLight->SetOuterConeAngle(68.0f);
	StoreUpperFloorSpotLight->SetSourceRadius(14.0f);
	StoreUpperFloorSpotLight->SetSoftSourceRadius(20.0f);
	StoreUpperFloorSpotLight->SetSourceLength(24.0f);
	StoreUpperFloorSpotLight->SetRelativeRotation(FRotator(-88.0f, 0.0f, 0.0f));
	StoreUpperFloorSpotLight->SetCastShadows(false);
	StoreUpperFloorSpotLight->SetVisibility(false);
	StoreUpperFloorSpotLight->MaxDrawDistance = 3800.0f;
	StoreUpperFloorSpotLight->MaxDistanceFadeRange = 800.0f;

	StoreInteriorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StoreInteriorLight"));
	StoreInteriorLight->SetupAttachment(SceneRoot);
	StoreInteriorLight->SetIntensityUnits(ELightUnits::Lumens);
	StoreInteriorLight->SetIntensity(85.0f);
	StoreInteriorLight->SetAttenuationRadius(520.0f);
	StoreInteriorLight->SetSourceRadius(14.0f);
	StoreInteriorLight->SetSoftSourceRadius(18.0f);
	StoreInteriorLight->SetSourceLength(16.0f);
	StoreInteriorLight->SetCastShadows(false);
	StoreInteriorLight->MaxDrawDistance = 3200.0f;
	StoreInteriorLight->MaxDistanceFadeRange = 600.0f;

	StoreUpperFloorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StoreUpperFloorLight"));
	StoreUpperFloorLight->SetupAttachment(SceneRoot);
	StoreUpperFloorLight->SetIntensityUnits(ELightUnits::Lumens);
	StoreUpperFloorLight->SetIntensity(0.0f);
	StoreUpperFloorLight->SetAttenuationRadius(480.0f);
	StoreUpperFloorLight->SetSourceRadius(14.0f);
	StoreUpperFloorLight->SetSoftSourceRadius(18.0f);
	StoreUpperFloorLight->SetSourceLength(16.0f);
	StoreUpperFloorLight->SetCastShadows(false);
	StoreUpperFloorLight->SetVisibility(false);
	StoreUpperFloorLight->MaxDrawDistance = 3200.0f;
	StoreUpperFloorLight->MaxDistanceFadeRange = 600.0f;

	StoreLightFixtureMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StoreLightFixtureMesh"));
	StoreLightFixtureMesh->SetupAttachment(SceneRoot);
	StoreLightFixtureMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StoreLightFixtureMesh->SetCastShadow(false);

	StoreUpperLightFixtureMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StoreUpperLightFixtureMesh"));
	StoreUpperLightFixtureMesh->SetupAttachment(SceneRoot);
	StoreUpperLightFixtureMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StoreUpperLightFixtureMesh->SetCastShadow(false);
	StoreUpperLightFixtureMesh->SetVisibility(false);
	StoreUpperLightFixtureMesh->SetHiddenInGame(true);

	StoreLightAimBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("StoreLightAimBounds"));
	StoreLightAimBounds->SetupAttachment(SceneRoot);
	StoreLightAimBounds->InitBoxExtent(FVector(78.0f, 45.0f, 32.0f));
	StoreLightAimBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StoreLightAimBounds->SetCollisionObjectType(ECC_WorldDynamic);
	StoreLightAimBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	StoreLightAimBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	StoreLightAimBounds->SetGenerateOverlapEvents(false);
	StoreLightAimBounds->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TrackLightFinder(TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25354_CeilingTrackLightingBar.SM_Retail_25354_CeilingTrackLightingBar"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackCylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (TrackLightFinder.Succeeded())
	{
		StoreLightFixtureMesh->SetStaticMesh(TrackLightFinder.Object);
		StoreUpperLightFixtureMesh->SetStaticMesh(TrackLightFinder.Object);
	}
	else if (FallbackCylinder.Succeeded())
	{
		StoreLightFixtureMesh->SetStaticMesh(FallbackCylinder.Object);
		StoreUpperLightFixtureMesh->SetStaticMesh(FallbackCylinder.Object);
	}

	RollerShutterComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RollerShutterComponent"));
	RollerShutterComponent->SetupAttachment(SceneRoot);
	RollerShutterComponent->SetCastShadow(false);

	CheckoutCounterComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CheckoutCounterComponent"));
	CheckoutCounterComponent->SetupAttachment(SceneRoot);
	CheckoutCounterComponent->SetCollisionProfileName(TEXT("BlockAll"));
	CheckoutCounterComponent->SetCastShadow(false);

	POSTerminalComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("POSTerminalComponent"));
	POSTerminalComponent->SetupAttachment(CheckoutCounterComponent);
	POSTerminalComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	POSTerminalComponent->SetCastShadow(false);

	BackPanelComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackPanelComponent"));
	BackPanelComponent->SetupAttachment(SceneRoot);
	BackPanelComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackPanelComponent->SetCastShadow(false);

	PrimaryFixtureInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("PrimaryFixtureInstances"));
	PrimaryFixtureInstances->SetupAttachment(SceneRoot);
	PrimaryFixtureInstances->SetCollisionProfileName(TEXT("BlockAll"));
	PrimaryFixtureInstances->SetCullDistances(2600, 4200);
	PrimaryFixtureInstances->SetCastShadow(false);

	SecondaryFixtureInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("SecondaryFixtureInstances"));
	SecondaryFixtureInstances->SetupAttachment(SceneRoot);
	SecondaryFixtureInstances->SetCollisionProfileName(TEXT("BlockAll"));
	SecondaryFixtureInstances->SetCullDistances(2600, 4200);
	SecondaryFixtureInstances->SetCastShadow(false);

	MannequinDisplayInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("MannequinDisplayInstances"));
	MannequinDisplayInstances->SetupAttachment(SceneRoot);
	MannequinDisplayInstances->SetCollisionProfileName(TEXT("BlockAll"));
	MannequinDisplayInstances->SetCullDistances(2400, 3800);
	MannequinDisplayInstances->SetCastShadow(false);

	WallBayInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("WallBayInstances"));
	WallBayInstances->SetupAttachment(SceneRoot);
	WallBayInstances->SetCollisionProfileName(TEXT("BlockAll"));
	WallBayInstances->SetCullDistances(2600, 4200);
	WallBayInstances->SetCastShadow(false);

	ShelfStockPrimaryInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ShelfStockPrimaryInstances"));
	ShelfStockPrimaryInstances->SetupAttachment(SceneRoot);
	ShelfStockPrimaryInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShelfStockPrimaryInstances->SetCullDistances(2200, 3600);
	ShelfStockPrimaryInstances->SetCastShadow(false);

	ShelfStockSecondaryInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ShelfStockSecondaryInstances"));
	ShelfStockSecondaryInstances->SetupAttachment(SceneRoot);
	ShelfStockSecondaryInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShelfStockSecondaryInstances->SetCullDistances(2200, 3600);
	ShelfStockSecondaryInstances->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MarbleMat(TEXT("/Game/Environment/Mall/Materials/M_MallFloorMarble.M_MallFloorMarble"));
	if (MarbleMat.Succeeded())
	{
		FloorMaterial = MarbleMat.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PlasterMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_carcass.M_MallRetail_carcass"));
	if (PlasterMat.Succeeded())
	{
		WallMaterial = PlasterMat.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlassMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_glass.M_MallRetail_glass"));
	if (GlassMat.Succeeded())
	{
		GlassMaterial = GlassMat.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MetalMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_metal.M_MallRetail_metal"));
	if (MetalMat.Succeeded())
	{
		MetalMaterial = MetalMat.Object;
	}
}

void AMallRetailStoreUnit::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildStoreUnit();
}

void AMallRetailStoreUnit::BeginPlay()
{
	Super::BeginPlay();
	if (StoreShellMesh && StoreShellMesh->GetNumSections() == 0)
	{
		RebuildStoreUnit();
	}
}

void AMallRetailStoreUnit::SetNightShutterClosed(bool bClosed)
{
	bNightShutterClosed = bClosed;
	UStaticMesh* OpenMesh = TryLoadRetailMesh(TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25345_RollerShutterOpen.SM_Retail_25345_RollerShutterOpen"));
	UStaticMesh* ClosedMesh = TryLoadRetailMesh(TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25344_RollerShutterClosed.SM_Retail_25344_RollerShutterClosed"));
	if (RollerShutterComponent)
	{
		RollerShutterComponent->SetStaticMesh(bNightShutterClosed ? ClosedMesh : OpenMesh);
		RollerShutterComponent->SetCollisionEnabled(bNightShutterClosed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
}

FTransform AMallRetailStoreUnit::GetShopkeeperCounterTransform() const
{
	const FVector LocalPos(StoreDepthCm - 140.0f, 0.0f, 0.0f);
	const FRotator LocalRot(0.0f, 180.0f, 0.0f);
	return FTransform(LocalRot, LocalPos) * GetActorTransform();
}

FTransform AMallRetailStoreUnit::GetStoreTerritoryCenterTransform() const
{
	const FVector LocalPos(StoreDepthCm * 0.52f, 0.0f, 92.0f);
	const FRotator LocalRot(0.0f, 180.0f, 0.0f);
	return FTransform(LocalRot, LocalPos) * GetActorTransform();
}

bool AMallRetailStoreUnit::ToggleStoreLights()
{
	bStoreLightPowered = !bStoreLightPowered;
	StoreLightBrightnessMode = bStoreLightPowered ? 2 : 0;
	ApplyStoreLightState();
	return bStoreLightPowered;
}

uint8 AMallRetailStoreUnit::CycleStoreLightBrightness()
{
	if (!bStoreLightPowered || StoreLightBrightnessMode == 0)
	{
		bStoreLightPowered = true;
		StoreLightBrightnessMode = 2;
	}
	else if (StoreLightBrightnessMode >= 2)
	{
		StoreLightBrightnessMode = 1;
	}
	else
	{
		bStoreLightPowered = false;
		StoreLightBrightnessMode = 0;
	}
	ApplyStoreLightState();
	return StoreLightBrightnessMode;
}

void AMallRetailStoreUnit::ApplyStoreLightState()
{
	const float Scale = (!bStoreLightPowered || StoreLightBrightnessMode == 0)
		? 0.0f
		: (StoreLightBrightnessMode == 1 ? 0.45f : 1.0f);
	if (StoreInteriorSpotLight)
	{
		StoreInteriorSpotLight->SetIntensity(BaseStoreLightIntensity * Scale);
		StoreInteriorSpotLight->SetVisibility(Scale > 0.0f);
	}
	if (StoreInteriorLight)
	{
		// Soft 14% room ambient bounce in the center of the shop (never forms circular wall blobs)
		StoreInteriorLight->SetIntensity(BaseStoreLightIntensity * 0.14f * Scale);
		StoreInteriorLight->SetVisibility(Scale > 0.0f);
	}
	const bool bShowUpper = bIsMultiFloorAnchorStore && Scale > 0.0f;
	if (StoreUpperFloorSpotLight)
	{
		StoreUpperFloorSpotLight->SetIntensity(bShowUpper ? (BaseStoreLightIntensity * 0.85f * Scale) : 0.0f);
		StoreUpperFloorSpotLight->SetVisibility(bShowUpper);
	}
	if (StoreUpperFloorLight)
	{
		StoreUpperFloorLight->SetIntensity(bShowUpper ? (BaseStoreLightIntensity * 0.12f * Scale) : 0.0f);
		StoreUpperFloorLight->SetVisibility(bShowUpper);
	}
}

void AMallRetailStoreUnit::RebuildStoreUnit()
{
	if (!StoreShellMesh)
	{
		return;
	}

	const float HalfW = FMath::Max(StoreWidthCm * 0.5f, 200.0f);
	const float Depth = FMath::Max(StoreDepthCm, 400.0f);
	const float UpperZ = FMath::Max(UpperFloorHeightCm, 450.0f);
	const float Height = bIsMultiFloorAnchorStore
		? FMath::Max(StoreHeightCm, UpperZ + 740.0f)
		: FMath::Max(StoreHeightCm, 320.0f);
	const float HalfDoor = FMath::Clamp(FrontEntranceWidthCm * 0.5f, 65.0f, HalfW - 80.0f);
	const float DoorH = FMath::Min(340.0f, (bIsMultiFloorAnchorStore ? UpperZ : Height) - 60.0f);
	const float WallThick = 24.0f;
	const float SlabThick = 28.0f;

	StoreShellMesh->ClearAllMeshSections();
	PrimaryFixtureInstances->ClearInstances();
	SecondaryFixtureInstances->ClearInstances();
	MannequinDisplayInstances->ClearInstances();
	WallBayInstances->ClearInstances();
	if (ShelfStockPrimaryInstances)
	{
		ShelfStockPrimaryInstances->ClearInstances();
	}
	if (ShelfStockSecondaryInstances)
	{
		ShelfStockSecondaryInstances->ClearInstances();
	}

	FStoreShellSection FloorSec;
	FStoreShellSection WallCeilingSec;
	FStoreShellSection GlassSec;
	FStoreShellSection MetalSec;
	FStoreShellSection ShelfMerchandiseSec;

	// 1. SOLID 3D Store Floor Slab (Z = -SlabThick .. 0) & SOLID 3D Roof/Ceiling Slab (Z = Height .. Height + SlabThick)
	// Blocks all exterior directional sunlight from leaking into hallway stores and is solid from all 6 sides!
	FloorSec.AddSolidBox(FVector(0.0f, -HalfW, -SlabThick), FVector(Depth, HalfW, 0.0f), 200.0f);
	WallCeilingSec.AddSolidBox(FVector(0.0f, -HalfW, Height), FVector(Depth, HalfW, Height + SlabThick), 200.0f);

	// 2. SOLID 3D Front Storefront Wall at X = 0 .. WallThick
	auto AddFramedStoreWindowAtZ = [&](float YStart, float YEnd, float BaseZ)
	{
		if (YEnd - YStart < 20.0f)
		{
			return;
		}
		MetalSec.AddSolidBox(FVector(2.0f, YStart, BaseZ + 0.0f), FVector(WallThick - 2.0f, YEnd, BaseZ + 22.0f), 100.0f);
		MetalSec.AddSolidBox(FVector(2.0f, YStart, BaseZ + DoorH - 10.0f), FVector(WallThick - 2.0f, YEnd, BaseZ + DoorH), 100.0f);
		const float YMid = (YStart + YEnd) * 0.5f;
		MetalSec.AddSolidBox(FVector(4.0f, YMid - 4.0f, BaseZ + 22.0f), FVector(WallThick - 4.0f, YMid + 4.0f, BaseZ + DoorH - 10.0f), 100.0f);
		GlassSec.AddSolidBox(FVector(7.0f, YStart, BaseZ + 22.0f), FVector(17.0f, YEnd, BaseZ + DoorH - 10.0f), 150.0f);
	};

	if (!bIsMultiFloorAnchorStore)
	{
		// Single-story Upper fascia header box above the doorway & windows (Z = DoorH .. Height)
		WallCeilingSec.AddSolidBox(FVector(0.0f, -HalfW, DoorH), FVector(WallThick, HalfW, Height), 200.0f);
		// Left & Right front corner piers
		WallCeilingSec.AddSolidBox(FVector(0.0f, -HalfW, 0.0f), FVector(WallThick, -HalfW + 32.0f, DoorH), 200.0f);
		WallCeilingSec.AddSolidBox(FVector(0.0f, HalfW - 32.0f, 0.0f), FVector(WallThick, HalfW, DoorH), 200.0f);
		// Solid Brushed-Metal Door Jambs flanking the entrance
		MetalSec.AddSolidBox(FVector(0.0f, -HalfDoor - 10.0f, 0.0f), FVector(WallThick, -HalfDoor, DoorH), 100.0f);
		MetalSec.AddSolidBox(FVector(0.0f, HalfDoor, 0.0f), FVector(WallThick, HalfDoor + 10.0f, DoorH), 100.0f);
		AddFramedStoreWindowAtZ(-HalfW + 32.0f, -HalfDoor - 10.0f, 0.0f);
		AddFramedStoreWindowAtZ(HalfDoor + 10.0f, HalfW - 32.0f, 0.0f);
	}
	else
	{
		// 2-Story Multi-Floor Anchor Department Store Front Facade!
		// Ground Floor Front Facade (Z = 0 .. UpperZ)
		WallCeilingSec.AddSolidBox(FVector(0.0f, -HalfW, DoorH), FVector(WallThick, HalfW, UpperZ), 200.0f);
		WallCeilingSec.AddSolidBox(FVector(0.0f, -HalfW, 0.0f), FVector(WallThick, -HalfW + 32.0f, DoorH), 200.0f);
		WallCeilingSec.AddSolidBox(FVector(0.0f, HalfW - 32.0f, 0.0f), FVector(WallThick, HalfW, DoorH), 200.0f);
		MetalSec.AddSolidBox(FVector(0.0f, -HalfDoor - 10.0f, 0.0f), FVector(WallThick, -HalfDoor, DoorH), 100.0f);
		MetalSec.AddSolidBox(FVector(0.0f, HalfDoor, 0.0f), FVector(WallThick, HalfDoor + 10.0f, DoorH), 100.0f);
		AddFramedStoreWindowAtZ(-HalfW + 32.0f, -HalfDoor - 10.0f, 0.0f);
		AddFramedStoreWindowAtZ(HalfDoor + 10.0f, HalfW - 32.0f, 0.0f);

		// Floor 2 Front Facade (Z = UpperZ .. Height) with matching Floor 2 Concourse Entrance Portal!
		WallCeilingSec.AddSolidBox(FVector(0.0f, -HalfW, UpperZ + DoorH), FVector(WallThick, HalfW, Height), 200.0f);
		WallCeilingSec.AddSolidBox(FVector(0.0f, -HalfW, UpperZ), FVector(WallThick, -HalfW + 32.0f, UpperZ + DoorH), 200.0f);
		WallCeilingSec.AddSolidBox(FVector(0.0f, HalfW - 32.0f, UpperZ), FVector(WallThick, HalfW, UpperZ + DoorH), 200.0f);
		MetalSec.AddSolidBox(FVector(0.0f, -HalfDoor - 10.0f, UpperZ), FVector(WallThick, -HalfDoor, UpperZ + DoorH), 100.0f);
		MetalSec.AddSolidBox(FVector(0.0f, HalfDoor, UpperZ), FVector(WallThick, HalfDoor + 10.0f, UpperZ + DoorH), 100.0f);
		AddFramedStoreWindowAtZ(-HalfW + 32.0f, -HalfDoor - 10.0f, UpperZ);
		AddFramedStoreWindowAtZ(HalfDoor + 10.0f, HalfW - 32.0f, UpperZ);

		// Internal Mezzanine Gallery Floor Slab at Z = UpperZ - SlabThick .. UpperZ
		// Front Entrance Bridge (X = 0 .. 220cm), Rear Gallery (X = Depth - 320cm .. Depth), and Right Gallery Wing (Y = 120cm .. HalfW)
		const float FrontBridgeX = 220.0f;
		const float RearGalleryX = FMath::Max(FrontBridgeX + 420.0f, Depth - 320.0f);
		const float StairWidth = 250.0f;
		const float StairY0 = -HalfW + WallThick + 10.0f;
		const float StairY1 = StairY0 + StairWidth;
		const float RightGalleryY0 = FMath::Max(StairY1 + 140.0f, 80.0f);

		// Front Mezzanine Bridge (connects Floor 2 doorway to Right Gallery and top/bottom of internal stairs)
		FloorSec.AddSolidBox(FVector(0.0f, -HalfW, UpperZ - SlabThick), FVector(FrontBridgeX, HalfW, UpperZ), 200.0f);
		// Rear Mezzanine Gallery (where the internal staircase lands at X = RearGalleryX)
		FloorSec.AddSolidBox(FVector(RearGalleryX, -HalfW, UpperZ - SlabThick), FVector(Depth, HalfW, UpperZ), 200.0f);
		// Right Mezzanine Gallery Wing (connecting Front Bridge and Rear Gallery along +Y)
		FloorSec.AddSolidBox(FVector(FrontBridgeX, RightGalleryY0, UpperZ - SlabThick), FVector(RearGalleryX, HalfW, UpperZ), 200.0f);

		// Safety Glass & Brushed-Metal Balustrades around the Mezzanine Atrium Well (112cm tall)
		const float RailH = 112.0f;
		// Front edge of Atrium Well (from StairY0 to RightGalleryY0 at X = FrontBridgeX)
		GlassSec.AddSolidBox(FVector(FrontBridgeX - 6.0f, StairY0, UpperZ), FVector(FrontBridgeX, RightGalleryY0, UpperZ + RailH - 8.0f), 120.0f);
		MetalSec.AddSolidBox(FVector(FrontBridgeX - 8.0f, StairY0, UpperZ + RailH - 8.0f), FVector(FrontBridgeX + 2.0f, RightGalleryY0, UpperZ + RailH), 80.0f);
		// Inner edge of Right Mezzanine Wing (at Y = RightGalleryY0 from X = FrontBridgeX to RearGalleryX)
		GlassSec.AddSolidBox(FVector(FrontBridgeX, RightGalleryY0, UpperZ), FVector(RearGalleryX, RightGalleryY0 + 6.0f, UpperZ + RailH - 8.0f), 120.0f);
		MetalSec.AddSolidBox(FVector(FrontBridgeX, RightGalleryY0 - 2.0f, UpperZ + RailH - 8.0f), FVector(RearGalleryX, RightGalleryY0 + 8.0f, UpperZ + RailH), 80.0f);
		// Rear edge of Atrium Well (only between StairY1 and RightGalleryY0, leaving StairY0..StairY1 open for the staircase landing!)
		if (RightGalleryY0 > StairY1 + 10.0f)
		{
			GlassSec.AddSolidBox(FVector(RearGalleryX, StairY1, UpperZ), FVector(RearGalleryX + 6.0f, RightGalleryY0, UpperZ + RailH - 8.0f), 120.0f);
			MetalSec.AddSolidBox(FVector(RearGalleryX - 2.0f, StairY1, UpperZ + RailH - 8.0f), FVector(RearGalleryX + 8.0f, RightGalleryY0, UpperZ + RailH), 80.0f);
		}

		// Walkable Internal Anchor Store Grand Staircase / Ramp connecting Ground (X = FrontBridgeX, Z = 0) -> Floor 2 (X = RearGalleryX, Z = UpperZ)
		const FVector SBotL(FrontBridgeX, StairY0, 0.0f);
		const FVector SBotR(FrontBridgeX, StairY1, 0.0f);
		const FVector STopL(RearGalleryX, StairY0, UpperZ);
		const FVector STopR(RearGalleryX, StairY1, UpperZ);
		const FVector RampNormal = FVector::CrossProduct(STopL - SBotL, SBotR - SBotL).GetSafeNormal();
		const FVector RampThickVec(0.0f, 0.0f, -24.0f);
		// Walkable sloped ramp deck (top, bottom, and side stringers) — 100% smooth for CharacterMovement & RecastNavMesh!
		FloorSec.AddQuad(SBotL, STopL, STopR, SBotR, RampNormal, 150.0f);
		WallCeilingSec.AddQuad(SBotL + RampThickVec, SBotR + RampThickVec, STopR + RampThickVec, STopL + RampThickVec, -RampNormal, 150.0f);
		MetalSec.AddQuad(SBotL + RampThickVec, STopL + RampThickVec, STopL, SBotL, FVector(0.0f, -1.0f, 0.0f), 100.0f);
		MetalSec.AddQuad(SBotR, STopR, STopR + RampThickVec, SBotR + RampThickVec, FVector(0.0f, 1.0f, 0.0f), 100.0f);
		// Sloped safety handrail along the open right side of the staircase (Y = StairY1)
		const FVector RailOffset(0.0f, 0.0f, 100.0f);
		MetalSec.AddQuad(SBotR, STopR, STopR + RailOffset, SBotR + RailOffset, FVector(0.0f, 1.0f, 0.0f), 100.0f);
		MetalSec.AddQuad(SBotR + RailOffset, STopR + RailOffset, STopR, SBotR, FVector(0.0f, -1.0f, 0.0f), 100.0f);
	}

	// 3. SOLID 3D Back Wall at X = Depth - WallThick .. Depth
	if (!bOpenRearPassThroughDoor)
	{
		WallCeilingSec.AddSolidBox(FVector(Depth - WallThick, -HalfW, 0.0f), FVector(Depth, HalfW, Height), 200.0f);
	}
	else
	{
		WallCeilingSec.AddSolidBox(FVector(Depth - WallThick, -HalfW, 0.0f), FVector(Depth, -HalfDoor, DoorH), 200.0f);
		WallCeilingSec.AddSolidBox(FVector(Depth - WallThick, HalfDoor, 0.0f), FVector(Depth, HalfW, DoorH), 200.0f);
		WallCeilingSec.AddSolidBox(FVector(Depth - WallThick, -HalfW, DoorH), FVector(Depth, HalfW, Height), 200.0f);
	}

	// 4. SOLID 3D Left (Y = -HalfW .. -HalfW + WallThick) and Right (Y = HalfW - WallThick .. HalfW) Side Walls
	auto BuildSolidSideWall = [&](float YMin, float YMax, bool bOpenSideDoor)
	{
		if (!bOpenSideDoor)
		{
			WallCeilingSec.AddSolidBox(FVector(0.0f, YMin, 0.0f), FVector(Depth, YMax, Height), 200.0f);
			return;
		}
		const float MidX = Depth * 0.5f;
		const float SideDoorHalf = 130.0f;
		WallCeilingSec.AddSolidBox(FVector(0.0f, YMin, 0.0f), FVector(MidX - SideDoorHalf, YMax, DoorH), 200.0f);
		WallCeilingSec.AddSolidBox(FVector(MidX + SideDoorHalf, YMin, 0.0f), FVector(Depth, YMax, DoorH), 200.0f);
		WallCeilingSec.AddSolidBox(FVector(0.0f, YMin, DoorH), FVector(Depth, YMax, Height), 200.0f);
	};
	BuildSolidSideWall(-HalfW, -HalfW + WallThick, bOpenNeighborPassThroughDoors || bOpenLeftNeighborDoor);
	BuildSolidSideWall(HalfW - WallThick, HalfW, bOpenNeighborPassThroughDoors || bOpenRightNeighborDoor);

	// 4B. Archetype-Specific Solid Architectural Sub-Structures (Showers/Sauna Stalls & Giant Parody Lego Brick Sculptures!)
	if (StoreArchetype == EMallStoreArchetype::BathhouseSaunaShowers)
	{
		// Build 3 Solid Tiled Shower Stalls along the Left Wall (Y = -HalfW + WallThick .. -HalfW + 220cm)
		// with frosted glass privacy dividers and overhead brushed-steel shower pipes & heads!
		for (int32 StallIdx = 0; StallIdx < 3; ++StallIdx)
		{
			const float StallX0 = 140.0f + StallIdx * 175.0f;
			const float StallX1 = StallX0 + 160.0f;
			// Solid Tiled Partition Wall between shower stalls
			WallCeilingSec.AddSolidBox(FVector(StallX1, -HalfW + WallThick, 0.0f), FVector(StallX1 + 14.0f, -HalfW + 210.0f, 235.0f), 100.0f);
			// Frosted Glass Front Screen
			GlassSec.AddSolidBox(FVector(StallX0 + 15.0f, -HalfW + 202.0f, 12.0f), FVector(StallX1 - 55.0f, -HalfW + 210.0f, 220.0f), 100.0f);
			// Raised Shower Curb
			FloorSec.AddSolidBox(FVector(StallX0, -HalfW + 200.0f, 0.0f), FVector(StallX1, -HalfW + 214.0f, 12.0f), 100.0f);
			// Chrome Overhead Shower Riser Pipe & Showerhead
			const float MidStallX = (StallX0 + StallX1) * 0.5f;
			MetalSec.AddSolidBox(FVector(MidStallX - 3.0f, -HalfW + WallThick, 110.0f), FVector(MidStallX + 3.0f, -HalfW + WallThick + 8.0f, 225.0f), 60.0f);
			MetalSec.AddSolidBox(FVector(MidStallX - 12.0f, -HalfW + WallThick, 218.0f), FVector(MidStallX + 12.0f, -HalfW + WallThick + 48.0f, 226.0f), 60.0f);
		}
		// Build a Two-Tiered Cedar/Stone Sauna Bench Chamber along the Right Wall (Y = HalfW - 240cm .. HalfW - WallThick)
		FloorSec.AddSolidBox(FVector(160.0f, HalfW - 230.0f, 0.0f), FVector(Depth - 220.0f, HalfW - WallThick, 48.0f), 120.0f);
		FloorSec.AddSolidBox(FVector(160.0f, HalfW - 130.0f, 48.0f), FVector(Depth - 220.0f, HalfW - WallThick, 92.0f), 120.0f);
		// Sauna Heater Stove Plinth with Glowing Stones
		MetalSec.AddSolidBox(FVector(210.0f, HalfW - 290.0f, 0.0f), FVector(290.0f, HalfW - 235.0f, 95.0f), 80.0f);
	}
	else if (StoreArchetype == EMallStoreArchetype::ParodyLegoBrickStore)
	{
		// Build Giant Interlocking Studded Plastic Brick Sculptures & Display Plinths!
		auto AddStuddedGiantBrick = [&](const FVector& MinCorner, const FVector& BrickSize, int32 StudsX, int32 StudsY)
		{
			const FVector MaxCorner = MinCorner + BrickSize;
			WallCeilingSec.AddSolidBox(MinCorner, MaxCorner, 80.0f);
			const float StepX = BrickSize.X / FMath::Max(1, StudsX);
			const float StepY = BrickSize.Y / FMath::Max(1, StudsY);
			for (int32 sx = 0; sx < StudsX; ++sx)
			{
				for (int32 sy = 0; sy < StudsY; ++sy)
				{
					const float CX = MinCorner.X + (sx + 0.5f) * StepX;
					const float CY = MinCorner.Y + (sy + 0.5f) * StepY;
					MetalSec.AddSolidBox(
						FVector(CX - StepX * 0.28f, CY - StepY * 0.28f, MaxCorner.Z),
						FVector(CX + StepX * 0.28f, CY + StepY * 0.28f, MaxCorner.Z + 14.0f),
						50.0f);
				}
			}
		};
		// Central Giant Stepped Brick Monument in the middle of the Parody Lego Store
		AddStuddedGiantBrick(FVector(Depth * 0.42f - 90.0f, -90.0f, 0.0f), FVector(180.0f, 180.0f, 65.0f), 4, 4);
		AddStuddedGiantBrick(FVector(Depth * 0.42f - 60.0f, -60.0f, 65.0f), FVector(120.0f, 120.0f, 65.0f), 2, 2);
		AddStuddedGiantBrick(FVector(Depth * 0.42f - 35.0f, -35.0f, 130.0f), FVector(70.0f, 70.0f, 65.0f), 1, 1);
		// Window Display Giant Brick Pedestals
		AddStuddedGiantBrick(FVector(55.0f, -HalfDoor - 140.0f, 0.0f), FVector(90.0f, 90.0f, 75.0f), 2, 2);
		AddStuddedGiantBrick(FVector(55.0f, HalfDoor + 50.0f, 0.0f), FVector(90.0f, 90.0f, 75.0f), 2, 2);
	}

	auto Commit = [&](int32 Idx, const FStoreShellSection& Sec, UMaterialInterface* Mat, bool bCollision)
	{
		if (Sec.Vertices.Num() > 0 && Sec.Triangles.Num() > 0)
		{
			StoreShellMesh->CreateMeshSection_LinearColor(
				Idx, Sec.Vertices, Sec.Triangles, Sec.Normals, Sec.UVs, Sec.Colors, Sec.Tangents, bCollision);
			if (Mat)
			{
				StoreShellMesh->SetMaterial(Idx, Mat);
			}
		}
	};
	Commit(0, FloorSec, FloorMaterial, true);
	Commit(1, WallCeilingSec, WallMaterial, true);
	Commit(2, GlassSec, GlassMaterial, true);
	Commit(3, MetalSec, MetalMaterial, true);

	// 5. Fascia Sign & Interior Lighting Mood by Archetype
	if (FasciaSignText)
	{
		FasciaSignText->SetText(FText::FromString(StoreSignTitle));
		FasciaSignText->SetRelativeLocation(FVector(-6.0f, 0.0f, DoorH + 55.0f));
		FasciaSignText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	}
	if (UpperFasciaSignText)
	{
		UpperFasciaSignText->SetHiddenInGame(!bIsMultiFloorAnchorStore);
		UpperFasciaSignText->SetVisibility(bIsMultiFloorAnchorStore);
		if (bIsMultiFloorAnchorStore)
		{
			UpperFasciaSignText->SetText(FText::FromString(StoreSignTitle + TEXT(" [FLOOR 2 GALLERY]")));
			UpperFasciaSignText->SetRelativeLocation(FVector(-6.0f, 0.0f, UpperZ + DoorH + 55.0f));
			UpperFasciaSignText->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
		}
	}

	if (StoreInteriorLight)
	{
		// In 1-story stores, mount the ceiling track bar FLUSH against the store roof ceiling (Height - 3.5 cm).
		// In 2-story anchor stores, mount the ground-floor track bar FLUSH against the Rear Mezzanine gallery ceiling slab (UpperZ - SlabThick - 3.5 cm at X = Depth - 165 cm)!
		const float LowerCeilingUndersideZ = bIsMultiFloorAnchorStore ? (UpperZ - SlabThick) : Height;
		const float LowerMountX = bIsMultiFloorAnchorStore ? FMath::Max(260.0f, Depth - 165.0f) : (Depth * 0.46f);
		const FVector LowerFixturePos(LowerMountX, 0.0f, LowerCeilingUndersideZ - 3.5f);

		if (StoreLightFixtureMesh)
		{
			StoreLightFixtureMesh->SetRelativeLocation(LowerFixturePos);
			StoreLightFixtureMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
			StoreLightFixtureMesh->SetRelativeScale3D(FVector(1.30f, 1.30f, 1.18f));
		}
		if (StoreLightAimBounds)
		{
			StoreLightAimBounds->SetRelativeLocation(LowerFixturePos - FVector(0.0f, 0.0f, 4.0f));
		}

		FLinearColor StoreTint(0.98f, 0.90f, 0.82f);
		float TargetRadius = 1080.0f;
		switch (StoreArchetype)
		{
		case EMallStoreArchetype::GhostHuntingEssentials:
			StoreTint = FLinearColor(0.90f, 0.97f, 1.0f);
			BaseStoreLightIntensity = 720.0f;
			TargetRadius = 1180.0f;
			break;
		case EMallStoreArchetype::FoodCourtStall:
			StoreTint = FLinearColor(1.0f, 0.80f, 0.52f);
			BaseStoreLightIntensity = 600.0f;
			TargetRadius = 1080.0f;
			break;
		case EMallStoreArchetype::DeliveryAirlockBay:
			StoreTint = FLinearColor(0.68f, 0.90f, 1.0f);
			BaseStoreLightIntensity = 560.0f;
			TargetRadius = 1080.0f;
			break;
		case EMallStoreArchetype::ParodyLegoBrickStore:
			StoreTint = FLinearColor(1.0f, 0.92f, 0.62f);
			BaseStoreLightIntensity = 680.0f;
			TargetRadius = 1150.0f;
			break;
		case EMallStoreArchetype::BathhouseSaunaShowers:
			StoreTint = FLinearColor(1.0f, 0.72f, 0.45f);
			BaseStoreLightIntensity = 480.0f;
			TargetRadius = 1020.0f;
			break;
		case EMallStoreArchetype::SurvivorCivicFacility:
			StoreTint = FLinearColor(0.82f, 0.94f, 1.0f);
			BaseStoreLightIntensity = 600.0f;
			TargetRadius = 1080.0f;
			break;
		case EMallStoreArchetype::LingerieParody:
			StoreTint = FLinearColor(1.0f, 0.68f, 0.84f);
			BaseStoreLightIntensity = 560.0f;
			TargetRadius = 1060.0f;
			break;
		case EMallStoreArchetype::HosierySpecialty:
			StoreTint = FLinearColor(0.88f, 0.72f, 1.0f);
			BaseStoreLightIntensity = 560.0f;
			TargetRadius = 1060.0f;
			break;
		default:
			StoreTint = FLinearColor(0.98f, 0.90f, 0.82f);
			BaseStoreLightIntensity = 580.0f;
			TargetRadius = 1080.0f;
			break;
		}

		if (StoreInteriorSpotLight)
		{
			StoreInteriorSpotLight->SetIntensityUnits(ELightUnits::Lumens);
			StoreInteriorSpotLight->SetLightColor(StoreTint);
			StoreInteriorSpotLight->SetAttenuationRadius(TargetRadius);
			StoreInteriorSpotLight->SetInnerConeAngle(36.0f);
			StoreInteriorSpotLight->SetOuterConeAngle(68.0f);
			StoreInteriorSpotLight->SetSourceRadius(14.0f);
			StoreInteriorSpotLight->SetSoftSourceRadius(20.0f);
			StoreInteriorSpotLight->SetSourceLength(24.0f);
			StoreInteriorSpotLight->SetCastShadows(bCastInteriorLightShadows);
			StoreInteriorSpotLight->MaxDrawDistance = 3800.0f;
			StoreInteriorSpotLight->MaxDistanceFadeRange = 800.0f;
			StoreInteriorSpotLight->SetRelativeLocation(LowerFixturePos - FVector(0.0f, 0.0f, 8.0f));
			// Aim downward at the central merchandise display table / checkout zone
			StoreInteriorSpotLight->SetRelativeRotation(FRotator(bIsMultiFloorAnchorStore ? -78.0f : -88.0f, 180.0f, 0.0f));
		}

		StoreInteriorLight->SetIntensityUnits(ELightUnits::Lumens);
		StoreInteriorLight->SetLightColor(StoreTint);
		StoreInteriorLight->SetAttenuationRadius(520.0f);
		StoreInteriorLight->SetSourceRadius(14.0f);
		StoreInteriorLight->SetSoftSourceRadius(18.0f);
		StoreInteriorLight->SetSourceLength(16.0f);
		StoreInteriorLight->SetCastShadows(false);
		StoreInteriorLight->MaxDrawDistance = 3200.0f;
		StoreInteriorLight->MaxDistanceFadeRange = 600.0f;
		StoreInteriorLight->SetRelativeLocation(FVector(Depth * 0.48f, 0.0f, FMath::Min(280.0f, LowerCeilingUndersideZ * 0.55f)));
	}

	if (StoreUpperFloorLight)
	{
		StoreUpperFloorLight->SetVisibility(bIsMultiFloorAnchorStore);
		StoreUpperFloorLight->SetHiddenInGame(!bIsMultiFloorAnchorStore);
		if (StoreUpperFloorSpotLight)
		{
			StoreUpperFloorSpotLight->SetVisibility(bIsMultiFloorAnchorStore);
			StoreUpperFloorSpotLight->SetHiddenInGame(!bIsMultiFloorAnchorStore);
		}
		if (StoreUpperLightFixtureMesh)
		{
			StoreUpperLightFixtureMesh->SetVisibility(bIsMultiFloorAnchorStore);
			StoreUpperLightFixtureMesh->SetHiddenInGame(!bIsMultiFloorAnchorStore);
		}
		if (bIsMultiFloorAnchorStore && StoreInteriorLight)
		{
			const FVector UpperFixturePos(Depth * 0.46f, 0.0f, Height - 3.5f);
			if (StoreUpperLightFixtureMesh)
			{
				StoreUpperLightFixtureMesh->SetRelativeLocation(UpperFixturePos);
				StoreUpperLightFixtureMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
				StoreUpperLightFixtureMesh->SetRelativeScale3D(FVector(1.35f, 1.35f, 1.18f));
			}
			if (StoreUpperFloorSpotLight)
			{
				StoreUpperFloorSpotLight->SetIntensityUnits(ELightUnits::Lumens);
				StoreUpperFloorSpotLight->SetLightColor(StoreInteriorLight->GetLightColor());
				StoreUpperFloorSpotLight->SetAttenuationRadius(1250.0f);
				StoreUpperFloorSpotLight->SetInnerConeAngle(36.0f);
				StoreUpperFloorSpotLight->SetOuterConeAngle(68.0f);
				StoreUpperFloorSpotLight->SetSourceRadius(14.0f);
				StoreUpperFloorSpotLight->SetSoftSourceRadius(20.0f);
				StoreUpperFloorSpotLight->SetSourceLength(24.0f);
				StoreUpperFloorSpotLight->SetCastShadows(bCastInteriorLightShadows);
				StoreUpperFloorSpotLight->MaxDrawDistance = 3800.0f;
				StoreUpperFloorSpotLight->MaxDistanceFadeRange = 800.0f;
				StoreUpperFloorSpotLight->SetRelativeLocation(UpperFixturePos - FVector(0.0f, 0.0f, 8.0f));
				StoreUpperFloorSpotLight->SetRelativeRotation(FRotator(-88.0f, 0.0f, 0.0f));
			}
			StoreUpperFloorLight->SetIntensityUnits(ELightUnits::Lumens);
			StoreUpperFloorLight->SetSourceRadius(14.0f);
			StoreUpperFloorLight->SetSoftSourceRadius(18.0f);
			StoreUpperFloorLight->SetSourceLength(16.0f);
			StoreUpperFloorLight->SetLightColor(StoreInteriorLight->GetLightColor());
			StoreUpperFloorLight->SetAttenuationRadius(480.0f);
			StoreUpperFloorLight->SetCastShadows(false);
			StoreUpperFloorLight->MaxDrawDistance = 3200.0f;
			StoreUpperFloorLight->MaxDistanceFadeRange = 600.0f;
			StoreUpperFloorLight->SetRelativeLocation(FVector(Depth * 0.48f, HalfW * 0.25f, UpperZ + 260.0f));
		}
		else
		{
			StoreUpperFloorLight->SetIntensity(0.0f);
			if (StoreUpperFloorSpotLight)
			{
				StoreUpperFloorSpotLight->SetIntensity(0.0f);
			}
		}
	}
	ApplyStoreLightState();

	// 6. Checkout Counter, POS Terminal & Shopkeeper Space
	UStaticMesh* GlazedCounter = TryLoadRetailMesh(
		TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25339_GlazedDisplayCounter.SM_Retail_25339_GlazedDisplayCounter"),
		TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25310_CashDesk120.SM_Retail_25310_CashDesk120"));
	UStaticMesh* StandardCounter = TryLoadRetailMesh(
		TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25310_CashDesk120.SM_Retail_25310_CashDesk120"));
	UStaticMesh* POSMesh = TryLoadRetailMesh(
		TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25324_POSTerminal.SM_Retail_25324_POSTerminal"));
	UStaticMesh* BackPanelMesh = TryLoadRetailMesh(
		TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25179_IlluminatedBackPanel.SM_Retail_25179_IlluminatedBackPanel"),
		TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25317_CashDeskBackFixture.SM_Retail_25317_CashDeskBackFixture"));

	if (CheckoutCounterComponent)
	{
		const bool bUseGlazed = (StoreArchetype == EMallStoreArchetype::GhostHuntingEssentials || StoreArchetype == EMallStoreArchetype::FoodCourtStall);
		CheckoutCounterComponent->SetStaticMesh(bUseGlazed ? GlazedCounter : StandardCounter);
		const float CounterX = (StoreArchetype == EMallStoreArchetype::FoodCourtStall) ? 140.0f : (Depth - 240.0f);
		CheckoutCounterComponent->SetRelativeLocation(FVector(CounterX, 0.0f, 0.0f));
		CheckoutCounterComponent->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}
	if (POSTerminalComponent && POSMesh)
	{
		POSTerminalComponent->SetStaticMesh(POSMesh);
		POSTerminalComponent->SetRelativeLocation(FVector(25.0f, 0.0f, 99.0f));
	}
	if (BackPanelComponent && BackPanelMesh)
	{
		BackPanelComponent->SetStaticMesh(BackPanelMesh);
		BackPanelComponent->SetRelativeLocation(FVector(Depth - 42.0f, 0.0f, 0.0f));
		BackPanelComponent->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	if (RollerShutterComponent)
	{
		RollerShutterComponent->SetRelativeLocation(FVector(4.0f, 0.0f, 0.0f));
		RollerShutterComponent->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		SetNightShutterClosed(bNightShutterClosed);
	}

	// 7. Populate Archetype-Specific Interior Fixtures & Display Forms
	if (StoreArchetype == EMallStoreArchetype::LingerieParody)
	{
		UStaticMesh* RoundTable = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25225_TieredRoundTable.SM_Retail_25225_TieredRoundTable"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25210_RoundHangingRail.SM_Retail_25210_RoundHangingRail"));
		UStaticMesh* FittingRoom = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25288_FittingRoomCurtain.SM_Retail_25288_FittingRoomCurtain"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25181_PerimeterWallBayHangingRails.SM_Retail_25181_PerimeterWallBayHangingRails"));
		UStaticMesh* BustOrTorso = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25271_TorsoFormStand.SM_Retail_25271_TorsoFormStand"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25208_StraightHangingRail.SM_Retail_25208_StraightHangingRail"));
		UStaticMesh* MirrorWall = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25198_WallBayMirror.SM_Retail_25198_WallBayMirror"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25184_PerimeterWallBayShelfAndRail.SM_Retail_25184_PerimeterWallBayShelfAndRail"));

		PrimaryFixtureInstances->SetStaticMesh(RoundTable);
		SecondaryFixtureInstances->SetStaticMesh(FittingRoom);
		MannequinDisplayInstances->SetStaticMesh(BustOrTorso);
		WallBayInstances->SetStaticMesh(MirrorWall);

		if (RoundTable)
		{
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Depth * 0.35f, -HalfW * 0.38f, 0.0f)));
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Depth * 0.35f, HalfW * 0.38f, 0.0f)));
		}
		if (FittingRoom)
		{
			SecondaryFixtureInstances->AddInstance(FTransform(FRotator(0, 0, 0), FVector(Depth - 140.0f, -HalfW + 90.0f, 0.0f)));
			SecondaryFixtureInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(Depth - 140.0f, HalfW - 90.0f, 0.0f)));
		}
		if (BustOrTorso)
		{
			MannequinDisplayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(120.0f, -HalfDoor - 70.0f, 0.0f)));
			MannequinDisplayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(120.0f, HalfDoor + 70.0f, 0.0f)));
		}
		if (MirrorWall)
		{
			WallBayInstances->AddInstance(FTransform(FRotator(0, 0, 0), FVector(Depth * 0.25f, -HalfW + 35.0f, 0.0f)));
			WallBayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(Depth * 0.25f, HalfW - 35.0f, 0.0f)));
		}
	}
	else if (StoreArchetype == EMallStoreArchetype::HosierySpecialty)
	{
		UStaticMesh* SpinnerRack = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25236_AccessorySpinnerRack.SM_Retail_25236_AccessorySpinnerRack"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25210_RoundHangingRail.SM_Retail_25210_RoundHangingRail"));
		UStaticMesh* LegForm = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25273_LegForm.SM_Retail_25273_LegForm"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25208_StraightHangingRail.SM_Retail_25208_StraightHangingRail"));
		UStaticMesh* CubbyWall = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25195_WallBayCubbies.SM_Retail_25195_WallBayCubbies"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25193_PerimeterWallBayShoeShelves.SM_Retail_25193_PerimeterWallBayShoeShelves"));

		PrimaryFixtureInstances->SetStaticMesh(SpinnerRack);
		MannequinDisplayInstances->SetStaticMesh(LegForm);
		WallBayInstances->SetStaticMesh(CubbyWall);

		if (SpinnerRack)
		{
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Depth * 0.36f, -HalfW * 0.35f, 0.0f)));
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Depth * 0.36f, HalfW * 0.35f, 0.0f)));
		}
		if (LegForm)
		{
			MannequinDisplayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(110.0f, -HalfDoor - 65.0f, 0.0f)));
			MannequinDisplayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(110.0f, HalfDoor + 65.0f, 0.0f)));
		}
		if (CubbyWall)
		{
			WallBayInstances->AddInstance(FTransform(FRotator(0, 0, 0), FVector(Depth * 0.25f, -HalfW + 35.0f, 0.0f)));
			WallBayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(Depth * 0.25f, HalfW - 35.0f, 0.0f)));
		}
	}
	else if (StoreArchetype == EMallStoreArchetype::GhostHuntingEssentials)
	{
		UStaticMesh* SlatShelf = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_RetailShelves01.SM_RetailShelves01"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25242_MerchandiseGondola.SM_Retail_25242_MerchandiseGondola"));
		UStaticMesh* WallBay = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25184_PerimeterWallBayShelfAndRail.SM_Retail_25184_PerimeterWallBayShelfAndRail"));

		PrimaryFixtureInstances->SetStaticMesh(SlatShelf);
		WallBayInstances->SetStaticMesh(WallBay);

		if (SlatShelf)
		{
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, 90, 0), FVector(Depth * 0.32f, -HalfW * 0.42f, 0.0f)));
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, 90, 0), FVector(Depth * 0.32f, HalfW * 0.42f, 0.0f)));
		}
		if (WallBay)
		{
			WallBayInstances->AddInstance(FTransform(FRotator(0, 0, 0), FVector(Depth * 0.22f, -HalfW + 35.0f, 0.0f)));
			WallBayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(Depth * 0.22f, HalfW - 35.0f, 0.0f)));
		}
	}
	else if (StoreArchetype == EMallStoreArchetype::FoodCourtStall)
	{
		UStaticMesh* KioskCart = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25374_ConcourseKioskCart.SM_Retail_25374_ConcourseKioskCart"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25310_CashDesk120.SM_Retail_25310_CashDesk120"));
		UStaticMesh* PrepShelf = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_RetailShelves01.SM_RetailShelves01"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25195_WallBayCubbies.SM_Retail_25195_WallBayCubbies"));

		PrimaryFixtureInstances->SetStaticMesh(KioskCart);
		WallBayInstances->SetStaticMesh(PrepShelf);

		if (KioskCart)
		{
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, -90, 0), FVector(150.0f, -HalfW * 0.42f, 0.0f)));
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, -90, 0), FVector(150.0f, HalfW * 0.42f, 0.0f)));
		}
		if (PrepShelf)
		{
			WallBayInstances->AddInstance(FTransform(FRotator(0, 0, 0), FVector(Depth * 0.55f, -HalfW + 40.0f, 0.0f)));
			WallBayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(Depth * 0.55f, HalfW - 40.0f, 0.0f)));
		}
	}
	else if (StoreArchetype == EMallStoreArchetype::DeliveryAirlockBay)
	{
		UStaticMesh* RollCage = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25400_StockRollCage.SM_Retail_25400_StockRollCage"),
			TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_RetailShelves01.SM_RetailShelves01"));
		UStaticMesh* LiftLobby = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25359_LiftLobbyModule.SM_Retail_25359_LiftLobbyModule"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25352_StockRoomDoor.SM_Retail_25352_StockRoomDoor"));

		PrimaryFixtureInstances->SetStaticMesh(RollCage);
		SecondaryFixtureInstances->SetStaticMesh(LiftLobby);

		if (RollCage)
		{
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, 15, 0), FVector(Depth * 0.32f, -150.0f, 0.0f)));
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, -10, 0), FVector(Depth * 0.35f, 150.0f, 0.0f)));
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, 85, 0), FVector(Depth * 0.58f, -180.0f, 0.0f)));
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, -95, 0), FVector(Depth * 0.58f, 180.0f, 0.0f)));
		}
		if (LiftLobby)
		{
			SecondaryFixtureInstances->AddInstance(FTransform(FRotator(0, -90, 0), FVector(Depth - 55.0f, -160.0f, 0.0f)));
			SecondaryFixtureInstances->AddInstance(FTransform(FRotator(0, -90, 0), FVector(Depth - 55.0f, 160.0f, 0.0f)));
		}
	}
	else if (StoreArchetype == EMallStoreArchetype::ParodyLegoBrickStore)
	{
		// Parody LEGO Store: Round Brick-Build Tables + Pick-A-Brick Wall Cubbies + Minifigure/Bust Displays!
		UStaticMesh* BuildTable = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25225_TieredRoundTable.SM_Retail_25225_TieredRoundTable"));
		UStaticMesh* BrickCubbies = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25195_WallBayCubbies.SM_Retail_25195_WallBayCubbies"));
		UStaticMesh* BustDisplay = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25272_BustForm.SM_Retail_25272_BustForm"));

		PrimaryFixtureInstances->SetStaticMesh(BuildTable);
		WallBayInstances->SetStaticMesh(BrickCubbies);
		MannequinDisplayInstances->SetStaticMesh(BustDisplay);

		if (BuildTable)
		{
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Depth * 0.28f, -HalfW * 0.45f, 0.0f)));
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Depth * 0.28f, HalfW * 0.45f, 0.0f)));
		}
		if (BrickCubbies)
		{
			WallBayInstances->AddInstance(FTransform(FRotator(0, 0, 0), FVector(Depth * 0.25f, -HalfW + 35.0f, 0.0f)));
			WallBayInstances->AddInstance(FTransform(FRotator(0, 0, 0), FVector(Depth * 0.58f, -HalfW + 35.0f, 0.0f)));
			WallBayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(Depth * 0.25f, HalfW - 35.0f, 0.0f)));
			WallBayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(Depth * 0.58f, HalfW - 35.0f, 0.0f)));
		}
		if (BustDisplay)
		{
			MannequinDisplayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(100.0f, -HalfDoor - 95.0f, 75.0f)));
			MannequinDisplayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(100.0f, HalfDoor + 95.0f, 75.0f)));
		}
	}
	else if (StoreArchetype == EMallStoreArchetype::BathhouseSaunaShowers)
	{
		// Mall City Tangquan Sauna & Shower Complex: Open Rinse Shower Island + Sauna Stove & Buckets + Towel/Locker Cubbies!
		UStaticMesh* LockerCubbies = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25195_WallBayCubbies.SM_Retail_25195_WallBayCubbies"));
		UStaticMesh* SaunaStoveSet = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Bath_SaunaStoveAndBuckets.SM_Bath_SaunaStoveAndBuckets"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25288_FittingRoomCurtain.SM_Retail_25288_FittingRoomCurtain"));
		UStaticMesh* OpenShowerRow = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Bath_OpenRinseShowerRow.SM_Bath_OpenRinseShowerRow"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25366_ConcourseBench.SM_Retail_25366_ConcourseBench"));

		WallBayInstances->SetStaticMesh(LockerCubbies);
		SecondaryFixtureInstances->SetStaticMesh(SaunaStoveSet);
		PrimaryFixtureInstances->SetStaticMesh(OpenShowerRow);

		if (LockerCubbies)
		{
			WallBayInstances->AddInstance(FTransform(FRotator(0, -90, 0), FVector(Depth - 45.0f, -HalfW * 0.45f, 0.0f)));
			WallBayInstances->AddInstance(FTransform(FRotator(0, -90, 0), FVector(Depth - 45.0f, HalfW * 0.45f, 0.0f)));
		}
		if (SaunaStoveSet)
		{
			SecondaryFixtureInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(Depth * 0.62f, HalfW * 0.32f, 0.0f)));
		}
		if (OpenShowerRow)
		{
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, 90, 0), FVector(Depth * 0.42f, -HalfW * 0.18f, 0.0f)));
		}
	}
	else if (StoreArchetype == EMallStoreArchetype::SurvivorCivicFacility)
	{
		// Mall City Civic Facility (Pharmacy/Clinic, DIY Hardware & Plumbing Workshop, or Sleep Pod/Mattress Lounge)
		UStaticMesh* HeavyShelf = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_RetailShelves01.SM_RetailShelves01"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25242_MerchandiseGondola.SM_Retail_25242_MerchandiseGondola"));
		UStaticMesh* SleepBench = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25381_ChargingBench.SM_Retail_25381_ChargingBench"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25366_ConcourseBench.SM_Retail_25366_ConcourseBench"));
		UStaticMesh* WallStorage = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25184_PerimeterWallBayShelfAndRail.SM_Retail_25184_PerimeterWallBayShelfAndRail"));

		PrimaryFixtureInstances->SetStaticMesh(HeavyShelf);
		SecondaryFixtureInstances->SetStaticMesh(SleepBench);
		WallBayInstances->SetStaticMesh(WallStorage);

		if (HeavyShelf)
		{
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, 90, 0), FVector(Depth * 0.34f, -HalfW * 0.38f, 0.0f)));
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, 90, 0), FVector(Depth * 0.34f, HalfW * 0.38f, 0.0f)));
		}
		if (SleepBench)
		{
			SecondaryFixtureInstances->AddInstance(FTransform(FRotator(0, 0, 0), FVector(Depth * 0.58f, -HalfW * 0.35f, 0.0f)));
			SecondaryFixtureInstances->AddInstance(FTransform(FRotator(0, 0, 0), FVector(Depth * 0.58f, HalfW * 0.35f, 0.0f)));
		}
		if (WallStorage)
		{
			WallBayInstances->AddInstance(FTransform(FRotator(0, 0, 0), FVector(Depth * 0.25f, -HalfW + 35.0f, 0.0f)));
			WallBayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(Depth * 0.25f, HalfW - 35.0f, 0.0f)));
		}
	}
	else // UselessClothing
	{
		UStaticMesh* Gondola = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25242_MerchandiseGondola.SM_Retail_25242_MerchandiseGondola"));
		UStaticMesh* RoundRail = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25210_RoundHangingRail.SM_Retail_25210_RoundHangingRail"));
		UStaticMesh* WallRail = TryLoadRetailMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25181_PerimeterWallBayHangingRails.SM_Retail_25181_PerimeterWallBayHangingRails"));

		PrimaryFixtureInstances->SetStaticMesh(Gondola);
		SecondaryFixtureInstances->SetStaticMesh(RoundRail);
		WallBayInstances->SetStaticMesh(WallRail);

		if (Gondola)
		{
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, 90, 0), FVector(Depth * 0.36f, -HalfW * 0.38f, 0.0f)));
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, 90, 0), FVector(Depth * 0.36f, HalfW * 0.38f, 0.0f)));
		}
		if (RoundRail)
		{
			SecondaryFixtureInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Depth * 0.62f, -HalfW * 0.35f, 0.0f)));
			SecondaryFixtureInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Depth * 0.62f, HalfW * 0.35f, 0.0f)));
		}
		if (WallRail)
		{
			WallBayInstances->AddInstance(FTransform(FRotator(0, 0, 0), FVector(Depth * 0.25f, -HalfW + 35.0f, 0.0f)));
			WallBayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(Depth * 0.25f, HalfW - 35.0f, 0.0f)));
		}
	}

	// 8. If this is a 2-Story Multi-Floor Anchor Store, clear the left-side ground-floor internal staircase ramp lane
	// (Y = -HalfW + 34 .. -HalfW + 284, X = 220 .. Depth - 320) and populate the Floor 2 Mezzanine Gallery (Z = UpperZ)!
	if (bIsMultiFloorAnchorStore)
	{
		const float RampClearMinY = -HalfW + 375.0f;
		const float RampMaxX = FMath::Max(640.0f, Depth - 290.0f);
		auto ShiftFloorFixturesClearOfStairRamp = [&](UHierarchicalInstancedStaticMeshComponent* Comp, bool bIsWallMounted)
		{
			if (!Comp)
			{
				return;
			}
			const int32 Count = Comp->GetInstanceCount();
			for (int32 Idx = Count - 1; Idx >= 0; --Idx)
			{
				FTransform Xf;
				if (!Comp->GetInstanceTransform(Idx, Xf, false))
				{
					continue;
				}
				FVector Loc = Xf.GetLocation();
				if (Loc.Z < 50.0f && Loc.Y < RampClearMinY && Loc.X >= 170.0f && Loc.X <= RampMaxX)
				{
					if (bIsWallMounted)
					{
						// Remove ground-floor left-wall bays that would sit inside the 2-story internal staircase ramp
						Comp->RemoveInstance(Idx);
					}
					else
					{
						Loc.Y = RampClearMinY;
						Xf.SetLocation(Loc);
						Comp->UpdateInstanceTransform(Idx, Xf, false, false, true);
					}
				}
			}
		};
		ShiftFloorFixturesClearOfStairRamp(PrimaryFixtureInstances, false);
		ShiftFloorFixturesClearOfStairRamp(SecondaryFixtureInstances, false);
		ShiftFloorFixturesClearOfStairRamp(WallBayInstances, true);

		if (PrimaryFixtureInstances && PrimaryFixtureInstances->GetStaticMesh())
		{
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, 90, 0), FVector(Depth - 170.0f, -HalfW * 0.32f, UpperZ)));
			PrimaryFixtureInstances->AddInstance(FTransform(FRotator(0, 90, 0), FVector(Depth - 170.0f, HalfW * 0.32f, UpperZ)));
		}
		if (SecondaryFixtureInstances && SecondaryFixtureInstances->GetStaticMesh())
		{
			SecondaryFixtureInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Depth * 0.50f, HalfW * 0.48f, UpperZ)));
		}
		if (WallBayInstances && WallBayInstances->GetStaticMesh())
		{
			WallBayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(Depth * 0.35f, HalfW - 35.0f, UpperZ)));
			WallBayInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(Depth * 0.68f, HalfW - 35.0f, UpperZ)));
		}
	}

	// 9. Populate Every Store Shelf, Gondola, Table, and Wall Bay with Textured Merchandise Display Planes & 3D CC0 Shelf Props!
	{
		const TCHAR* DefaultShelfMatPath = TEXT("/Game/Environment/Mall/Materials/Shelves/MI_ShelfDisplay_Fashion.MI_ShelfDisplay_Fashion");
		UStaticMesh* PrimaryPropMesh = nullptr;
		UStaticMesh* SecondaryPropMesh = nullptr;
		FVector PrimaryPropScale(0.85f, 0.85f, 0.85f);
		FVector SecondaryPropScale(0.90f, 0.90f, 0.90f);

		switch (StoreArchetype)
		{
		case EMallStoreArchetype::GhostHuntingEssentials:
			DefaultShelfMatPath = TEXT("/Game/Environment/Mall/Materials/Shelves/MI_ShelfDisplay_Essentials.MI_ShelfDisplay_Essentials");
			PrimaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_CardboardBox01.SM_PH_CardboardBox01"),
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_BookHardcover02.SM_BookHardcover02"));
			SecondaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_SprayBottles.SM_PH_SprayBottles"),
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_MedicalBox.SM_PH_MedicalBox"));
			PrimaryPropScale = FVector(0.62f, 0.62f, 0.62f);
			SecondaryPropScale = FVector(1.05f, 1.05f, 1.05f);
			break;

		case EMallStoreArchetype::FoodCourtStall:
			DefaultShelfMatPath = TEXT("/Game/Environment/Mall/Materials/Shelves/MI_ShelfDisplay_FoodCourt.MI_ShelfDisplay_FoodCourt");
			PrimaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_FoodCans01.SM_PH_FoodCans01"),
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_WickerBasket01.SM_PH_WickerBasket01"));
			SecondaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_PlasticCrate01.SM_PH_PlasticCrate01"),
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_CardboardBox01.SM_PH_CardboardBox01"));
			PrimaryPropScale = FVector(1.45f, 1.45f, 1.45f);
			SecondaryPropScale = FVector(0.72f, 0.72f, 0.72f);
			break;

		case EMallStoreArchetype::DeliveryAirlockBay:
			DefaultShelfMatPath = TEXT("/Game/Environment/Mall/Materials/Shelves/MI_ShelfDisplay_Essentials.MI_ShelfDisplay_Essentials");
			PrimaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_CardboardBox01.SM_PH_CardboardBox01"));
			SecondaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_PlasticCrate01.SM_PH_PlasticCrate01"));
			PrimaryPropScale = FVector(0.82f, 0.82f, 0.82f);
			SecondaryPropScale = FVector(0.88f, 0.88f, 0.88f);
			break;

		case EMallStoreArchetype::ParodyLegoBrickStore:
			DefaultShelfMatPath = TEXT("/Game/Environment/Mall/Materials/Shelves/MI_ShelfDisplay_LegoBricks.MI_ShelfDisplay_LegoBricks");
			PrimaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_PlasticCrate01.SM_PH_PlasticCrate01"),
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_CardboardBox01.SM_PH_CardboardBox01"));
			SecondaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_EncyclopediaSet.SM_PH_EncyclopediaSet"),
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_BookHardcover02.SM_BookHardcover02"));
			PrimaryPropScale = FVector(0.75f, 0.75f, 0.75f);
			SecondaryPropScale = FVector(0.85f, 0.85f, 0.85f);
			break;

		case EMallStoreArchetype::BathhouseSaunaShowers:
			DefaultShelfMatPath = TEXT("/Game/Environment/Mall/Materials/Shelves/MI_ShelfDisplay_BathSpa.MI_ShelfDisplay_BathSpa");
			PrimaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_WickerBasket01.SM_PH_WickerBasket01"));
			SecondaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_CeramicVase01.SM_PH_CeramicVase01"),
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_SprayBottles.SM_PH_SprayBottles"));
			PrimaryPropScale = FVector(0.85f, 0.85f, 0.85f);
			SecondaryPropScale = FVector(0.82f, 0.82f, 0.82f);
			break;

		case EMallStoreArchetype::SurvivorCivicFacility:
			DefaultShelfMatPath = TEXT("/Game/Environment/Mall/Materials/Shelves/MI_ShelfDisplay_CivicClinic.MI_ShelfDisplay_CivicClinic");
			PrimaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_MedicalBox.SM_PH_MedicalBox"),
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_CardboardBox01.SM_PH_CardboardBox01"));
			SecondaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_EncyclopediaSet.SM_PH_EncyclopediaSet"),
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_SprayBottles.SM_PH_SprayBottles"));
			PrimaryPropScale = FVector(0.78f, 0.78f, 0.78f);
			SecondaryPropScale = FVector(0.82f, 0.82f, 0.82f);
			break;

		case EMallStoreArchetype::LingerieParody:
		case EMallStoreArchetype::HosierySpecialty:
			DefaultShelfMatPath = TEXT("/Game/Environment/Mall/Materials/Shelves/MI_ShelfDisplay_Fashion.MI_ShelfDisplay_Fashion");
			PrimaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_WickerBasket01.SM_PH_WickerBasket01"),
				TEXT("/Game/Fab/MetaHuman/Flipflops/meshes/f_med_nrw_shs_flipflops.f_med_nrw_shs_flipflops"));
			SecondaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Fab/MetaHuman/Flipflops/meshes/f_med_nrw_shs_flipflops.f_med_nrw_shs_flipflops"),
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_CeramicVase01.SM_PH_CeramicVase01"));
			PrimaryPropScale = FVector(0.82f, 0.82f, 0.82f);
			SecondaryPropScale = FVector(1.00f, 1.00f, 1.00f);
			break;

		default: // UselessClothing
			DefaultShelfMatPath = TEXT("/Game/Environment/Mall/Materials/Shelves/MI_ShelfDisplay_Fashion.MI_ShelfDisplay_Fashion");
			PrimaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_WickerBasket01.SM_PH_WickerBasket01"),
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_CardboardBox01.SM_PH_CardboardBox01"));
			SecondaryPropMesh = TryLoadRetailMesh(
				TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_PH_CardboardBox01.SM_PH_CardboardBox01"),
				TEXT("/Game/Fab/MetaHuman/Flipflops/meshes/f_med_nrw_shs_flipflops.f_med_nrw_shs_flipflops"));
			PrimaryPropScale = FVector(0.84f, 0.84f, 0.84f);
			SecondaryPropScale = FVector(0.58f, 0.58f, 0.58f);
			break;
		}

		if (ShelfStockPrimaryInstances)
		{
			ShelfStockPrimaryInstances->SetStaticMesh(PrimaryPropMesh);
		}
		if (ShelfStockSecondaryInstances)
		{
			ShelfStockSecondaryInstances->SetStaticMesh(SecondaryPropMesh);
		}

		auto PopulateFixtureComponentShelves = [&](UHierarchicalInstancedStaticMeshComponent* FixtureComp, bool bIsWallBay)
		{
			if (!FixtureComp || !FixtureComp->GetStaticMesh())
			{
				return;
			}
			const FString MeshName = FixtureComp->GetStaticMesh()->GetName();
			const int32 NumInst = FixtureComp->GetInstanceCount();
			for (int32 i = 0; i < NumInst; ++i)
			{
				FTransform InstXf;
				if (!FixtureComp->GetInstanceTransform(i, InstXf, false))
				{
					continue;
				}

				if (MeshName.Contains(TEXT("TieredRoundTable")))
				{
					// Tiered round table: lower ring Z=48..66cm, upper ring Z=82..98cm
					ShelfMerchandiseSec.AddTransformedMerchandiseTier(InstXf, FVector(-34.0f, -22.0f, 50.0f), FVector(34.0f, 22.0f, 68.0f));
					ShelfMerchandiseSec.AddTransformedMerchandiseTier(InstXf, FVector(-22.0f, -15.0f, 82.0f), FVector(22.0f, 15.0f, 98.0f));
					if (ShelfStockPrimaryInstances && PrimaryPropMesh)
					{
						ShelfStockPrimaryInstances->AddInstance(FTransform(FRotator(0, 25, 0), FVector(-20.0f, -14.0f, 50.0f), PrimaryPropScale) * InstXf);
						ShelfStockPrimaryInstances->AddInstance(FTransform(FRotator(0, -35, 0), FVector(20.0f, 14.0f, 50.0f), PrimaryPropScale) * InstXf);
					}
					if (ShelfStockSecondaryInstances && SecondaryPropMesh)
					{
						ShelfStockSecondaryInstances->AddInstance(FTransform(FRotator(0, 90, 0), FVector(0.0f, 0.0f, 84.0f), SecondaryPropScale) * InstXf);
					}
				}
				else if (MeshName.Contains(TEXT("SpinnerRack")))
				{
					// Accessory spinner rack: vertical display cards + base merchandise
					ShelfMerchandiseSec.AddTransformedMerchandiseTier(InstXf, FVector(-24.0f, -16.0f, 58.0f), FVector(24.0f, 16.0f, 92.0f));
					ShelfMerchandiseSec.AddTransformedMerchandiseTier(InstXf, FVector(-22.0f, -16.0f, 102.0f), FVector(22.0f, 16.0f, 136.0f));
					if (ShelfStockPrimaryInstances && PrimaryPropMesh)
					{
						ShelfStockPrimaryInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 138.0f), PrimaryPropScale) * InstXf);
					}
				}
				else if (MeshName.Contains(TEXT("MerchandiseGondola")))
				{
					// Double-sided retail gondola (X=-56..+56, Y=-24..+24, Z=0..150): 3 tiers
					const float GondolaTiers[3] = { 28.0f, 72.0f, 114.0f };
					for (int32 t = 0; t < 3; ++t)
					{
						const float TZ = GondolaTiers[t];
						ShelfMerchandiseSec.AddTransformedMerchandiseTier(InstXf, FVector(-54.0f, -18.0f, TZ), FVector(54.0f, 18.0f, TZ + 22.0f));
						if (ShelfStockPrimaryInstances && PrimaryPropMesh)
						{
							ShelfStockPrimaryInstances->AddInstance(FTransform(FRotator(0, (t * 45) % 180, 0), FVector(-32.0f, -10.0f, TZ), PrimaryPropScale) * InstXf);
							ShelfStockPrimaryInstances->AddInstance(FTransform(FRotator(0, 180 + (t * 30) % 180, 0), FVector(30.0f, 10.0f, TZ), PrimaryPropScale) * InstXf);
						}
						if (ShelfStockSecondaryInstances && SecondaryPropMesh)
						{
							ShelfStockSecondaryInstances->AddInstance(FTransform(FRotator(0, 90, 0), FVector(0.0f, (t % 2 == 0) ? -10.0f : 10.0f, TZ), SecondaryPropScale) * InstXf);
						}
					}
				}
				else if (MeshName.Contains(TEXT("ConcourseKioskCart")) || MeshName.Contains(TEXT("StockRollCage")))
				{
					// Food kiosk cart or delivery roll cage: counter/shelf stock at Z=42cm and Z=92cm
					ShelfMerchandiseSec.AddTransformedMerchandiseTier(InstXf, FVector(-38.0f, -20.0f, 42.0f), FVector(38.0f, 20.0f, 68.0f));
					ShelfMerchandiseSec.AddTransformedMerchandiseTier(InstXf, FVector(-38.0f, -18.0f, 92.0f), FVector(38.0f, 18.0f, 116.0f));
					if (ShelfStockPrimaryInstances && PrimaryPropMesh)
					{
						ShelfStockPrimaryInstances->AddInstance(FTransform(FRotator(0, 15, 0), FVector(-18.0f, 0.0f, 92.0f), PrimaryPropScale) * InstXf);
					}
					if (ShelfStockSecondaryInstances && SecondaryPropMesh)
					{
						ShelfStockSecondaryInstances->AddInstance(FTransform(FRotator(0, -20, 0), FVector(18.0f, 0.0f, 92.0f), SecondaryPropScale) * InstXf);
					}
				}
				else if (!MeshName.Contains(TEXT("OpenRinseShowerRow")))
				{
					// Standard 4-tier RetailShelves01, WallBayShelfAndRail, WallBayCubbies, WallBayHangingRails, WallBayMirror
					const float HalfSpanX = bIsWallBay ? 54.0f : 44.0f;
					const float HalfSpanY = bIsWallBay ? 13.0f : 16.0f;
					const float TierHeights[4] = { 24.0f, 68.0f, 112.0f, 154.0f };
					for (int32 t = 0; t < 4; ++t)
					{
						const float TZ = TierHeights[t];
						ShelfMerchandiseSec.AddTransformedMerchandiseTier(
							InstXf,
							FVector(-HalfSpanX, -HalfSpanY, TZ),
							FVector(HalfSpanX, HalfSpanY, TZ + 24.0f));

						if (ShelfStockPrimaryInstances && PrimaryPropMesh && (t % 2 == 0))
						{
							ShelfStockPrimaryInstances->AddInstance(FTransform(FRotator(0, 10, 0), FVector(-HalfSpanX * 0.55f, 0.0f, TZ), PrimaryPropScale) * InstXf);
							ShelfStockPrimaryInstances->AddInstance(FTransform(FRotator(0, -15, 0), FVector(HalfSpanX * 0.55f, 0.0f, TZ), PrimaryPropScale) * InstXf);
						}
						if (ShelfStockSecondaryInstances && SecondaryPropMesh && (t % 2 == 1))
						{
							ShelfStockSecondaryInstances->AddInstance(FTransform(FRotator(0, 0, 0), FVector(-HalfSpanX * 0.42f, 0.0f, TZ), SecondaryPropScale) * InstXf);
							ShelfStockSecondaryInstances->AddInstance(FTransform(FRotator(0, 180, 0), FVector(HalfSpanX * 0.42f, 0.0f, TZ), SecondaryPropScale) * InstXf);
						}
					}
				}
			}
		};

		PopulateFixtureComponentShelves(PrimaryFixtureInstances, false);
		PopulateFixtureComponentShelves(WallBayInstances, true);

		// Also place a checkout counter display card & prop on the Checkout Counter!
		if (CheckoutCounterComponent && CheckoutCounterComponent->GetStaticMesh())
		{
			const FTransform CounterXf = CheckoutCounterComponent->GetRelativeTransform();
			ShelfMerchandiseSec.AddTransformedMerchandiseTier(CounterXf, FVector(-42.0f, -16.0f, 32.0f), FVector(42.0f, 16.0f, 74.0f));
			if (ShelfStockPrimaryInstances && PrimaryPropMesh)
			{
				ShelfStockPrimaryInstances->AddInstance(FTransform(FRotator(0, 20, 0), FVector(-34.0f, 0.0f, 96.0f), PrimaryPropScale * 0.85f) * CounterXf);
			}
		}

		UMaterialInterface* ResolvedShelfMat = ShelfDisplayMaterial
			? ShelfDisplayMaterial.Get()
			: LoadObject<UMaterialInterface>(nullptr, DefaultShelfMatPath);
		if (!ResolvedShelfMat)
		{
			ResolvedShelfMat = WallMaterial;
		}
		Commit(4, ShelfMerchandiseSec, ResolvedShelfMat, false);
	}
}
