#include "Equipment/MallGhostEquipment.h"

#include "Environment/MallRetailStoreUnit.h"
#include "Environment/MallSpaAndFoodSystem.h"
#include "Equipment/MallCartProtectionSubsystem.h"
#include "Equipment/MallHotbarInventory.h"
#include "Equipment/MallPushCart.h"
#include "Ghost/MallBookInteractionComponent.h"
#include "Ghost/MallGhostDisturbanceProp.h"
#include "Ghost/MallGhostInteractionComponent.h"
#include "Ghost/MallGhostWritingBook.h"
#include "Ghost/MallSpiritBox.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace MallGhostEquipmentInternal
{
	constexpr double PickupReachCm = 280.0;

	UMaterialInterface* GetTintableDeviceMaterial()
	{
		if (UMaterialInterface* BlockoutMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Blockouts/Materials/LDA/M_LDA_Blockout.M_LDA_Blockout")))
		{
			return BlockoutMat;
		}
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	void ApplyMIDColor(UMaterialInstanceDynamic* MID, const FLinearColor& Color, float Roughness = 0.25f)
	{
		if (!MID)
		{
			return;
		}
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
		MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	}

	bool IsGhostSuppressedByCart(const AActor* Ghost)
	{
		if (!IsValid(Ghost) || !Ghost->GetWorld() || !Ghost->ActorHasTag(TEXT("MallCartBoundary")))
		{
			return false;
		}
		const UMallCartProtectionSubsystem* Protection = Ghost->GetWorld()->GetSubsystem<UMallCartProtectionSubsystem>();
		return Protection && Protection->IsStoreSuppressedByCart(const_cast<AActor*>(Ghost));
	}

	bool IsStoreGhostActor(const AActor* Candidate)
	{
		return IsValid(Candidate) && (Candidate->ActorHasTag(TEXT("StoreGhostPrototype"))
			|| Candidate->FindComponentByClass<UMallGhostInteractionComponent>() != nullptr);
	}
}

// ============================================================================
// AMallHandheldItem
// ============================================================================

AMallHandheldItem::AMallHandheldItem()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(20.0f);
	SetMinNetUpdateFrequency(5.0f);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	ItemRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ItemRoot"));
	SetRootComponent(ItemRoot);

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(ItemRoot);
	InteractionBounds->InitBoxExtent(FVector(12.0, 9.0, 9.0));
	InteractionBounds->SetRelativeLocation(FVector(0.0, 0.0, 9.0));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionBounds->SetGenerateOverlapEvents(false);
	InteractionBounds->SetCanEverAffectNavigation(false);

	DeviceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeviceMesh"));
	DeviceMesh->SetupAttachment(ItemRoot);
	DeviceMesh->SetRelativeLocation(FVector(0.0, 0.0, 7.0));
	DeviceMesh->SetRelativeScale3D(FVector(0.20, 0.14, 0.09));
	DeviceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DeviceMesh->SetCanEverAffectNavigation(false);

	GripMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GripMesh"));
	GripMesh->SetupAttachment(ItemRoot);
	GripMesh->SetRelativeLocation(FVector(-3.0, 0.0, 0.5));
	GripMesh->SetRelativeRotation(FRotator(-16.0, 0.0, 0.0));
	GripMesh->SetRelativeScale3D(FVector(0.045, 0.045, 0.11));
	GripMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GripMesh->SetCanEverAffectNavigation(false);

	ScreenBackdropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScreenBackdropMesh"));
	ScreenBackdropMesh->SetupAttachment(ItemRoot);
	ScreenBackdropMesh->SetRelativeLocation(FVector(-1.5, 0.0, 15.5));
	ScreenBackdropMesh->SetRelativeScale3D(FVector(0.015, 0.16, 0.075));
	ScreenBackdropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ScreenBackdropMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CubeMesh.Succeeded())
	{
		DeviceMesh->SetStaticMesh(CubeMesh.Object);
		ScreenBackdropMesh->SetStaticMesh(CubeMesh.Object);
	}
	if (CylinderMesh.Succeeded())
	{
		GripMesh->SetStaticMesh(CylinderMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UTexture2D> DefaultIcon(TEXT("/Game/Equipment/Flashlight/Textures/base_color.base_color"));
	if (DefaultIcon.Succeeded())
	{
		Icon = DefaultIcon.Object;
	}

	ReadoutText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ReadoutText"));
	ReadoutText->SetupAttachment(ItemRoot);
	ReadoutText->SetRelativeLocation(FVector(1.5, 0.0, 15.8));
	ReadoutText->SetHorizontalAlignment(EHTA_Center);
	ReadoutText->SetVerticalAlignment(EVRTA_TextCenter);
	ReadoutText->SetWorldSize(6.2f);
	ReadoutText->SetTextRenderColor(FColor(190, 235, 220));
	ReadoutText->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(ItemRoot);
	StatusLight->SetRelativeLocation(FVector(6.0, 0.0, 12.0));
	StatusLight->SetIntensity(0.0f);
	StatusLight->SetAttenuationRadius(140.0f);
	StatusLight->SetCastShadows(false);

	DeviceAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("DeviceAudio"));
	DeviceAudio->SetupAttachment(ItemRoot);
	DeviceAudio->bAutoActivate = false;
	DeviceAudio->bAllowSpatialization = true;
}

void AMallHandheldItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallHandheldItem, Carrier);
	DOREPLIFETIME(AMallHandheldItem, StoredOnCart);
	DOREPLIFETIME(AMallHandheldItem, CarriedHotbarSlot);
	DOREPLIFETIME(AMallHandheldItem, CarrierSelectedSlot);
	DOREPLIFETIME(AMallHandheldItem, bPowered);
}

void AMallHandheldItem::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		SafeWorldTransform = GetActorTransform();
		bHasSafeWorldTransform = true;
	}
	if (!SharedAttenuation)
	{
		SharedAttenuation = NewObject<USoundAttenuation>(this, TEXT("EquipmentAttenuation"));
		SharedAttenuation->Attenuation.bAttenuate = true;
		SharedAttenuation->Attenuation.bSpatialize = true;
		SharedAttenuation->Attenuation.FalloffDistance = 1200.0f;
		if (DeviceAudio)
		{
			DeviceAudio->AttenuationSettings = SharedAttenuation;
		}
	}
	ApplyCarrierPresentation();
	UpdateDeviceVisuals();
}

void AMallHandheldItem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* OldCarrier = LastHotbarCarrier.Get())
	{
		MallHotbarInventory::RemoveItem(OldCarrier, DisplayName, LastHotbarSlot);
		LastHotbarCarrier.Reset();
		LastHotbarSlot = INDEX_NONE;
	}
	Super::EndPlay(EndPlayReason);
}

APawn* AMallHandheldItem::GetCarrier() const
{
	return Carrier;
}

bool AMallHandheldItem::IsPowered() const
{
	return bPowered;
}

bool AMallHandheldItem::IsSelectedByCarrier() const
{
	if (!IsValid(Carrier))
	{
		return false;
	}
	if (!MallHotbarInventory::HasInventory(Carrier))
	{
		return true;
	}
	const int32 ActiveSlot = Carrier->IsLocallyControlled() ? MallHotbarInventory::GetSelectedSlot(Carrier) : CarrierSelectedSlot;
	return CarriedHotbarSlot != INDEX_NONE && ActiveSlot == CarriedHotbarSlot;
}

void AMallHandheldItem::SetCarrierSelectedSlot(int32 NewSelectedSlot)
{
	if (!HasAuthority() || CarrierSelectedSlot == NewSelectedSlot)
	{
		return;
	}
	CarrierSelectedSlot = NewSelectedSlot;
	ApplyCarrierPresentation();
	UpdateDeviceVisuals();
	ForceNetUpdate();
}

bool AMallHandheldItem::CanPickup(APlayerController* Player) const
{
	if (!HasAuthority() || !GetWorld() || !AMallPushCart::IsEligiblePlayer(Player) || Player->GetWorld() != GetWorld()
		|| Carrier || StoredOnCart)
	{
		return false;
	}
	APawn* Pawn = Player->GetPawn();
	if (!MallHotbarInventory::CanAcceptItem(Pawn, DisplayName))
	{
		return false;
	}
	for (TActorIterator<AMallHandheldItem> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->GetCarrier() == Pawn && It->GetClass() == GetClass())
		{
			return false;
		}
	}
	return true;
}

bool AMallHandheldItem::TryPickup(APlayerController* Player)
{
	if (!CanPickup(Player))
	{
		return false;
	}
	APawn* Pawn = Player->GetPawn();
	if (FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(MallGhostEquipmentInternal::PickupReachCm))
	{
		return false;
	}
	const int32 ReservedSlot = MallHotbarInventory::EnsureItemInSlot(Pawn, DisplayName, Icon, ItemValue, false, CarriedHotbarSlot, true);
	if (ReservedSlot == INDEX_NONE)
	{
		return false;
	}
	SafeWorldTransform = GetActorTransform();
	bHasSafeWorldTransform = true;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	StoredOnCart = nullptr;
	Carrier = Pawn;
	CarriedHotbarSlot = ReservedSlot;
	CarrierSelectedSlot = ReservedSlot;
	LastHotbarCarrier = Pawn;
	LastHotbarSlot = ReservedSlot;
	SetOwner(Player);
	FollowCarrier();
	ApplyCarrierPresentation();
	UpdateDeviceVisuals();
	ForceNetUpdate();
	return true;
}

bool AMallHandheldItem::TryPlace(APlayerController* Player, const FTransform& TargetTransform)
{
	if (!HasAuthority() || !Player || Player->GetPawn() != Carrier || StoredOnCart
		|| TargetTransform.ContainsNaN() || !TargetTransform.IsValid())
	{
		return false;
	}
	APawn* OldCarrier = Carrier;
	const int32 OldSlot = CarriedHotbarSlot;
	Carrier = nullptr;
	CarriedHotbarSlot = INDEX_NONE;
	CarrierSelectedSlot = INDEX_NONE;
	MallHotbarInventory::RemoveItem(OldCarrier, DisplayName, OldSlot);
	LastHotbarCarrier.Reset();
	LastHotbarSlot = INDEX_NONE;
	SetOwner(nullptr);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorTransform(TargetTransform, false, nullptr, ETeleportType::TeleportPhysics);
	SafeWorldTransform = TargetTransform;
	bHasSafeWorldTransform = true;
	ApplyCarrierPresentation();
	UpdateDeviceVisuals();
	ForceNetUpdate();
	return true;
}

bool AMallHandheldItem::CanPlayerControlDevice(APlayerController* Player) const
{
	if (!HasAuthority() || !AMallPushCart::IsEligiblePlayer(Player) || Player->GetWorld() != GetWorld() || StoredOnCart)
	{
		return false;
	}
	APawn* Pawn = Player->GetPawn();
	if (Carrier)
	{
		return Carrier == Pawn && IsSelectedByCarrier();
	}
	if (!bCanUseWhilePlaced)
	{
		return false;
	}
	return FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(MallGhostEquipmentInternal::PickupReachCm);
}

bool AMallHandheldItem::TrySetPowered(APlayerController* Player, bool bNewPowered)
{
	if (!CanPlayerControlDevice(Player))
	{
		return false;
	}
	if (bPowered != bNewPowered)
	{
		bPowered = bNewPowered;
		TickServerDetection(0.0f);
		UpdateDeviceVisuals();
		ForceNetUpdate();
	}
	return true;
}

bool AMallHandheldItem::TryPrimaryUse(APlayerController* Player)
{
	return TrySetPowered(Player, !bPowered);
}

bool AMallHandheldItem::TrySecondaryUse(APlayerController* Player)
{
	return CanPlayerControlDevice(Player);
}

FString AMallHandheldItem::GetHUDPrompt(bool bIsHeld) const
{
	return FString::Printf(TEXT("%s [%s]  |  LMB: power  |  E: mode  |  F: %s%s"),
		*DisplayName.ToString(),
		bPowered ? TEXT("ON") : TEXT("OFF"),
		bIsHeld ? TEXT("place / store on cart") : TEXT("pick up"),
		bIsHeld ? TEXT("  |  G: drop") : TEXT(""));
}

FString AMallHandheldItem::GetHUDDetail() const
{
	return bPowered ? TEXT("Device active.") : TEXT("Click LMB or press R to power on.");
}

FString AMallHandheldItem::GetPickupFeedback() const
{
	return FString::Printf(TEXT("%s picked up. LMB: use | E: mode | F: place | G: drop"), *DisplayName.ToString());
}

FString AMallHandheldItem::GetPlaceFeedback() const
{
	return FString::Printf(TEXT("%s placed. Aim at it and click LMB to use or press F to pick up."), *DisplayName.ToString());
}

FString AMallHandheldItem::GetPrimaryUseFeedback() const
{
	return FString::Printf(TEXT("%s %s."), *DisplayName.ToString(), bPowered ? TEXT("powered ON") : TEXT("powered OFF"));
}

FString AMallHandheldItem::GetSecondaryUseFeedback() const
{
	return FString::Printf(TEXT("%s mode updated."), *DisplayName.ToString());
}

APawn* AMallHandheldItem::GetCartItemCarrier_Implementation() const
{
	return Carrier;
}

AMallPushCart* AMallHandheldItem::GetCartStorage_Implementation() const
{
	return StoredOnCart;
}

bool AMallHandheldItem::TryStoreOnCart_Implementation(APlayerController* Player, AMallPushCart* Cart, const FTransform& SlotTransform)
{
	if (!HasAuthority() || !AMallPushCart::IsEligiblePlayer(Player) || Carrier != Player->GetPawn()
		|| !IsSelectedByCarrier() || StoredOnCart || !IsValid(Cart) || Cart->GetWorld() != GetWorld()
		|| !Cart->IsParked() || !Cart->CanPlayerReach(Player) || SlotTransform.ContainsNaN() || !SlotTransform.IsValid()
		|| FVector::DistSquared(SlotTransform.GetLocation(), Cart->GetActorLocation()) > FMath::Square(120.0))
	{
		return false;
	}
	APawn* OldCarrier = Carrier;
	const int32 OldSlot = CarriedHotbarSlot;
	Carrier = nullptr;
	CarriedHotbarSlot = INDEX_NONE;
	CarrierSelectedSlot = INDEX_NONE;
	StoredOnCart = Cart;
	bPowered = false;
	MallHotbarInventory::RemoveItem(OldCarrier, DisplayName, OldSlot);
	LastHotbarCarrier.Reset();
	LastHotbarSlot = INDEX_NONE;
	SetOwner(nullptr);
	SetActorTransform(SlotTransform, false, nullptr, ETeleportType::TeleportPhysics);
	AttachToActor(Cart, FAttachmentTransformRules::KeepWorldTransform);
	ApplyCarrierPresentation();
	UpdateDeviceVisuals();
	ForceNetUpdate();
	return true;
}

bool AMallHandheldItem::TryTakeFromCart_Implementation(APlayerController* Player, AMallPushCart* Cart)
{
	if (!HasAuthority() || !AMallPushCart::IsEligiblePlayer(Player) || StoredOnCart != Cart
		|| !IsValid(Cart) || !Cart->ContainsStoredItem(this) || !Cart->IsParked() || !Cart->CanPlayerReach(Player))
	{
		return false;
	}
	APawn* Pawn = Player->GetPawn();
	if (!MallHotbarInventory::CanAcceptItem(Pawn, DisplayName))
	{
		return false;
	}
	for (TActorIterator<AMallHandheldItem> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->GetCarrier() == Pawn && It->GetClass() == GetClass())
		{
			return false;
		}
	}
	UMallBookInteractionComponent* Interaction = Player->FindComponentByClass<UMallBookInteractionComponent>();
	if (Interaction && !Interaction->CanAcceptCartItem(this))
	{
		return false;
	}
	const int32 ReservedSlot = MallHotbarInventory::EnsureItemInSlot(Pawn, DisplayName, Icon, ItemValue, false, CarriedHotbarSlot, true);
	if (ReservedSlot == INDEX_NONE)
	{
		return false;
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	StoredOnCart = nullptr;
	Carrier = Pawn;
	CarriedHotbarSlot = ReservedSlot;
	CarrierSelectedSlot = ReservedSlot;
	LastHotbarCarrier = Pawn;
	LastHotbarSlot = ReservedSlot;
	SetOwner(Player);
	if (Interaction && !Interaction->AdoptCartItem(this))
	{
		MallHotbarInventory::RemoveItem(Pawn, DisplayName, ReservedSlot);
		LastHotbarCarrier.Reset();
		LastHotbarSlot = INDEX_NONE;
		Carrier = nullptr;
		CarriedHotbarSlot = INDEX_NONE;
		CarrierSelectedSlot = INDEX_NONE;
		StoredOnCart = Cart;
		SetOwner(nullptr);
		AttachToActor(Cart, FAttachmentTransformRules::KeepWorldTransform);
		return false;
	}
	FollowCarrier();
	ApplyCarrierPresentation();
	UpdateDeviceVisuals();
	ForceNetUpdate();
	return true;
}

void AMallHandheldItem::ReleaseFromCart_Implementation(AMallPushCart* Cart, const FTransform& FallbackWorldTransform)
{
	if (!HasAuthority() || !Cart || StoredOnCart != Cart)
	{
		return;
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	StoredOnCart = nullptr;
	if (bHasSafeWorldTransform)
	{
		SetActorTransform(SafeWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else if (!FallbackWorldTransform.ContainsNaN() && FallbackWorldTransform.IsValid())
	{
		SetActorTransform(FallbackWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
		SafeWorldTransform = FallbackWorldTransform;
		bHasSafeWorldTransform = true;
	}
	ApplyCarrierPresentation();
	UpdateDeviceVisuals();
	ForceNetUpdate();
}

void AMallHandheldItem::OnRep_ItemState()
{
	ApplyCarrierPresentation();
	UpdateDeviceVisuals();
}

void AMallHandheldItem::ApplyCarrierPresentation()
{
	if (InteractionBounds)
	{
		InteractionBounds->SetCollisionEnabled((!Carrier && !StoredOnCart) ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
	if (StoredOnCart && GetAttachParentActor() != StoredOnCart)
	{
		AttachToActor(StoredOnCart, FAttachmentTransformRules::KeepWorldTransform);
	}
	if (!StoredOnCart && GetAttachParentActor())
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	const bool bVisibleMesh = !IsValid(Carrier) || IsSelectedByCarrier();
	if (ItemRoot)
	{
		ItemRoot->SetHiddenInGame(!bVisibleMesh, true);
	}
	SyncHotbarInventory();
}

void AMallHandheldItem::SyncHotbarInventory()
{
	APawn* PreviousCarrier = LastHotbarCarrier.Get();
	if (PreviousCarrier && PreviousCarrier != Carrier)
	{
		MallHotbarInventory::RemoveItem(PreviousCarrier, DisplayName, LastHotbarSlot);
		LastHotbarCarrier.Reset();
		LastHotbarSlot = INDEX_NONE;
	}
	if (!IsValid(Carrier))
	{
		return;
	}
	if (!Carrier->HasAuthority() && !Carrier->IsLocallyControlled())
	{
		return;
	}
	const bool bSelectOnInitialPickup = (LastHotbarCarrier.Get() != Carrier);
	const int32 Slot = MallHotbarInventory::EnsureItemInSlot(
		Carrier,
		DisplayName,
		Icon,
		ItemValue,
		false,
		CarriedHotbarSlot,
		bSelectOnInitialPickup);
	if (Slot != INDEX_NONE)
	{
		LastHotbarCarrier = Carrier;
		LastHotbarSlot = Slot;
		if (HasAuthority() && CarriedHotbarSlot != Slot)
		{
			CarriedHotbarSlot = Slot;
			ForceNetUpdate();
		}
	}
}

void AMallHandheldItem::FollowCarrier()
{
	if (!IsValid(Carrier))
	{
		return;
	}
	FVector EyeOrigin = Carrier->GetPawnViewLocation();
	if (EyeOrigin.ContainsNaN() || EyeOrigin.IsZero())
	{
		EyeOrigin = Carrier->GetActorLocation() + FVector(0.0, 0.0, 64.0);
	}
	FRotator ControlRotation = Carrier->GetActorRotation();
	if (AController* Controller = Carrier->GetController())
	{
		ControlRotation = Controller->GetControlRotation();
	}
	const FRotator FirstPersonViewRot(FMath::ClampAngle(ControlRotation.Pitch, -55.0f, 55.0f), ControlRotation.Yaw, 0.0);

	// Anchor the held item in First-Person Viewmodel space directly in front of the player's eye camera so the 3D LEDs, digital screens, and gauges are clearly readable.
	const FVector TargetLocation = EyeOrigin + FirstPersonViewRot.RotateVector(CarriedViewOffset);
	const FRotator TargetRotation = (FirstPersonViewRot.Quaternion() * CarriedViewRotationOffset.Quaternion()).Rotator();
	SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

void AMallHandheldItem::GetActiveAimRay(FVector& OutOrigin, FVector& OutDirection) const
{
	if (IsValid(Carrier))
	{
		OutOrigin = Carrier->GetPawnViewLocation();
		if (APlayerController* PC = Cast<APlayerController>(Carrier->GetController()))
		{
			OutDirection = PC->GetControlRotation().Vector();
			return;
		}
		OutDirection = Carrier->GetControlRotation().Vector();
		return;
	}
	OutOrigin = GetActorLocation() + FVector(0.0, 0.0, 8.0);
	OutDirection = GetActorForwardVector();
}

void AMallHandheldItem::RecoverWorldPlacement()
{
	if (!HasAuthority())
	{
		return;
	}
	APawn* OldCarrier = Carrier;
	const int32 OldSlot = CarriedHotbarSlot;
	Carrier = nullptr;
	CarriedHotbarSlot = INDEX_NONE;
	CarrierSelectedSlot = INDEX_NONE;
	MallHotbarInventory::RemoveItem(OldCarrier, DisplayName, OldSlot);
	LastHotbarCarrier.Reset();
	LastHotbarSlot = INDEX_NONE;
	SetOwner(nullptr);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (bHasSafeWorldTransform)
	{
		SetActorTransform(SafeWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
	ApplyCarrierPresentation();
	UpdateDeviceVisuals();
	ForceNetUpdate();
}

void AMallHandheldItem::UpdateDeviceVisuals()
{
}

void AMallHandheldItem::TickServerDetection(float DeltaSeconds)
{
}

void AMallHandheldItem::TickClientEffects(float DeltaSeconds)
{
}

void AMallHandheldItem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && Carrier)
	{
		APlayerController* PC = Cast<APlayerController>(Carrier->GetController());
		if (!AMallPushCart::IsEligiblePlayer(PC) || PC->GetPawn() != Carrier)
		{
			RecoverWorldPlacement();
			return;
		}
		const int32 LiveSelected = MallHotbarInventory::GetSelectedSlot(Carrier);
		if (LiveSelected != INDEX_NONE && LiveSelected != CarrierSelectedSlot)
		{
			CarrierSelectedSlot = LiveSelected;
		}
	}

	if (Carrier)
	{
		FollowCarrier();
		ApplyCarrierPresentation();
	}

	if (HasAuthority())
	{
		TickServerDetection(DeltaSeconds);
	}

	TickClientEffects(DeltaSeconds);
}

// ============================================================================
// AMallEMFReader
// ============================================================================

AMallEMFReader::AMallEMFReader()
{
	DisplayName = FText::FromString(TEXT("EMF Reader"));
	ItemValue = 35.0f;
	PlacementHalfExtent = FVector(9.0, 6.0, 7.0);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<USoundBase> DefaultBeep(TEXT("/Engine/VREditor/Sounds/VR_click1.VR_click1"));
	if (DefaultBeep.Succeeded())
	{
		BeepSound = DefaultBeep.Object;
	}

	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FName CompName(*FString::Printf(TEXT("LED_%d"), Index + 1));
		UStaticMeshComponent* LED = CreateDefaultSubobject<UStaticMeshComponent>(CompName);
		LED->SetupAttachment(ItemRoot);
		const double OffsetY = -6.0 + Index * 3.0;
		LED->SetRelativeLocation(FVector(5.0, OffsetY, 12.2));
		LED->SetRelativeScale3D(FVector(0.022, 0.022, 0.022));
		LED->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LED->SetCanEverAffectNavigation(false);
		LED->SetCastShadow(false);
		if (SphereMesh.Succeeded())
		{
			LED->SetStaticMesh(SphereMesh.Object);
		}
		LEDMeshes.Add(LED);
	}
}

void AMallEMFReader::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallEMFReader, CurrentEMFLevel);
	DOREPLIFETIME(AMallEMFReader, PeakEMFLevel);
	DOREPLIFETIME(AMallEMFReader, bEMF5Detected);
	DOREPLIFETIME(AMallEMFReader, bAudioMuted);
}

int32 AMallEMFReader::EvaluateEMFLevel() const
{
	if (!bPowered || !GetWorld() || StoredOnCart)
	{
		return 0;
	}

	int32 BestLevel = 1;
	const FVector ReaderLocation = GetActorLocation();
	const AActor* ContextActor = IsValid(Carrier) ? static_cast<const AActor*>(Carrier) : static_cast<const AActor*>(this);
	const double Now = GetWorld()->GetTimeSeconds();

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Ghost = *It;
		if (!MallGhostEquipmentInternal::IsStoreGhostActor(Ghost) || MallGhostEquipmentInternal::IsGhostSuppressedByCart(Ghost))
		{
			continue;
		}

		const UMallGhostInteractionComponent* GhostComp = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
		const double Dist = FVector::Dist(ReaderLocation, Ghost->GetActorLocation());
		const bool bInGhostStore = UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, ContextActor);
		const bool bGhostSupportsEMF5 = !Ghost->ActorHasTag(TEXT("NoEMF5"));

		if (bInGhostStore || Dist <= MaxDetectionRangeCm)
		{
			BestLevel = FMath::Max(BestLevel, 2);
		}
		if (Dist <= MediumRangeCm)
		{
			BestLevel = FMath::Max(BestLevel, 3);
		}
		if (Dist <= CloseRangeCm)
		{
			BestLevel = FMath::Max(BestLevel, 4);
		}
		if (bGhostSupportsEMF5 && Dist <= EMF5RangeCm)
		{
			BestLevel = FMath::Max(BestLevel, 5);
		}

		if (GhostComp)
		{
			if ((GhostComp->GetGhostMode() == EMallGhostMode::VisibleWarning
				|| GhostComp->GetGhostMode() == EMallGhostMode::VisibleHunt)
				&& (bInGhostStore || Dist <= MaxDetectionRangeCm))
			{
				BestLevel = FMath::Max(BestLevel, 4);
				if (bGhostSupportsEMF5 && Dist <= CloseRangeCm)
				{
					BestLevel = FMath::Max(BestLevel, 5);
				}
			}

			const double TimeSinceDisturbance = Now - GhostComp->LastDisturbanceServerTime;
			if (GhostComp->DisturbanceCount > 0 && GhostComp->LastDisturbanceServerTime > 0.0
				&& TimeSinceDisturbance >= 0.0 && TimeSinceDisturbance <= DisturbanceMemorySeconds
				&& (bInGhostStore || Dist <= MaxDetectionRangeCm))
			{
				BestLevel = FMath::Max(BestLevel, 3);
				if (TimeSinceDisturbance <= 4.5 && Dist <= MediumRangeCm)
				{
					BestLevel = FMath::Max(BestLevel, bGhostSupportsEMF5 ? 5 : 4);
				}
			}
		}
	}

	for (TActorIterator<AMallGhostDisturbanceProp> It(GetWorld()); It; ++It)
	{
		const AMallGhostDisturbanceProp* Prop = *It;
		if (!IsValid(Prop) || Prop->DisturbanceCount <= 0)
		{
			continue;
		}
		const double Dist = FVector::Dist(ReaderLocation, Prop->GetActorLocation());
		if (Dist <= 380.0)
		{
			BestLevel = FMath::Max(BestLevel, Prop->bLastDisturbanceWasThrow ? 4 : 3);
			if (Dist <= 220.0 && (Prop->bLastDisturbanceWasThrow || Prop->DisturbanceCount >= 2))
			{
				BestLevel = FMath::Max(BestLevel, 5);
			}
		}
	}

	for (TActorIterator<AMallGhostWritingBook> It(GetWorld()); It; ++It)
	{
		if (It->IsWritten() && FVector::DistSquared(ReaderLocation, It->GetActorLocation()) <= FMath::Square(280.0))
		{
			BestLevel = FMath::Max(BestLevel, 3);
		}
	}

	for (TActorIterator<AMallSpiritBox> It(GetWorld()); It; ++It)
	{
		if (It->IsPowered() && It->ResponseCount > 0 && FVector::DistSquared(ReaderLocation, It->GetActorLocation()) <= FMath::Square(280.0))
		{
			BestLevel = FMath::Max(BestLevel, 3);
		}
	}

	return FMath::Clamp(BestLevel, 1, 5);
}

bool AMallEMFReader::TrySetPowered(APlayerController* Player, bool bNewPowered)
{
	if (!Super::TrySetPowered(Player, bNewPowered))
	{
		return false;
	}
	if (!bPowered)
	{
		CurrentEMFLevel = 0;
		UpdateDeviceVisuals();
		ForceNetUpdate();
	}
	return true;
}

bool AMallEMFReader::TrySecondaryUse(APlayerController* Player)
{
	if (!CanPlayerControlDevice(Player))
	{
		return false;
	}
	bAudioMuted = !bAudioMuted;
	UpdateDeviceVisuals();
	ForceNetUpdate();
	return true;
}

void AMallEMFReader::OnRep_EMFState()
{
	UpdateDeviceVisuals();
}

void AMallEMFReader::TickServerDetection(float DeltaSeconds)
{
	const int32 Evaluated = EvaluateEMFLevel();
	bool bDirty = false;
	if (CurrentEMFLevel != Evaluated)
	{
		CurrentEMFLevel = Evaluated;
		bDirty = true;
	}
	if (Evaluated > PeakEMFLevel)
	{
		PeakEMFLevel = Evaluated;
		bDirty = true;
	}
	if (Evaluated >= 5 && !bEMF5Detected)
	{
		bEMF5Detected = true;
		bDirty = true;
	}
	if (bDirty)
	{
		UpdateDeviceVisuals();
		ForceNetUpdate();
	}
}

void AMallEMFReader::TickClientEffects(float DeltaSeconds)
{
	if (!bPowered || bAudioMuted || CurrentEMFLevel <= 1 || !GetWorld() || !DeviceAudio || !BeepSound)
	{
		return;
	}
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (Now < NextBeepTime)
	{
		return;
	}
	const double Interval = (CurrentEMFLevel >= 5) ? 0.14
		: (CurrentEMFLevel == 4) ? 0.26
		: (CurrentEMFLevel == 3) ? 0.50
		: 0.90;
	NextBeepTime = Now + Interval;
	DeviceAudio->SetSound(BeepSound);
	DeviceAudio->SetPitchMultiplier(0.85f + CurrentEMFLevel * 0.14f);
	DeviceAudio->Play();
}

void AMallEMFReader::UpdateDeviceVisuals()
{
	static const FColor LevelColors[6] = {
		FColor(90, 100, 105),   // 0: OFF
		FColor(50, 225, 90),    // 1: Green
		FColor(150, 235, 60),   // 2: Yellow-Green
		FColor(245, 220, 45),   // 3: Yellow
		FColor(255, 140, 35),   // 4: Orange
		FColor(255, 35, 45)     // 5: Red (EMF-5 Evidence)
	};

	const int32 Clamped = FMath::Clamp(CurrentEMFLevel, 0, 5);
	const FColor ActiveColor = LevelColors[Clamped];

	if (ReadoutText)
	{
		if (!bPowered)
		{
			ReadoutText->SetText(FText::FromString(TEXT("EMF: OFF")));
			ReadoutText->SetTextRenderColor(LevelColors[0]);
		}
		else
		{
			const FString MuteTag = bAudioMuted ? TEXT(" [MUTE]") : TEXT("");
			const FString EMF5Tag = bEMF5Detected ? TEXT(" [EMF-5!]") : TEXT("");
			ReadoutText->SetText(FText::FromString(FString::Printf(TEXT("EMF LVL %d%s%s"), Clamped, *EMF5Tag, *MuteTag)));
			ReadoutText->SetTextRenderColor(ActiveColor);
		}
	}

	if (StatusLight)
	{
		StatusLight->SetLightColor(FLinearColor(ActiveColor));
		StatusLight->SetIntensity(bPowered ? (140.0f + Clamped * 165.0f) : 0.0f);
		StatusLight->SetAttenuationRadius(bPowered ? (150.0f + Clamped * 35.0f) : 90.0f);
	}

	if (LEDMaterials.Num() != LEDMeshes.Num())
	{
		LEDMaterials.SetNum(LEDMeshes.Num());
	}

	UMaterialInterface* BaseMat = MallGhostEquipmentInternal::GetTintableDeviceMaterial();
	for (int32 Index = 0; Index < LEDMeshes.Num(); ++Index)
	{
		if (UStaticMeshComponent* LED = LEDMeshes[Index])
		{
			const bool bLit = bPowered && (Index + 1) <= Clamped;
			LED->SetRelativeScale3D(bLit ? FVector(0.032, 0.032, 0.032) : FVector(0.016, 0.016, 0.016));
			if (!LEDMaterials[Index] && BaseMat)
			{
				LEDMaterials[Index] = UMaterialInstanceDynamic::Create(BaseMat, this);
				LED->SetMaterial(0, LEDMaterials[Index]);
			}
			const FLinearColor BulbColor = bLit
				? FLinearColor(LevelColors[FMath::Clamp(Index + 1, 1, 5)])
				: FLinearColor(0.06f, 0.07f, 0.08f, 1.0f);
			MallGhostEquipmentInternal::ApplyMIDColor(LEDMaterials[Index], BulbColor, bLit ? 0.08f : 0.75f);
		}
	}
}

FString AMallEMFReader::GetHUDPrompt(bool bIsHeld) const
{
	return FString::Printf(TEXT("EMF Reader [%s | LVL %d%s]  |  LMB: power  |  E: %s  |  F: %s%s"),
		bPowered ? TEXT("ON") : TEXT("OFF"),
		CurrentEMFLevel,
		bEMF5Detected ? TEXT(" | EMF-5") : TEXT(""),
		bAudioMuted ? TEXT("unmute") : TEXT("mute"),
		bIsHeld ? TEXT("place / store on cart") : TEXT("pick up"),
		bIsHeld ? TEXT("  |  G: drop") : TEXT(""));
}

FString AMallEMFReader::GetHUDDetail() const
{
	if (!bPowered)
	{
		return TEXT("Click LMB to power on. Reads electromagnetic spikes from Level 1 to Level 5.");
	}
	if (CurrentEMFLevel >= 5)
	{
		return TEXT("EMF LEVEL 5 CONFIRMED! Strong paranormal electromagnetic spike right here!");
	}
	if (CurrentEMFLevel == 4)
	{
		return TEXT("EMF Level 4: Intense paranormal surge or ghost manifestation very close!");
	}
	if (CurrentEMFLevel == 3)
	{
		return TEXT("EMF Level 3: Recent ghost disturbance or close entity activity.");
	}
	if (CurrentEMFLevel == 2)
	{
		return TEXT("EMF Level 2: Weak electromagnetic field detected in this store.");
	}
	return FString::Printf(TEXT("EMF Level 1 (Baseline) | Peak recorded: Level %d"), FMath::Max(1, PeakEMFLevel));
}

FString AMallEMFReader::GetPickupFeedback() const
{
	return TEXT("EMF Reader picked up. LMB: power | E: mute beeps | F: place | G: drop");
}

FString AMallEMFReader::GetPrimaryUseFeedback() const
{
	return bPowered
		? FString::Printf(TEXT("EMF Reader ON (Level %d). Walk into stores to track paranormal spikes."), CurrentEMFLevel)
		: TEXT("EMF Reader OFF.");
}

FString AMallEMFReader::GetSecondaryUseFeedback() const
{
	return bAudioMuted ? TEXT("EMF Reader audio muted (LEDs only).") : TEXT("EMF Reader audio unmuted.");
}

// ============================================================================
// AMallUVEvidence
// ============================================================================

AMallUVEvidence::AMallUVEvidence()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	EvidenceRoot = CreateDefaultSubobject<USceneComponent>(TEXT("EvidenceRoot"));
	SetRootComponent(EvidenceRoot);

	EvidenceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EvidenceMesh"));
	EvidenceMesh->SetupAttachment(EvidenceRoot);
	EvidenceMesh->SetRelativeScale3D(FVector(0.22, 0.12, 0.008));
	EvidenceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EvidenceMesh->SetCanEverAffectNavigation(false);
	EvidenceMesh->SetCastShadow(false);
	EvidenceMesh->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		EvidenceMesh->SetStaticMesh(CylinderMesh.Object);
	}

	EvidenceLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("EvidenceLabel"));
	EvidenceLabel->SetupAttachment(EvidenceRoot);
	EvidenceLabel->SetRelativeLocation(FVector(0.0, 0.0, 12.0));
	EvidenceLabel->SetHorizontalAlignment(EHTA_Center);
	EvidenceLabel->SetVerticalAlignment(EVRTA_TextCenter);
	EvidenceLabel->SetWorldSize(6.5f);
	EvidenceLabel->SetTextRenderColor(FColor(75, 255, 145));
	EvidenceLabel->SetText(FText::FromString(TEXT("UV FOOTPRINT")));
	EvidenceLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EvidenceLabel->SetHiddenInGame(true);

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(EvidenceRoot);
	GlowLight->SetRelativeLocation(FVector(0.0, 0.0, 8.0));
	GlowLight->SetLightColor(FLinearColor(0.15f, 1.0f, 0.45f));
	GlowLight->SetIntensity(0.0f);
	GlowLight->SetAttenuationRadius(110.0f);
	GlowLight->SetCastShadows(false);
}

void AMallUVEvidence::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallUVEvidence, bIlluminatedByUV);
	DOREPLIFETIME(AMallUVEvidence, bEverDiscovered);
}

void AMallUVEvidence::BeginPlay()
{
	Super::BeginPlay();
	RefreshVisibility();
}

void AMallUVEvidence::NotifyUVIlluminated()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}
	IlluminatedUntilServerTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.25f, AfterglowDurationSeconds);
	if (!bIlluminatedByUV || !bEverDiscovered)
	{
		bIlluminatedByUV = true;
		bEverDiscovered = true;
		RefreshVisibility();
		ForceNetUpdate();
	}
}

void AMallUVEvidence::OnRep_UVState()
{
	RefreshVisibility();
}

void AMallUVEvidence::RefreshVisibility()
{
	if (EvidenceLabel)
	{
		EvidenceLabel->SetText(FText::FromString(PrintTitle));
		EvidenceLabel->SetHiddenInGame(!bIlluminatedByUV);
	}
	if (EvidenceMesh)
	{
		EvidenceMesh->SetHiddenInGame(!bIlluminatedByUV);
	}
	if (GlowLight)
	{
		GlowLight->SetIntensity(bIlluminatedByUV ? 420.0f : 0.0f);
	}
}

void AMallUVEvidence::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && bIlluminatedByUV && GetWorld() && GetWorld()->GetTimeSeconds() > IlluminatedUntilServerTime)
	{
		bIlluminatedByUV = false;
		RefreshVisibility();
		ForceNetUpdate();
	}
}

// ============================================================================
// AMallUVLight
// ============================================================================

AMallUVLight::AMallUVLight()
{
	DisplayName = FText::FromString(TEXT("UV Blacklight"));
	ItemValue = 30.0f;
	PlacementHalfExtent = FVector(11.0, 6.0, 6.0);
	CarriedViewOffset = FVector(38.0, 16.0, -13.0);
	CarriedViewRotationOffset = FRotator(0.0, 0.0, 0.0);

	if (ReadoutText)
	{
		ReadoutText->SetRelativeLocation(FVector(-9.5, 0.0, 15.8));
		ReadoutText->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
	}
	if (ScreenBackdropMesh)
	{
		ScreenBackdropMesh->SetRelativeLocation(FVector(-8.0, 0.0, 15.5));
	}

	UVSpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("UVSpotLight"));
	UVSpotLight->SetupAttachment(ItemRoot);
	UVSpotLight->SetRelativeLocation(FVector(10.0, 0.0, 6.0));
	UVSpotLight->SetLightColor(FLinearColor(0.42f, 0.08f, 1.0f));
	UVSpotLight->SetIntensity(0.0f);
	UVSpotLight->SetAttenuationRadius(FocusedRangeCm);
	UVSpotLight->SetInnerConeAngle(12.0f);
	UVSpotLight->SetOuterConeAngle(FocusedHalfAngleDegrees);
	UVSpotLight->SetCastShadows(false);
}

void AMallUVLight::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallUVLight, bWideFloodMode);
	DOREPLIFETIME(AMallUVLight, IlluminatedPrintCount);
	DOREPLIFETIME(AMallUVLight, TotalPrintsRevealed);
}

bool AMallUVLight::TrySecondaryUse(APlayerController* Player)
{
	if (!CanPlayerControlDevice(Player))
	{
		return false;
	}
	bWideFloodMode = !bWideFloodMode;
	UpdateDeviceVisuals();
	ForceNetUpdate();
	return true;
}

void AMallUVLight::OnRep_UVLightState()
{
	UpdateDeviceVisuals();
}

void AMallUVLight::UpdateDeviceVisuals()
{
	const bool bEmitBeam = bPowered && !StoredOnCart && (!Carrier || IsSelectedByCarrier());
	const float Range = GetActiveBeamRangeCm();
	const float Cone = GetActiveBeamHalfAngleDegrees();

	if (UVSpotLight)
	{
		UVSpotLight->SetAttenuationRadius(Range);
		UVSpotLight->SetInnerConeAngle(Cone * 0.55f);
		UVSpotLight->SetOuterConeAngle(Cone);
		UVSpotLight->SetIntensity(bEmitBeam ? (bWideFloodMode ? 3200.0f : 5000.0f) : 0.0f);
	}
	if (StatusLight)
	{
		StatusLight->SetLightColor(IlluminatedPrintCount > 0 ? FLinearColor(0.2f, 1.0f, 0.45f) : FLinearColor(0.5f, 0.15f, 1.0f));
		StatusLight->SetIntensity(bEmitBeam ? 180.0f : 0.0f);
	}
	if (ReadoutText)
	{
		if (!bPowered)
		{
			ReadoutText->SetText(FText::FromString(TEXT("UV: OFF")));
			ReadoutText->SetTextRenderColor(FColor(120, 110, 140));
		}
		else
		{
			ReadoutText->SetText(FText::FromString(FString::Printf(TEXT("UV %s [%d LIT]"),
				bWideFloodMode ? TEXT("FLOOD") : TEXT("FOCUS"),
				IlluminatedPrintCount)));
			ReadoutText->SetTextRenderColor(IlluminatedPrintCount > 0 ? FColor(85, 255, 150) : FColor(185, 120, 255));
		}
	}
}

void AMallUVLight::TrySpawnDynamicGhostPrints()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (Now < NextFootprintSpawnTime)
	{
		return;
	}

	for (TActorIterator<AActor> GhostIt(World); GhostIt; ++GhostIt)
	{
		AActor* Ghost = *GhostIt;
		if (!MallGhostEquipmentInternal::IsStoreGhostActor(Ghost) || MallGhostEquipmentInternal::IsGhostSuppressedByCart(Ghost))
		{
			continue;
		}
		const UMallGhostInteractionComponent* GhostComp = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
		const bool bNewDisturbance = GhostComp && GhostComp->DisturbanceCount > LastObservedGhostDisturbanceCount;
		const bool bCloseToUV = FVector::DistSquared(GetActorLocation(), Ghost->GetActorLocation()) <= FMath::Square(1400.0);
		if (!bNewDisturbance && !bCloseToUV)
		{
			continue;
		}

		int32 ExistingStorePrints = 0;
		for (TActorIterator<AMallUVEvidence> PrintIt(World); PrintIt; ++PrintIt)
		{
			if (UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, *PrintIt))
			{
				++ExistingStorePrints;
			}
		}
		if (ExistingStorePrints >= MaxDynamicPrintsPerStore)
		{
			if (GhostComp) { LastObservedGhostDisturbanceCount = GhostComp->DisturbanceCount; }
			continue;
		}

		FCollisionQueryParams Params(SCENE_QUERY_STAT(MallUVPrintSpawn), false, Ghost);
		FHitResult FloorHit;
		const FVector GhostLoc = Ghost->GetActorLocation();
		if (World->LineTraceSingleByChannel(FloorHit, GhostLoc + FVector(0.0, 0.0, 20.0), GhostLoc - FVector(0.0, 0.0, 160.0), ECC_WorldStatic, Params)
			&& FloorHit.ImpactNormal.Z >= 0.85f)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FRotator PrintRot(0.0, Ghost->GetActorRotation().Yaw, 0.0);
			if (AMallUVEvidence* Spawned = World->SpawnActor<AMallUVEvidence>(FloorHit.ImpactPoint + FVector(0.0, 0.0, 1.0), PrintRot, SpawnParams))
			{
				Spawned->PrintTitle = bNewDisturbance ? TEXT("UV Handprint (Disturbance)") : TEXT("UV Ghost Footprint");
				NextFootprintSpawnTime = Now + 4.0;
			}
		}
		if (GhostComp)
		{
			LastObservedGhostDisturbanceCount = GhostComp->DisturbanceCount;
		}
	}
}

void AMallUVLight::TickServerDetection(float DeltaSeconds)
{
	if (!bPowered || StoredOnCart || !GetWorld() || (Carrier && !IsSelectedByCarrier()))
	{
		if (IlluminatedPrintCount != 0)
		{
			IlluminatedPrintCount = 0;
			UpdateDeviceVisuals();
			ForceNetUpdate();
		}
		return;
	}

	TrySpawnDynamicGhostPrints();

	FVector BeamOrigin, BeamDir;
	GetActiveAimRay(BeamOrigin, BeamDir);
	const float MaxRange = GetActiveBeamRangeCm();
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(GetActiveBeamHalfAngleDegrees()));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallUVBeamCheck), true, this);
	if (Carrier)
	{
		Params.AddIgnoredActor(Carrier);
	}

	int32 LitNow = 0;
	bool bRevealedNew = false;

	for (TActorIterator<AMallUVEvidence> It(GetWorld()); It; ++It)
	{
		AMallUVEvidence* Evidence = *It;
		if (!IsValid(Evidence))
		{
			continue;
		}
		const FVector ToTarget = Evidence->GetActorLocation() - BeamOrigin;
		const double Dist = ToTarget.Size();
		if (Dist > MaxRange || Dist <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const double Dot = FVector::DotProduct(ToTarget / Dist, BeamDir);
		if (Dot < CosHalfAngle)
		{
			continue;
		}
		FHitResult Hit;
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			Hit,
			BeamOrigin,
			Evidence->GetActorLocation() + FVector(0.0, 0.0, 6.0),
			ECC_Visibility,
			Params) && Hit.GetActor() != Evidence;
		if (!bBlocked)
		{
			Evidence->NotifyUVIlluminated();
			++LitNow;
			const TWeakObjectPtr<AMallUVEvidence> Key(Evidence);
			if (!DiscoveredSet.Contains(Key))
			{
				DiscoveredSet.Add(Key);
				TotalPrintsRevealed = DiscoveredSet.Num();
				bRevealedNew = true;
			}
		}
	}

	if (IlluminatedPrintCount != LitNow || bRevealedNew)
	{
		IlluminatedPrintCount = LitNow;
		UpdateDeviceVisuals();
		ForceNetUpdate();
	}
}

FString AMallUVLight::GetHUDPrompt(bool bIsHeld) const
{
	return FString::Printf(TEXT("UV Blacklight [%s | %s]  |  LMB: beam  |  E: optics  |  F: %s%s"),
		bPowered ? TEXT("ON") : TEXT("OFF"),
		bWideFloodMode ? TEXT("Wide Flood") : TEXT("Focused Beam"),
		bIsHeld ? TEXT("place / store on cart") : TEXT("pick up"),
		bIsHeld ? TEXT("  |  G: drop") : TEXT(""));
}

FString AMallUVLight::GetHUDDetail() const
{
	if (!bPowered)
	{
		return TEXT("Click LMB to shine UV beam. Reveals hidden fluorescent ghost footprints & handprints.");
	}
	if (IlluminatedPrintCount > 0)
	{
		return FString::Printf(TEXT("UV EVIDENCE REVEALED! (%d fluorescent print%s glowing in beam | %d total found)"),
			IlluminatedPrintCount,
			IlluminatedPrintCount == 1 ? TEXT("") : TEXT("s"),
			TotalPrintsRevealed);
	}
	return FString::Printf(TEXT("Scanning with %s (%.0fm range). Total UV prints discovered: %d"),
		bWideFloodMode ? TEXT("Wide Flood UV") : TEXT("Focused UV Beam"),
		GetActiveBeamRangeCm() * 0.01f,
		TotalPrintsRevealed);
}

FString AMallUVLight::GetPickupFeedback() const
{
	return TEXT("UV Blacklight picked up. LMB: toggle UV beam | E: focus/flood | F: place | G: drop");
}

FString AMallUVLight::GetPrimaryUseFeedback() const
{
	return bPowered
		? TEXT("UV Blacklight ON. Sweep floors and disturbed store counters for fluorescent prints.")
		: TEXT("UV Blacklight OFF.");
}

FString AMallUVLight::GetSecondaryUseFeedback() const
{
	return bWideFloodMode
		? TEXT("UV Blacklight switched to Wide Flood optics (40 deg cone).")
		: TEXT("UV Blacklight switched to Focused Long-Range optics (22 deg cone).");
}

// ============================================================================
// AMallThermometer
// ============================================================================

AMallThermometer::AMallThermometer()
{
	DisplayName = FText::FromString(TEXT("Thermal Scanner"));
	ItemValue = 35.0f;
	PlacementHalfExtent = FVector(9.0, 6.0, 7.0);

	ThermalBarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThermalBarMesh"));
	ThermalBarMesh->SetupAttachment(ItemRoot);
	ThermalBarMesh->SetRelativeLocation(FVector(4.5, 0.0, 12.2));
	ThermalBarMesh->SetRelativeScale3D(FVector(0.025, 0.12, 0.022));
	ThermalBarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ThermalBarMesh->SetCanEverAffectNavigation(false);
	ThermalBarMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ThermalBarMesh->SetStaticMesh(CubeMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<USoundBase> DefaultAlert(TEXT("/Engine/VREditor/Sounds/VR_click1.VR_click1"));
	if (DefaultAlert.Succeeded())
	{
		FreezeAlertSound = DefaultAlert.Object;
	}
}

void AMallThermometer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallThermometer, CurrentTemperatureCelsius);
	DOREPLIFETIME(AMallThermometer, LowestRecordedCelsius);
	DOREPLIFETIME(AMallThermometer, bFreezingDetected);
	DOREPLIFETIME(AMallThermometer, bUseFahrenheit);
}

float AMallThermometer::EvaluateTemperatureCelsius() const
{
	if (!GetWorld())
	{
		return BaselineTemperatureCelsius;
	}

	float TargetCelsius = BaselineTemperatureCelsius;
	const FVector DeviceLocation = GetActorLocation();
	FVector AimOrigin, AimDirection;
	GetActiveAimRay(AimOrigin, AimDirection);
	const AActor* ContextActor = IsValid(Carrier) ? static_cast<const AActor*>(Carrier) : static_cast<const AActor*>(this);
	const double Now = GetWorld()->GetTimeSeconds();

	if (const APawn* CarrierPawn = Cast<APawn>(ContextActor))
	{
		if (AMallSpaHealingZone::FindSpaZoneContainingPawn(CarrierPawn) != nullptr)
		{
			TargetCelsius = 40.5f;
		}
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Ghost = *It;
		if (!MallGhostEquipmentInternal::IsStoreGhostActor(Ghost) || MallGhostEquipmentInternal::IsGhostSuppressedByCart(Ghost))
		{
			continue;
		}

		const UMallGhostInteractionComponent* GhostComp = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
		const bool bGhostSupportsFreezing = !Ghost->ActorHasTag(TEXT("NoFreezing"));
		const bool bInGhostStore = UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, ContextActor);

		const FVector ToGhost = Ghost->GetActorLocation() - AimOrigin;
		const double DistToGhost = FVector::Dist(DeviceLocation, Ghost->GetActorLocation());
		const double AimDist = ToGhost.Size();
		const bool bAimedAtGhost = AimDist <= IRScanRangeCm && AimDist > KINDA_SMALL_NUMBER
			&& FVector::DotProduct(ToGhost / AimDist, AimDirection) >= FMath::Cos(FMath::DegreesToRadians(24.0f));

		if (bInGhostStore || DistToGhost <= IRScanRangeCm)
		{
			TargetCelsius = FMath::Min(TargetCelsius, HauntedStoreAmbientCelsius);
		}

		if (DistToGhost <= FreezingProximityCm || bAimedAtGhost)
		{
			if (bGhostSupportsFreezing)
			{
				const float Alpha = FMath::Clamp(static_cast<float>(DistToGhost / FMath::Max(100.0f, FreezingProximityCm)), 0.0f, 1.0f);
				const float SubZero = FMath::Lerp(FreezingGhostTemperatureCelsius, -1.2f, Alpha);
				TargetCelsius = FMath::Min(TargetCelsius, SubZero);
			}
			else
			{
				TargetCelsius = FMath::Min(TargetCelsius, 1.5f);
			}
		}

		if (GhostComp && GhostComp->DisturbanceCount > 0 && GhostComp->LastDisturbanceServerTime > 0.0
			&& (Now - GhostComp->LastDisturbanceServerTime) <= 10.0 && (bInGhostStore || DistToGhost <= IRScanRangeCm))
		{
			TargetCelsius = FMath::Min(TargetCelsius, bGhostSupportsFreezing ? -4.5f : 2.0f);
		}
	}

	return FMath::RoundToFloat(TargetCelsius * 10.0f) / 10.0f;
}

FString AMallThermometer::FormatTemperature(float TempCelsius) const
{
	if (bUseFahrenheit)
	{
		const float TempF = TempCelsius * 1.8f + 32.0f;
		return FString::Printf(TEXT("%.1f F"), TempF);
	}
	return FString::Printf(TEXT("%.1f C"), TempCelsius);
}

bool AMallThermometer::TrySetPowered(APlayerController* Player, bool bNewPowered)
{
	return Super::TrySetPowered(Player, bNewPowered);
}

bool AMallThermometer::TrySecondaryUse(APlayerController* Player)
{
	if (!CanPlayerControlDevice(Player))
	{
		return false;
	}
	bUseFahrenheit = !bUseFahrenheit;
	UpdateDeviceVisuals();
	ForceNetUpdate();
	return true;
}

void AMallThermometer::OnRep_ThermalState()
{
	UpdateDeviceVisuals();
}

void AMallThermometer::TickServerDetection(float DeltaSeconds)
{
	if (!bPowered || StoredOnCart)
	{
		return;
	}
	const float Evaluated = EvaluateTemperatureCelsius();
	bool bDirty = false;
	if (!FMath::IsNearlyEqual(CurrentTemperatureCelsius, Evaluated, 0.05f))
	{
		CurrentTemperatureCelsius = Evaluated;
		bDirty = true;
	}
	if (Evaluated < LowestRecordedCelsius)
	{
		LowestRecordedCelsius = Evaluated;
		bDirty = true;
	}
	if (Evaluated <= 0.0f && !bFreezingDetected)
	{
		bFreezingDetected = true;
		bDirty = true;
		if (DeviceAudio && FreezeAlertSound && GetNetMode() != NM_DedicatedServer)
		{
			DeviceAudio->SetSound(FreezeAlertSound);
			DeviceAudio->SetPitchMultiplier(1.55f);
			DeviceAudio->Play();
		}
	}
	if (bDirty)
	{
		UpdateDeviceVisuals();
		ForceNetUpdate();
	}
}

void AMallThermometer::UpdateDeviceVisuals()
{
	const bool bFreezingNow = bPowered && CurrentTemperatureCelsius <= 0.0f;
	const bool bColdStore = bPowered && CurrentTemperatureCelsius <= 10.0f;
	const bool bWarmSpa = bPowered && CurrentTemperatureCelsius >= 32.0f;

	const FColor DisplayColor = !bPowered ? FColor(110, 120, 125)
		: bFreezingNow ? FColor(70, 235, 255)
		: bColdStore ? FColor(125, 200, 255)
		: bWarmSpa ? FColor(255, 165, 195)
		: FColor(175, 240, 185);

	if (ReadoutText)
	{
		if (!bPowered)
		{
			ReadoutText->SetText(FText::FromString(TEXT("TEMP: OFF")));
		}
		else
		{
			const FString FreezeTag = bFreezingNow ? TEXT(" [FREEZING!]")
				: (bColdStore ? TEXT(" [COLD]") : (bWarmSpa ? TEXT(" [SPA WARM]") : TEXT("")));
			ReadoutText->SetText(FText::FromString(FString::Printf(TEXT("%s%s"), *FormatTemperature(CurrentTemperatureCelsius), *FreezeTag)));
		}
		ReadoutText->SetTextRenderColor(DisplayColor);
	}

	if (ThermalBarMesh)
	{
		const float BarWidth = !bPowered ? 0.03f
			: bFreezingNow ? 0.14f
			: bColdStore ? 0.10f
			: bWarmSpa ? 0.13f
			: 0.07f;
		ThermalBarMesh->SetRelativeScale3D(FVector(0.028, BarWidth, 0.024));
		if (!ThermalBarMID)
		{
			if (UMaterialInterface* BaseMat = MallGhostEquipmentInternal::GetTintableDeviceMaterial())
			{
				ThermalBarMID = UMaterialInstanceDynamic::Create(BaseMat, this);
				ThermalBarMesh->SetMaterial(0, ThermalBarMID);
			}
		}
		MallGhostEquipmentInternal::ApplyMIDColor(ThermalBarMID, FLinearColor(DisplayColor), bPowered ? 0.10f : 0.70f);
	}

	if (StatusLight)
	{
		StatusLight->SetLightColor(FLinearColor(DisplayColor));
		StatusLight->SetIntensity(bPowered ? (bFreezingNow ? 420.0f : 180.0f) : 0.0f);
	}
}

FString AMallThermometer::GetHUDPrompt(bool bIsHeld) const
{
	return FString::Printf(TEXT("Thermal Scanner [%s | %s]  |  LMB: power  |  E: C/F units  |  F: %s%s"),
		bPowered ? *FormatTemperature(CurrentTemperatureCelsius) : TEXT("OFF"),
		bFreezingDetected ? TEXT("FREEZING CONFIRMED") : TEXT("Scanning"),
		bIsHeld ? TEXT("place / store on cart") : TEXT("pick up"),
		bIsHeld ? TEXT("  |  G: drop") : TEXT(""));
}

FString AMallThermometer::GetHUDDetail() const
{
	if (!bPowered)
	{
		return TEXT("Click LMB to power on IR thermal scanner. Locates cold stores and sub-zero Freezing Temps.");
	}
	if (CurrentTemperatureCelsius <= 0.0f)
	{
		return FString::Printf(TEXT("FREEZING TEMPERATURES (%s)! Sub-zero paranormal cold spot confirmed!"), *FormatTemperature(CurrentTemperatureCelsius));
	}
	if (CurrentTemperatureCelsius <= 10.0f)
	{
		return FString::Printf(TEXT("Cold Store Detected (%s) | Lowest recorded: %s"),
			*FormatTemperature(CurrentTemperatureCelsius),
			*FormatTemperature(LowestRecordedCelsius));
	}
	return FString::Printf(TEXT("Normal Ambient (%s) | Lowest recorded: %s"),
		*FormatTemperature(CurrentTemperatureCelsius),
		*FormatTemperature(LowestRecordedCelsius));
}

FString AMallThermometer::GetPickupFeedback() const
{
	return TEXT("Thermal Scanner picked up. LMB: power | E: toggle C/F | F: place | G: drop");
}

FString AMallThermometer::GetPrimaryUseFeedback() const
{
	return bPowered
		? FString::Printf(TEXT("Thermal Scanner ON (%s)."), *FormatTemperature(CurrentTemperatureCelsius))
		: TEXT("Thermal Scanner OFF.");
}

FString AMallThermometer::GetSecondaryUseFeedback() const
{
	return bUseFahrenheit ? TEXT("Thermal Scanner switched to Fahrenheit (F).") : TEXT("Thermal Scanner switched to Celsius (C).");
}

// ============================================================================
// AMallMotionSensor
// ============================================================================

AMallMotionSensor::AMallMotionSensor()
{
	DisplayName = FText::FromString(TEXT("Motion Sensor"));
	ItemValue = 40.0f;
	PlacementHalfExtent = FVector(10.0, 8.0, 9.0);

	static ConstructorHelpers::FObjectFinder<USoundBase> DefaultChime(TEXT("/Engine/VREditor/Sounds/VR_click1.VR_click1"));
	if (DefaultChime.Succeeded())
	{
		AlertChimeSound = DefaultChime.Object;
	}
}

void AMallMotionSensor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallMotionSensor, bMotionDetected);
	DOREPLIFETIME(AMallMotionSensor, TriggerCount);
	DOREPLIFETIME(AMallMotionSensor, bSilentAlarm);
}

bool AMallMotionSensor::EvaluateGhostMotion() const
{
	if (!bPowered || StoredOnCart || !GetWorld())
	{
		return false;
	}

	FVector SensorOrigin, SensorForward;
	GetActiveAimRay(SensorOrigin, SensorForward);
	const float CosHalfCone = FMath::Cos(FMath::DegreesToRadians(DetectionHalfAngleDegrees));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallMotionSensorTrace), true, this);
	if (Carrier)
	{
		Params.AddIgnoredActor(Carrier);
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Ghost = *It;
		if (!MallGhostEquipmentInternal::IsStoreGhostActor(Ghost) || MallGhostEquipmentInternal::IsGhostSuppressedByCart(Ghost))
		{
			continue;
		}
		const FVector GhostCenter = Ghost->GetActorLocation() + FVector(0.0, 0.0, 40.0);
		const FVector ToGhost = GhostCenter - SensorOrigin;
		const double Dist = ToGhost.Size();
		if (Dist > DetectionRangeCm || Dist <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const bool bInForwardCone = FVector::DotProduct(ToGhost / Dist, SensorForward) >= CosHalfCone;
		const bool bInOmniRadius = Dist <= OmniProximityRadiusCm;
		if (!bInForwardCone && !bInOmniRadius)
		{
			continue;
		}

		FHitResult Hit;
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, SensorOrigin, GhostCenter, ECC_Visibility, Params)
			&& Hit.GetActor() != Ghost;
		if (!bBlocked)
		{
			return true;
		}
	}
	return false;
}

bool AMallMotionSensor::TrySecondaryUse(APlayerController* Player)
{
	if (!CanPlayerControlDevice(Player))
	{
		return false;
	}
	bSilentAlarm = !bSilentAlarm;
	UpdateDeviceVisuals();
	ForceNetUpdate();
	return true;
}

void AMallMotionSensor::OnRep_MotionState()
{
	UpdateDeviceVisuals();
	if (bPowered && bMotionDetected && !bSilentAlarm && DeviceAudio && AlertChimeSound)
	{
		DeviceAudio->SetSound(AlertChimeSound);
		DeviceAudio->SetPitchMultiplier(1.35f);
		DeviceAudio->Play();
	}
}

void AMallMotionSensor::TickServerDetection(float DeltaSeconds)
{
	const bool bDetectedNow = EvaluateGhostMotion();
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	bool bDirty = false;

	if (bDetectedNow && (!bMotionDetected || (Now - LastTriggerServerTime) >= 2.5))
	{
		++TriggerCount;
		LastTriggerServerTime = Now;
		bDirty = true;
		if (!bSilentAlarm && DeviceAudio && AlertChimeSound && GetNetMode() != NM_DedicatedServer)
		{
			DeviceAudio->SetSound(AlertChimeSound);
			DeviceAudio->SetPitchMultiplier(1.35f);
			DeviceAudio->Play();
		}
	}
	if (bMotionDetected != bDetectedNow)
	{
		bMotionDetected = bDetectedNow;
		bDirty = true;
	}
	if (bDirty)
	{
		UpdateDeviceVisuals();
		ForceNetUpdate();
	}
}

void AMallMotionSensor::UpdateDeviceVisuals()
{
	const FColor Color = !bPowered ? FColor(100, 105, 110)
		: bMotionDetected ? FColor(45, 255, 195)
		: FColor(90, 180, 120);

	if (ReadoutText)
	{
		if (!bPowered)
		{
			ReadoutText->SetText(FText::FromString(TEXT("MOTION: OFF")));
		}
		else if (bMotionDetected)
		{
			ReadoutText->SetText(FText::FromString(FString::Printf(TEXT("MOTION ALERT! [%d]"), TriggerCount)));
		}
		else
		{
			ReadoutText->SetText(FText::FromString(FString::Printf(TEXT("ARMED [%d HITS]"), TriggerCount)));
		}
		ReadoutText->SetTextRenderColor(Color);
	}

	if (StatusLight)
	{
		StatusLight->SetLightColor(FLinearColor(Color));
		StatusLight->SetIntensity(bPowered ? (bMotionDetected ? 650.0f : 110.0f) : 0.0f);
		StatusLight->SetAttenuationRadius(bMotionDetected ? 280.0f : 130.0f);
	}
}

FString AMallMotionSensor::GetHUDPrompt(bool bIsHeld) const
{
	return FString::Printf(TEXT("Motion Sensor [%s | %d hits]  |  LMB: arm  |  E: %s  |  F: %s%s"),
		!bPowered ? TEXT("OFF") : (bMotionDetected ? TEXT("MOTION!") : TEXT("ARMED")),
		TriggerCount,
		bSilentAlarm ? TEXT("enable chime") : TEXT("silent mode"),
		bIsHeld ? TEXT("place / store on cart") : TEXT("pick up"),
		bIsHeld ? TEXT("  |  G: drop") : TEXT(""));
}

FString AMallMotionSensor::GetHUDDetail() const
{
	if (!bPowered)
	{
		return TEXT("Click LMB to arm IR motion beam. Detects invisible & visible ghosts crossing the store.");
	}
	if (bMotionDetected)
	{
		return FString::Printf(TEXT("PARANORMAL MOTION DETECTED IN BEAM! (Total triggers: %d)"), TriggerCount);
	}
	return FString::Printf(TEXT("Monitoring %.1fm IR zone (%s). Total ghost crossings: %d"),
		DetectionRangeCm * 0.01f,
		bSilentAlarm ? TEXT("Silent Strobe") : TEXT("Chime + Strobe"),
		TriggerCount);
}

FString AMallMotionSensor::GetPickupFeedback() const
{
	return TEXT("Motion Sensor picked up. LMB: arm | E: chime/silent | F: place on floor/table | G: drop");
}

FString AMallMotionSensor::GetPlaceFeedback() const
{
	return TEXT("Motion Sensor placed facing forward. Click LMB to arm if not already armed.");
}

FString AMallMotionSensor::GetPrimaryUseFeedback() const
{
	return bPowered
		? TEXT("Motion Sensor ARMED. Place it with F inside a store to track invisible ghost movement.")
		: TEXT("Motion Sensor disarmed.");
}

FString AMallMotionSensor::GetSecondaryUseFeedback() const
{
	return bSilentAlarm
		? TEXT("Motion Sensor set to Silent Strobe mode.")
		: TEXT("Motion Sensor set to Audible Chime + Strobe mode.");
}

// ============================================================================
// AMallPhotoCamera
// ============================================================================

AMallPhotoCamera::AMallPhotoCamera()
{
	DisplayName = FText::FromString(TEXT("Photo Camera"));
	ItemValue = 45.0f;
	PlacementHalfExtent = FVector(9.0, 8.0, 8.0);
	bPowered = true;

	XenonFlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("XenonFlashLight"));
	XenonFlashLight->SetupAttachment(ItemRoot);
	XenonFlashLight->SetRelativeLocation(FVector(12.0, 0.0, 10.0));
	XenonFlashLight->SetLightColor(FLinearColor(0.95f, 0.98f, 1.0f));
	XenonFlashLight->SetIntensity(0.0f);
	XenonFlashLight->SetAttenuationRadius(950.0f);
	XenonFlashLight->SetCastShadows(false);

	static ConstructorHelpers::FObjectFinder<USoundBase> DefaultShutter(TEXT("/Engine/VREditor/Sounds/VR_click1.VR_click1"));
	if (DefaultShutter.Succeeded())
	{
		ShutterSound = DefaultShutter.Object;
	}
}

void AMallPhotoCamera::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallPhotoCamera, PhotosTaken);
	DOREPLIFETIME(AMallPhotoCamera, LastPhotoStars);
	DOREPLIFETIME(AMallPhotoCamera, LastPhotoReward);
	DOREPLIFETIME(AMallPhotoCamera, TotalPhotoValue);
	DOREPLIFETIME(AMallPhotoCamera, LastPhotoSubject);
	DOREPLIFETIME(AMallPhotoCamera, bReviewLogMode);
	DOREPLIFETIME(AMallPhotoCamera, bFlashBurstActive);
}

bool AMallPhotoCamera::HasCameraLineOfSight(const FVector& Origin, const FVector& TargetLocation, const AActor* AllowedTarget) const
{
	if (!GetWorld())
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallPhotoCameraLOS), true, this);
	if (Carrier)
	{
		Params.AddIgnoredActor(Carrier);
	}
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, TargetLocation, ECC_Visibility, Params))
	{
		return true;
	}
	return Hit.GetActor() == AllowedTarget || (Hit.GetActor() && Hit.GetActor()->GetAttachParentActor() == AllowedTarget);
}

void AMallPhotoCamera::ScoreBestSubjectInFrame(
	const FVector& Origin,
	const FVector& Direction,
	int32& OutStars,
	int32& OutReward,
	FString& OutSubject,
	AActor*& OutMatchedGhost) const
{
	OutStars = 0;
	OutReward = 0;
	OutSubject = TEXT("No Paranormal Subject");
	OutMatchedGhost = nullptr;

	if (!GetWorld())
	{
		return;
	}

	const float CosHalfCone = FMath::Cos(FMath::DegreesToRadians(ViewConeHalfAngleDegrees));
	auto IsInFrustum = [&](const FVector& TargetPos, float MaxRange) -> bool
	{
		const FVector Delta = TargetPos - Origin;
		const double Dist = Delta.Size();
		return Dist > KINDA_SMALL_NUMBER && Dist <= MaxRange && FVector::DotProduct(Delta / Dist, Direction) >= CosHalfCone;
	};

	auto Consider = [&](int32 Stars, int32 Reward, const FString& Label, AActor* GhostRef = nullptr)
	{
		if (Stars > OutStars || (Stars == OutStars && Reward > OutReward))
		{
			OutStars = Stars;
			OutReward = Reward;
			OutSubject = Label;
			if (GhostRef)
			{
				OutMatchedGhost = GhostRef;
			}
		}
	};

	// 1. Store Ghosts (Visible Hunt/Warning = 5 Stars; Invisible Distortion in close range = 4 Stars)
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Ghost = *It;
		if (!MallGhostEquipmentInternal::IsStoreGhostActor(Ghost) || MallGhostEquipmentInternal::IsGhostSuppressedByCart(Ghost))
		{
			continue;
		}
		const FVector TargetPos = Ghost->GetActorLocation() + FVector(0.0, 0.0, 35.0);
		if (!IsInFrustum(TargetPos, MaxPhotoRangeCm) || !HasCameraLineOfSight(Origin, TargetPos, Ghost))
		{
			continue;
		}
		const UMallGhostInteractionComponent* GhostComp = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
		const bool bVisibleManifestation = GhostComp && !GhostComp->IsInvisible();
		if (bVisibleManifestation)
		{
			Consider(5, 100, TEXT("Manifested Store Ghost"), Ghost);
		}
		else if (FVector::DistSquared(Origin, TargetPos) <= FMath::Square(550.0))
		{
			Consider(4, 60, TEXT("Ectoplasmic Distortion"), Ghost);
		}
	}

	// 2. UV Evidence Prints (4 Stars when glowing under UV, 3 Stars if previously discovered)
	for (TActorIterator<AMallUVEvidence> It(GetWorld()); It; ++It)
	{
		const AMallUVEvidence* UV = *It;
		if (!IsValid(UV) || (!UV->bIlluminatedByUV && !UV->bEverDiscovered))
		{
			continue;
		}
		const FVector TargetPos = UV->GetActorLocation() + FVector(0.0, 0.0, 6.0);
		if (IsInFrustum(TargetPos, MaxPhotoRangeCm) && HasCameraLineOfSight(Origin, TargetPos, UV))
		{
			Consider(UV->bIlluminatedByUV ? 4 : 3, UV->bIlluminatedByUV ? 65 : 40, UV->PrintTitle);
		}
	}

	// 3. Written Ghost Writing Book (3 Stars)
	for (TActorIterator<AMallGhostWritingBook> It(GetWorld()); It; ++It)
	{
		const AMallGhostWritingBook* Book = *It;
		if (!IsValid(Book) || !Book->IsWritten())
		{
			continue;
		}
		const FVector TargetPos = Book->GetActorLocation() + FVector(0.0, 0.0, 6.0);
		if (IsInFrustum(TargetPos, MaxPhotoRangeCm) && HasCameraLineOfSight(Origin, TargetPos, Book))
		{
			Consider(3, 50, TEXT("Ghost Writing Book"));
		}
	}

	// 4. Active EMF Spike (Level 4-5) or Triggered Motion Sensor (3 Stars)
	for (TActorIterator<AMallEMFReader> It(GetWorld()); It; ++It)
	{
		const AMallEMFReader* EMF = *It;
		if (!IsValid(EMF) || !EMF->IsPowered() || EMF->CurrentEMFLevel < 4)
		{
			continue;
		}
		const FVector TargetPos = EMF->GetActorLocation() + FVector(0.0, 0.0, 8.0);
		if (IsInFrustum(TargetPos, MaxPhotoRangeCm) && HasCameraLineOfSight(Origin, TargetPos, EMF))
		{
			Consider(EMF->CurrentEMFLevel >= 5 ? 4 : 3, EMF->CurrentEMFLevel >= 5 ? 55 : 40,
				FString::Printf(TEXT("EMF Level %d Reading"), EMF->CurrentEMFLevel));
		}
	}

	// 5. Disturbed Store Props (2 Stars)
	for (TActorIterator<AMallGhostDisturbanceProp> It(GetWorld()); It; ++It)
	{
		const AMallGhostDisturbanceProp* Prop = *It;
		if (!IsValid(Prop) || Prop->DisturbanceCount <= 0)
		{
			continue;
		}
		const FVector TargetPos = Prop->GetActorLocation() + FVector(0.0, 0.0, 8.0);
		if (IsInFrustum(TargetPos, MaxPhotoRangeCm) && HasCameraLineOfSight(Origin, TargetPos, Prop))
		{
			Consider(2, 30, TEXT("Poltergeist Prop Disturbance"));
		}
	}
}

bool AMallPhotoCamera::TryCapturePhoto(APlayerController* Player)
{
	if (!CanPlayerControlDevice(Player) || !GetWorld())
	{
		return false;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (PhotosTaken >= MaxPhotos || (FlashCooldownSeconds > 0.0f && Now < NextShutterAllowedTime))
	{
		return false;
	}
	NextShutterAllowedTime = (FlashCooldownSeconds > 0.0f) ? (Now + FlashCooldownSeconds) : 0.0;
	FlashEndsServerTime = Now + 0.22;
	bFlashBurstActive = true;
	bPowered = true;
	++PhotosTaken;

	FVector Origin, Direction;
	GetActiveAimRay(Origin, Direction);

	int32 Stars = 0;
	int32 Reward = 0;
	FString Subject;
	AActor* MatchedGhost = nullptr;
	ScoreBestSubjectInFrame(Origin, Direction, Stars, Reward, Subject, MatchedGhost);

	LastPhotoStars = Stars;
	LastPhotoReward = Reward;
	TotalPhotoValue += Reward;
	LastPhotoSubject = Subject;

	if (MatchedGhost && GhostFlashPauseSeconds > 0.0f
		&& FVector::DistSquared(Origin, MatchedGhost->GetActorLocation()) <= FMath::Square(650.0))
	{
		if (UMallGhostInteractionComponent* GhostComp = MatchedGhost->FindComponentByClass<UMallGhostInteractionComponent>())
		{
			GhostComp->PauseDisturbances(GhostFlashPauseSeconds);
		}
	}

	if (DeviceAudio && ShutterSound && GetNetMode() != NM_DedicatedServer)
	{
		DeviceAudio->SetSound(ShutterSound);
		DeviceAudio->SetPitchMultiplier(1.05f);
		DeviceAudio->Play();
	}

	UpdateDeviceVisuals();
	ForceNetUpdate();
	return true;
}

bool AMallPhotoCamera::TryPrimaryUse(APlayerController* Player)
{
	return TryCapturePhoto(Player);
}

bool AMallPhotoCamera::TrySecondaryUse(APlayerController* Player)
{
	if (!CanPlayerControlDevice(Player))
	{
		return false;
	}
	bReviewLogMode = !bReviewLogMode;
	UpdateDeviceVisuals();
	ForceNetUpdate();
	return true;
}

void AMallPhotoCamera::OnRep_CameraState()
{
	UpdateDeviceVisuals();
	if (bFlashBurstActive && DeviceAudio && ShutterSound)
	{
		DeviceAudio->SetSound(ShutterSound);
		DeviceAudio->SetPitchMultiplier(1.05f);
		DeviceAudio->Play();
	}
}

void AMallPhotoCamera::TickServerDetection(float DeltaSeconds)
{
	if (bFlashBurstActive && GetWorld() && GetWorld()->GetTimeSeconds() >= FlashEndsServerTime)
	{
		bFlashBurstActive = false;
		UpdateDeviceVisuals();
		ForceNetUpdate();
	}
}

void AMallPhotoCamera::UpdateDeviceVisuals()
{
	const int32 Remaining = FMath::Max(0, MaxPhotos - PhotosTaken);
	const FColor DisplayColor = (LastPhotoStars >= 4) ? FColor(255, 225, 75)
		: (LastPhotoStars >= 2) ? FColor(115, 245, 165)
		: FColor(180, 225, 240);

	if (ReadoutText)
	{
		if (bReviewLogMode && PhotosTaken > 0)
		{
			ReadoutText->SetText(FText::FromString(FString::Printf(TEXT("%d* %s ($%d)"),
				LastPhotoStars, *LastPhotoSubject, TotalPhotoValue)));
		}
		else
		{
			ReadoutText->SetText(FText::FromString(FString::Printf(TEXT("CAM [%d/%d] $%d"),
				Remaining, MaxPhotos, TotalPhotoValue)));
		}
		ReadoutText->SetTextRenderColor(DisplayColor);
	}

	if (XenonFlashLight)
	{
		XenonFlashLight->SetIntensity(bFlashBurstActive ? 8500.0f : 0.0f);
	}
	if (StatusLight)
	{
		StatusLight->SetLightColor(FLinearColor(DisplayColor));
		StatusLight->SetIntensity(Remaining > 0 ? 110.0f : 35.0f);
	}
}

FString AMallPhotoCamera::GetHUDPrompt(bool bIsHeld) const
{
	const int32 Remaining = FMath::Max(0, MaxPhotos - PhotosTaken);
	return FString::Printf(TEXT("Photo Camera [%d/%d shots | $%d]  |  LMB: snap photo  |  E: %s  |  F: %s%s"),
		Remaining,
		MaxPhotos,
		TotalPhotoValue,
		bReviewLogMode ? TEXT("viewfinder") : TEXT("photo log"),
		bIsHeld ? TEXT("place / store on cart") : TEXT("pick up"),
		bIsHeld ? TEXT("  |  G: drop") : TEXT(""));
}

FString AMallPhotoCamera::GetHUDDetail() const
{
	if (PhotosTaken == 0)
	{
		return TEXT("Aim at the ghost, UV prints, ghost writing, or EMF spikes and click LMB to snap a photo.");
	}
	return FString::Printf(TEXT("Last Photo: %d-Star %s (+$%d)  |  Total Evidence Value: $%d"),
		LastPhotoStars,
		*LastPhotoSubject,
		LastPhotoReward,
		TotalPhotoValue);
}

FString AMallPhotoCamera::GetPickupFeedback() const
{
	return TEXT("Photo Camera picked up. LMB: snap flash photo | E: review log | F: place | G: drop");
}

FString AMallPhotoCamera::GetPrimaryUseFeedback() const
{
	if (PhotosTaken >= MaxPhotos && LastPhotoSubject.IsEmpty())
	{
		return TEXT("Camera roll is full (0 exposures remaining).");
	}
	return FString::Printf(TEXT("SNAP! %d-Star Photo: %s (+$%d | Total $%d | %d shots left)"),
		LastPhotoStars,
		*LastPhotoSubject,
		LastPhotoReward,
		TotalPhotoValue,
		FMath::Max(0, MaxPhotos - PhotosTaken));
}

FString AMallPhotoCamera::GetSecondaryUseFeedback() const
{
	if (PhotosTaken == 0)
	{
		return TEXT("Photo Log: No photos taken yet.");
	}
	return FString::Printf(TEXT("Photo Log [%d/%d]: Last %d* %s | Total value $%d"),
		PhotosTaken, MaxPhotos, LastPhotoStars, *LastPhotoSubject, TotalPhotoValue);
}

// ============================================================================
// AMallLightFixture
// ============================================================================

AMallLightFixture::AMallLightFixture()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;

	FixtureRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FixtureRoot"));
	SetRootComponent(FixtureRoot);

	AimBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("AimBounds"));
	AimBounds->SetupAttachment(FixtureRoot);
	AimBounds->InitBoxExtent(FVector(72.0f, 44.0f, 32.0f));
	AimBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	AimBounds->SetCollisionObjectType(ECC_WorldDynamic);
	AimBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	AimBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	AimBounds->SetGenerateOverlapEvents(false);
	AimBounds->SetCanEverAffectNavigation(false);

	FixtureHousingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FixtureHousingMesh"));
	FixtureHousingMesh->SetupAttachment(FixtureRoot);
	FixtureHousingMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 3.0f));
	FixtureHousingMesh->SetRelativeScale3D(FVector(1.25f, 1.25f, 1.15f));
	FixtureHousingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FixtureHousingMesh->SetCanEverAffectNavigation(false);
	FixtureHousingMesh->SetCastShadow(false);

	GlowingBulbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlowingBulbMesh"));
	GlowingBulbMesh->SetupAttachment(FixtureRoot);
	GlowingBulbMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -1.2f));
	GlowingBulbMesh->SetRelativeScale3D(FVector(0.58f, 0.075f, 0.025f));
	GlowingBulbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GlowingBulbMesh->SetCanEverAffectNavigation(false);
	GlowingBulbMesh->SetCastShadow(false);

	SuspensionStemLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SuspensionStemLeft"));
	SuspensionStemLeft->SetupAttachment(FixtureRoot);
	SuspensionStemLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SuspensionStemLeft->SetCanEverAffectNavigation(false);
	SuspensionStemLeft->SetCastShadow(false);
	SuspensionStemLeft->SetVisibility(false);

	SuspensionStemRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SuspensionStemRight"));
	SuspensionStemRight->SetupAttachment(FixtureRoot);
	SuspensionStemRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SuspensionStemRight->SetCanEverAffectNavigation(false);
	SuspensionStemRight->SetCastShadow(false);
	SuspensionStemRight->SetVisibility(false);

	CeilingCanopyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CeilingCanopyMesh"));
	CeilingCanopyMesh->SetupAttachment(FixtureRoot);
	CeilingCanopyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CeilingCanopyMesh->SetCanEverAffectNavigation(false);
	CeilingCanopyMesh->SetCastShadow(false);
	CeilingCanopyMesh->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TrackBarMesh(TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25354_CeilingTrackLightingBar.SM_Retail_25354_CeilingTrackLightingBar"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (TrackBarMesh.Succeeded())
	{
		CachedTrackMesh = TrackBarMesh.Object;
		FixtureHousingMesh->SetStaticMesh(CachedTrackMesh);
	}
	if (CylinderMesh.Succeeded())
	{
		CachedCylinderMesh = CylinderMesh.Object;
		if (!CachedTrackMesh)
		{
			FixtureHousingMesh->SetStaticMesh(CachedCylinderMesh);
			FixtureHousingMesh->SetRelativeScale3D(FVector(0.85f, 0.85f, 0.18f));
		}
		SuspensionStemLeft->SetStaticMesh(CachedCylinderMesh);
		SuspensionStemRight->SetStaticMesh(CachedCylinderMesh);
	}
	if (CubeMesh.Succeeded())
	{
		CachedCubeMesh = CubeMesh.Object;
		GlowingBulbMesh->SetStaticMesh(CachedCubeMesh);
		CeilingCanopyMesh->SetStaticMesh(CachedCubeMesh);
	}
	if (UMaterialInterface* MetalMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Environment/Mall/Materials/M_MallRetail_metal.M_MallRetail_metal")))
	{
		SuspensionStemLeft->SetMaterial(0, MetalMat);
		SuspensionStemRight->SetMaterial(0, MetalMat);
		CeilingCanopyMesh->SetMaterial(0, MetalMat);
	}

	// Primary Directional Architectural Spotlight (eliminates omnidirectional circular wall blobs!)
	FixtureSpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("FixtureSpotLight"));
	FixtureSpotLight->SetupAttachment(FixtureRoot);
	FixtureSpotLight->SetRelativeLocation(FVector(0.0f, 0.0f, -6.0f));
	FixtureSpotLight->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	FixtureSpotLight->SetIntensityUnits(ELightUnits::Lumens);
	FixtureSpotLight->SetIntensity(DefaultIntensityLumens);
	FixtureSpotLight->SetAttenuationRadius(DefaultAttenuationRadius);
	FixtureSpotLight->SetInnerConeAngle(SpotInnerConeAngle);
	FixtureSpotLight->SetOuterConeAngle(SpotOuterConeAngle);
	FixtureSpotLight->SetSourceRadius(14.0f);
	FixtureSpotLight->SetSoftSourceRadius(18.0f);
	FixtureSpotLight->SetSourceLength(16.0f);
	FixtureSpotLight->SetLightColor(DefaultLightColor);
	FixtureSpotLight->SetCastShadows(false);
	FixtureSpotLight->MaxDrawDistance = 4200.0f;
	FixtureSpotLight->MaxDistanceFadeRange = 900.0f;

	// Subtle 6% local ceiling/housing bounce fill (never reaches walls to form circular hotspots)
	FixturePointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FixturePointLight"));
	FixturePointLight->SetupAttachment(FixtureRoot);
	FixturePointLight->SetRelativeLocation(FVector(0.0f, 0.0f, -8.0f));
	FixturePointLight->SetIntensityUnits(ELightUnits::Lumens);
	FixturePointLight->SetIntensity(DefaultIntensityLumens * 0.06f);
	FixturePointLight->SetAttenuationRadius(240.0f);
	FixturePointLight->SetSourceRadius(10.0f);
	FixturePointLight->SetSoftSourceRadius(14.0f);
	FixturePointLight->SetSourceLength(12.0f);
	FixturePointLight->SetLightColor(DefaultLightColor);
	FixturePointLight->SetCastShadows(false);
	FixturePointLight->MaxDrawDistance = 2800.0f;
	FixturePointLight->MaxDistanceFadeRange = 600.0f;
}

void AMallLightFixture::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallLightFixture, FixtureLabel);
	DOREPLIFETIME(AMallLightFixture, DefaultIntensityLumens);
	DOREPLIFETIME(AMallLightFixture, DefaultAttenuationRadius);
	DOREPLIFETIME(AMallLightFixture, DefaultLightColor);
	DOREPLIFETIME(AMallLightFixture, MountType);
	DOREPLIFETIME(AMallLightFixture, CeilingDropCm);
	DOREPLIFETIME(AMallLightFixture, SpotAimRotation);
	DOREPLIFETIME(AMallLightFixture, SpotInnerConeAngle);
	DOREPLIFETIME(AMallLightFixture, SpotOuterConeAngle);
	DOREPLIFETIME(AMallLightFixture, bLightPowered);
	DOREPLIFETIME(AMallLightFixture, BrightnessMode);
}

void AMallLightFixture::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateFixtureVisuals();
}

void AMallLightFixture::BeginPlay()
{
	Super::BeginPlay();
	UpdateFixtureVisuals();
}

void AMallLightFixture::ConfigureFixture(const FString& InLabel, float InIntensityLumens, float InAttenuationRadius, FLinearColor InColor, bool bInPowered)
{
	ConfigureArchitecturalFixture(
		InLabel,
		InIntensityLumens,
		InAttenuationRadius,
		InColor,
		MountType,
		CeilingDropCm,
		SpotAimRotation,
		SpotInnerConeAngle,
		SpotOuterConeAngle,
		bInPowered);
}

void AMallLightFixture::ConfigureArchitecturalFixture(
	const FString& InLabel,
	float InIntensityLumens,
	float InAttenuationRadius,
	FLinearColor InColor,
	uint8 InMountType,
	float InCeilingDropCm,
	FRotator InSpotAimRotation,
	float InInnerConeAngle,
	float InOuterConeAngle,
	bool bInPowered)
{
	FixtureLabel = InLabel;
	DefaultIntensityLumens = FMath::Max(10.0f, InIntensityLumens);
	DefaultAttenuationRadius = FMath::Max(150.0f, InAttenuationRadius);
	DefaultLightColor = InColor;
	MountType = InMountType;
	CeilingDropCm = FMath::Max(0.0f, InCeilingDropCm);
	SpotAimRotation = InSpotAimRotation;
	SpotInnerConeAngle = FMath::Clamp(InInnerConeAngle, 5.0f, 80.0f);
	SpotOuterConeAngle = FMath::Clamp(FMath::Max(InOuterConeAngle, SpotInnerConeAngle + 6.0f), 15.0f, 88.0f);
	bLightPowered = bInPowered;
	BrightnessMode = bInPowered ? 2 : 0;
	UpdateFixtureVisuals();
	if (HasAuthority())
	{
		ForceNetUpdate();
	}
}

void AMallLightFixture::SetLightPowered(bool bNewPowered, uint8 NewBrightnessMode)
{
	bLightPowered = bNewPowered;
	BrightnessMode = bNewPowered ? FMath::Clamp<uint8>(NewBrightnessMode, 1, 2) : 0;
	UpdateFixtureVisuals();
	if (HasAuthority())
	{
		ForceNetUpdate();
	}
}

bool AMallLightFixture::ToggleLight()
{
	bLightPowered = !bLightPowered;
	BrightnessMode = bLightPowered ? 2 : 0;
	UpdateFixtureVisuals();
	if (HasAuthority())
	{
		ForceNetUpdate();
	}
	return bLightPowered;
}

uint8 AMallLightFixture::CycleBrightnessMode()
{
	if (!bLightPowered || BrightnessMode == 0)
	{
		bLightPowered = true;
		BrightnessMode = 2;
	}
	else if (BrightnessMode >= 2)
	{
		BrightnessMode = 1;
	}
	else
	{
		bLightPowered = false;
		BrightnessMode = 0;
	}
	UpdateFixtureVisuals();
	if (HasAuthority())
	{
		ForceNetUpdate();
	}
	return BrightnessMode;
}

FString AMallLightFixture::GetFixtureStatusText() const
{
	const TCHAR* ModeStr = (!bLightPowered || BrightnessMode == 0)
		? TEXT("OFF (0%)")
		: (BrightnessMode == 1 ? TEXT("GRAY-SPACE DIM (45%)") : TEXT("WELL-LIT ON (100%)"));
	return FString::Printf(TEXT("%s [%s]"), *FixtureLabel, ModeStr);
}

void AMallLightFixture::OnRep_LightState()
{
	UpdateFixtureVisuals();
}

void AMallLightFixture::UpdateFixtureVisuals()
{
	const float Scale = (!bLightPowered || BrightnessMode == 0)
		? 0.0f
		: (BrightnessMode == 1 ? 0.45f : 1.0f);

	// Update physical geometry by MountType (0 = FlushCeiling, 1 = SuspendedPendant, 2 = GroundedBollard)
	if (MountType == 2)
	{
		// Grounded Architectural Bronze/Stone Landscape & Pool-Rim Bollard (seated directly on floor/curb!)
		if (FixtureHousingMesh)
		{
			if (CachedCylinderMesh)
			{
				FixtureHousingMesh->SetStaticMesh(CachedCylinderMesh);
			}
			FixtureHousingMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 26.0f));
			FixtureHousingMesh->SetRelativeScale3D(FVector(0.24f, 0.24f, 0.52f));
		}
		if (GlowingBulbMesh)
		{
			if (CachedCylinderMesh)
			{
				GlowingBulbMesh->SetStaticMesh(CachedCylinderMesh);
			}
			GlowingBulbMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 56.0f));
			GlowingBulbMesh->SetRelativeScale3D(FVector(0.21f, 0.21f, 0.14f));
		}
		if (SuspensionStemLeft)
		{
			SuspensionStemLeft->SetVisibility(false);
		}
		if (SuspensionStemRight)
		{
			SuspensionStemRight->SetVisibility(false);
		}
		if (CeilingCanopyMesh)
		{
			CeilingCanopyMesh->SetVisibility(false);
		}
		if (FixtureSpotLight)
		{
			FixtureSpotLight->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
		}
		if (FixturePointLight)
		{
			FixturePointLight->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
		}
	}
	else
	{
		// Ceiling Track Bar (Flush or Suspended via Dual Steel Conduit Stems anchored to the ceiling slab above)
		if (FixtureHousingMesh)
		{
			if (CachedTrackMesh)
			{
				FixtureHousingMesh->SetStaticMesh(CachedTrackMesh);
				FixtureHousingMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 3.0f));
				FixtureHousingMesh->SetRelativeScale3D(FVector(1.25f, 1.25f, 1.15f));
			}
		}
		if (GlowingBulbMesh)
		{
			if (CachedCubeMesh)
			{
				GlowingBulbMesh->SetStaticMesh(CachedCubeMesh);
			}
			GlowingBulbMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -1.2f));
			GlowingBulbMesh->SetRelativeScale3D(FVector(0.58f, 0.075f, 0.025f));
		}
		if (FixtureSpotLight)
		{
			FixtureSpotLight->SetRelativeLocation(FVector(0.0f, 0.0f, -6.0f));
		}
		if (FixturePointLight)
		{
			FixturePointLight->SetRelativeLocation(FVector(0.0f, 0.0f, -8.0f));
		}

		float EffectiveDropCm = CeilingDropCm;
		if (EffectiveDropCm <= 1.0f && GetWorld())
		{
			FHitResult CeilingHit;
			FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(MallFixtureCeilingSnap), false, this);
			const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 4.0f);
			const FVector End = Start + FVector(0.0f, 0.0f, 680.0f);
			if (GetWorld()->LineTraceSingleByChannel(CeilingHit, Start, End, ECC_Visibility, TraceParams))
			{
				const float HitDist = CeilingHit.Distance + 4.0f;
				if (HitDist > 12.0f && HitDist <= 680.0f)
				{
					EffectiveDropCm = HitDist;
				}
			}
		}

		const bool bShowStems = (EffectiveDropCm > 10.0f);
		if (SuspensionStemLeft && SuspensionStemRight && CeilingCanopyMesh)
		{
			SuspensionStemLeft->SetVisibility(bShowStems);
			SuspensionStemRight->SetVisibility(bShowStems);
			CeilingCanopyMesh->SetVisibility(bShowStems);
			if (bShowStems)
			{
				const float HalfDrop = EffectiveDropCm * 0.5f;
				const float StemScaleZ = FMath::Max(0.05f, EffectiveDropCm / 100.0f);
				SuspensionStemLeft->SetRelativeLocation(FVector(-42.0f, 0.0f, HalfDrop));
				SuspensionStemLeft->SetRelativeScale3D(FVector(0.032f, 0.032f, StemScaleZ));
				SuspensionStemRight->SetRelativeLocation(FVector(42.0f, 0.0f, HalfDrop));
				SuspensionStemRight->SetRelativeScale3D(FVector(0.032f, 0.032f, StemScaleZ));
				CeilingCanopyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, EffectiveDropCm - 2.0f));
				CeilingCanopyMesh->SetRelativeScale3D(FVector(1.05f, 0.16f, 0.04f));
			}
		}
	}

	if (FixtureSpotLight)
	{
		FixtureSpotLight->SetIntensityUnits(ELightUnits::Lumens);
		FixtureSpotLight->SetLightColor(DefaultLightColor);
		FixtureSpotLight->SetAttenuationRadius(DefaultAttenuationRadius);
		FixtureSpotLight->SetRelativeRotation(SpotAimRotation);
		FixtureSpotLight->SetInnerConeAngle(SpotInnerConeAngle);
		FixtureSpotLight->SetOuterConeAngle(SpotOuterConeAngle);
		FixtureSpotLight->SetSourceRadius(14.0f);
		FixtureSpotLight->SetSoftSourceRadius(18.0f);
		FixtureSpotLight->SetSourceLength(16.0f);
		FixtureSpotLight->SetCastShadows(false);
		FixtureSpotLight->MaxDrawDistance = 4200.0f;
		FixtureSpotLight->MaxDistanceFadeRange = 900.0f;
		FixtureSpotLight->SetIntensity(DefaultIntensityLumens * Scale);
		FixtureSpotLight->SetVisibility(Scale > 0.0f);
	}

	if (FixturePointLight)
	{
		FixturePointLight->SetIntensityUnits(ELightUnits::Lumens);
		FixturePointLight->SetLightColor(DefaultLightColor);
		FixturePointLight->SetAttenuationRadius(FMath::Min(240.0f, DefaultAttenuationRadius * 0.22f));
		FixturePointLight->SetSourceRadius(10.0f);
		FixturePointLight->SetSoftSourceRadius(14.0f);
		FixturePointLight->SetSourceLength(12.0f);
		FixturePointLight->SetCastShadows(false);
		FixturePointLight->MaxDrawDistance = 2800.0f;
		FixturePointLight->MaxDistanceFadeRange = 600.0f;
		FixturePointLight->SetIntensity(DefaultIntensityLumens * 0.06f * Scale);
		FixturePointLight->SetVisibility(Scale > 0.0f);
	}

	if (GlowingBulbMesh)
	{
		if (!BulbMID)
		{
			if (UMaterialInterface* BaseMat = MallGhostEquipmentInternal::GetTintableDeviceMaterial())
			{
				BulbMID = UMaterialInstanceDynamic::Create(BaseMat, this);
				GlowingBulbMesh->SetMaterial(0, BulbMID);
			}
		}
		if (BulbMID)
		{
			const FLinearColor BulbColor = (Scale > 0.0f)
				? (DefaultLightColor * FMath::Lerp(0.55f, 1.25f, Scale))
				: FLinearColor(0.06f, 0.07f, 0.08f, 1.0f);
			MallGhostEquipmentInternal::ApplyMIDColor(BulbMID, BulbColor, Scale > 0.0f ? 0.08f : 0.65f);
		}
	}
}

// ============================================================================
// AMallLightRemoteGun
// ============================================================================

AMallLightRemoteGun::AMallLightRemoteGun()
{
	DisplayName = FText::FromString(TEXT("Light Control Remote Gun"));
	ItemValue = 0.0f;
	bPowered = true;
	CarriedViewOffset = FVector(38.0, 15.0, -12.5);
	CarriedViewRotationOffset = FRotator(8.0, 180.0, 0.0);

	if (DeviceMesh)
	{
		DeviceMesh->SetRelativeLocation(FVector(0.0, 0.0, 7.5));
		DeviceMesh->SetRelativeScale3D(FVector(0.24, 0.12, 0.085));
	}

	BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
	BarrelMesh->SetupAttachment(ItemRoot);
	BarrelMesh->SetRelativeLocation(FVector(15.0, 0.0, 8.0));
	BarrelMesh->SetRelativeRotation(FRotator(90.0, 0.0, 0.0));
	BarrelMesh->SetRelativeScale3D(FVector(0.055, 0.055, 0.16));
	BarrelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrelMesh->SetCanEverAffectNavigation(false);
	BarrelMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		BarrelMesh->SetStaticMesh(CylinderMesh.Object);
	}

	LaserPointerLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("LaserPointerLight"));
	LaserPointerLight->SetupAttachment(ItemRoot);
	LaserPointerLight->SetRelativeLocation(FVector(22.0, 0.0, 8.0));
	LaserPointerLight->SetIntensityUnits(ELightUnits::Lumens);
	LaserPointerLight->SetIntensity(45.0f);
	LaserPointerLight->SetAttenuationRadius(1800.0f);
	LaserPointerLight->SetInnerConeAngle(1.2f);
	LaserPointerLight->SetOuterConeAngle(3.2f);
	LaserPointerLight->SetLightColor(FLinearColor(0.15f, 0.95f, 1.0f));
	LaserPointerLight->SetCastShadows(false);

	if (ReadoutText)
	{
		ReadoutText->SetRelativeLocation(FVector(-12.5, 0.0, 13.5));
		ReadoutText->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
		ReadoutText->SetWorldSize(3.6f);
		ReadoutText->SetText(FText::FromString(TEXT("LIGHT REMOTE READY")));
	}
}

void AMallLightRemoteGun::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallLightRemoteGun, AimedTargetTitle);
	DOREPLIFETIME(AMallLightRemoteGun, bAimedTargetPowered);
	DOREPLIFETIME(AMallLightRemoteGun, AimedTargetBrightnessMode);
	DOREPLIFETIME(AMallLightRemoteGun, LastActionFeedback);
	DOREPLIFETIME(AMallLightRemoteGun, TotalLightsToggled);
}

bool AMallLightRemoteGun::FindAimedLightTarget(
	const FVector& Origin,
	const FVector& Direction,
	AMallLightFixture*& OutFixture,
	AMallRetailStoreUnit*& OutStoreUnit,
	FString& OutTargetName,
	bool& bOutIsOn,
	uint8& OutBrightnessMode) const
{
	OutFixture = nullptr;
	OutStoreUnit = nullptr;
	OutTargetName.Empty();
	bOutIsOn = false;
	OutBrightnessMode = 0;

	if (!GetWorld() || Direction.IsNearlyZero())
	{
		return false;
	}

	const FVector SafeDir = Direction.GetSafeNormal();
	const FVector TraceEnd = Origin + SafeDir * MaxRemoteRangeCm;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallLightRemoteTrace), false, this);
	if (Carrier)
	{
		Params.AddIgnoredActor(Carrier);
	}

	auto EvaluateCandidateActor = [&](AActor* Actor, UPrimitiveComponent* HitComp) -> bool
	{
		if (AMallLightFixture* Fixture = Cast<AMallLightFixture>(Actor))
		{
			OutFixture = Fixture;
			OutTargetName = Fixture->FixtureLabel;
			bOutIsOn = Fixture->bLightPowered && Fixture->BrightnessMode > 0;
			OutBrightnessMode = Fixture->BrightnessMode;
			return true;
		}
		if (AMallRetailStoreUnit* Store = Cast<AMallRetailStoreUnit>(Actor))
		{
			if (!HitComp || HitComp == Store->StoreLightAimBounds || HitComp == Store->StoreLightFixtureMesh || HitComp == Store->StoreUpperLightFixtureMesh)
			{
				OutStoreUnit = Store;
				OutTargetName = Store->StoreSignTitle + TEXT(" Shop Lights");
				bOutIsOn = Store->bStoreLightPowered && Store->StoreLightBrightnessMode > 0;
				OutBrightnessMode = Store->StoreLightBrightnessMode;
				return true;
			}
		}
		return false;
	};

	// 1. Direct line trace first
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, TraceEnd, ECC_Visibility, Params))
	{
		if (EvaluateCandidateActor(Hit.GetActor(), Hit.GetComponent()))
		{
			return true;
		}
	}

	// 2. Forgiving ray-distance check against all AMallLightFixture and AMallRetailStoreUnit light fixtures
	double BestDistToRaySq = FMath::Square(145.0);
	double BestDistAlongRay = MaxRemoteRangeCm;

	for (TActorIterator<AMallLightFixture> It(GetWorld()); It; ++It)
	{
		AMallLightFixture* Fixture = *It;
		if (!IsValid(Fixture))
		{
			continue;
		}
		const FVector Pos = Fixture->GetActorLocation();
		const double Proj = FVector::DotProduct(Pos - Origin, SafeDir);
		if (Proj < 30.0 || Proj > MaxRemoteRangeCm)
		{
			continue;
		}
		const FVector ClosestPt = Origin + SafeDir * Proj;
		const double DistToRaySq = FVector::DistSquared(Pos, ClosestPt);
		if (DistToRaySq < BestDistToRaySq || (FMath::IsNearlyEqual(DistToRaySq, BestDistToRaySq, 25.0) && Proj < BestDistAlongRay))
		{
			BestDistToRaySq = DistToRaySq;
			BestDistAlongRay = Proj;
			OutFixture = Fixture;
			OutStoreUnit = nullptr;
			OutTargetName = Fixture->FixtureLabel;
			bOutIsOn = Fixture->bLightPowered && Fixture->BrightnessMode > 0;
			OutBrightnessMode = Fixture->BrightnessMode;
		}
	}

	for (TActorIterator<AMallRetailStoreUnit> It(GetWorld()); It; ++It)
	{
		AMallRetailStoreUnit* Store = *It;
		if (!IsValid(Store) || !Store->StoreInteriorLight)
		{
			continue;
		}
		const FVector Pos = Store->StoreInteriorLight->GetComponentLocation();
		const double Proj = FVector::DotProduct(Pos - Origin, SafeDir);
		if (Proj < 30.0 || Proj > MaxRemoteRangeCm)
		{
			continue;
		}
		const FVector ClosestPt = Origin + SafeDir * Proj;
		const double DistToRaySq = FVector::DistSquared(Pos, ClosestPt);
		if (DistToRaySq < BestDistToRaySq)
		{
			BestDistToRaySq = DistToRaySq;
			BestDistAlongRay = Proj;
			OutFixture = nullptr;
			OutStoreUnit = Store;
			OutTargetName = Store->StoreSignTitle + TEXT(" Shop Lights");
			bOutIsOn = Store->bStoreLightPowered && Store->StoreLightBrightnessMode > 0;
			OutBrightnessMode = Store->StoreLightBrightnessMode;
		}
	}

	return (OutFixture != nullptr || OutStoreUnit != nullptr);
}

bool AMallLightRemoteGun::TryPrimaryUse(APlayerController* Player)
{
	if (!CanPlayerControlDevice(Player) || !GetWorld())
	{
		return false;
	}

	bPowered = true;
	FVector Origin, Direction;
	GetActiveAimRay(Origin, Direction);

	AMallLightFixture* Fixture = nullptr;
	AMallRetailStoreUnit* Store = nullptr;
	FString TargetName;
	bool bIsOn = false;
	uint8 Mode = 0;

	if (!FindAimedLightTarget(Origin, Direction, Fixture, Store, TargetName, bIsOn, Mode))
	{
		LastActionFeedback = TEXT("No light fixture in crosshair. Aim at any ceiling light model and click LMB to toggle ON/OFF.");
		UpdateDeviceVisuals();
		ForceNetUpdate();
		return true;
	}

	if (Fixture)
	{
		const bool bNewState = Fixture->ToggleLight();
		AimedTargetTitle = Fixture->FixtureLabel;
		bAimedTargetPowered = bNewState;
		AimedTargetBrightnessMode = Fixture->BrightnessMode;
		++TotalLightsToggled;
		LastActionFeedback = FString::Printf(TEXT("LIGHT REMOTE: %s -> %s"),
			*Fixture->FixtureLabel, bNewState ? TEXT("ON (100% WELL-LIT)") : TEXT("OFF (0% DARK)"));
	}
	else if (Store)
	{
		const bool bNewState = Store->ToggleStoreLights();
		AimedTargetTitle = TargetName;
		bAimedTargetPowered = bNewState;
		AimedTargetBrightnessMode = Store->StoreLightBrightnessMode;
		++TotalLightsToggled;
		LastActionFeedback = FString::Printf(TEXT("LIGHT REMOTE: %s -> %s"),
			*TargetName, bNewState ? TEXT("ON (100% WELL-LIT)") : TEXT("OFF (0% DARK)"));
	}

	UpdateDeviceVisuals();
	ForceNetUpdate();
	return true;
}

bool AMallLightRemoteGun::TrySecondaryUse(APlayerController* Player)
{
	if (!CanPlayerControlDevice(Player) || !GetWorld())
	{
		return false;
	}

	bPowered = true;
	FVector Origin, Direction;
	GetActiveAimRay(Origin, Direction);

	AMallLightFixture* Fixture = nullptr;
	AMallRetailStoreUnit* Store = nullptr;
	FString TargetName;
	bool bIsOn = false;
	uint8 Mode = 0;

	auto ModeLabel = [](uint8 InMode) -> const TCHAR*
	{
		return (InMode == 0) ? TEXT("OFF (0% DARK)")
			: (InMode == 1) ? TEXT("GRAY-SPACE DIM (45%)")
			: TEXT("WELL-LIT ON (100%)");
	};

	if (FindAimedLightTarget(Origin, Direction, Fixture, Store, TargetName, bIsOn, Mode))
	{
		if (Fixture)
		{
			const uint8 NewMode = Fixture->CycleBrightnessMode();
			AimedTargetTitle = Fixture->FixtureLabel;
			bAimedTargetPowered = Fixture->bLightPowered;
			AimedTargetBrightnessMode = NewMode;
			++TotalLightsToggled;
			LastActionFeedback = FString::Printf(TEXT("LIGHT REMOTE: %s -> %s"), *Fixture->FixtureLabel, ModeLabel(NewMode));
		}
		else if (Store)
		{
			const uint8 NewMode = Store->CycleStoreLightBrightness();
			AimedTargetTitle = TargetName;
			bAimedTargetPowered = Store->bStoreLightPowered;
			AimedTargetBrightnessMode = NewMode;
			++TotalLightsToggled;
			LastActionFeedback = FString::Printf(TEXT("LIGHT REMOTE: %s -> %s"), *TargetName, ModeLabel(NewMode));
		}
	}
	else
	{
		// Area cycle for all fixtures within 18m if not aiming at a single fixture
		int32 Count = 0;
		uint8 AppliedMode = 2;
		for (TActorIterator<AMallLightFixture> It(GetWorld()); It; ++It)
		{
			if (IsValid(*It) && FVector::DistSquared(Origin, It->GetActorLocation()) <= FMath::Square(1800.0))
			{
				AppliedMode = It->CycleBrightnessMode();
				++Count;
			}
		}
		LastActionFeedback = (Count > 0)
			? FString::Printf(TEXT("LIGHT REMOTE ZONE CYCLE: %d nearby fixtures -> %s"), Count, ModeLabel(AppliedMode))
			: TEXT("Aim at any ceiling light fixture and press E to cycle 100% -> 45% Gray-Space -> OFF.");
	}

	UpdateDeviceVisuals();
	ForceNetUpdate();
	return true;
}

void AMallLightRemoteGun::TickServerDetection(float DeltaSeconds)
{
	if (!GetWorld() || (!IsValid(Carrier) && bCanUseWhilePlaced == false))
	{
		return;
	}
	FVector Origin, Direction;
	GetActiveAimRay(Origin, Direction);

	AMallLightFixture* Fixture = nullptr;
	AMallRetailStoreUnit* Store = nullptr;
	FString TargetName;
	bool bIsOn = false;
	uint8 Mode = 0;
	FindAimedLightTarget(Origin, Direction, Fixture, Store, TargetName, bIsOn, Mode);

	if (AimedTargetTitle != TargetName || bAimedTargetPowered != bIsOn || AimedTargetBrightnessMode != Mode)
	{
		AimedTargetTitle = TargetName;
		bAimedTargetPowered = bIsOn;
		AimedTargetBrightnessMode = Mode;
		UpdateDeviceVisuals();
		ForceNetUpdate();
	}
}

void AMallLightRemoteGun::OnRep_RemoteGunState()
{
	UpdateDeviceVisuals();
}

void AMallLightRemoteGun::UpdateDeviceVisuals()
{
	const bool bHasTarget = !AimedTargetTitle.IsEmpty();
	const FColor DisplayColor = !bHasTarget
		? FColor(150, 225, 255)
		: (!bAimedTargetPowered || AimedTargetBrightnessMode == 0)
			? FColor(255, 110, 95)
			: (AimedTargetBrightnessMode == 1 ? FColor(255, 215, 95) : FColor(95, 255, 165));

	if (ReadoutText)
	{
		if (bHasTarget)
		{
			const TCHAR* StateTag = (!bAimedTargetPowered || AimedTargetBrightnessMode == 0) ? TEXT("OFF")
				: (AimedTargetBrightnessMode == 1 ? TEXT("DIM 45%") : TEXT("ON 100%"));
			ReadoutText->SetText(FText::FromString(FString::Printf(TEXT("TARGET: %s [%s]"), *AimedTargetTitle.Left(20), StateTag)));
		}
		else
		{
			ReadoutText->SetText(FText::FromString(TEXT("AIM AT ANY LIGHT MODEL")));
		}
		ReadoutText->SetTextRenderColor(DisplayColor);
	}
	if (LaserPointerLight)
	{
		const bool bShowBeam = IsPowered() && (!IsValid(Carrier) || IsSelectedByCarrier());
		LaserPointerLight->SetVisibility(bShowBeam);
		LaserPointerLight->SetIntensity(bShowBeam ? 55.0f : 0.0f);
		LaserPointerLight->SetLightColor(FLinearColor(DisplayColor));
	}
	if (StatusLight)
	{
		StatusLight->SetLightColor(FLinearColor(DisplayColor));
		StatusLight->SetIntensity(IsPowered() ? 85.0f : 0.0f);
	}
}

FString AMallLightRemoteGun::GetHUDPrompt(bool bIsHeld) const
{
	const FString TargetInfo = AimedTargetTitle.IsEmpty()
		? TEXT("Aim at any Light Fixture")
		: FString::Printf(TEXT("Target: %s [%s]"),
			*AimedTargetTitle,
			(!bAimedTargetPowered || AimedTargetBrightnessMode == 0) ? TEXT("OFF")
				: (AimedTargetBrightnessMode == 1 ? TEXT("45% GRAY-SPACE") : TEXT("100% ON")));
	return FString::Printf(TEXT("Light Remote Gun (%s)  |  LMB: Toggle Light ON/OFF  |  E: Cycle Dim (100%%/45%%/OFF)  |  F: %s%s"),
		*TargetInfo,
		bIsHeld ? TEXT("Place / Store on Cart") : TEXT("Pick Up"),
		bIsHeld ? TEXT("  |  G: Drop") : TEXT(""));
}

FString AMallLightRemoteGun::GetHUDDetail() const
{
	if (!LastActionFeedback.IsEmpty())
	{
		return LastActionFeedback;
	}
	return TEXT("Aim at any ceiling light fixture model in corridors, atrium, or shops: LMB toggles ON/OFF, E cycles Well-Lit (100%) / Gray-Space (45%) / OFF.");
}

FString AMallLightRemoteGun::GetPickupFeedback() const
{
	return TEXT("Light Control Remote Gun equipped! Aim at any light fixture model: LMB toggles ON/OFF | E cycles brightness (100% / 45% Gray-Space / OFF).");
}

FString AMallLightRemoteGun::GetPrimaryUseFeedback() const
{
	return LastActionFeedback.IsEmpty() ? TEXT("Light Remote fired.") : LastActionFeedback;
}

FString AMallLightRemoteGun::GetSecondaryUseFeedback() const
{
	return LastActionFeedback.IsEmpty() ? TEXT("Light Remote brightness cycled.") : LastActionFeedback;
}
