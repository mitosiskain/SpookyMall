#include "Environment/MallStoreVendorNPC.h"
#include "Environment/MallEscalatorFuse.h"
#include "Environment/MallSpaAndFoodSystem.h"
#include "Equipment/MallCartFlashlight.h"
#include "Equipment/MallGhostEquipment.h"
#include "Ghost/MallBookInteractionComponent.h"
#include "Ghost/MallGhostWritingBook.h"
#include "Ghost/MallSpiritBox.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

namespace
{
	UStaticMesh* LoadVendorMesh(const TCHAR* PrimaryPath, const TCHAR* FallbackPath = nullptr)
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

AMallStoreVendorNPC::AMallStoreVendorNPC()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(SceneRoot);
	InteractionBounds->SetBoxExtent(FVector(95.0f, 85.0f, 110.0f));
	InteractionBounds->SetRelativeLocation(FVector(45.0f, 0.0f, 105.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	VendorBodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VendorBodyMesh"));
	VendorBodyMesh->SetupAttachment(SceneRoot);
	VendorBodyMesh->SetCollisionProfileName(TEXT("BlockAll"));

	CounterSampleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CounterSampleMesh"));
	CounterSampleMesh->SetupAttachment(SceneRoot);
	CounterSampleMesh->SetRelativeLocation(FVector(95.0f, 35.0f, 102.0f));
	CounterSampleMesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.35f));
	CounterSampleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	VendorNameText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("VendorNameText"));
	VendorNameText->SetupAttachment(SceneRoot);
	VendorNameText->SetHorizontalAlignment(EHTA_Center);
	VendorNameText->SetVerticalAlignment(EVRTA_TextCenter);
	VendorNameText->SetWorldSize(22.0f);
	VendorNameText->SetTextRenderColor(FColor(255, 225, 130));
	VendorNameText->SetRelativeLocation(FVector(25.0f, 0.0f, 218.0f));

	CatalogDisplayText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CatalogDisplayText"));
	CatalogDisplayText->SetupAttachment(SceneRoot);
	CatalogDisplayText->SetHorizontalAlignment(EHTA_Center);
	CatalogDisplayText->SetVerticalAlignment(EVRTA_TextCenter);
	CatalogDisplayText->SetWorldSize(16.5f);
	CatalogDisplayText->SetTextRenderColor(FColor(165, 245, 255));
	CatalogDisplayText->SetRelativeLocation(FVector(25.0f, 0.0f, 192.0f));

	CounterLampLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CounterLampLight"));
	CounterLampLight->SetupAttachment(SceneRoot);
	CounterLampLight->SetIntensityUnits(ELightUnits::Lumens);
	CounterLampLight->SetIntensity(95.0f);
	CounterLampLight->SetAttenuationRadius(320.0f);
	CounterLampLight->SetSourceRadius(18.0f);
	CounterLampLight->SetCastShadows(false);
	CounterLampLight->SetLightColor(FLinearColor(1.0f, 0.88f, 0.62f));
	CounterLampLight->SetRelativeLocation(FVector(65.0f, 0.0f, 165.0f));

	CatalogItems = {
		EMallVendorItemType::EMFReader,
		EMallVendorItemType::UVLight,
		EMallVendorItemType::Thermometer,
		EMallVendorItemType::SpiritBox,
		EMallVendorItemType::GhostWritingBook,
		EMallVendorItemType::PhotoCamera,
		EMallVendorItemType::MotionSensor,
		EMallVendorItemType::Flashlight,
		EMallVendorItemType::EscalatorFuse
	};
}

void AMallStoreVendorNPC::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildVendorVisuals();
}

void AMallStoreVendorNPC::BeginPlay()
{
	Super::BeginPlay();
	RebuildVendorVisuals();
}

void AMallStoreVendorNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallStoreVendorNPC, SelectedCatalogIndex);
	DOREPLIFETIME(AMallStoreVendorNPC, TotalItemsDispensed);
}

EMallVendorItemType AMallStoreVendorNPC::GetCurrentCatalogItem() const
{
	if (CatalogItems.Num() == 0)
	{
		return EMallVendorItemType::EMFReader;
	}
	const int32 SafeIdx = ((SelectedCatalogIndex % CatalogItems.Num()) + CatalogItems.Num()) % CatalogItems.Num();
	return CatalogItems[SafeIdx];
}

FString AMallStoreVendorNPC::GetItemDisplayName(EMallVendorItemType ItemType)
{
	switch (ItemType)
	{
	case EMallVendorItemType::EMFReader:
		return TEXT("5-LED EMF Reader");
	case EMallVendorItemType::UVLight:
		return TEXT("UV Blacklight Torch");
	case EMallVendorItemType::Thermometer:
		return TEXT("Digital IR Thermometer");
	case EMallVendorItemType::MotionSensor:
		return TEXT("IR Motion & Presence Sensor");
	case EMallVendorItemType::PhotoCamera:
		return TEXT("Paranormal Photo Camera");
	case EMallVendorItemType::SpiritBox:
		return TEXT("Frequency Spirit Box Radio");
	case EMallVendorItemType::GhostWritingBook:
		return TEXT("Occult Ghost Writing Ledger");
	case EMallVendorItemType::Flashlight:
		return TEXT("Tactical Belt Flashlight");
	case EMallVendorItemType::EscalatorFuse:
		return TEXT("Escalator Relay Fuse");
	case EMallVendorItemType::WarmRation:
	default:
		return TEXT("Hot Chongqing Broth & Ration");
	}
}

void AMallStoreVendorNPC::RebuildVendorVisuals()
{
	UStaticMesh* BodyMesh = bUseFemaleFigure
		? LoadVendorMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25257_FemaleMannequin.SM_Retail_25257_FemaleMannequin"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25271_TorsoFormStand.SM_Retail_25271_TorsoFormStand"))
		: LoadVendorMesh(
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25259_MaleMannequin.SM_Retail_25259_MaleMannequin"),
			TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25257_FemaleMannequin.SM_Retail_25257_FemaleMannequin"));

	if (VendorBodyMesh && BodyMesh)
	{
		VendorBodyMesh->SetStaticMesh(BodyMesh);
		VendorBodyMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	}

	const EMallVendorItemType CurrentItem = GetCurrentCatalogItem();
	if (VendorNameText)
	{
		VendorNameText->SetText(FText::FromString(FString::Printf(TEXT("%s (%s)"), *VendorName, *StoreTitle)));
	}
	if (CatalogDisplayText)
	{
		CatalogDisplayText->SetText(FText::FromString(
			FString::Printf(TEXT("[F: BUY / EQUIP] %s  |  [E: NEXT ITEM]"), *GetItemDisplayName(CurrentItem))));
	}
}

void AMallStoreVendorNPC::OnRep_VendorState()
{
	RebuildVendorVisuals();
}

FString AMallStoreVendorNPC::GetHUDPrompt() const
{
	const EMallVendorItemType CurrentItem = GetCurrentCatalogItem();
	const int32 TotalCatalog = FMath::Max(1, CatalogItems.Num());
	const int32 DisplayIdx = ((SelectedCatalogIndex % TotalCatalog) + TotalCatalog) % TotalCatalog + 1;
	return FString::Printf(
		TEXT("%s — %s [Item %d/%d: %s]\nF: Acquire & Equip Item | E: Cycle Store Catalog"),
		*VendorName,
		*StoreTitle,
		DisplayIdx,
		TotalCatalog,
		*GetItemDisplayName(CurrentItem));
}

bool AMallStoreVendorNPC::TryCycleCatalogItem(APlayerController* Player, FString& OutFeedback)
{
	if (!Player || !Player->HasAuthority() || CatalogItems.Num() == 0)
	{
		return false;
	}
	SelectedCatalogIndex = (SelectedCatalogIndex + 1) % CatalogItems.Num();
	RebuildVendorVisuals();
	ForceNetUpdate();
	OutFeedback = FString::Printf(
		TEXT("%s selected: %s (Press F to acquire, E for next item)."),
		*VendorName,
		*GetItemDisplayName(GetCurrentCatalogItem()));
	return true;
}

bool AMallStoreVendorNPC::TryPurchaseOrDispense(APlayerController* Player, FString& OutFeedback)
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
	if (FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(380.0f))
	{
		OutFeedback = TEXT("Step up to the store counter to trade with the vendor.");
		return false;
	}

	const EMallVendorItemType ItemType = GetCurrentCatalogItem();

	// Counter spawn location right in front of the vendor (local X = +105cm, Z = +104cm)
	const FVector SpawnLoc = GetActorTransform().TransformPosition(FVector(105.0f, FMath::FRandRange(-28.0f, 28.0f), 104.0f));
	const FRotator SpawnRot = GetActorRotation();
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	bool bPickedUpDirectly = false;
	AActor* DispensedActor = nullptr;
	UMallBookInteractionComponent* BookComp = Player->FindComponentByClass<UMallBookInteractionComponent>();

	switch (ItemType)
	{
	case EMallVendorItemType::WarmRation:
		if (AMallFoodRationItem* Item = GetWorld()->SpawnActor<AMallFoodRationItem>(AMallFoodRationItem::StaticClass(), SpawnLoc, SpawnRot, SpawnParams))
		{
			DispensedActor = Item;
			bPickedUpDirectly = Item->TryPickup(Player);
			if (bPickedUpDirectly && BookComp)
			{
				BookComp->AdoptCartItem(Item);
			}
			else if (!bPickedUpDirectly)
			{
				if (UMallPlayerVitalityComponent* Vit = UMallPlayerVitalityComponent::FindOrCreateForPawn(Pawn))
				{
					Vit->ApplyFoodConsumption(Item->MealBuffType, Item->HealPerServing, Item->StaminaPerServing, 35.0f, Item->BuffDurationSeconds, Item->CartBatteryBoostPerServing);
				}
			}
		}
		break;
	case EMallVendorItemType::EMFReader:
		if (AMallEMFReader* Item = GetWorld()->SpawnActor<AMallEMFReader>(AMallEMFReader::StaticClass(), SpawnLoc, SpawnRot, SpawnParams))
		{
			DispensedActor = Item;
			bPickedUpDirectly = Item->TryPickup(Player);
			if (bPickedUpDirectly)
			{
				if (BookComp) { BookComp->AdoptCartItem(Item); }
				Item->TrySetPowered(Player, true);
			}
		}
		break;
	case EMallVendorItemType::UVLight:
		if (AMallUVLight* Item = GetWorld()->SpawnActor<AMallUVLight>(AMallUVLight::StaticClass(), SpawnLoc, SpawnRot, SpawnParams))
		{
			DispensedActor = Item;
			bPickedUpDirectly = Item->TryPickup(Player);
			if (bPickedUpDirectly)
			{
				if (BookComp) { BookComp->AdoptCartItem(Item); }
				Item->TrySetPowered(Player, true);
			}
		}
		break;
	case EMallVendorItemType::Thermometer:
		if (AMallThermometer* Item = GetWorld()->SpawnActor<AMallThermometer>(AMallThermometer::StaticClass(), SpawnLoc, SpawnRot, SpawnParams))
		{
			DispensedActor = Item;
			bPickedUpDirectly = Item->TryPickup(Player);
			if (bPickedUpDirectly)
			{
				if (BookComp) { BookComp->AdoptCartItem(Item); }
				Item->TrySetPowered(Player, true);
			}
		}
		break;
	case EMallVendorItemType::MotionSensor:
		if (AMallMotionSensor* Item = GetWorld()->SpawnActor<AMallMotionSensor>(AMallMotionSensor::StaticClass(), SpawnLoc, SpawnRot, SpawnParams))
		{
			DispensedActor = Item;
			bPickedUpDirectly = Item->TryPickup(Player);
			if (bPickedUpDirectly)
			{
				if (BookComp) { BookComp->AdoptCartItem(Item); }
				Item->TrySetPowered(Player, true);
			}
		}
		break;
	case EMallVendorItemType::PhotoCamera:
		if (AMallPhotoCamera* Item = GetWorld()->SpawnActor<AMallPhotoCamera>(AMallPhotoCamera::StaticClass(), SpawnLoc, SpawnRot, SpawnParams))
		{
			DispensedActor = Item;
			bPickedUpDirectly = Item->TryPickup(Player);
			if (bPickedUpDirectly && BookComp)
			{
				BookComp->AdoptCartItem(Item);
			}
		}
		break;
	case EMallVendorItemType::SpiritBox:
		if (AMallSpiritBox* Item = GetWorld()->SpawnActor<AMallSpiritBox>(AMallSpiritBox::StaticClass(), SpawnLoc, SpawnRot, SpawnParams))
		{
			DispensedActor = Item;
			bPickedUpDirectly = Item->TryPickup(Player);
			if (bPickedUpDirectly)
			{
				if (BookComp) { BookComp->AdoptCartItem(Item); }
				Item->TrySetPowered(Player, true);
			}
		}
		break;
	case EMallVendorItemType::GhostWritingBook:
		if (AMallGhostWritingBook* Item = GetWorld()->SpawnActor<AMallGhostWritingBook>(AMallGhostWritingBook::StaticClass(), SpawnLoc, SpawnRot, SpawnParams))
		{
			DispensedActor = Item;
			bPickedUpDirectly = Item->TryPickup(Player);
			if (bPickedUpDirectly && BookComp)
			{
				BookComp->AdoptCartItem(Item);
			}
		}
		break;
	case EMallVendorItemType::Flashlight:
		if (AMallCartFlashlight* Item = GetWorld()->SpawnActor<AMallCartFlashlight>(AMallCartFlashlight::StaticClass(), SpawnLoc, SpawnRot, SpawnParams))
		{
			DispensedActor = Item;
			bPickedUpDirectly = Item->TryPickup(Player);
		}
		break;
	case EMallVendorItemType::EscalatorFuse:
		if (AMallEscalatorFuse* Item = GetWorld()->SpawnActor<AMallEscalatorFuse>(AMallEscalatorFuse::StaticClass(), SpawnLoc, SpawnRot, SpawnParams))
		{
			DispensedActor = Item;
			bPickedUpDirectly = Item->TryPickup(Player);
		}
		break;
	default:
		break;
	}

	if (!DispensedActor)
	{
		OutFeedback = TEXT("Vendor could not dispense that item right now.");
		return false;
	}

	++TotalItemsDispensed;
	ForceNetUpdate();

	const FString ItemLabel = GetItemDisplayName(ItemType);
	if (bPickedUpDirectly)
	{
		OutFeedback = FString::Printf(
			TEXT("Acquired %s from %s! Equipped in your hotbar (LMB/R: Use, E: Mode/Store on Cart, G: Drop)."),
			*ItemLabel,
			*VendorName);
	}
	else
	{
		OutFeedback = FString::Printf(
			TEXT("%s placed %s on the counter! (Free a hotbar slot or load items onto your Push Cart with E)."),
			*VendorName,
			*ItemLabel);
	}
	return true;
}

AMallStoreVendorNPC* AMallStoreVendorNPC::FindVendorInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction)
{
	if (!Pawn || !Pawn->GetWorld())
	{
		return nullptr;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallVendorFocus), true, Pawn);
	FHitResult Hit;
	if (Pawn->GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 380.0f, ECC_Visibility, Params))
	{
		if (AMallStoreVendorNPC* Vendor = Cast<AMallStoreVendorNPC>(Hit.GetActor()))
		{
			if (FVector::DistSquared(Pawn->GetActorLocation(), Vendor->GetActorLocation()) <= FMath::Square(380.0f))
			{
				return Vendor;
			}
		}
	}
	return nullptr;
}
