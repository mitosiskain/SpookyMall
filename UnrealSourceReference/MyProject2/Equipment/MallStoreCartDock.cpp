#include "Equipment/MallStoreCartDock.h"

#include "Equipment/MallLightCart.h"
#include "Equipment/MallCartProtectionSubsystem.h"
#include "Equipment/MallPushCart.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AMallStoreCartDock::AMallStoreCartDock()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.15f;
	Tags.AddUnique(TEXT("MallStoreCartDock"));
	DockRoot = CreateDefaultSubobject<UBoxComponent>(TEXT("DockRoot"));
	SetRootComponent(DockRoot);
	DockRoot->InitBoxExtent(FVector(85.0, 65.0, 3.0));
	DockRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DockRoot->SetCanEverAffectNavigation(false);
	ParkingMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ParkingMarker"));
	ParkingMarker->SetupAttachment(DockRoot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	ParkingMarker->SetStaticMesh(Cube.Object);
	ParkingMarker->SetRelativeScale3D(FVector(1.7, 1.3, 0.012));
	ParkingMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ParkingMarker->SetCanEverAffectNavigation(false);
	DockDisplay = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DockDisplay"));
	DockDisplay->SetupAttachment(DockRoot);
	DockDisplay->SetRelativeLocation(FVector(-90.0, 0.0, 80.0));
	DockDisplay->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	DockDisplay->SetHorizontalAlignment(EHTA_Center);
	DockDisplay->SetWorldSize(10.0f);
	DockDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DockDisplay->SetCanEverAffectNavigation(false);
}

void AMallStoreCartDock::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshDisplay();
}

void AMallStoreCartDock::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallStoreCartDock, bStoreSealed);
	DOREPLIFETIME(AMallStoreCartDock, bStoreSuppressed);
}

bool AMallStoreCartDock::MatchesGhost(const AActor* Ghost) const
{
	return bEnabled && !StoreTag.IsNone() && IsValid(Ghost) && Ghost->GetWorld() == GetWorld() && Ghost->ActorHasTag(StoreTag);
}

bool AMallStoreCartDock::IsCartCorrectlyParked(const AMallPushCart* Cart) const
{
	if (!bEnabled || StoreTag.IsNone() || !IsValid(Cart) || Cart->IsActorBeingDestroyed() || Cart->GetWorld() != GetWorld()
		|| !Cart->IsParked() || Cart->IsA<AMallLightCart>() || (AssignedCart && AssignedCart != Cart)
		|| !FMath::IsFinite(ParkingToleranceCm) || !FMath::IsFinite(FacingToleranceDegrees)) { return false; }
	// Cart pivots are 45 cm above their wheels; dock pivots sit on the floor.
	const FVector Delta = GetActorTransform().InverseTransformPosition(Cart->GetActorLocation() - FVector(0.0, 0.0, 45.0));
	const UBoxComponent* Body = Cast<UBoxComponent>(Cart->GetRootComponent());
	if (!Body || !FMath::IsFinite(ThresholdDepthToleranceCm)) { return false; }
	const FVector BodyExtent = Body->GetScaledBoxExtent();
	const FVector Normal = GetActorRightVector();
	const double HalfDepth = FMath::Abs(FVector::DotProduct(Body->GetForwardVector(), Normal)) * BodyExtent.X
		+ FMath::Abs(FVector::DotProduct(Body->GetRightVector(), Normal)) * BodyExtent.Y;
	const double MaxDepth = FMath::Min(static_cast<double>(FMath::Clamp(ThresholdDepthToleranceCm, 5.0f, 50.0f)), HalfDepth - 5.0);
	if (MaxDepth <= 0.0 || FMath::Abs(Delta.Y) > MaxDepth || FMath::Abs(Delta.X) > FMath::Clamp(ParkingToleranceCm, 20.0f, 250.0f)
		|| FMath::Abs(Delta.Z) > 35.0 || FVector::DotProduct(Cart->GetActorUpVector(), GetActorUpVector()) < 0.95) { return false; }
	const double Yaw = FMath::Abs(FMath::FindDeltaAngleDegrees(Cart->GetActorRotation().Yaw, GetActorRotation().Yaw));
	const double Facing = FMath::Min(Yaw, FMath::Abs(180.0 - Yaw));
	return Facing <= FMath::Clamp(FacingToleranceDegrees, 5.0f, 90.0f);
}

bool AMallStoreCartDock::IsSealed() const
{
	if (!HasAuthority()) { return bStoreSealed; }
	if (!bEnabled || StoreTag.IsNone() || IsActorBeingDestroyed()) { return false; }
	if (AssignedCart) { return IsCartCorrectlyParked(AssignedCart); }
	for (TActorIterator<AMallPushCart> It(GetWorld()); It; ++It) { if (IsCartCorrectlyParked(*It)) { return true; } }
	return false;
}

void AMallStoreCartDock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		const bool bCurrent = IsSealed();
		bool bSuppressed = false;
		if (UMallCartProtectionSubsystem* Protection = GetWorld()->GetSubsystem<UMallCartProtectionSubsystem>())
		{
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				if (It->ActorHasTag(TEXT("StoreGhostPrototype")) && MatchesGhost(*It) && Protection->IsStoreSuppressedByCart(*It))
				{
					bSuppressed = true; break;
				}
			}
		}
		if (bStoreSealed != bCurrent || bStoreSuppressed != bSuppressed)
		{
			bStoreSealed = bCurrent; bStoreSuppressed = bSuppressed; ForceNetUpdate(); RefreshDisplay();
		}
	}
}

void AMallStoreCartDock::OnRep_Sealed()
{
	RefreshDisplay();
}

void AMallStoreCartDock::RefreshDisplay()
{
	DockDisplay->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s"), *StoreTag.ToString(),
		bStoreSuppressed ? TEXT("CART INSIDE / STORE DORMANT") : bStoreSealed ? TEXT("DOORWAY BLOCKED") : TEXT("PARK CART ACROSS DOORWAY"))));
	DockDisplay->SetTextRenderColor(bStoreSealed ? FColor(90, 255, 150) : FColor(255, 170, 60));
}
