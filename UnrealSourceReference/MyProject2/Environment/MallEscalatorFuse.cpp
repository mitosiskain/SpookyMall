#include "Environment/MallEscalatorFuse.h"
#include "MallEscalator.h"
#include "Equipment/MallHotbarInventory.h"
#include "Equipment/MallPushCart.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AMallEscalatorFuse::AMallEscalatorFuse()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	SetRootComponent(InteractionBounds);
	InteractionBounds->SetBoxExtent(FVector(24.0f, 16.0f, 14.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	FuseBodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FuseBodyMesh"));
	FuseBodyMesh->SetupAttachment(InteractionBounds);
	FuseBodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FuseBodyMesh->SetRelativeScale3D(FVector(0.32f, 0.16f, 0.14f));

	FuseCoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FuseCoreMesh"));
	FuseCoreMesh->SetupAttachment(FuseBodyMesh);
	FuseCoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FuseCoreMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 35.0f));
	FuseCoreMesh->SetRelativeScale3D(FVector(0.75f, 0.55f, 0.45f));

	IndicatorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("IndicatorLight"));
	IndicatorLight->SetupAttachment(InteractionBounds);
	IndicatorLight->SetRelativeLocation(FVector(0.0f, 0.0f, 18.0f));
	IndicatorLight->SetIntensity(350.0f);
	IndicatorLight->SetAttenuationRadius(180.0f);
	IndicatorLight->SetLightColor(FLinearColor(0.15f, 0.85f, 1.0f));
	IndicatorLight->SetCastShadows(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		FuseBodyMesh->SetStaticMesh(CubeFinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		FuseCoreMesh->SetStaticMesh(CylinderFinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MetalMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_metal.M_MallRetail_metal"));
	if (MetalMat.Succeeded())
	{
		FuseBodyMesh->SetMaterial(0, MetalMat.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> AccentMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_accent.M_MallRetail_accent"));
	if (AccentMat.Succeeded())
	{
		FuseCoreMesh->SetMaterial(0, AccentMat.Object);
	}
	static ConstructorHelpers::FObjectFinder<UTexture2D> DefaultIcon(TEXT("/Game/Equipment/Flashlight/Textures/base_color.base_color"));
	if (DefaultIcon.Succeeded())
	{
		HotbarIcon = DefaultIcon.Object;
	}
}

void AMallEscalatorFuse::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallEscalatorFuse, Carrier);
	DOREPLIFETIME(AMallEscalatorFuse, StoredOnCart);
	DOREPLIFETIME(AMallEscalatorFuse, bConsumedInEscalator);
	DOREPLIFETIME(AMallEscalatorFuse, InventorySlotIndex);
}

void AMallEscalatorFuse::BeginPlay()
{
	Super::BeginPlay();
	LastWorldTransform = GetActorTransform();
	RefreshVisualAndHotbarState();
}

FText AMallEscalatorFuse::GetHotbarDisplayName()
{
	return FText::FromString(TEXT("Escalator Fuse"));
}

AMallEscalatorFuse* AMallEscalatorFuse::FindCarriedBy(const APawn* Pawn)
{
	if (!IsValid(Pawn) || !Pawn->GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<AMallEscalatorFuse> It(Pawn->GetWorld()); It; ++It)
	{
		if (!It->bConsumedInEscalator && It->Carrier == Pawn)
		{
			return *It;
		}
	}
	return nullptr;
}

AMallEscalatorFuse* AMallEscalatorFuse::FindFuseInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction)
{
	if (!IsValid(Pawn) || !Pawn->GetWorld())
	{
		return nullptr;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallEscalatorFuseFocus), true, Pawn);
	FHitResult Hit;
	if (Pawn->GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 850.0f, ECC_Visibility, Params))
	{
		if (AMallEscalatorFuse* Fuse = Cast<AMallEscalatorFuse>(Hit.GetActor()))
		{
			if (!Fuse->bConsumedInEscalator && !Fuse->Carrier && !Fuse->StoredOnCart &&
				FVector::DistSquared(Pawn->GetActorLocation(), Fuse->GetActorLocation()) <= FMath::Square(320.0f))
			{
				return Fuse;
			}
		}
	}
	return nullptr;
}

bool AMallEscalatorFuse::TryPickup(APlayerController* Player)
{
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!HasAuthority() || !IsValid(Pawn) || bConsumedInEscalator || Carrier != nullptr || StoredOnCart != nullptr)
	{
		return false;
	}
	if (!MallHotbarInventory::CanAcceptItem(Pawn, GetHotbarDisplayName()))
	{
		return false;
	}
	LastWorldTransform = GetActorTransform();
	Carrier = Pawn;
	InventorySlotIndex = MallHotbarInventory::EnsureItemInSlot(
		Pawn, GetHotbarDisplayName(), HotbarIcon, 1.0, false, INDEX_NONE, true);
	RefreshVisualAndHotbarState();
	ForceNetUpdate();
	return true;
}

bool AMallEscalatorFuse::TryDrop(APlayerController* Player)
{
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!HasAuthority() || !IsValid(Pawn) || Carrier != Pawn || bConsumedInEscalator)
	{
		return false;
	}
	MallHotbarInventory::RemoveItem(Pawn, GetHotbarDisplayName(), InventorySlotIndex);
	Carrier = nullptr;
	InventorySlotIndex = INDEX_NONE;

	const FVector DropLoc = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 85.0f - FVector(0.0f, 0.0f, 55.0f);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocationAndRotation(DropLoc, FRotator(0.0f, Pawn->GetActorRotation().Yaw, 0.0f));
	LastWorldTransform = GetActorTransform();
	RefreshVisualAndHotbarState();
	ForceNetUpdate();
	return true;
}

bool AMallEscalatorFuse::ConsumeForEscalatorRepair(APawn* ConsumingPawn)
{
	if (!HasAuthority() || bConsumedInEscalator)
	{
		return false;
	}
	if (IsValid(ConsumingPawn))
	{
		MallHotbarInventory::RemoveItem(ConsumingPawn, GetHotbarDisplayName(), InventorySlotIndex);
	}
	Carrier = nullptr;
	StoredOnCart = nullptr;
	InventorySlotIndex = INDEX_NONE;
	bConsumedInEscalator = true;
	RefreshVisualAndHotbarState();
	ForceNetUpdate();
	return true;
}

bool AMallEscalatorFuse::TryStoreOnCart_Implementation(APlayerController* Player, AMallPushCart* Cart, const FTransform& WorldTransform)
{
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!HasAuthority() || !IsValid(Pawn) || !IsValid(Cart) || Carrier != Pawn || bConsumedInEscalator)
	{
		return false;
	}
	MallHotbarInventory::RemoveItem(Pawn, GetHotbarDisplayName(), InventorySlotIndex);
	Carrier = nullptr;
	InventorySlotIndex = INDEX_NONE;
	StoredOnCart = Cart;
	SetActorTransform(WorldTransform);
	AttachToActor(Cart, FAttachmentTransformRules::KeepWorldTransform);
	RefreshVisualAndHotbarState();
	ForceNetUpdate();
	return true;
}

bool AMallEscalatorFuse::TryTakeFromCart_Implementation(APlayerController* Player, AMallPushCart* Cart)
{
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!HasAuthority() || !IsValid(Pawn) || StoredOnCart != Cart || bConsumedInEscalator)
	{
		return false;
	}
	if (!MallHotbarInventory::CanAcceptItem(Pawn, GetHotbarDisplayName()))
	{
		return false;
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	StoredOnCart = nullptr;
	Carrier = Pawn;
	InventorySlotIndex = MallHotbarInventory::EnsureItemInSlot(
		Pawn, GetHotbarDisplayName(), HotbarIcon, 1.0, false, INDEX_NONE, true);
	RefreshVisualAndHotbarState();
	ForceNetUpdate();
	return true;
}

void AMallEscalatorFuse::ReleaseFromCart_Implementation(AMallPushCart* Cart, const FTransform& WorldTransform)
{
	if (!HasAuthority() || StoredOnCart != Cart)
	{
		return;
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	StoredOnCart = nullptr;
	SetActorTransform(WorldTransform);
	LastWorldTransform = WorldTransform;
	RefreshVisualAndHotbarState();
	ForceNetUpdate();
}

void AMallEscalatorFuse::OnRep_FuseState()
{
	RefreshVisualAndHotbarState();
}

void AMallEscalatorFuse::RefreshVisualAndHotbarState()
{
	if (PreviousCarrier.IsValid() && PreviousCarrier.Get() != Carrier)
	{
		MallHotbarInventory::RemoveItem(PreviousCarrier.Get(), GetHotbarDisplayName(), InventorySlotIndex);
	}
	if (IsValid(Carrier))
	{
		MallHotbarInventory::EnsureItemInSlot(
			Carrier, GetHotbarDisplayName(), HotbarIcon, 1.0, false, InventorySlotIndex, false);
	}
	PreviousCarrier = Carrier;

	const bool bSelectedInHand = IsValid(Carrier) && (!MallHotbarInventory::HasInventory(Carrier)
		|| MallHotbarInventory::GetSelectedSlot(Carrier) == InventorySlotIndex);
	const bool bVisible = !bConsumedInEscalator && (Carrier == nullptr || bSelectedInHand);
	SetActorHiddenInGame(!bVisible);
	InteractionBounds->SetCollisionEnabled(
		(!bConsumedInEscalator && Carrier == nullptr && StoredOnCart == nullptr)
			? ECollisionEnabled::QueryOnly
			: ECollisionEnabled::NoCollision);
	if (IndicatorLight)
	{
		IndicatorLight->SetVisibility(bVisible);
	}
}

void AMallEscalatorFuse::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsValid(Carrier))
	{
		FRotator ControlRot = Carrier->GetActorRotation();
		if (AController* Controller = Carrier->GetController())
		{
			ControlRot = Controller->GetControlRotation();
		}
		const FRotator FirstPersonRot(FMath::ClampAngle(ControlRot.Pitch, -50.0f, 50.0f), ControlRot.Yaw, 0.0);
		FVector EyeOrigin = Carrier->GetPawnViewLocation();
		if (EyeOrigin.ContainsNaN() || EyeOrigin.IsZero()) { EyeOrigin = Carrier->GetActorLocation() + FVector(0.0, 0.0, 64.0); }
		SetActorLocationAndRotation(
			EyeOrigin + FirstPersonRot.RotateVector(FVector(36.0, 16.0, -13.0)),
			FirstPersonRot,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		RefreshVisualAndHotbarState();
	}
}

// ============================================================================
// AMallEscalatorBreakerPanel
// ============================================================================

AMallEscalatorBreakerPanel::AMallEscalatorBreakerPanel()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	PanelBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("PanelBounds"));
	SetRootComponent(PanelBounds);
	PanelBounds->SetBoxExtent(FVector(16.0f, 35.0f, 55.0f));
	PanelBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PanelBounds->SetCollisionObjectType(ECC_WorldDynamic);
	PanelBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	PanelBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	CabinetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CabinetMesh"));
	CabinetMesh->SetupAttachment(PanelBounds);
	CabinetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CabinetMesh->SetRelativeScale3D(FVector(0.18f, 0.55f, 0.90f));

	HandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandleMesh"));
	HandleMesh->SetupAttachment(PanelBounds);
	HandleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HandleMesh->SetRelativeLocation(FVector(12.0f, 0.0f, 0.0f));
	HandleMesh->SetRelativeScale3D(FVector(0.12f, 0.18f, 0.08f));

	StatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StatusLight"));
	StatusLight->SetupAttachment(PanelBounds);
	StatusLight->SetRelativeLocation(FVector(22.0f, 0.0f, 38.0f));
	StatusLight->SetIntensity(600.0f);
	StatusLight->SetAttenuationRadius(240.0f);
	StatusLight->SetCastShadows(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		CabinetMesh->SetStaticMesh(CubeFinder.Object);
		HandleMesh->SetStaticMesh(CubeFinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DarkMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_dark.M_MallRetail_dark"));
	if (DarkMat.Succeeded())
	{
		CabinetMesh->SetMaterial(0, DarkMat.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> AccentMat(TEXT("/Game/Environment/Mall/Materials/M_MallRetail_accent.M_MallRetail_accent"));
	if (AccentMat.Succeeded())
	{
		HandleMesh->SetMaterial(0, AccentMat.Object);
	}
}

void AMallEscalatorBreakerPanel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallEscalatorBreakerPanel, bBreakerTripped);
}

void AMallEscalatorBreakerPanel::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshBreakerAppearance();
}

void AMallEscalatorBreakerPanel::BeginPlay()
{
	Super::BeginPlay();
	RefreshBreakerAppearance();
}

void AMallEscalatorBreakerPanel::OnRep_BreakerTripped()
{
	RefreshBreakerAppearance();
}

void AMallEscalatorBreakerPanel::RefreshBreakerAppearance()
{
	if (HandleMesh)
	{
		HandleMesh->SetRelativeRotation(FRotator(bBreakerTripped ? -35.0f : 35.0f, 0.0f, 0.0f));
	}
	if (StatusLight)
	{
		StatusLight->SetLightColor(bBreakerTripped
			? FLinearColor(1.0f, 0.18f, 0.05f)
			: FLinearColor(0.1f, 1.0f, 0.35f));
	}
}

bool AMallEscalatorBreakerPanel::TryResetBreaker(APlayerController* Player, FString& OutFeedback)
{
	if (!HasAuthority() || !GetWorld())
	{
		return false;
	}
	if (!bBreakerTripped)
	{
		OutFeedback = TEXT("Escalator Breaker is already energized (ONLINE).");
		return true;
	}
	bBreakerTripped = false;
	RefreshBreakerAppearance();
	ForceNetUpdate();

	int32 RepairedCount = 0;
	for (TActorIterator<AMallEscalator> It(GetWorld()); It; ++It)
	{
		if (LinkedBreakerTag != NAME_None && It->LinkedBreakerTag == LinkedBreakerTag)
		{
			It->SetMaintenanceState(EEscalatorMaintenanceState::Operational);
			++RepairedCount;
		}
	}
	OutFeedback = FString::Printf(
		TEXT("Escalator Breaker [%s] reset! %d high-rise escalator(s) restored to service."),
		*LinkedBreakerTag.ToString(), RepairedCount);
	return true;
}

AMallEscalatorBreakerPanel* AMallEscalatorBreakerPanel::FindBreakerInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction)
{
	if (!IsValid(Pawn) || !Pawn->GetWorld())
	{
		return nullptr;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallBreakerFocus), true, Pawn);
	FHitResult Hit;
	if (Pawn->GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 850.0f, ECC_Visibility, Params))
	{
		if (AMallEscalatorBreakerPanel* Panel = Cast<AMallEscalatorBreakerPanel>(Hit.GetActor()))
		{
			if (FVector::DistSquared(Pawn->GetActorLocation(), Panel->GetActorLocation()) <= FMath::Square(350.0f))
			{
				return Panel;
			}
		}
	}
	return nullptr;
}
