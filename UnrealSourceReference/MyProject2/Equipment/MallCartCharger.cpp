#include "Equipment/MallCartCharger.h"

#include "Equipment/MallLightCart.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

AMallCartCharger::AMallCartCharger()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	Tags.AddUnique(TEXT("MallCartCharger"));
	ChargerBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("ChargerBounds"));
	SetRootComponent(ChargerBounds);
	ChargerBounds->InitBoxExtent(FVector(12.0, 18.0, 24.0));
	ChargerBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ChargerBounds->SetCollisionObjectType(ECC_WorldDynamic);
	ChargerBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	ChargerBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ChargerBounds->SetCanEverAffectNavigation(false);
	OutletMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OutletMesh"));
	OutletMesh->SetupAttachment(ChargerBounds);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	OutletMesh->SetStaticMesh(Cube.Object);
	OutletMesh->SetRelativeScale3D(FVector(0.12, 0.25, 0.35));
	OutletMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChargerDisplay = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ChargerDisplay"));
	ChargerDisplay->SetupAttachment(ChargerBounds);
	ChargerDisplay->SetRelativeLocation(FVector(15.0, 0.0, 38.0));
	ChargerDisplay->SetWorldSize(9.0f);
	ChargerDisplay->SetHorizontalAlignment(EHTA_Center);
	ChargerDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChargerDisplay->SetCanEverAffectNavigation(false);
}

void AMallCartCharger::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ChargerDisplay->SetText(FText::FromString(bExtensionLead ? TEXT("POWERED EXTENSION\nAIM AT NEARBY LIGHT CART + E") : TEXT("CART CHARGER\nAIM AT NEARBY LIGHT CART + E")));
	ChargerDisplay->SetTextRenderColor(bPowered ? FColor(100, 180, 255) : FColor(100, 100, 100));
}

FVector AMallCartCharger::GetConnectionLocation() const
{
	return GetActorLocation() + GetActorForwardVector() * 15.0;
}

bool AMallCartCharger::CanSupply(const AMallLightCart* Cart) const
{
	if (!HasAuthority() || !bPowered || IsActorBeingDestroyed() || !IsValid(Cart) || Cart->IsActorBeingDestroyed()
		|| Cart->GetWorld() != GetWorld() || !Cart->IsParked() || !FMath::IsFinite(ConnectionRangeCm)) { return false; }
	const float Reach = FMath::Clamp(ConnectionRangeCm, 50.0f, 3000.0f);
	const FVector CartPlug = Cart->GetActorLocation() + FVector(0.0, 0.0, 70.0);
	if (FVector::DistSquared(GetConnectionLocation(), CartPlug) > FMath::Square(Reach)) { return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(MallCartChargingCable), false, this);
	Query.AddIgnoredActor(Cart);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByObjectType(Hit, GetConnectionLocation(), CartPlug,
		FCollisionObjectQueryParams(ECC_WorldStatic), Query);
}

bool AMallCartCharger::TryUse(APlayerController* Player)
{
	const APawn* Pawn = IsValid(Player) && Player->HasAuthority() ? Player->GetPawn() : nullptr;
	if (!HasAuthority() || !AMallPushCart::IsEligiblePlayer(Player) || !IsValid(Pawn) || Pawn->GetWorld() != GetWorld() || Pawn->GetController() != Player
		|| FVector::DistSquared(Pawn->GetActorLocation(), GetConnectionLocation()) > FMath::Square(350.0)) { return false; }
	AMallLightCart* Nearest = nullptr;
	double NearestDistance = UE_BIG_NUMBER;
	for (TActorIterator<AMallLightCart> It(GetWorld()); It; ++It)
	{
		const double Distance = FVector::DistSquared(Pawn->GetActorLocation(), It->GetActorLocation());
		if ((It->CurrentCharger == this || CanSupply(*It)) && Distance < NearestDistance)
		{
			Nearest = *It; NearestDistance = Distance;
		}
	}
	if (!Nearest) { return false; }
	if (Nearest->CurrentCharger == this) { Nearest->DisconnectCharger(); return true; }
	return Nearest->ConnectToCharger(this);
}
