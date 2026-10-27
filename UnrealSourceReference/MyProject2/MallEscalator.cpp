#include "MallEscalator.h"
#include "Environment/MallEscalatorFuse.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float DeckThickness = 16.0f;
	constexpr float DeckSurfaceOffsetZ = 2.0f;
	constexpr float GuardHeight = 96.0f;
	constexpr float GuardThickness = 8.0f;

	void ConfigureDeckCollision(UBoxComponent* Box)
	{
		Box->SetMobility(EComponentMobility::Movable);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionObjectType(ECC_WorldDynamic);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetGenerateOverlapEvents(false);
		Box->CanCharacterStepUpOn = ECB_Yes;
		Box->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Increase, 55.0f));
		Box->SetHiddenInGame(true);
		Box->SetVisibility(false);
	}

	struct FEscalatorProcMeshBuilder
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
		TArray<FProcMeshTangent> Tangents;

		void AddQuad(const FVector& V0, const FVector& V1, const FVector& V2, const FVector& V3, const FVector& Normal, float UVScale = 100.0f)
		{
			const int32 Base = Vertices.Num();
			Vertices.Add(V0);
			Vertices.Add(V1);
			Vertices.Add(V2);
			Vertices.Add(V3);

			const FVector SafeN = Normal.GetSafeNormal();
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
				return FVector2D(P.X * Inv, P.Z * Inv);
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

		void AddExtrudedRibbon(
			const FVector& P0,
			const FVector& P1,
			float YCenter,
			float HalfWidthY,
			float BottomOffsetZ,
			float TopOffsetZ)
		{
			const float YMin = YCenter - HalfWidthY;
			const float YMax = YCenter + HalfWidthY;

			const FVector B0L(P0.X, YMin, P0.Z + BottomOffsetZ);
			const FVector B1L(P1.X, YMin, P1.Z + BottomOffsetZ);
			const FVector T1L(P1.X, YMin, P1.Z + TopOffsetZ);
			const FVector T0L(P0.X, YMin, P0.Z + TopOffsetZ);

			const FVector B0R(P0.X, YMax, P0.Z + BottomOffsetZ);
			const FVector B1R(P1.X, YMax, P1.Z + BottomOffsetZ);
			const FVector T1R(P1.X, YMax, P1.Z + TopOffsetZ);
			const FVector T0R(P0.X, YMax, P0.Z + TopOffsetZ);

			const FVector SegDir = (P1 - P0).GetSafeNormal();
			const FVector UpNormal = FVector::CrossProduct(FVector::RightVector, SegDir).GetSafeNormal();

			// Left (-Y) and Right (+Y) faces
			AddQuad(B0L, B1L, T1L, T0L, FVector(0.0f, -1.0f, 0.0f));
			AddQuad(B0R, B1R, T1R, T0R, FVector(0.0f, 1.0f, 0.0f));
			// Top and Bottom faces
			AddQuad(T0L, T1L, T1R, T0R, UpNormal);
			AddQuad(B0L, B1L, B1R, B0R, -UpNormal);
			// End caps
			AddQuad(B0L, B0R, T0R, T0L, -SegDir);
			AddQuad(B1L, B1R, T1R, T1L, SegDir);
		}
	};
}

AMallEscalator::AMallEscalator()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	EscalatorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("EscalatorRoot"));
	SetRootComponent(EscalatorRoot);
	EscalatorRoot->SetMobility(EComponentMobility::Movable);

	Shell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell"));
	Shell->SetupAttachment(EscalatorRoot);
	Shell->SetMobility(EComponentMobility::Movable);
	Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProceduralShell = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralShell"));
	ProceduralShell->SetupAttachment(EscalatorRoot);
	ProceduralShell->SetMobility(EComponentMobility::Movable);
	ProceduralShell->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Steps = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Steps"));
	Steps->SetupAttachment(EscalatorRoot);
	Steps->SetMobility(EComponentMobility::Movable);
	Steps->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Steps->SetGenerateOverlapEvents(false);
	Steps->CanCharacterStepUpOn = ECB_No;

	LowerDeck = CreateDefaultSubobject<UBoxComponent>(TEXT("LowerWalkingDeck"));
	InclineDeck = CreateDefaultSubobject<UBoxComponent>(TEXT("InclinedWalkingDeck"));
	UpperDeck = CreateDefaultSubobject<UBoxComponent>(TEXT("UpperWalkingDeck"));
	for (UBoxComponent* Deck : { LowerDeck.Get(), InclineDeck.Get(), UpperDeck.Get() })
	{
		Deck->SetupAttachment(EscalatorRoot);
		ConfigureDeckCollision(Deck);
	}

	for (int32 Index = 0; Index < 6; ++Index)
	{
		UBoxComponent* Guard = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("SideGuard%d"), Index));
		Guard->SetupAttachment(EscalatorRoot);
		ConfigureDeckCollision(Guard);
		Guard->CanCharacterStepUpOn = ECB_No;
		Guard->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.0f));
		SideGuards.Add(Guard);
	}

	BrokenBlockerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BrokenBlockerBox"));
	BrokenBlockerBox->SetupAttachment(EscalatorRoot);
	BrokenBlockerBox->SetMobility(EComponentMobility::Movable);
	BrokenBlockerBox->SetCollisionObjectType(ECC_WorldDynamic);
	BrokenBlockerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	BrokenBlockerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	BrokenBlockerBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BrokenBlockerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BrokenBlockerBox->SetHiddenInGame(true);

	StatusBeaconLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusBeaconLight"));
	StatusBeaconLight->SetupAttachment(EscalatorRoot);
	StatusBeaconLight->SetIntensity(650.0f);
	StatusBeaconLight->SetAttenuationRadius(260.0f);
	StatusBeaconLight->SetCastShadows(false);

	RiderVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("RiderVolume"));
	RiderVolume->SetupAttachment(EscalatorRoot);
	RiderVolume->SetMobility(EComponentMobility::Movable);
	RiderVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RiderVolume->SetCollisionObjectType(ECC_WorldDynamic);
	RiderVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	RiderVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RiderVolume->SetGenerateOverlapEvents(true);
	RiderVolume->SetHiddenInGame(true);
	RiderVolume->SetVisibility(false);

	LowerEndPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LowerEndPlate"));
	UpperEndPlate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UpperEndPlate"));
	for (UStaticMeshComponent* Plate : { LowerEndPlate.Get(), UpperEndPlate.Get() })
	{
		Plate->SetupAttachment(EscalatorRoot);
		Plate->SetMobility(EComponentMobility::Movable);
		Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		PlateMesh = Cube.Object;
		StepMeshAsset = Cube.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MetalMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_metal.M_MallRetail_metal"));
	if (MetalMat.Succeeded())
	{
		TrussMetalMaterial = MetalMat.Object;
		EndPlateMaterial = MetalMat.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlassMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_glass.M_MallRetail_glass"));
	if (GlassMat.Succeeded())
	{
		BalustradeGlassMaterial = GlassMat.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DarkMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_dark.M_MallRetail_dark"));
	if (DarkMat.Succeeded())
	{
		HandrailDarkMaterial = DarkMat.Object;
		StepMaterial = DarkMat.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> AccentMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_accent.M_MallRetail_accent"));
	if (AccentMat.Succeeded())
	{
		HazardAccentMaterial = AccentMat.Object;
	}
}

void AMallEscalator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallEscalator, Running);
	DOREPLIFETIME(AMallEscalator, ReverseDirection);
	DOREPLIFETIME(AMallEscalator, MaintenanceState);
}

void AMallEscalator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildEscalator();
}

#if WITH_EDITOR
void AMallEscalator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildEscalator();
}
#endif

void AMallEscalator::BeginPlay()
{
	Super::BeginPlay();
	RebuildEscalator();
	CurrentBeltSpeed = SignedTargetSpeed();
	PreviousSpeedTarget = CurrentBeltSpeed;
}

void AMallEscalator::OnRep_MaintenanceOrMotion()
{
	RebuildEscalator();
}

float AMallEscalator::InclineLength() const
{
	return FMath::Sqrt(Run * Run + Rise * Rise);
}

float AMallEscalator::GetTravelLength() const
{
	return 2.0f * LandingLength + InclineLength();
}

float AMallEscalator::SignedTargetSpeed() const
{
	if (MaintenanceState != EEscalatorMaintenanceState::Operational || !Running)
	{
		return 0.0f;
	}
	return FMath::Max(0.0f, TravelSpeed) * (ReverseDirection ? -1.0f : 1.0f);
}

void AMallEscalator::SetRunning(bool NewRunning)
{
	Running = NewRunning;
	if (Running && MaintenanceState == EEscalatorMaintenanceState::StalledWalkable)
	{
		MaintenanceState = EEscalatorMaintenanceState::Operational;
	}
	RebuildEscalator();
}

void AMallEscalator::SetReverseDirection(bool NewReverseDirection)
{
	ReverseDirection = NewReverseDirection;
	RebuildEscalator();
}

void AMallEscalator::SetMaintenanceState(EEscalatorMaintenanceState NewState)
{
	MaintenanceState = NewState;
	if (MaintenanceState == EEscalatorMaintenanceState::Operational)
	{
		Running = true;
	}
	else
	{
		Running = false;
	}
	RebuildEscalator();
	CurrentBeltSpeed = SignedTargetSpeed();
	PreviousSpeedTarget = CurrentBeltSpeed;
	if (HasAuthority())
	{
		ForceNetUpdate();
	}
}

FVector AMallEscalator::GetServicePanelWorldLocation() const
{
	return GetActorTransform().TransformPosition(FVector(55.0f, BeltWidth * 0.5f + 22.0f, BottomDeckHeight + 80.0f));
}

bool AMallEscalator::TryRepairEscalator(APlayerController* RepairingPlayer, FString& OutFeedback)
{
	if (!HasAuthority())
	{
		return false;
	}
	if (MaintenanceState == EEscalatorMaintenanceState::Operational)
	{
		OutFeedback = TEXT("Escalator is already operational.");
		return true;
	}

	APawn* Pawn = RepairingPlayer ? RepairingPlayer->GetPawn() : nullptr;
	if (MaintenanceState == EEscalatorMaintenanceState::BrokenBlocked && bRequiresFuseToRepair)
	{
		AMallEscalatorFuse* CarriedFuse = AMallEscalatorFuse::FindCarriedBy(Pawn);
		if (!CarriedFuse)
		{
			OutFeedback = LinkedBreakerTag != NAME_None
				? FString::Printf(TEXT("ESCALATOR OUT OF ORDER: Needs an Escalator Relay Fuse (Hotbar) OR Breaker [%s] reset!"), *LinkedBreakerTag.ToString())
				: TEXT("ESCALATOR OUT OF ORDER: Find an Escalator Relay Fuse and press F to repair.");
			return false;
		}
		CarriedFuse->ConsumeForEscalatorRepair(Pawn);
	}

	SetMaintenanceState(EEscalatorMaintenanceState::Operational);
	OutFeedback = TEXT("Escalator repaired! Relay engaged and high-rise drive started.");
	return true;
}

AMallEscalator* AMallEscalator::FindBrokenEscalatorInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction)
{
	if (!IsValid(Pawn) || !Pawn->GetWorld())
	{
		return nullptr;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallEscalatorRepairFocus), true, Pawn);
	FHitResult Hit;
	if (Pawn->GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 850.0f, ECC_Visibility, Params))
	{
		if (AMallEscalator* Esc = Cast<AMallEscalator>(Hit.GetActor()))
		{
			if (Esc->MaintenanceState != EEscalatorMaintenanceState::Operational &&
				FVector::DistSquared(Pawn->GetActorLocation(), Esc->GetServicePanelWorldLocation()) <= FMath::Square(420.0f))
			{
				return Esc;
			}
		}
	}
	// Fallback proximity check near the lower entrance service panel
	for (TActorIterator<AMallEscalator> It(Pawn->GetWorld()); It; ++It)
	{
		if (It->MaintenanceState != EEscalatorMaintenanceState::Operational)
		{
			const FVector PanelPos = It->GetServicePanelWorldLocation();
			const FVector ToPanel = PanelPos - Origin;
			if (FVector::DistSquared(Pawn->GetActorLocation(), PanelPos) <= FMath::Square(340.0f) &&
				FVector::DotProduct(ToPanel.GetSafeNormal(), Direction) >= 0.55f)
			{
				return *It;
			}
		}
	}
	return nullptr;
}

FVector AMallEscalator::DeckLocalPoint(float Distance) const
{
	const float SlopeLength = InclineLength();
	const float ClampedDistance = FMath::Clamp(Distance, 0.0f, GetTravelLength());
	if (ClampedDistance <= LandingLength)
	{
		return FVector(ClampedDistance, 0.0f, BottomDeckHeight);
	}
	if (ClampedDistance < LandingLength + SlopeLength)
	{
		const float Fraction = (ClampedDistance - LandingLength) / SlopeLength;
		return FVector(LandingLength + Run * Fraction, 0.0f, BottomDeckHeight + Rise * Fraction);
	}
	return FVector(LandingLength + Run + ClampedDistance - LandingLength - SlopeLength, 0.0f, BottomDeckHeight + Rise);
}

float AMallEscalator::DistanceAtLocalX(float LocalX) const
{
	if (LocalX <= LandingLength)
	{
		return FMath::Max(0.0f, LocalX);
	}
	if (LocalX < LandingLength + Run)
	{
		return LandingLength + (LocalX - LandingLength) * InclineLength() / Run;
	}
	return FMath::Min(GetTravelLength(), LandingLength + InclineLength() + LocalX - LandingLength - Run);
}

FVector AMallEscalator::GetDeckWorldPoint(float Distance) const
{
	return GetActorTransform().TransformPosition(DeckLocalPoint(Distance));
}

FVector AMallEscalator::GetBeltVelocityAtWorldPosition(FVector WorldPosition) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldPosition);
	const float TotalRun = Run + 2.0f * LandingLength;
	if (Local.X < StationaryEndLength || Local.X > TotalRun - StationaryEndLength || FMath::Abs(Local.Y) > BeltWidth * 0.5f)
	{
		return FVector::ZeroVector;
	}
	const bool OnIncline = Local.X > LandingLength && Local.X < LandingLength + Run;
	const FVector Tangent = OnIncline ? FVector(Run, 0.0f, Rise).GetSafeNormal() : FVector::ForwardVector;
	return GetActorTransform().TransformVector(Tangent * CurrentBeltSpeed);
}

void AMallEscalator::RefreshMaintenanceState()
{
	const bool bBlocked = (MaintenanceState == EEscalatorMaintenanceState::BrokenBlocked);
	if (BrokenBlockerBox)
	{
		BrokenBlockerBox->SetBoxExtent(FVector(42.0f, BeltWidth * 0.55f + 24.0f, 95.0f));
		BrokenBlockerBox->SetRelativeLocation(FVector(LandingLength * 0.65f, 0.0f, BottomDeckHeight + 95.0f));
		BrokenBlockerBox->SetCollisionEnabled(bBlocked ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
	if (StatusBeaconLight)
	{
		StatusBeaconLight->SetRelativeLocation(FVector(55.0f, BeltWidth * 0.5f + 26.0f, BottomDeckHeight + 115.0f));
		if (MaintenanceState == EEscalatorMaintenanceState::Operational && Running)
		{
			StatusBeaconLight->SetLightColor(FLinearColor(0.1f, 0.95f, 0.45f));
			StatusBeaconLight->SetIntensity(350.0f);
		}
		else if (MaintenanceState == EEscalatorMaintenanceState::BrokenBlocked)
		{
			StatusBeaconLight->SetLightColor(FLinearColor(1.0f, 0.15f, 0.05f));
			StatusBeaconLight->SetIntensity(900.0f);
		}
		else
		{
			StatusBeaconLight->SetLightColor(FLinearColor(1.0f, 0.65f, 0.1f));
			StatusBeaconLight->SetIntensity(450.0f);
		}
	}
}

void AMallEscalator::RebuildProceduralShell()
{
	if (!ProceduralShell)
	{
		return;
	}
	ProceduralShell->ClearAllMeshSections();
	if (!bUseProceduralShell && ShellMeshAsset != nullptr)
	{
		return;
	}

	FEscalatorProcMeshBuilder TrussSection;
	FEscalatorProcMeshBuilder GlassSection;
	FEscalatorProcMeshBuilder HandrailSection;
	FEscalatorProcMeshBuilder HazardSection;

	const FVector P0(0.0f, 0.0f, BottomDeckHeight);
	const FVector P1(LandingLength, 0.0f, BottomDeckHeight);
	const FVector P2(LandingLength + Run, 0.0f, BottomDeckHeight + Rise);
	const FVector P3(LandingLength + Run + LandingLength, 0.0f, BottomDeckHeight + Rise);

	const float HalfBelt = BeltWidth * 0.5f;

	// 1. Structural Stainless-Steel Under-Truss & Side Skirts (fits any Rise/Run height, including 2-3 floor spans!)
	TrussSection.AddExtrudedRibbon(P0, P1, 0.0f, HalfBelt + 18.0f, -68.0f, -12.0f);
	TrussSection.AddExtrudedRibbon(P1, P2, 0.0f, HalfBelt + 18.0f, -68.0f, -12.0f);
	TrussSection.AddExtrudedRibbon(P2, P3, 0.0f, HalfBelt + 18.0f, -68.0f, -12.0f);

	// 2. Left & Right Skirt Curbs, Tempered Glass Balustrades, and Dark Moving-Style Handrails
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float SideSign = (Side == 0) ? -1.0f : 1.0f;
		const float BalustradeY = SideSign * (HalfBelt + 10.0f);

		// Metallic skirt base curb
		TrussSection.AddExtrudedRibbon(P0, P1, BalustradeY, 8.0f, -12.0f, 18.0f);
		TrussSection.AddExtrudedRibbon(P1, P2, BalustradeY, 8.0f, -12.0f, 18.0f);
		TrussSection.AddExtrudedRibbon(P2, P3, BalustradeY, 8.0f, -12.0f, 18.0f);

		// Tempered glass balustrade panels
		GlassSection.AddExtrudedRibbon(P0, P1, BalustradeY, 2.5f, 18.0f, 92.0f);
		GlassSection.AddExtrudedRibbon(P1, P2, BalustradeY, 2.5f, 18.0f, 92.0f);
		GlassSection.AddExtrudedRibbon(P2, P3, BalustradeY, 2.5f, 18.0f, 92.0f);

		// Top black rubber handrail + front/rear newel returns
		HandrailSection.AddExtrudedRibbon(P0, P1, BalustradeY, 5.5f, 92.0f, 99.0f);
		HandrailSection.AddExtrudedRibbon(P1, P2, BalustradeY, 5.5f, 92.0f, 99.0f);
		HandrailSection.AddExtrudedRibbon(P2, P3, BalustradeY, 5.5f, 92.0f, 99.0f);
		HandrailSection.AddExtrudedRibbon(
			P0 - FVector(14.0f, 0.0f, 0.0f), P0 + FVector(12.0f, 0.0f, 0.0f),
			BalustradeY, 5.5f, 18.0f, 99.0f);
		HandrailSection.AddExtrudedRibbon(
			P3 - FVector(12.0f, 0.0f, 0.0f), P3 + FVector(14.0f, 0.0f, 0.0f),
			BalustradeY, 5.5f, 18.0f, 99.0f);
	}

	// 3. Service / Fuse Panel mounted on the right lower balustrade newel
	const FVector PanelStart(35.0f, 0.0f, BottomDeckHeight);
	const FVector PanelEnd(75.0f, 0.0f, BottomDeckHeight);
	TrussSection.AddExtrudedRibbon(PanelStart, PanelEnd, HalfBelt + 22.0f, 6.0f, 35.0f, 105.0f);

	// 4. Broken & Blocked Caution Barricade across lower and upper portals when BrokenBlocked
	if (MaintenanceState == EEscalatorMaintenanceState::BrokenBlocked)
	{
		const FVector BarLow0(LandingLength * 0.55f, 0.0f, BottomDeckHeight);
		const FVector BarLow1(LandingLength * 0.75f, 0.0f, BottomDeckHeight);
		HazardSection.AddExtrudedRibbon(BarLow0, BarLow1, 0.0f, HalfBelt + 4.0f, 45.0f, 62.0f);
		HazardSection.AddExtrudedRibbon(BarLow0, BarLow1, 0.0f, HalfBelt + 4.0f, 75.0f, 90.0f);

		const FVector BarHigh0(LandingLength + Run + LandingLength * 0.25f, 0.0f, BottomDeckHeight + Rise);
		const FVector BarHigh1(LandingLength + Run + LandingLength * 0.45f, 0.0f, BottomDeckHeight + Rise);
		HazardSection.AddExtrudedRibbon(BarHigh0, BarHigh1, 0.0f, HalfBelt + 4.0f, 45.0f, 62.0f);
	}

	auto Commit = [&](int32 Idx, const FEscalatorProcMeshBuilder& Builder, UMaterialInterface* Mat)
	{
		if (Builder.Vertices.Num() > 0)
		{
			ProceduralShell->CreateMeshSection_LinearColor(
				Idx, Builder.Vertices, Builder.Triangles, Builder.Normals, Builder.UVs, Builder.Colors, Builder.Tangents, false);
			if (Mat)
			{
				ProceduralShell->SetMaterial(Idx, Mat);
			}
		}
	};

	Commit(0, TrussSection, TrussMetalMaterial);
	Commit(1, GlassSection, BalustradeGlassMaterial);
	Commit(2, HandrailSection, HandrailDarkMaterial);
	Commit(3, HazardSection, HazardAccentMaterial);
}

void AMallEscalator::RebuildEscalator()
{
	Run = FMath::Max(100.0f, Run);
	Rise = FMath::Max(0.0f, Rise);
	LandingLength = FMath::Max(30.0f, LandingLength);
	BeltWidth = FMath::Max(60.0f, BeltWidth);
	StepSpacing = FMath::Max(15.0f, StepSpacing);
	StepDepth = FMath::Max(15.0f, StepDepth);
	StationaryEndLength = FMath::Clamp(StationaryEndLength, 10.0f, LandingLength * 0.8f);
	const float TotalRun = Run + 2.0f * LandingLength;
	const float SlopeLength = InclineLength();
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Rise, Run));
	const FRotator SlopeRotation(Pitch, 0.0f, 0.0f);
	const FVector SlopeNormal = SlopeRotation.RotateVector(FVector::UpVector);

	Shell->SetStaticMesh(ShellMeshAsset);
	Shell->SetRelativeTransform(ShellMeshTransform);
	Shell->SetVisibility(ShellMeshAsset != nullptr && !bUseProceduralShell);

	LowerDeck->SetBoxExtent(FVector(LandingLength * 0.5f, BeltWidth * 0.5f, DeckThickness * 0.5f));
	LowerDeck->SetRelativeLocation(FVector(LandingLength * 0.5f, 0.0f, BottomDeckHeight + DeckSurfaceOffsetZ - DeckThickness * 0.5f));
	LowerDeck->SetRelativeRotation(FRotator::ZeroRotator);
	UpperDeck->SetBoxExtent(FVector(LandingLength * 0.5f, BeltWidth * 0.5f, DeckThickness * 0.5f));
	UpperDeck->SetRelativeLocation(FVector(LandingLength + Run + LandingLength * 0.5f, 0.0f, BottomDeckHeight + Rise + DeckSurfaceOffsetZ - DeckThickness * 0.5f));
	UpperDeck->SetRelativeRotation(FRotator::ZeroRotator);
	InclineDeck->SetBoxExtent(FVector(SlopeLength * 0.5f, BeltWidth * 0.5f, DeckThickness * 0.5f));
	InclineDeck->SetRelativeLocation(FVector(LandingLength + Run * 0.5f, 0.0f, BottomDeckHeight + Rise * 0.5f + DeckSurfaceOffsetZ) - SlopeNormal * (DeckThickness * 0.5f));
	InclineDeck->SetRelativeRotation(SlopeRotation);

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float SideY = (Side == 0 ? -1.0f : 1.0f) * (BeltWidth * 0.5f + GuardThickness * 0.5f);
		UBoxComponent* LowerGuard = SideGuards[Side * 3];
		UBoxComponent* SlopeGuard = SideGuards[Side * 3 + 1];
		UBoxComponent* UpperGuard = SideGuards[Side * 3 + 2];
		LowerGuard->SetBoxExtent(FVector(LandingLength * 0.5f, GuardThickness * 0.5f, GuardHeight * 0.5f));
		LowerGuard->SetRelativeLocation(FVector(LandingLength * 0.5f, SideY, BottomDeckHeight + GuardHeight * 0.5f));
		LowerGuard->SetRelativeRotation(FRotator::ZeroRotator);
		SlopeGuard->SetBoxExtent(FVector(SlopeLength * 0.5f, GuardThickness * 0.5f, GuardHeight * 0.5f));
		SlopeGuard->SetRelativeLocation(FVector(LandingLength + Run * 0.5f, SideY, BottomDeckHeight + Rise * 0.5f) + SlopeNormal * (GuardHeight * 0.5f));
		SlopeGuard->SetRelativeRotation(SlopeRotation);
		UpperGuard->SetBoxExtent(FVector(LandingLength * 0.5f, GuardThickness * 0.5f, GuardHeight * 0.5f));
		UpperGuard->SetRelativeLocation(FVector(LandingLength + Run + LandingLength * 0.5f, SideY, BottomDeckHeight + Rise + GuardHeight * 0.5f));
		UpperGuard->SetRelativeRotation(FRotator::ZeroRotator);
	}

	RiderVolume->SetBoxExtent(FVector(TotalRun * 0.5f + 80.0f, BeltWidth * 0.5f + 80.0f, Rise * 0.5f + 180.0f));
	RiderVolume->SetRelativeLocation(FVector(TotalRun * 0.5f, 0.0f, BottomDeckHeight + Rise * 0.5f + 70.0f));

	const FVector PlateScale(StationaryEndLength / 100.0f, BeltWidth / 100.0f, 0.06f);
	LowerEndPlate->SetStaticMesh(PlateMesh);
	LowerEndPlate->SetRelativeScale3D(PlateScale);
	LowerEndPlate->SetRelativeLocation(FVector(StationaryEndLength * 0.5f, 0.0f, BottomDeckHeight - 3.0f));
	UpperEndPlate->SetStaticMesh(PlateMesh);
	UpperEndPlate->SetRelativeScale3D(PlateScale);
	UpperEndPlate->SetRelativeLocation(FVector(TotalRun - StationaryEndLength * 0.5f, 0.0f, BottomDeckHeight + Rise - 3.0f));
	LowerEndPlate->SetVisibility(ShowEndPlates);
	UpperEndPlate->SetVisibility(ShowEndPlates);
	if (EndPlateMaterial)
	{
		LowerEndPlate->SetMaterial(0, EndPlateMaterial);
		UpperEndPlate->SetMaterial(0, EndPlateMaterial);
	}

	RefreshMaintenanceState();
	RebuildProceduralShell();

	Steps->ClearInstances();
	Steps->SetStaticMesh(StepMeshAsset);
	TreadCount = 0;
	if (!StepMeshAsset)
	{
		return;
	}

	TreadCount = FMath::Clamp(FMath::CeilToInt(GetTravelLength() / StepSpacing), 4, 256);
	ActualTreadSpacing = GetTravelLength() / TreadCount;
	const float RiserHeight = StepHeight > 0.0f ? StepHeight : FMath::Max(8.0f, ActualTreadSpacing * Rise / SlopeLength);
	const float FittedStepDepth = FMath::Min(StepDepth, ActualTreadSpacing - 0.15f);
	const FBox Bounds = StepMeshAsset->GetBoundingBox();
	const FVector Size = Bounds.GetSize().ComponentMax(FVector(0.1f));
	TreadMeshScale = FVector(FittedStepDepth / Size.X, BeltWidth / Size.Y, RiserHeight / Size.Z);
	TreadMeshPivotOffset = -FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Max.Z) * TreadMeshScale;
	if (StepMaterial)
	{
		for (int32 Index = 0; Index < Steps->GetNumMaterials(); ++Index)
		{
			Steps->SetMaterial(Index, StepMaterial);
		}
	}
	for (int32 Index = 0; Index < TreadCount; ++Index)
	{
		Steps->AddInstance(FTransform(FQuat::Identity, DeckLocalPoint(Index * ActualTreadSpacing) + TreadMeshPivotOffset, TreadMeshScale));
	}
	UpdateTreads();
}

void AMallEscalator::UpdateTreads()
{
	if (TreadCount == 0)
	{
		return;
	}
	const double Length = GetTravelLength();
	const int32 MissingPitCenter = TreadCount / 2;
	for (int32 Index = 0; Index < TreadCount; ++Index)
	{
		double Distance = FMath::Fmod(BeltDistance + Index * ActualTreadSpacing, Length);
		if (Distance < 0.0)
		{
			Distance += Length;
		}
		const FVector Point = DeckLocalPoint(static_cast<float>(Distance)) + TreadMeshPivotOffset;
		const bool AtHiddenReturn = Distance < StepDepth * 0.5 || Distance > Length - StepDepth * 0.5;
		// When BrokenBlocked, expose an unsettling open mechanical pit in the steps
		const bool InBrokenMaintenancePit =
			(MaintenanceState == EEscalatorMaintenanceState::BrokenBlocked) &&
			(FMath::Abs(Index - MissingPitCenter) <= 1);
		const FVector InstanceScale = (AtHiddenReturn || InBrokenMaintenancePit) ? FVector::ZeroVector : TreadMeshScale;
		Steps->UpdateInstanceTransform(Index, FTransform(FQuat::Identity, Point, InstanceScale), false, Index == TreadCount - 1, true);
	}
}

bool AMallEscalator::IsWalkingDeck(const UPrimitiveComponent* Component) const
{
	return Component == LowerDeck || Component == InclineDeck || Component == UpperDeck;
}

void AMallEscalator::CarryGroundedRiders(float DeltaSeconds)
{
	if (FMath::IsNearlyZero(CurrentBeltSpeed) || !GetWorld())
	{
		return;
	}
	TArray<AActor*> Candidates;
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, RiderVolume->GetComponentLocation(), RiderVolume->GetComponentQuat(),
		ObjectQuery, FCollisionShape::MakeBox(RiderVolume->GetScaledBoxExtent()), QueryParams);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (ACharacter* Character = Cast<ACharacter>(Overlap.GetActor()))
		{
			Candidates.AddUnique(Character);
		}
	}

	const FTransform ActorTransform = GetActorTransform();
	const float TotalRun = Run + 2.0f * LandingLength;
	const float TravelLen = GetTravelLength();
	const float MinDistance = (CurrentBeltSpeed > 0.0f) ? 8.0f : -24.0f;
	const float MaxDistance = (CurrentBeltSpeed > 0.0f) ? (TravelLen + 24.0f) : (TravelLen - 8.0f);

	auto EvalExtendedDeckLocalPoint = [&](float Dist) -> FVector
	{
		if (Dist < 0.0f)
		{
			return FVector(Dist, 0.0f, BottomDeckHeight);
		}
		if (Dist > TravelLen)
		{
			return FVector(TotalRun + (Dist - TravelLen), 0.0f, BottomDeckHeight + Rise);
		}
		return DeckLocalPoint(Dist);
	};

	for (AActor* Candidate : Candidates)
	{
		ACharacter* Character = Cast<ACharacter>(Candidate);
		UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		if (!Movement || !Movement->IsMovingOnGround() || (!Character->HasAuthority() && !Character->IsLocallyControlled()))
		{
			continue;
		}

		const FVector Local = ActorTransform.InverseTransformPosition(Character->GetActorLocation());
		if (FMath::Abs(Local.Y) > BeltWidth * 0.5f + 12.0f)
		{
			continue;
		}

		const float Distance = (Local.X < 0.0f)
			? Local.X
			: (Local.X > TotalRun ? (TravelLen + (Local.X - TotalRun)) : DistanceAtLocalX(Local.X));
		if (Distance < MinDistance || Distance > MaxDistance)
		{
			continue;
		}

		const bool bOnEscalatorDeck =
			IsWalkingDeck(Cast<UPrimitiveComponent>(Character->GetMovementBaseObject())) ||
			IsWalkingDeck(Movement->CurrentFloor.HitResult.GetComponent());
		if (!bOnEscalatorDeck)
		{
			const float CapsuleHalfH = Character->GetCapsuleComponent()
				? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
				: 90.0f;
			const float FootLocalZ = Local.Z - CapsuleHalfH;
			const float DeckZ = EvalExtendedDeckLocalPoint(Distance).Z;
			if (FootLocalZ < DeckZ - 24.0f || FootLocalZ > DeckZ + 48.0f)
			{
				continue;
			}
		}

		const float NextDistance = FMath::Clamp(Distance + CurrentBeltSpeed * DeltaSeconds, MinDistance, MaxDistance);
		const FVector LocalDelta = EvalExtendedDeckLocalPoint(NextDistance) - EvalExtendedDeckLocalPoint(Distance);
		const FVector WorldDelta = ActorTransform.TransformVector(LocalDelta);
		if (WorldDelta.IsNearlyZero())
		{
			continue;
		}

		FHitResult Hit;
		Movement->SafeMoveUpdatedComponent(WorldDelta, Character->GetActorQuat(), true, Hit);
		if (Hit.IsValidBlockingHit())
		{
			const FVector RemainingDelta = FVector::VectorPlaneProject(WorldDelta, Hit.Normal) * (1.0f - Hit.Time);
			if (!RemainingDelta.IsNearlyZero() && FVector::DotProduct(RemainingDelta, WorldDelta) > 0.0f)
			{
				FHitResult SlideHit;
				Movement->SafeMoveUpdatedComponent(RemainingDelta, Character->GetActorQuat(), true, SlideHit);
			}
		}
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			Movement->FindFloor(Capsule->GetComponentLocation(), Movement->CurrentFloor, false);
			Movement->AdjustFloorHeight();
		}
		Movement->bForceNextFloorCheck = true;
	}
}

void AMallEscalator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float TargetSpeed = SignedTargetSpeed();
	if (!FMath::IsNearlyEqual(TargetSpeed, PreviousSpeedTarget))
	{
		PreviousSpeedTarget = TargetSpeed;
		SpeedChangeRate = SpeedChangeTime > UE_SMALL_NUMBER ? FMath::Abs(TargetSpeed - CurrentBeltSpeed) / SpeedChangeTime : 0.0f;
	}
	CurrentBeltSpeed = SpeedChangeTime <= UE_SMALL_NUMBER
		? TargetSpeed
		: FMath::FInterpConstantTo(CurrentBeltSpeed, TargetSpeed, DeltaSeconds, SpeedChangeRate);
	BeltDistance = FMath::Fmod(BeltDistance + CurrentBeltSpeed * DeltaSeconds, static_cast<double>(GetTravelLength()));
	UpdateTreads();
	CarryGroundedRiders(DeltaSeconds);
}
