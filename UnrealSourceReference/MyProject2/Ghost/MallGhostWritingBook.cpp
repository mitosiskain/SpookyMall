#include "Ghost/MallGhostWritingBook.h"
#include "Ghost/MallBookInteractionComponent.h"
#include "Equipment/MallHotbarInventory.h"
#include "Equipment/MallPushCart.h"

#include "Ghost/MallGhostInteractionComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace MallBookInteraction
{
	constexpr double ReachCm = 300.0;

	bool IsAlive(const APawn* Pawn)
	{
		if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed()) { return false; }
		// The project character currently exposes Blueprint Health; characters without
		// a health system remain usable, while a dead or invalid health value fails closed.
		if (const FNumericProperty* Health = FindFProperty<FNumericProperty>(Pawn->GetClass(), TEXT("Health")))
		{
			const void* Value = Health->ContainerPtrToValuePtr<void>(Pawn);
			const double Amount = Health->IsFloatingPoint() ? Health->GetFloatingPointPropertyValue(Value)
				: static_cast<double>(Health->GetSignedIntPropertyValue(Value));
			if (!FMath::IsFinite(Amount) || Amount <= 0.0) { return false; }
		}
		if (const FBoolProperty* Dead = FindFProperty<FBoolProperty>(Pawn->GetClass(), TEXT("bIsDead")))
		{
			if (Dead->GetPropertyValue_InContainer(Pawn)) { return false; }
		}
		return true;
	}
}

AMallGhostWritingBook::AMallGhostWritingBook()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.04f;
	SetNetUpdateFrequency(20.0f);
	BookRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BookRoot"));
	BookRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(BookRoot);
	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(BookRoot);
	InteractionBounds->SetMobility(EComponentMobility::Movable);
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionBounds->SetGenerateOverlapEvents(false);
	InteractionBounds->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	auto AddPart = [this](const TCHAR* Name, UStaticMesh* Mesh, const FVector& Position, const FVector& Scale)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(BookRoot);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeLocation(Position);
		Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		return Part;
	};
	BookModel = AddPart(TEXT("BookModel"), nullptr, FVector::ZeroVector, FVector::OneVector);
	LeftCover = AddPart(TEXT("LeftCover"), Cube.Object, FVector(-10.0, 0.0, 0.2), FVector(0.203, 0.300, 0.004));
	RightCover = AddPart(TEXT("RightCover"), Cube.Object, FVector(10.0, 0.0, 0.2), FVector(0.203, 0.300, 0.004));
	LeftPages = AddPart(TEXT("LeftPages"), Cube.Object, FVector(-10.0, 0.0, 0.7), FVector(0.195, 0.285, 0.006));
	RightPages = AddPart(TEXT("RightPages"), Cube.Object, FVector(10.0, 0.0, 0.7), FVector(0.195, 0.285, 0.006));
	WritingPage = AddPart(TEXT("WritingPage"), Plane.Object, FVector(10.0, 0.0, 1.015), FVector(0.195, 0.285, 1.0));

	FallbackWriting = CreateDefaultSubobject<UTextRenderComponent>(TEXT("FallbackWriting"));
	FallbackWriting->SetupAttachment(BookRoot);
	FallbackWriting->SetRelativeLocation(FVector(10.0, 5.0, 1.05));
	// TextRender's normal is local +X and its text-up axis is local +Z.
	FallbackWriting->SetRelativeRotation(FRotationMatrix::MakeFromXZ(FVector::UpVector, FVector::ForwardVector).Rotator());
	FallbackWriting->SetHorizontalAlignment(EHTA_Center);
	FallbackWriting->SetVerticalAlignment(EVRTA_TextCenter);
	FallbackWriting->SetWorldSize(3.3f);
	FallbackWriting->SetTextRenderColor(FColor(45, 30, 25));
	FallbackWriting->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FallbackWriting->SetCanEverAffectNavigation(false);

	DisplayName = FText::FromString(TEXT("Ghost Writing Book"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> BookIcon(TEXT("/Game/Environment/Mall/Textures/book_hardcover_01_cover02_2k.book_hardcover_01_cover02_2k"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> FallbackIcon(TEXT("/Game/Equipment/Flashlight/Textures/base_color.base_color"));
	if (BookIcon.Succeeded()) { Icon = BookIcon.Object; }
	else if (FallbackIcon.Succeeded()) { Icon = FallbackIcon.Object; }
}

void AMallGhostWritingBook::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallGhostWritingBook, bWritten);
	DOREPLIFETIME(AMallGhostWritingBook, WritingStartedAtServerTime);
	DOREPLIFETIME(AMallGhostWritingBook, BookMesh);
	DOREPLIFETIME(AMallGhostWritingBook, PageMaterial);
	DOREPLIFETIME(AMallGhostWritingBook, WritingTexture);
	DOREPLIFETIME(AMallGhostWritingBook, WritingDuration);
	DOREPLIFETIME(AMallGhostWritingBook, TestWriting);
	DOREPLIFETIME(AMallGhostWritingBook, Carrier);
	DOREPLIFETIME(AMallGhostWritingBook, StoredOnCart);
	DOREPLIFETIME(AMallGhostWritingBook, bOpen);
	DOREPLIFETIME(AMallGhostWritingBook, CarriedHotbarSlot);
	DOREPLIFETIME(AMallGhostWritingBook, CarrierSelectedSlot);
}

void AMallGhostWritingBook::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisuals();
}

void AMallGhostWritingBook::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && !Carrier)
	{
		LastPlacedTransform = GetActorTransform();
		bHasLastPlacedTransform = true;
	}
	RefreshVisuals();
}

void AMallGhostWritingBook::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(Carrier)) { Carrier->OnDestroyed.RemoveDynamic(this, &AMallGhostWritingBook::OnCarrierDestroyed); }
	if (LastSyncedCarrier.IsValid())
	{
		MallHotbarInventory::RemoveItem(LastSyncedCarrier.Get(), DisplayName, LastSyncedSlot);
		LastSyncedCarrier.Reset();
		LastSyncedSlot = INDEX_NONE;
	}
	Super::EndPlay(EndPlayReason);
}

void AMallGhostWritingBook::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && Carrier)
	{
		APlayerController* Player = CarrierController.Get();
		if (!IsPlayerEligible(Player) || Player->GetPawn() != Carrier) { RestoreLastPlacement(); }
		else { FollowCarrier(); }
	}
	if (IsValid(Carrier))
	{
		const bool bCurrentlySelected = IsSelectedByCarrier();
		if (bCurrentlySelected != bLastSelectedByCarrier)
		{
			bLastSelectedByCarrier = bCurrentlySelected;
			ApplyInteractionPresentation();
		}
	}
	if (bWritten && WritingProgress < 1.0f)
	{
		UpdateWritingProgress();
	}
}

bool AMallGhostWritingBook::IsSelectedByCarrier() const
{
	if (!IsValid(Carrier)) { return false; }
	if (!MallHotbarInventory::HasInventory(Carrier) || CarriedHotbarSlot == INDEX_NONE) { return true; }
	if (Carrier->IsLocallyControlled() || HasAuthority())
	{
		const int32 LocalSlot = MallHotbarInventory::GetSelectedSlot(Carrier);
		return LocalSlot == INDEX_NONE || LocalSlot == CarriedHotbarSlot;
	}
	return CarrierSelectedSlot == CarriedHotbarSlot;
}

void AMallGhostWritingBook::SetCarrierSelectedSlot(int32 NewSelectedSlot)
{
	if (CarrierSelectedSlot != NewSelectedSlot)
	{
		CarrierSelectedSlot = NewSelectedSlot;
		ApplyInteractionPresentation();
		if (HasAuthority()) { ForceNetUpdate(); }
	}
}

void AMallGhostWritingBook::SyncHotbarState()
{
	if (LastSyncedCarrier.IsValid() && LastSyncedCarrier.Get() != Carrier)
	{
		MallHotbarInventory::RemoveItem(LastSyncedCarrier.Get(), DisplayName, LastSyncedSlot);
		LastSyncedCarrier.Reset();
		LastSyncedSlot = INDEX_NONE;
	}
	if (IsValid(Carrier))
	{
		const bool bInitialEquip = (LastSyncedCarrier.Get() != Carrier);
		const int32 Slot = MallHotbarInventory::EnsureItemInSlot(Carrier, DisplayName, Icon, 0.0, false, CarriedHotbarSlot, bInitialEquip);
		if (Slot != INDEX_NONE)
		{
			if (HasAuthority())
			{
				CarriedHotbarSlot = Slot;
				if (bInitialEquip) { CarrierSelectedSlot = Slot; }
			}
			LastSyncedCarrier = Carrier;
			LastSyncedSlot = Slot;
		}
		bLastSelectedByCarrier = IsSelectedByCarrier();
	}
}

APawn* AMallGhostWritingBook::GetCarrier() const
{
	return IsValid(Carrier) ? Carrier.Get() : nullptr;
}

bool AMallGhostWritingBook::IsPlacedForWriting() const
{
	return Carrier == nullptr && StoredOnCart == nullptr && bOpen;
}

bool AMallGhostWritingBook::IsPlayerEligible(APlayerController* Player) const
{
	const APawn* Pawn = IsValid(Player) ? Player->GetPawn() : nullptr;
	return HasAuthority() && IsValid(Player) && !Player->IsActorBeingDestroyed()
		&& Player->HasAuthority() && Player->GetWorld() == GetWorld()
		&& MallBookInteraction::IsAlive(Pawn) && Pawn->HasAuthority()
		&& Pawn->GetWorld() == GetWorld() && Pawn->GetController() == Player;
}

bool AMallGhostWritingBook::CanReachPlacedBook(APlayerController* Player) const
{
	if (!IsPlayerEligible(Player) || Carrier || StoredOnCart
		|| FVector::DistSquared(Player->GetPawn()->GetActorLocation(), GetActorLocation()) > FMath::Square(MallBookInteraction::ReachCm))
	{
		return false;
	}
	FVector Eye;
	FRotator View;
	Player->GetPlayerViewPoint(Eye, View);
	if (Eye.ContainsNaN()) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallBookReach), true, this);
	Params.AddIgnoredActor(Player->GetPawn());
	FHitResult Hit;
	const FVector Target = GetActorLocation() + FVector(0.0, 0.0, 0.8);
	const FVector PawnEye = Player->GetPawn()->GetPawnViewLocation();
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params)
		|| (!PawnEye.ContainsNaN() && !GetWorld()->LineTraceSingleByChannel(Hit, PawnEye, Target, ECC_Visibility, Params));
}


bool AMallGhostWritingBook::TryStoreOnCart_Implementation(APlayerController* Player, AMallPushCart* Cart, const FTransform& WorldTransform)
{
	if (!IsPlayerEligible(Player) || Carrier != Player->GetPawn() || StoredOnCart || !IsValid(Cart)
		|| !Cart->HasAuthority() || Cart->GetWorld() != GetWorld() || !Cart->IsParked()
		|| !Cart->CanPlayerReach(Player) || WorldTransform.ContainsNaN()
		|| !WorldTransform.IsValid() || !WorldTransform.GetScale3D().Equals(FVector::OneVector, .001)
		|| FVector::DistSquared(WorldTransform.GetLocation(), Cart->GetActorLocation()) > FMath::Square(120.0)) { return false; }
	ClearCarrier();
	StoredOnCart = Cart;
	bOpen = false;
	SetActorTransform(WorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
	AttachToActor(Cart, FAttachmentTransformRules::KeepWorldTransform);
	OnRep_InteractionState();
	ForceNetUpdate();
	return true;
}

bool AMallGhostWritingBook::TryTakeFromCart_Implementation(APlayerController* Player, AMallPushCart* Cart)
{
	UMallBookInteractionComponent* Endpoint = IsValid(Player) ? Player->FindComponentByClass<UMallBookInteractionComponent>() : nullptr;
	if (!IsPlayerEligible(Player) || !IsValid(Cart) || StoredOnCart != Cart || !Cart->ContainsStoredItem(this)
		|| !Cart->IsParked() || !Cart->CanPlayerReach(Player) || !Endpoint || !Endpoint->CanAcceptCartItem(this)
		|| !MallHotbarInventory::CanAcceptItem(Player->GetPawn(), DisplayName)) { return false; }
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	StoredOnCart = nullptr;
	APawn* Pawn = Player->GetPawn();
	const int32 FreeSlot = MallHotbarInventory::HasInventory(Pawn) ? MallHotbarInventory::FindFreeSlot(Pawn, DisplayName) : 0;
	CarriedHotbarSlot = FreeSlot;
	CarrierSelectedSlot = FreeSlot;
	Carrier = Pawn;
	CarrierController = Player;
	Carrier->OnDestroyed.AddUniqueDynamic(this, &AMallGhostWritingBook::OnCarrierDestroyed);
	FollowCarrier();
	OnRep_InteractionState();
	Endpoint->AdoptCartItem(this);
	ForceNetUpdate();
	return true;
}

void AMallGhostWritingBook::ReleaseFromCart_Implementation(AMallPushCart* Cart, const FTransform& WorldTransform)
{
	if (!HasAuthority() || !Cart || StoredOnCart != Cart) { return; }
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	StoredOnCart = nullptr;
	// Recover the last validated placement if a cart is deleted during play.
	if (bHasLastPlacedTransform) { SetActorTransform(LastPlacedTransform, false, nullptr, ETeleportType::TeleportPhysics); }
	else if (!WorldTransform.ContainsNaN()) { SetActorTransform(WorldTransform, false, nullptr, ETeleportType::TeleportPhysics); }
	OnRep_InteractionState();
	ForceNetUpdate();
}

bool AMallGhostWritingBook::TryPickup(APlayerController* Player)
{
	if (!CanReachPlacedBook(Player)) { return false; }
	APawn* Pawn = Player->GetPawn();
	if (!MallHotbarInventory::CanAcceptItem(Pawn, DisplayName)) { return false; }
	for (TActorIterator<AMallGhostWritingBook> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->Carrier == Pawn) { return false; }
	}
	LastPlacedTransform = GetActorTransform();
	bHasLastPlacedTransform = true;
	const int32 FreeSlot = MallHotbarInventory::HasInventory(Pawn) ? MallHotbarInventory::FindFreeSlot(Pawn, DisplayName) : 0;
	CarriedHotbarSlot = FreeSlot;
	CarrierSelectedSlot = FreeSlot;
	Carrier = Pawn;
	CarrierController = Player;
	Carrier->OnDestroyed.AddUniqueDynamic(this, &AMallGhostWritingBook::OnCarrierDestroyed);
	FollowCarrier();
	OnRep_InteractionState();
	ForceNetUpdate();
	return true;
}

bool AMallGhostWritingBook::ValidatePlacement(APlayerController* Player, const FTransform& Requested, FTransform& Supported) const
{
	if (!IsPlayerEligible(Player) || Requested.ContainsNaN() || !Requested.IsValid()
		|| !Requested.GetScale3D().Equals(FVector::OneVector, 0.001)
		|| FVector::DistSquared(Player->GetPawn()->GetActorLocation(), Requested.GetLocation()) > FMath::Square(MallBookInteraction::ReachCm))
	{
		return false;
	}
	const FRotator Rotation = Requested.Rotator().GetNormalized();
	if (FMath::Abs(Rotation.Pitch) > 5.0 || FMath::Abs(Rotation.Roll) > 5.0) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallBookPlace), true, this);
	Params.AddIgnoredActor(Player->GetPawn());
	auto FindSupport = [this, &Params](const FVector& Point, FHitResult& Hit)
	{
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Point + FVector(0.0, 0.0, 20.0), Point - FVector(0.0, 0.0, 25.0), ECC_Visibility, Params)
			|| Hit.ImpactNormal.Z < 0.7 || !IsValid(Hit.GetComponent())
			|| Hit.GetComponent()->IsSimulatingPhysics() || Cast<APawn>(Hit.GetActor()) || Cast<AMallGhostWritingBook>(Hit.GetActor()))
		{
			return false;
		}
		return true;
	};
	FHitResult Center;
	if (!FindSupport(Requested.GetLocation(), Center) || FMath::Abs(Center.ImpactPoint.Z - Requested.GetLocation().Z) > 8.0) { return false; }
	const FRotator FlatRotation(0.0, Rotation.Yaw, 0.0);
	// Validate the complete open footprint, so opening a placed book cannot hang
	// it through an edge. A single center hit is insufficient for a tabletop.
	for (const double X : {-20.3, 20.3})
	{
		for (const double Y : {-15.0, 15.0})
		{
			FHitResult Corner;
			const FVector Position = Requested.GetLocation() + FlatRotation.RotateVector(FVector(X, Y, 0.0));
			if (!FindSupport(Position, Corner) || FMath::Abs(Corner.ImpactPoint.Z - Center.ImpactPoint.Z) > 2.0) { return false; }
		}
	}
	Supported = FTransform(FlatRotation, FVector(Requested.GetLocation().X, Requested.GetLocation().Y, Center.ImpactPoint.Z + 0.3), FVector::OneVector);
	return true;
}

bool AMallGhostWritingBook::TryPlace(APlayerController* Player, const FTransform& Placement)
{
	FTransform Supported;
	if (!IsPlayerEligible(Player) || Carrier != Player->GetPawn() || CarrierController.Get() != Player
		|| !ValidatePlacement(Player, Placement, Supported))
	{
		return false;
	}
	ClearCarrier();
	SetActorTransform(Supported, false, nullptr, ETeleportType::TeleportPhysics);
	LastPlacedTransform = Supported;
	bHasLastPlacedTransform = true;
	OnRep_InteractionState();
	ForceNetUpdate();
	return true;
}

bool AMallGhostWritingBook::TrySetOpen(APlayerController* Player, const bool bNewOpen)
{
	if (!IsPlayerEligible(Player)) { return false; }
	if (Carrier)
	{
		if (Carrier != Player->GetPawn() || CarrierController.Get() != Player) { return false; }
	}
	else if (!CanReachPlacedBook(Player)) { return false; }
	bOpen = bNewOpen;
	OnRep_InteractionState();
	ForceNetUpdate();
	return true;
}

void AMallGhostWritingBook::FollowCarrier()
{
	if (!IsValid(Carrier)) { return; }
	FRotator ControlRot = Carrier->GetActorRotation();
	if (AController* Controller = Carrier->GetController())
	{
		ControlRot = Controller->GetControlRotation();
	}
	const FRotator FirstPersonRot(FMath::ClampAngle(ControlRot.Pitch, -50.0f, 50.0f), ControlRot.Yaw, 0.0);
	FVector EyeOrigin = Carrier->GetPawnViewLocation();
	if (EyeOrigin.ContainsNaN() || EyeOrigin.IsZero()) { EyeOrigin = Carrier->GetActorLocation() + FVector(0.0, 0.0, 64.0); }
	const FVector Position = EyeOrigin + FirstPersonRot.RotateVector(FVector(38.0, 14.0, -16.0));
	SetActorLocationAndRotation(Position, FirstPersonRot, false, nullptr, ETeleportType::TeleportPhysics);
}

void AMallGhostWritingBook::ClearCarrier()
{
	if (IsValid(Carrier))
	{
		Carrier->OnDestroyed.RemoveDynamic(this, &AMallGhostWritingBook::OnCarrierDestroyed);
		MallHotbarInventory::RemoveItem(Carrier, DisplayName, CarriedHotbarSlot);
	}
	if (LastSyncedCarrier.IsValid())
	{
		MallHotbarInventory::RemoveItem(LastSyncedCarrier.Get(), DisplayName, LastSyncedSlot);
		LastSyncedCarrier.Reset();
		LastSyncedSlot = INDEX_NONE;
	}
	Carrier = nullptr;
	CarriedHotbarSlot = INDEX_NONE;
	CarrierController.Reset();
}

void AMallGhostWritingBook::RestoreLastPlacement()
{
	if (!HasAuthority()) { return; }
	ClearCarrier();
	if (bHasLastPlacedTransform) { SetActorTransform(LastPlacedTransform, false, nullptr, ETeleportType::TeleportPhysics); }
	OnRep_InteractionState();
	ForceNetUpdate();
}

void AMallGhostWritingBook::OnCarrierDestroyed(AActor* DestroyedActor)
{
	if (HasAuthority()) { RestoreLastPlacement(); }
}

void AMallGhostWritingBook::OnRep_InteractionState()
{
	ApplyInteractionPresentation();
}

bool AMallGhostWritingBook::TryWrite(AActor* Ghost)
{
	if (!HasAuthority() || bWritten || !IsPlacedForWriting() || !IsValid(Ghost) || Ghost->GetWorld() != GetWorld())
	{
		return false;
	}
	const UMallGhostInteractionComponent* Interaction = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
	if (!Interaction || !Interaction->CanPerformDisturbance() || !UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, this))
	{
		return false;
	}
	WritingStartedAtServerTime = GetSynchronizedTime();
	bWritten = true;
	OnRep_Writing();
	ForceNetUpdate();
	return true;
}

void AMallGhostWritingBook::ResetWriting()
{
	if (!HasAuthority())
	{
		return;
	}
	bWritten = false;
	WritingStartedAtServerTime = -1.0;
	OnRep_Writing();
	ForceNetUpdate();
}

void AMallGhostWritingBook::OnRep_Writing()
{
	UpdateWritingProgress();
}

void AMallGhostWritingBook::OnRep_Appearance()
{
	RefreshVisuals();
}

void AMallGhostWritingBook::RefreshVisuals()
{
	BookModel->SetStaticMesh(BookMesh);
	PageMID = nullptr;
	if (PageMaterial)
	{
		PageMID = UMaterialInstanceDynamic::Create(PageMaterial, this);
		WritingPage->SetMaterial(0, PageMID);
		if (WritingTexture)
		{
			PageMID->SetTextureParameterValue(TEXT("WritingTexture"), WritingTexture);
		}
	}
	ApplyInteractionPresentation();
	UpdateWritingProgress();
}

void AMallGhostWritingBook::ApplyInteractionPresentation()
{
	SyncHotbarState();
	const bool bShowVisuals = (!Carrier || IsSelectedByCarrier());
	const bool bFallbackBook = BookMesh == nullptr || !bOpen;
	BookModel->SetVisibility(bShowVisuals && BookMesh != nullptr && bOpen);
	LeftCover->SetVisibility(bShowVisuals && bFallbackBook);
	RightCover->SetVisibility(bShowVisuals && bFallbackBook);
	LeftPages->SetVisibility(bShowVisuals && bFallbackBook && bOpen);
	RightPages->SetVisibility(bShowVisuals && bFallbackBook);
	LeftCover->SetRelativeLocation(bOpen ? FVector(-10.0, 0.0, 0.2) : FVector(0.0, 0.0, 1.2));
	RightCover->SetRelativeLocation(bOpen ? FVector(10.0, 0.0, 0.2) : FVector(0.0, 0.0, 0.2));
	RightPages->SetRelativeLocation(bOpen ? FVector(10.0, 0.0, 0.7) : FVector(0.0, 0.0, 0.7));
	WritingPage->SetVisibility(bShowVisuals && bOpen && PageMaterial != nullptr && WritingTexture != nullptr);
	FallbackWriting->SetVisibility(bShowVisuals && bOpen && (PageMaterial == nullptr || WritingTexture == nullptr));
	InteractionBounds->SetRelativeLocation(FVector(0.0, 0.0, 0.7));
	InteractionBounds->SetBoxExtent(bOpen ? FVector(20.4, 15.1, 1.0) : FVector(10.3, 15.1, 1.0), false);
	InteractionBounds->SetCollisionEnabled(Carrier || StoredOnCart ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}

double AMallGhostWritingBook::GetSynchronizedTime() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* State = World->GetGameState())
		{
			return State->GetServerWorldTimeSeconds();
		}
		return World->GetTimeSeconds();
	}
	return 0.0;
}

void AMallGhostWritingBook::UpdateWritingProgress()
{
	WritingProgress = bWritten && WritingStartedAtServerTime >= 0.0
		? static_cast<float>(FMath::Clamp((GetSynchronizedTime() - WritingStartedAtServerTime) / FMath::Max(0.1f, WritingDuration), 0.0, 1.0))
		: 0.0f;
	if (PageMID)
	{
		PageMID->SetScalarParameterValue(TEXT("WritingProgress"), WritingProgress);
	}
	const int32 VisibleCharacters = FMath::Clamp(FMath::FloorToInt(TestWriting.Len() * WritingProgress), 0, TestWriting.Len());
	FallbackWriting->SetText(FText::FromString(TestWriting.Left(VisibleCharacters)));
}
