#include "Environment/MallHidingSpot.h"
#include "Ghost/MallGhostInteractionComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

namespace
{
	UStaticMesh* LoadHidingMesh(const TCHAR* PrimaryPath, const TCHAR* FallbackPath = nullptr)
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

AMallHidingSpot::AMallHidingSpot()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(SceneRoot);
	InteractionBounds->SetBoxExtent(FVector(75.0f, 80.0f, 115.0f));
	InteractionBounds->SetRelativeLocation(FVector(0.0f, 0.0f, 115.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	EnclosureMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnclosureMesh"));
	EnclosureMesh->SetupAttachment(SceneRoot);
	EnclosureMesh->SetCollisionProfileName(TEXT("BlockAll"));

	SecondaryCoverMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SecondaryCoverMesh"));
	SecondaryCoverMesh->SetupAttachment(SceneRoot);
	SecondaryCoverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SpotSignText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SpotSignText"));
	SpotSignText->SetupAttachment(SceneRoot);
	SpotSignText->SetHorizontalAlignment(EHTA_Center);
	SpotSignText->SetVerticalAlignment(EVRTA_TextCenter);
	SpotSignText->SetWorldSize(18.0f);
	SpotSignText->SetTextRenderColor(FColor(145, 255, 195));
	SpotSignText->SetRelativeLocation(FVector(0.0f, -68.0f, 232.0f));
	SpotSignText->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	IndicatorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("IndicatorLight"));
	IndicatorLight->SetupAttachment(SceneRoot);
	IndicatorLight->SetIntensityUnits(ELightUnits::Lumens);
	IndicatorLight->SetIntensity(28.0f);
	IndicatorLight->SetAttenuationRadius(240.0f);
	IndicatorLight->SetSourceRadius(12.0f);
	IndicatorLight->SetCastShadows(false);
	IndicatorLight->SetLightColor(FLinearColor(0.35f, 0.95f, 0.65f));
	IndicatorLight->SetRelativeLocation(FVector(0.0f, -60.0f, 215.0f));
}

void AMallHidingSpot::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildHidingSpotVisuals();
}

void AMallHidingSpot::BeginPlay()
{
	Super::BeginPlay();
	RebuildHidingSpotVisuals();
}

void AMallHidingSpot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OccupantPawn = nullptr;
	bOccupiedWithoutWitness = false;
	Super::EndPlay(EndPlayReason);
}

void AMallHidingSpot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallHidingSpot, OccupantPawn);
	DOREPLIFETIME(AMallHidingSpot, bOccupiedWithoutWitness);
}

void AMallHidingSpot::RebuildHidingSpotVisuals()
{
	UStaticMesh* PrimaryMesh = nullptr;
	UStaticMesh* ExtraMesh = nullptr;

	switch (HidingSpotType)
	{
	case EMallHidingSpotType::FittingRoomBooth:
		PrimaryMesh = LoadHidingMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25284_FittingCubicleClosed.SM_Retail_25284_FittingCubicleClosed"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25288_FittingRoomCurtain.SM_Retail_25288_FittingRoomCurtain"));
		ExtraMesh = LoadHidingMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25296_LeaningMirror.SM_Retail_25296_LeaningMirror"));
		if (SpotDisplayName.IsEmpty())
		{
			SpotDisplayName = TEXT("Fitting Room Cubicle [HIDE]");
		}
		HiddenLocalOffset = FVector(0.0f, 5.0f, 92.0f);
		ExitLocalOffset = FVector(0.0f, -135.0f, 92.0f);
		break;

	case EMallHidingSpotType::CircularClothingRack:
		PrimaryMesh = LoadHidingMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25210_RoundHangingRail.SM_Retail_25210_RoundHangingRail"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25236_AccessorySpinnerRack.SM_Retail_25236_AccessorySpinnerRack"));
		if (SpotDisplayName.IsEmpty() || SpotDisplayName == TEXT("Fitting Room Booth"))
		{
			SpotDisplayName = TEXT("Overstocked Garment Rack [HIDE]");
		}
		HiddenLocalOffset = FVector(0.0f, 0.0f, 92.0f);
		ExitLocalOffset = FVector(0.0f, -125.0f, 92.0f);
		break;

	case EMallHidingSpotType::SaunaWoodenBooth:
		PrimaryMesh = LoadHidingMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25284_FittingCubicleClosed.SM_Retail_25284_FittingCubicleClosed"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Bath_SaunaStoveAndBuckets.SM_Bath_SaunaStoveAndBuckets"));
		ExtraMesh = LoadHidingMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Bath_SaunaStoveAndBuckets.SM_Bath_SaunaStoveAndBuckets"));
		if (SpotDisplayName.IsEmpty() || SpotDisplayName == TEXT("Fitting Room Booth"))
		{
			SpotDisplayName = TEXT("Steamy Cedar Sauna Booth [HIDE]");
		}
		HiddenLocalOffset = FVector(0.0f, 0.0f, 92.0f);
		ExitLocalOffset = FVector(0.0f, -135.0f, 92.0f);
		break;

	case EMallHidingSpotType::SleepPodBunk:
		PrimaryMesh = LoadHidingMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25288_FittingRoomCurtain.SM_Retail_25288_FittingRoomCurtain"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25381_ChargingBench.SM_Retail_25381_ChargingBench"));
		ExtraMesh = LoadHidingMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25381_ChargingBench.SM_Retail_25381_ChargingBench"));
		if (SpotDisplayName.IsEmpty() || SpotDisplayName == TEXT("Fitting Room Booth"))
		{
			SpotDisplayName = TEXT("Curtained Sleep-Pod Capsule [HIDE]");
		}
		HiddenLocalOffset = FVector(0.0f, 0.0f, 92.0f);
		ExitLocalOffset = FVector(0.0f, -135.0f, 92.0f);
		break;

	case EMallHidingSpotType::ServiceUtilityCloset:
	default:
		PrimaryMesh = LoadHidingMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25284_FittingCubicleClosed.SM_Retail_25284_FittingCubicleClosed"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25352_StockRoomDoor.SM_Retail_25352_StockRoomDoor"));
		ExtraMesh = LoadHidingMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25375_CleaningTrolley.SM_Retail_25375_CleaningTrolley"));
		if (SpotDisplayName.IsEmpty() || SpotDisplayName == TEXT("Fitting Room Booth"))
		{
			SpotDisplayName = TEXT("BOH Janitor Utility Closet [HIDE]");
		}
		HiddenLocalOffset = FVector(0.0f, 0.0f, 92.0f);
		ExitLocalOffset = FVector(0.0f, -135.0f, 92.0f);
		break;
	}

	if (EnclosureMesh)
	{
		EnclosureMesh->SetStaticMesh(PrimaryMesh);
		// Allow pawn to step inside when teleported while still blocking visibility traces
		EnclosureMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	if (SecondaryCoverMesh)
	{
		SecondaryCoverMesh->SetStaticMesh(ExtraMesh);
		SecondaryCoverMesh->SetRelativeLocation(FVector(55.0f, -45.0f, 0.0f));
	}
	if (SpotSignText)
	{
		const FString StateTag = IsValid(OccupantPawn) ? TEXT(" [OCCUPIED]") : TEXT(" [F: HIDE]");
		SpotSignText->SetText(FText::FromString(SpotDisplayName + StateTag));
	}
	if (IndicatorLight)
	{
		if (IsValid(OccupantPawn))
		{
			IndicatorLight->SetLightColor(bOccupiedWithoutWitness
				? FLinearColor(0.20f, 0.85f, 1.0f)
				: FLinearColor(1.0f, 0.25f, 0.15f));
		}
		else
		{
			IndicatorLight->SetLightColor(FLinearColor(0.35f, 0.95f, 0.65f));
		}
	}
}

void AMallHidingSpot::OnRep_OccupantState()
{
	RebuildHidingSpotVisuals();
}

bool AMallHidingSpot::HasHuntingGhostWitness(const APawn* EnteringPawn) const
{
	if (!EnteringPawn || !GetWorld())
	{
		return false;
	}
	const FVector PlayerEye = EnteringPawn->GetPawnViewLocation();
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Ghost = *It;
		if (!IsValid(Ghost) || !Ghost->ActorHasTag(TEXT("StoreGhostPrototype")))
		{
			continue;
		}
		if (!UMallGhostInteractionComponent::CanGhostHunt(Ghost))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Ghost->GetActorLocation(), EnteringPawn->GetActorLocation());
		if (DistSq > FMath::Square(850.0f))
		{
			continue;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MallHidingWitnessTrace), true, Ghost);
		Params.AddIgnoredActor(EnteringPawn);
		Params.AddIgnoredActor(this);
		FHitResult Hit;
		const FVector GhostEye = Ghost->GetActorLocation() + FVector(0.0f, 0.0f, 65.0f);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, GhostEye, PlayerEye, ECC_Visibility, Params))
		{
			return true;
		}
	}
	return false;
}

bool AMallHidingSpot::TryToggleHide(APlayerController* Player, FString& OutFeedback)
{
	if (!Player || !Player->HasAuthority() || !GetWorld())
	{
		return false;
	}
	APawn* Pawn = Player->GetPawn();
	if (!IsValid(Pawn))
	{
		return false;
	}

	// 1. If this player is already hidden inside this spot, exit cleanly!
	if (OccupantPawn == Pawn)
	{
		const FVector WorldExit = GetActorTransform().TransformPosition(ExitLocalOffset);
		OccupantPawn = nullptr;
		bOccupiedWithoutWitness = false;
		if (ACharacter* Char = Cast<ACharacter>(Pawn))
		{
			if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Walking);
			}
		}
		Pawn->SetActorLocation(WorldExit, false, nullptr, ETeleportType::TeleportPhysics);
		RebuildHidingSpotVisuals();
		ForceNetUpdate();
		OutFeedback = FString::Printf(TEXT("Stepped out of %s."), *SpotDisplayName);
		return true;
	}

	// 2. If occupied by another player, reject
	if (IsValid(OccupantPawn) && OccupantPawn != Pawn)
	{
		OutFeedback = FString::Printf(TEXT("%s is already occupied by another survivor!"), *SpotDisplayName);
		return false;
	}

	// 3. Check reach distance
	if (FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(320.0f))
	{
		OutFeedback = TEXT("Move closer to duck inside this hiding spot.");
		return false;
	}

	// 4. Clear any previous hiding spot occupied by this pawn
	if (AMallHidingSpot* PreviousSpot = FindSpotOccupiedBy(Pawn))
	{
		PreviousSpot->OccupantPawn = nullptr;
		PreviousSpot->bOccupiedWithoutWitness = false;
		PreviousSpot->RebuildHidingSpotVisuals();
		PreviousSpot->ForceNetUpdate();
	}

	const bool bSawEntry = HasHuntingGhostWitness(Pawn);
	OccupantPawn = Pawn;
	bOccupiedWithoutWitness = !bSawEntry;

	const FVector WorldHidePos = GetActorTransform().TransformPosition(HiddenLocalOffset);
	if (ACharacter* Char = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
		}
	}
	Pawn->SetActorLocation(WorldHidePos, false, nullptr, ETeleportType::TeleportPhysics);
	RebuildHidingSpotVisuals();
	ForceNetUpdate();

	if (bOccupiedWithoutWitness)
	{
		OutFeedback = FString::Printf(TEXT("Hidden inside %s! Store Ghosts cannot detect you here. Press F to step out."), *SpotDisplayName);
	}
	else
	{
		OutFeedback = FString::Printf(TEXT("WARNING: A hunting ghost saw you duck into %s! Break line-of-sight before hiding!"), *SpotDisplayName);
	}
	return true;
}

FString AMallHidingSpot::GetHUDPrompt(const APawn* ViewerPawn) const
{
	if (IsValid(OccupantPawn) && OccupantPawn == ViewerPawn)
	{
		return FString::Printf(TEXT("%s (CONCEALED) | F: Step out of hiding"), *SpotDisplayName);
	}
	if (IsValid(OccupantPawn))
	{
		return FString::Printf(TEXT("%s (Occupied)"), *SpotDisplayName);
	}
	return FString::Printf(TEXT("%s | F: Duck inside to hide from Ghosts"), *SpotDisplayName);
}

AMallHidingSpot* AMallHidingSpot::FindHidingSpotInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction)
{
	if (!Pawn || !Pawn->GetWorld())
	{
		return nullptr;
	}
	// If the pawn is already inside a hiding spot, return that spot immediately so pressing F always exits!
	if (AMallHidingSpot* CurrentSpot = FindSpotOccupiedBy(Pawn))
	{
		return CurrentSpot;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallHidingSpotFocus), true, Pawn);
	FHitResult Hit;
	if (Pawn->GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 350.0f, ECC_Visibility, Params))
	{
		if (AMallHidingSpot* Spot = Cast<AMallHidingSpot>(Hit.GetActor()))
		{
			if (FVector::DistSquared(Pawn->GetActorLocation(), Spot->GetActorLocation()) <= FMath::Square(320.0f))
			{
				return Spot;
			}
		}
	}
	return nullptr;
}

AMallHidingSpot* AMallHidingSpot::FindSpotOccupiedBy(const APawn* Pawn)
{
	if (!Pawn || !Pawn->GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<AMallHidingSpot> It(Pawn->GetWorld()); It; ++It)
	{
		if (It->OccupantPawn == Pawn)
		{
			return *It;
		}
	}
	return nullptr;
}

bool AMallHidingSpot::IsPlayerHiddenAndSafe(const AActor* Player)
{
	const APawn* Pawn = Cast<APawn>(Player);
	if (!Pawn)
	{
		return false;
	}
	if (const AMallHidingSpot* Spot = FindSpotOccupiedBy(Pawn))
	{
		return Spot->bOccupiedWithoutWitness;
	}
	return false;
}

bool AMallHidingSpot::IsPlayerConcealedFromGhost(const AActor* Player, const AActor* Ghost)
{
	return IsPlayerHiddenAndSafe(Player);
}
