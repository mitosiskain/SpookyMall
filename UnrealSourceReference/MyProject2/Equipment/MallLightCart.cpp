#include "Equipment/MallLightCart.h"

#include "Equipment/MallCartCharger.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace MallSafetyCone
{
	double PointDistanceSquared(FVector Point, FVector Apex, FVector Axis, double Length, double Tangent)
	{
		const FVector Offset = Point - Apex;
		const double Depth = FVector::DotProduct(Offset, Axis);
		const double Radial = (Offset - Axis * Depth).Size();
		// Spotlights have a spherical attenuation radius, rather than a flat base cap.
		const FVector2D Point2D(Radial, Depth);
		if (Depth >= 0.0 && Radial <= Depth * Tangent)
		{
			return FMath::Square(FMath::Max(0.0, Point2D.Size() - Length));
		}
		const double Cos = 1.0 / FMath::Sqrt(1.0 + Tangent * Tangent);
		const FVector2D ConeEdge(Tangent * Cos, Cos);
		const double Projection = FMath::Clamp(FVector2D::DotProduct(Point2D, ConeEdge), 0.0, Length);
		return FVector2D::DistSquared(Point2D, ConeEdge * Projection);
	}

	bool Valid(FVector Apex, FVector Direction, float Length, float Angle, FVector Centre, float Radius, float HalfHeight)
	{
		return !Apex.ContainsNaN() && !Direction.ContainsNaN() && !Centre.ContainsNaN()
			&& !Direction.IsNearlyZero() && FMath::IsFinite(Length) && Length > 0.0f
			&& FMath::IsFinite(Angle) && Angle > 0.0f && Angle < 89.0f
			&& FMath::IsFinite(Radius) && Radius > 0.0f && FMath::IsFinite(HalfHeight) && HalfHeight >= Radius;
	}
}

AMallLightCart::AMallLightCart()
{
	PrimaryActorTick.bCanEverTick = true;
	Tags.AddUnique(TEXT("MallLightCart"));
	CartName = TEXT("Safety light cart");
	StorageCapacity = 0;
	bHasComputerTerminal = false;
	UpdateComputerVisuals();
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	FallbackLampHeadMesh = Cube.Object;
	LampMast = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LampMast"));
	LampMast->SetupAttachment(GetRootComponent());
	LampMast->SetStaticMesh(Cylinder.Object);
	LampMast->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LampMast->SetCanEverAffectNavigation(false);
	LampMast->SetMobility(EComponentMobility::Movable);
	LampHead = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LampHead"));
	LampHead->SetupAttachment(GetRootComponent());
	LampHead->SetStaticMesh(Cube.Object);
	LampHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// The emitter is inside the lamp's lens/housing; do not shadow its own beam.
	LampHead->SetCastShadow(false);
	LampHead->SetCanEverAffectNavigation(false);
	LampHead->SetMobility(EComponentMobility::Movable);
	ChargingCable = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChargingCable"));
	ChargingCable->SetupAttachment(GetRootComponent());
	ChargingCable->SetStaticMesh(Cylinder.Object);
	ChargingCable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChargingCable->SetCanEverAffectNavigation(false);
	ChargingCable->SetMobility(EComponentMobility::Movable);
	ChargingCable->SetVisibility(false);
	SafetySpotlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("SafetySpotlight"));
	SafetySpotlight->SetupAttachment(GetRootComponent());
	SafetySpotlight->SetMobility(EComponentMobility::Movable);
	SafetySpotlight->SetCastShadows(true);
	SafetySpotlight->SetIntensityUnits(ELightUnits::Lumens);
	SafetySpotlight->SetLightColor(FLinearColor(0.90f, 0.96f, 1.0f));
	BatteryDisplay = CreateDefaultSubobject<UTextRenderComponent>(TEXT("BatteryDisplay"));
	BatteryDisplay->SetupAttachment(GetRootComponent());
	BatteryDisplay->SetRelativeLocation(FVector(-65.0, 0.0, 135.0));
	BatteryDisplay->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	BatteryDisplay->SetHorizontalAlignment(EHTA_Center);
	BatteryDisplay->SetWorldSize(10.0f);
	BatteryDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BatteryDisplay->SetCanEverAffectNavigation(false);
}

void AMallLightCart::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshLightPresentation();
}

void AMallLightCart::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) { BatteryFraction = FMath::IsFinite(BatteryFraction) ? FMath::Clamp(BatteryFraction, 0.0f, 1.0f) : 0.0f; }
	RefreshLightPresentation();
}

void AMallLightCart::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallLightCart, BatteryFraction);
	DOREPLIFETIME(AMallLightCart, bPowerRequested);
	DOREPLIFETIME(AMallLightCart, CurrentCharger);
	DOREPLIFETIME(AMallLightCart, bSafetyWhilePushing);
	DOREPLIFETIME(AMallLightCart, LampHeightCm);
	DOREPLIFETIME(AMallLightCart, ConeHalfAngleDegrees);
	DOREPLIFETIME(AMallLightCart, ConeLengthCm);
	DOREPLIFETIME(AMallLightCart, LampIntensityLumens);
	DOREPLIFETIME(AMallLightCart, LampHeadMesh);
}

void AMallLightCart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0.0f)
	{
		if (CurrentCharger && (!IsValid(CurrentCharger) || !CurrentCharger->CanSupply(this))) { DisconnectCharger(); }
		if (IsValid(CurrentCharger))
		{
			const float Duration = FMath::IsFinite(RechargeSeconds) ? FMath::Clamp(RechargeSeconds, 1.0f, 86400.0f) : 300.0f;
			BatteryFraction = FMath::Clamp(BatteryFraction + DeltaSeconds / Duration, 0.0f, 1.0f);
		}
		else if (IsSafetyActive())
		{
			const float Duration = FMath::IsFinite(RuntimeSeconds) ? FMath::Clamp(RuntimeSeconds, 1.0f, 86400.0f) : 900.0f;
			BatteryFraction = FMath::Max(0.0f, BatteryFraction - DeltaSeconds / Duration);
		}
	}
	RefreshLightPresentation();
}

void AMallLightCart::OnPushStateChanged()
{
	Super::OnPushStateChanged();
	if (HasAuthority() && !IsParked()) { DisconnectCharger(); }
	RefreshLightPresentation();
}

bool AMallLightCart::TryUse(APlayerController* Player)
{
	if (!HasAuthority() || !CanPlayerReach(Player)) { return false; }
	if (CurrentCharger) { DisconnectCharger(); return true; }
	AMallCartCharger* Nearest = nullptr;
	double NearestDistance = UE_BIG_NUMBER;
	for (TActorIterator<AMallCartCharger> It(GetWorld()); It; ++It)
	{
		const double Distance = FVector::DistSquared(It->GetConnectionLocation(), GetActorLocation());
		if (Distance < NearestDistance && It->CanSupply(this)) { Nearest = *It; NearestDistance = Distance; }
	}
	if (Nearest) { return ConnectToCharger(Nearest); }
	return SetPowerRequested(!bPowerRequested);
}

FString AMallLightCart::GetCartStatus() const
{
	const TCHAR* State = CurrentCharger ? TEXT("Charging (light off)") : IsSafetyActive() ? TEXT("Safety light on") :
		BatteryFraction <= 0.0f ? TEXT("Battery empty") : TEXT("Light off");
	return FString::Printf(TEXT("%s | Battery %d%% | %s\nH: power / connect near a charger"),
		*CartName, FMath::RoundToInt(FMath::Clamp(BatteryFraction, 0.0f, 1.0f) * 100.0f), State);
}

bool AMallLightCart::SetPowerRequested(bool bRequested)
{
	if (!HasAuthority() || IsActorBeingDestroyed()) { return false; }
	bPowerRequested = bRequested;
	ForceNetUpdate();
	RefreshLightPresentation();
	return true;
}

bool AMallLightCart::ConnectToCharger(AMallCartCharger* Charger)
{
	if (!HasAuthority() || !IsValid(Charger) || !Charger->CanSupply(this)) { return false; }
	CurrentCharger = Charger;
	ForceNetUpdate();
	RefreshLightPresentation();
	return true;
}

void AMallLightCart::DisconnectCharger()
{
	if (!HasAuthority()) { return; }
	CurrentCharger = nullptr;
	ForceNetUpdate();
	RefreshLightPresentation();
}

bool AMallLightCart::IsSafetyActive() const
{
	return !IsActorBeingDestroyed() && bPowerRequested && FMath::IsFinite(BatteryFraction) && BatteryFraction > 0.0f
		&& FMath::IsFinite(LampIntensityLumens) && LampIntensityLumens > 0.0f
		&& !CurrentCharger && (bSafetyWhilePushing || IsParked());
}

void AMallLightCart::GetSafeCone(FVector& Apex, FVector& Direction, float& Length, float& Angle) const
{
	Apex = SafetySpotlight->GetComponentLocation();
	Direction = SafetySpotlight->GetForwardVector();
	Length = FMath::IsFinite(ConeLengthCm) ? FMath::Clamp(ConeLengthCm, 150.0f, 2000.0f) : 650.0f;
	Angle = FMath::IsFinite(ConeHalfAngleDegrees) ? FMath::Clamp(ConeHalfAngleDegrees, 10.0f, 75.0f) : 55.0f;
}

bool AMallLightCart::HasClearLightPath(FVector Target) const
{
	if (!GetWorld() || Target.ContainsNaN()) { return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(MallCartLightOcclusion), false, this);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByObjectType(Hit, SafetySpotlight->GetComponentLocation(), Target,
		FCollisionObjectQueryParams(ECC_WorldStatic), Query);
}

bool AMallLightCart::IsCapsuleProtected(FVector Centre, float Radius, float HalfHeight) const
{
	if (!IsSafetyActive()) { return false; }
	FVector Apex, Direction; float Length, Angle;
	GetSafeCone(Apex, Direction, Length, Angle);
	if (!ConeIntersectsCapsule(Apex, Direction, Length, Angle, Centre, Radius, HalfHeight)) { return false; }
	const float Stem = FMath::Max(0.0f, HalfHeight - Radius);
	return HasClearLightPath(Centre) || HasClearLightPath(Centre + FVector(0.0, 0.0, Stem))
		|| HasClearLightPath(Centre - FVector(0.0, 0.0, Stem));
}

bool AMallLightCart::IsCapsuleFullyProtected(FVector Centre, float Radius, float HalfHeight) const
{
	if (!IsSafetyActive()) { return false; }
	FVector Apex, Direction; float Length, Angle;
	GetSafeCone(Apex, Direction, Length, Angle);
	const float Stem = FMath::Max(0.0f, HalfHeight - Radius);
	return ConeContainsCapsule(Apex, Direction, Length, Angle, Centre, Radius, HalfHeight)
		&& HasClearLightPath(Centre) && HasClearLightPath(Centre + FVector(0.0, 0.0, Stem))
		&& HasClearLightPath(Centre - FVector(0.0, 0.0, Stem));
}

bool AMallLightCart::ConeIntersectsCapsule(FVector Apex, FVector Direction, float Length, float HalfAngleDegrees,
	FVector Centre, float Radius, float HalfHeight)
{
	if (!MallSafetyCone::Valid(Apex, Direction, Length, HalfAngleDegrees, Centre, Radius, HalfHeight)) { return false; }
	const FVector Axis = Direction.GetSafeNormal();
	const double Tangent = FMath::Tan(FMath::DegreesToRadians(static_cast<double>(HalfAngleDegrees)));
	const double Stem = HalfHeight - Radius;
	auto Distance = [&](double Height) { return MallSafetyCone::PointDistanceSquared(Centre + FVector(0.0, 0.0, Height), Apex, Axis, Length, Tangent); };
	// Distance to a convex finite cone is convex along the capsule's vertical centre segment.
	double Lower = -Stem, Upper = Stem;
	for (int32 I = 0; I < 40; ++I)
	{
		const double Left = Lower + (Upper - Lower) / 3.0, Right = Upper - (Upper - Lower) / 3.0;
		if (Distance(Left) <= Distance(Right)) { Upper = Right; } else { Lower = Left; }
	}
	const double Closest = FMath::Min3(Distance(-Stem), Distance(Stem), Distance((Lower + Upper) * 0.5));
	return Closest <= FMath::Square(static_cast<double>(Radius));
}

bool AMallLightCart::ConeContainsCapsule(FVector Apex, FVector Direction, float Length, float HalfAngleDegrees,
	FVector Centre, float Radius, float HalfHeight)
{
	if (!MallSafetyCone::Valid(Apex, Direction, Length, HalfAngleDegrees, Centre, Radius, HalfHeight)) { return false; }
	const FVector Axis = Direction.GetSafeNormal();
	const double Angle = FMath::DegreesToRadians(static_cast<double>(HalfAngleDegrees));
	const double Sin = FMath::Sin(Angle), Cos = FMath::Cos(Angle), Stem = HalfHeight - Radius;
	for (double Height : { -Stem, Stem })
	{
		const FVector Offset = Centre + FVector(0.0, 0.0, Height) - Apex;
		const double Depth = FVector::DotProduct(Offset, Axis);
		const double Radial = (Offset - Axis * Depth).Size();
		if (Depth < Radius || Offset.Size() + Radius > Length || Radial * Cos - Depth * Sin + Radius > 0.0) { return false; }
	}
	return true;
}

void AMallLightCart::OnRep_LightState()
{
	RefreshLightPresentation();
}

void AMallLightCart::RefreshLightPresentation()
{
	const float Height = FMath::IsFinite(LampHeightCm) ? FMath::Clamp(LampHeightCm, 150.0f, 1000.0f) : 320.0f;
	const float Angle = FMath::IsFinite(ConeHalfAngleDegrees) ? FMath::Clamp(ConeHalfAngleDegrees, 10.0f, 75.0f) : 55.0f;
	const float Length = FMath::IsFinite(ConeLengthCm) ? FMath::Clamp(ConeLengthCm, 150.0f, 2000.0f) : 650.0f;
	LampMast->SetRelativeLocation(FVector(0.0, 0.0, (Height + 65.0f) * 0.5f));
	LampMast->SetRelativeScale3D(FVector(0.055, 0.055, (Height - 65.0f) / 100.0f));
	LampHead->SetRelativeLocation(FVector(0.0, 0.0, Height + 5.0f));
	LampHead->SetStaticMesh(LampHeadMesh ? LampHeadMesh : FallbackLampHeadMesh);
	LampHead->SetRelativeRotation(LampHeadMesh ? FRotator(-80.0, 0.0, 0.0) : FRotator::ZeroRotator);
	LampHead->SetRelativeScale3D(LampHeadMesh ? FVector::OneVector : FVector(0.38, 0.28, 0.12));
	ChargingCable->SetVisibility(IsValid(CurrentCharger));
	if (IsValid(CurrentCharger))
	{
		const FVector Start = CurrentCharger->GetConnectionLocation();
		const FVector End = GetActorLocation() + FVector(0.0, 0.0, 70.0);
		const FVector Cable = End - Start;
		ChargingCable->SetWorldLocation((Start + End) * 0.5);
		ChargingCable->SetWorldRotation(FRotationMatrix::MakeFromZ(Cable.GetSafeNormal()).Rotator());
		ChargingCable->SetWorldScale3D(FVector(0.025, 0.025, FMath::Max(1.0, Cable.Size()) / 100.0));
	}
	SafetySpotlight->SetRelativeLocation(FVector(0.0, 0.0, Height));
	SafetySpotlight->SetRelativeRotation(FRotator(-90.0, 0.0, 0.0));
	SafetySpotlight->SetOuterConeAngle(Angle);
	SafetySpotlight->SetInnerConeAngle(FMath::Max(5.0f, Angle - 8.0f));
	SafetySpotlight->SetAttenuationRadius(Length);
	SafetySpotlight->SetIntensity(FMath::IsFinite(LampIntensityLumens) ? FMath::Clamp(LampIntensityLumens, 0.0f, 1000000.0f) : 18000.0f);
	SafetySpotlight->SetVisibility(IsSafetyActive());
	const TCHAR* State = CurrentCharger ? TEXT("CHARGING / LIGHT OFF") : IsSafetyActive() ? TEXT("PROTECTED LIGHT") :
		BatteryFraction <= 0.0f ? TEXT("BATTERY EMPTY") : !IsParked() && !bSafetyWhilePushing ? TEXT("PARK TO PROTECT") : TEXT("LIGHT OFF");
	BatteryDisplay->SetText(FText::FromString(FString::Printf(TEXT("%s\nBATTERY %d%%\nH: POWER / CONNECT"), State,
		FMath::RoundToInt(FMath::Clamp(BatteryFraction, 0.0f, 1.0f) * 100.0f))));
	BatteryDisplay->SetTextRenderColor(IsSafetyActive() ? FColor(110, 255, 175) : CurrentCharger ? FColor(100, 180, 255) : FColor(255, 190, 100));
}
