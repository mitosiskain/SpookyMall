#include "Equipment/MallCartInteractionComponent.h"
#include "Equipment/MallCartInteractionWidget.h"
#include "Equipment/MallCartItem.h"
#include "Equipment/MallCartCharger.h"
#include "Equipment/MallCartFlashlight.h"
#include "Equipment/MallGhostEquipment.h"
#include "Equipment/MallHotbarInventory.h"
#include "Equipment/MallLightCart.h"
#include "Equipment/MallPushCart.h"
#include "Environment/MallEscalatorFuse.h"
#include "Environment/MallHidingSpot.h"
#include "Environment/MallSpaAndFoodSystem.h"
#include "Environment/MallStoreVendorNPC.h"
#include "MallEscalator.h"
#include "Ghost/MallBookInteractionComponent.h"
#include "Ghost/MallGhostWritingBook.h"
#include "Ghost/MallSpiritBox.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Net/UnrealNetwork.h"

UMallCartInteractionComponent::UMallCartInteractionComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UMallCartInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UMallCartInteractionComponent, ActiveCart, COND_OwnerOnly);
}

APlayerController* UMallCartInteractionComponent::GetController() const { return Cast<APlayerController>(GetOwner()); }

AActor* UMallCartInteractionComponent::FindCarriedItem() const
{
	const APlayerController* PC = GetController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !GetWorld()) { return nullptr; }
	if (UMallBookInteractionComponent* BookComp = PC->FindComponentByClass<UMallBookInteractionComponent>())
	{
		if (AMallGhostWritingBook* ActiveBook = BookComp->GetActiveHeldBook()) { return ActiveBook; }
		if (AMallSpiritBox* ActiveBox = BookComp->GetActiveHeldSpiritBox()) { return ActiveBox; }
		if (AMallHandheldItem* ActiveEquip = BookComp->GetActiveHeldEquipment()) { return ActiveEquip; }
	}
	AMallCartFlashlight* Flashlight = AMallCartFlashlight::FindCarriedBy(Pawn);
	if (Flashlight && Flashlight->IsSelectedByCarrier()) { return Flashlight; }
	if (AMallEscalatorFuse* Fuse = AMallEscalatorFuse::FindCarriedBy(Pawn))
	{
		if (!MallHotbarInventory::HasInventory(Pawn) ||
			MallHotbarInventory::GetSelectedSlot(Pawn) == Fuse->InventorySlotIndex)
		{
			return Fuse;
		}
	}
	if (!MallHotbarInventory::HasInventory(Pawn))
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (!It->IsA<AMallCartFlashlight>() && It->GetClass()->ImplementsInterface(UMallCartItem::StaticClass())
				&& IMallCartItem::Execute_GetCartItemCarrier(*It) == Pawn) { return *It; }
		}
	}
	return nullptr;
}

bool UMallCartInteractionComponent::IsValidView(const FVector& Origin, const FVector& Direction) const
{
	const APlayerController* PC = GetController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return Pawn && Pawn->GetController() == PC && !Origin.ContainsNaN() && !Direction.ContainsNaN()
		&& FMath::Abs(Direction.SizeSquared()-1) < .05
		&& FVector::DistSquared(Origin, Pawn->GetActorLocation()) <= FMath::Square(750.0)
		&& FVector::DotProduct(Direction, PC->GetControlRotation().Vector()) >= .8;
}

AMallPushCart* UMallCartInteractionComponent::FindCart(const FVector& Origin, const FVector& Direction) const
{
	const APlayerController* PC = GetController();
	return AMallPushCart::FindCartInView(PC ? PC->GetPawn() : nullptr,Origin,Direction);
}

AMallPushCart* UMallCartInteractionComponent::GetFocusedCart() const
{
	APlayerController* PC = GetController();
	if (!PC || !PC->IsLocalController()) { return nullptr; }
	if (AMallPushCart* ComputerCart = GetLocalActiveComputerCart()) { return ComputerCart; }
	if (IsValid(ActiveCart) && ActiveCart->GetPusher() == PC->GetPawn()) { return ActiveCart; }
	FVector Origin; FRotator Rotation;
	PC->GetPlayerViewPoint(Origin, Rotation);
	return FindCart(Origin, Rotation.Vector());
}

AMallPushCart* UMallCartInteractionComponent::GetLocalActiveComputerCart() const
{
	const APlayerController* PC = GetController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !GetWorld()) { return nullptr; }
	for (TActorIterator<AMallPushCart> It(GetWorld()); It; ++It)
	{
		if (It->IsComputerInUseBy(Pawn))
		{
			return *It;
		}
	}
	return nullptr;
}

AMallCartCharger* UMallCartInteractionComponent::FindCharger(const FVector& Origin, const FVector& Direction) const
{
	const APlayerController* PC = GetController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !GetWorld()) { return nullptr; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallCartChargerFocus), true, Pawn);
	if (AActor* Held = FindCarriedItem()) { Params.AddIgnoredActor(Held); }
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 1000.0, ECC_Visibility, Params)) { return nullptr; }
	AMallCartCharger* Charger = Cast<AMallCartCharger>(Hit.GetActor());
	return Charger && FVector::DistSquared(Pawn->GetActorLocation(), Charger->GetConnectionLocation()) <= FMath::Square(350.0) ? Charger : nullptr;
}

AMallCartCharger* UMallCartInteractionComponent::GetFocusedCharger() const
{
	APlayerController* PC = GetController();
	if (!PC || !PC->IsLocalController()) { return nullptr; }
	FVector Origin; FRotator Rotation;
	PC->GetPlayerViewPoint(Origin, Rotation);
	return FindCharger(Origin, Rotation.Vector());
}

AMallCartFlashlight* UMallCartInteractionComponent::FindFlashlight(const FVector& Origin, const FVector& Direction) const
{
	const APlayerController* PC = GetController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !GetWorld()) { return nullptr; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallCartFlashlightFocus), true, Pawn);
	if (AActor* Held = FindCarriedItem()) { Params.AddIgnoredActor(Held); }
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit,Origin,Origin+Direction*1000,ECC_Visibility,Params)) { return nullptr; }
	AMallCartFlashlight* Item = Cast<AMallCartFlashlight>(Hit.GetActor());
	return Item && !Item->Carrier && !Item->StoredOnCart
		&& FVector::DistSquared(Pawn->GetActorLocation(),Item->GetActorLocation())<=FMath::Square(280.0) ? Item : nullptr;
}

AMallCartFlashlight* UMallCartInteractionComponent::GetFocusedFlashlight() const
{
	APlayerController* PC=GetController();
	if (!PC || !PC->IsLocalController()) { return nullptr; }
	FVector Origin; FRotator Rotation; PC->GetPlayerViewPoint(Origin,Rotation);
	return FindFlashlight(Origin,Rotation.Vector());
}

void UMallCartInteractionComponent::ServerFlashlightState_Implementation(int32 SelectedSlot,bool bLightOn)
{
	APlayerController* PC=GetController();
	if (!GetWorld() || !PC || !PC->HasAuthority() || GetWorld()->GetTimeSeconds()<NextServerFlashlightSync) { return; }
	NextServerFlashlightSync=GetWorld()->GetTimeSeconds()+.08;
	AMallCartFlashlight::UpdateOwnerState(PC,SelectedSlot,bLightOn);
}

void UMallCartInteractionComponent::RequestAction(uint8 Action, int32 SlotIndex)
{
	APlayerController* PC = GetController();
	if (!PC || !PC->IsLocalController() || !PC->GetPawn()) { return; }
	FVector Origin; FRotator Rotation;
	PC->GetPlayerViewPoint(Origin, Rotation);
	ServerCartAction(Action, Origin, Rotation.Vector(), SlotIndex);
}

void UMallCartInteractionComponent::RequestPush() { RequestAction(0); }
void UMallCartInteractionComponent::RequestTransfer() { RequestAction(1, AMallCartFlashlight::GetSelectedInventorySlot(GetController() ? GetController()->GetPawn() : nullptr)); }
void UMallCartInteractionComponent::RequestUse() { RequestAction(2); }
void UMallCartInteractionComponent::RequestRetrieveSlot(int32 SlotIndex) { RequestAction(3, SlotIndex); }
void UMallCartInteractionComponent::RequestCycleComputerTab() { RequestAction(7); }
void UMallCartInteractionComponent::RequestComputerRemoteFeed() { RequestAction(8); }

void UMallCartInteractionComponent::RequestPrimaryInteract()
{
	APlayerController* PC = GetController();
	if (!PC || !PC->IsLocalController() || !PC->GetPawn() || !GetWorld()) { return; }
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (Now < NextLocalInputTime) { return; }
	NextLocalInputTime = Now + 0.12;

	// 0. If currently operating the Full-Screen Cart Computer, F exits full-screen terminal view.
	if (GetLocalActiveComputerCart())
	{
		RequestAction(9);
		return;
	}
	// 1. If currently hidden inside a Hiding Spot or seated on a Rest Bench, F steps out / stands up immediately.
	if (AMallHidingSpot::FindSpotOccupiedBy(PC->GetPawn()) ||
		AMallRestBench::FindBenchOccupiedBy(PC->GetPawn()))
	{
		RequestAction(5);
		return;
	}
	// 2. If currently pushing a cart, F parks/releases it.
	if (IsValid(ActiveCart) && ActiveCart->GetPusher() == PC->GetPawn())
	{
		RequestPush();
		return;
	}
	// 3. If aiming at a Store Vendor NPC, a Mall Hiding Spot, or a Mall Rest Bench, F interacts with it.
	{
		FVector Origin; FRotator Rotation;
		PC->GetPlayerViewPoint(Origin, Rotation);
		if (AMallStoreVendorNPC::FindVendorInView(PC->GetPawn(), Origin, Rotation.Vector()) ||
			AMallHidingSpot::FindHidingSpotInView(PC->GetPawn(), Origin, Rotation.Vector()) ||
			AMallRestBench::FindBenchInView(PC->GetPawn(), Origin, Rotation.Vector()))
		{
			RequestAction(5);
			return;
		}
	}
	// 4. If holding or looking at an evidence item (Book / Spirit Box), delegate F to BookInteraction.
	if (UMallBookInteractionComponent* BookComp = PC->FindComponentByClass<UMallBookInteractionComponent>())
	{
		if (BookComp->HasHeldEvidenceItem() || BookComp->HasFocusedEvidenceItem())
		{
			BookComp->RequestInteract();
			return;
		}
	}
	// 3. If aiming at a world Flashlight, F picks it up.
	if (GetFocusedFlashlight())
	{
		RequestTransfer();
		return;
	}
	// 4. If aiming at a Cart:
	if (AMallPushCart* Cart = GetFocusedCart())
	{
		FVector Origin; FRotator Rotation;
		PC->GetPlayerViewPoint(Origin, Rotation);
		const int32 AimedSlot = Cart->FindAimedStoredItemSlot(Origin, Rotation.Vector());
		if (AimedSlot != INDEX_NONE && !FindCarriedItem())
		{
			RequestRetrieveSlot(AimedSlot);
			return;
		}
		if (!Cart->CanPlayerPushFromHandle(PC) && Cart->HasStoredItems() && !FindCarriedItem())
		{
			RequestTransfer();
			return;
		}
		RequestPush();
		return;
	}
	// 5. If aiming at an Escalator Fuse, Breaker Panel, Broken Escalator, or Spa Healing Zone:
	{
		FVector Origin; FRotator Rotation;
		PC->GetPlayerViewPoint(Origin, Rotation);
		if (AMallEscalatorFuse::FindFuseInView(PC->GetPawn(), Origin, Rotation.Vector()) ||
			AMallEscalatorBreakerPanel::FindBreakerInView(PC->GetPawn(), Origin, Rotation.Vector()) ||
			AMallEscalator::FindBrokenEscalatorInView(PC->GetPawn(), Origin, Rotation.Vector()) ||
			AMallSpaHealingZone::FindSpaZoneInViewOrOverlapping(PC->GetPawn(), Origin, Rotation.Vector()))
		{
			RequestAction(5);
			return;
		}
	}
}

void UMallCartInteractionComponent::RequestSecondaryInteract()
{
	APlayerController* PC = GetController();
	if (!PC || !PC->IsLocalController() || !PC->GetPawn() || !GetWorld()) { return; }
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (Now < NextLocalInputTime) { return; }
	NextLocalInputTime = Now + 0.12;

	// 0. If currently seated on a Rest Bench, E also stands up (or sits down when aiming at a bench).
	if (AMallRestBench::FindBenchOccupiedBy(PC->GetPawn()))
	{
		RequestAction(5);
		return;
	}
	// 1. If aiming at a wall charger or a Light Cart, E switches power / connects charger.
	if (GetFocusedCharger())
	{
		RequestUse();
		return;
	}
	if (AMallPushCart* Cart = GetFocusedCart())
	{
		if (Cart->IsA<AMallLightCart>())
		{
			RequestUse();
			return;
		}
		RequestTransfer();
		return;
	}
	// 2. If aiming at a Store Vendor NPC, E cycles the vendor's item catalog; if aiming at a Rest Bench, E sits down.
	{
		FVector Origin; FRotator Rotation;
		PC->GetPlayerViewPoint(Origin, Rotation);
		if (AMallStoreVendorNPC::FindVendorInView(PC->GetPawn(), Origin, Rotation.Vector()))
		{
			RequestAction(6);
			return;
		}
		if (AMallRestBench::FindBenchInView(PC->GetPawn(), Origin, Rotation.Vector()))
		{
			RequestAction(5);
			return;
		}
	}
	// 3. Otherwise delegate E to Spirit Box channel cycling.
	if (UMallBookInteractionComponent* BookComp = PC->FindComponentByClass<UMallBookInteractionComponent>())
	{
		BookComp->RequestSecondaryInteract();
	}
}

bool UMallCartInteractionComponent::RequestDropOrRelease()
{
	APlayerController* PC = GetController();
	if (!PC || !PC->IsLocalController() || !PC->GetPawn() || !GetWorld()) { return false; }
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (Now < NextLocalInputTime) { return true; }

	if (GetLocalActiveComputerCart())
	{
		NextLocalInputTime = Now + 0.12;
		RequestAction(9);
		return true;
	}
	if (AMallRestBench::FindBenchOccupiedBy(PC->GetPawn()))
	{
		NextLocalInputTime = Now + 0.12;
		RequestAction(5);
		return true;
	}
	if (IsValid(ActiveCart) && ActiveCart->GetPusher() == PC->GetPawn())
	{
		NextLocalInputTime = Now + 0.12;
		RequestPush();
		return true;
	}
	if (UMallBookInteractionComponent* BookComp = PC->FindComponentByClass<UMallBookInteractionComponent>())
	{
		if (BookComp->RequestDrop())
		{
			NextLocalInputTime = Now + 0.12;
			return true;
		}
	}
	if (AMallCartFlashlight* Flashlight = AMallCartFlashlight::FindCarriedBy(PC->GetPawn()))
	{
		if (Flashlight->IsSelectedByCarrier())
		{
			NextLocalInputTime = Now + 0.12;
			RequestAction(4);
			return true;
		}
	}
	if (AMallEscalatorFuse* Fuse = AMallEscalatorFuse::FindCarriedBy(PC->GetPawn()))
	{
		if (!MallHotbarInventory::HasInventory(PC->GetPawn()) ||
			MallHotbarInventory::GetSelectedSlot(PC->GetPawn()) == Fuse->InventorySlotIndex)
		{
			NextLocalInputTime = Now + 0.12;
			RequestAction(4);
			return true;
		}
	}
	return false;
}

bool UMallCartInteractionComponent::RequestToggleFlashlight()
{
	APlayerController* PC = GetController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!PC || !PC->IsLocalController() || !Pawn || !GetWorld()) { return false; }
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (Now < NextLocalInputTime) { return true; }
	if (!AMallCartFlashlight::TogglePawnFlashlight(Pawn))
	{
		ClientFeedback_Implementation(TEXT("Pick up a flashlight first (press F when aiming at a flashlight)."));
		return false;
	}
	NextLocalInputTime = Now + 0.12;
	ServerFlashlightState(AMallCartFlashlight::GetSelectedInventorySlot(Pawn), AMallCartFlashlight::GetInventoryLightOn(Pawn));
	return true;
}

void UMallCartInteractionComponent::OnKeyDropOrRelease()
{
	if (RequestDropOrRelease()) { return; }
	APlayerController* PC = GetController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (Pawn)
	{
		if (UFunction* DropFunc = Pawn->FindFunction(TEXT("DropSelectedItem")))
		{
			Pawn->ProcessEvent(DropFunc, nullptr);
		}
	}
}

void UMallCartInteractionComponent::OnKeyToggleFlashlight()
{
	RequestToggleFlashlight();
}

void UMallCartInteractionComponent::ClientFeedback_Implementation(const FString& Message)
{
	LastFeedback = Message;
	FeedbackUntil = GetWorld() ? GetWorld()->GetRealTimeSeconds()+3.0 : 0;
}

void UMallCartInteractionComponent::ServerCartAction_Implementation(uint8 Action, FVector_NetQuantize Origin,
	FVector_NetQuantizeNormal Direction, int32 SlotIndex)
{
	APlayerController* PC = GetController();
	if (!GetWorld() || !PC || !PC->HasAuthority() || !AMallPushCart::IsEligiblePlayer(PC)
		|| GetWorld()->GetTimeSeconds()<NextActionTime || Action>9) { return; }
	NextActionTime = GetWorld()->GetTimeSeconds()+.2;
	if (Action==9)
	{
		for (TActorIterator<AMallPushCart> It(GetWorld()); It; ++It)
		{
			if (It->IsComputerInUseBy(PC->GetPawn()))
			{
				It->StopUseComputer(PC);
				ClientFeedback(TEXT("Exited Full-Screen Cart Computer Terminal."));
				return;
			}
		}
		return;
	}
	if (Action==7 || Action==8)
	{
		AMallPushCart* TargetCart = nullptr;
		for (TActorIterator<AMallPushCart> It(GetWorld()); It; ++It)
		{
			if (It->IsComputerInUseBy(PC->GetPawn()))
			{
				TargetCart = *It;
				break;
			}
		}
		if (!TargetCart)
		{
			TargetCart = (IsValid(ActiveCart) && ActiveCart->GetPusher() == PC->GetPawn() && ActiveCart->bHasComputerTerminal)
				? ActiveCart.Get()
				: (IsValidView(Origin, Direction) ? FindCart(Origin, Direction) : nullptr);
		}
		if (!TargetCart || !TargetCart->bHasComputerTerminal)
		{
			for (TActorIterator<AMallPushCart> It(GetWorld()); It; ++It)
			{
				if (It->bHasComputerTerminal && It->CanPlayerReach(PC))
				{
					TargetCart = *It;
					break;
				}
			}
		}
		if (!TargetCart || !TargetCart->CanPlayerReach(PC)) { return; }
		if (!TargetCart->bHasComputerTerminal)
		{
			ClientFeedback(TEXT("This is the Light & Battery Safe-Zone Cart. Use the Equipment Cart for the Info Computer Screen!"));
			return;
		}
		if (TargetCart->IsComputerInUse() && !TargetCart->IsComputerInUseBy(PC->GetPawn()))
		{
			ClientFeedback(TEXT("Cart Computer Screen is currently occupied by another player! Only one person can use it at a time."));
			return;
		}
		if (Action==7)
		{
			if (!TargetCart->IsComputerInUseBy(PC->GetPawn()))
			{
				if (TargetCart->TryBeginUseComputer(PC))
				{
					if (ActiveCart == TargetCart)
					{
						ActiveCart = nullptr;
						PC->ForceNetUpdate();
					}
					ClientFeedback(TEXT("Full-Screen Cart Computer active! [C] Switch Tab | [V] Remote Cam/Photo | [F] or [G] Exit"));
				}
				return;
			}
			TargetCart->TryCycleComputerTab(PC);
			ClientFeedback(TargetCart->GetComputerSummaryText());
		}
		else
		{
			if (!TargetCart->IsComputerInUseBy(PC->GetPawn()))
			{
				TargetCart->TryBeginUseComputer(PC);
			}
			TargetCart->TryCycleRemoteFeed(PC, TargetCart->ActiveComputerTab == 2);
			ClientFeedback(TargetCart->GetComputerSummaryText());
		}
		return;
	}
	if (Action==4)
	{
		if (AMallEscalatorFuse* Fuse = AMallEscalatorFuse::FindCarriedBy(PC->GetPawn()))
		{
			if (!MallHotbarInventory::HasInventory(PC->GetPawn()) ||
				MallHotbarInventory::GetSelectedSlot(PC->GetPawn()) == Fuse->InventorySlotIndex)
			{
				if (Fuse->TryDrop(PC))
				{
					ClientFeedback(TEXT("Escalator Fuse dropped."));
					return;
				}
			}
		}
		if (AMallCartFlashlight* Flashlight = AMallCartFlashlight::FindCarriedBy(PC->GetPawn()))
		{
			if (Flashlight->TryDrop(PC))
			{
				ClientFeedback(TEXT("Flashlight dropped."));
			}
		}
		return;
	}
	if (Action==5)
	{
		if (AMallRestBench* CurrentBench = AMallRestBench::FindBenchOccupiedBy(PC->GetPawn()))
		{
			FString Msg;
			CurrentBench->TryToggleSit(PC, Msg);
			ClientFeedback(Msg);
			return;
		}
		if (AMallHidingSpot* CurrentSpot = AMallHidingSpot::FindSpotOccupiedBy(PC->GetPawn()))
		{
			FString Msg;
			CurrentSpot->TryToggleHide(PC, Msg);
			ClientFeedback(Msg);
			return;
		}
		if (!IsValidView(Origin, Direction)) { return; }
		if (AMallRestBench* Bench = AMallRestBench::FindBenchInView(PC->GetPawn(), Origin, Direction))
		{
			FString Msg;
			Bench->TryToggleSit(PC, Msg);
			ClientFeedback(Msg);
			return;
		}
		if (AMallStoreVendorNPC* Vendor = AMallStoreVendorNPC::FindVendorInView(PC->GetPawn(), Origin, Direction))
		{
			FString Msg;
			Vendor->TryPurchaseOrDispense(PC, Msg);
			ClientFeedback(Msg);
			return;
		}
		if (AMallHidingSpot* Spot = AMallHidingSpot::FindHidingSpotInView(PC->GetPawn(), Origin, Direction))
		{
			FString Msg;
			Spot->TryToggleHide(PC, Msg);
			ClientFeedback(Msg);
			return;
		}
		if (AMallEscalatorFuse* Fuse = AMallEscalatorFuse::FindFuseInView(PC->GetPawn(), Origin, Direction))
		{
			ClientFeedback(Fuse->TryPickup(PC)
				? TEXT("Picked up Escalator Relay Fuse! Carry in hotbar or store on Cart (E). Press F at a broken escalator to repair.")
				: TEXT("Cannot pick up Escalator Fuse. Free a hotbar slot first."));
			return;
		}
		if (AMallEscalatorBreakerPanel* Breaker = AMallEscalatorBreakerPanel::FindBreakerInView(PC->GetPawn(), Origin, Direction))
		{
			FString Msg;
			Breaker->TryResetBreaker(PC, Msg);
			ClientFeedback(Msg);
			return;
		}
		if (AMallEscalator* BrokenEsc = AMallEscalator::FindBrokenEscalatorInView(PC->GetPawn(), Origin, Direction))
		{
			FString Msg;
			BrokenEsc->TryRepairEscalator(PC, Msg);
			ClientFeedback(Msg);
			return;
		}
		if (AMallSpaHealingZone* SpaZone = AMallSpaHealingZone::FindSpaZoneInViewOrOverlapping(PC->GetPawn(), Origin, Direction))
		{
			FString Msg;
			SpaZone->TryInteractiveSoak(PC, Msg);
			ClientFeedback(Msg);
			return;
		}
		return;
	}
	if (Action==6)
	{
		if (!IsValidView(Origin, Direction)) { return; }
		if (AMallStoreVendorNPC* Vendor = AMallStoreVendorNPC::FindVendorInView(PC->GetPawn(), Origin, Direction))
		{
			FString Msg;
			Vendor->TryCycleCatalogItem(PC, Msg);
			ClientFeedback(Msg);
			return;
		}
		return;
	}
	if (Action==0 && IsValid(ActiveCart) && ActiveCart->GetPusher()==PC->GetPawn())
	{
		ActiveCart->StopPushing(PC);
		ActiveCart = nullptr;
		PC->ForceNetUpdate();
		ClientFeedback(TEXT("Cart parked. Normal movement restored."));
		return;
	}
	if (!IsValidView(Origin, Direction)) { return; }
	if (Action==1)
	{
		AMallCartFlashlight::UpdateOwnerState(PC,SlotIndex,AMallCartFlashlight::GetInventoryLightOn(PC->GetPawn()));
		if (AMallCartFlashlight* Item=FindFlashlight(Origin,Direction))
		{
			ClientFeedback(Item->TryPickup(PC) ? TEXT("Flashlight picked up. T: toggle light | E at a cart: store.") : TEXT("Cannot take flashlight. Check free hotbar slots."));
			return;
		}
	}
	if (Action==2)
	{
		if (AMallCartCharger* Charger = FindCharger(Origin, Direction))
		{
			ClientFeedback(Charger->TryUse(PC) ? TEXT("Charger toggled for nearby light cart.") : TEXT("Park a light cart within cable range of this outlet first."));
			return;
		}
	}
	AMallPushCart* Cart = FindCart(Origin, Direction);
	if (!Cart || !Cart->CanPlayerReach(PC)) { return; }
	if (Action==0)
	{
		if (!Cart->TryBeginPush(PC)) { ClientFeedback(TEXT("Stand behind the handle and press F. One player can push each cart.")); return; }
		ActiveCart = Cart;
		PC->ForceNetUpdate();
		ClientFeedback(TEXT("Pushing cart. W/S: push/pull | F or G: release."));
	}
	else if (Action==1)
	{
		if (!Cart->IsParked()) { ClientFeedback(TEXT("Park the cart before transferring equipment.")); return; }
		AActor* Item = FindCarriedItem();
		const bool bSuccess = Item ? Cart->TryDeposit(PC, Item) : Cart->TryRetrieve(PC);
		ClientFeedback(bSuccess ? Item ? TEXT("Equipment stored. Its evidence is preserved.") : TEXT("Equipment retrieved. Use LMB / E / F / G.")
			: Item ? TEXT("Cannot store this equipment here. Check free slots.") : TEXT("No equipment available, or your hands are occupied."));
	}
	else if (Action==2)
	{
		ClientFeedback(Cart->TryUse(PC) ? Cart->GetCartStatus() : TEXT("This cart has no available power action. Park it first."));
	}
	else
	{
		if (SlotIndex<0 || SlotIndex>=8) { return; }
		ClientFeedback(Cart->TryRetrieve(PC, SlotIndex) ? TEXT("Selected equipment retrieved.") : TEXT("Cannot retrieve that slot."));
	}
}

void UMallCartInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Tick)
{
	Super::TickComponent(DeltaTime, TickType, Tick);
	APlayerController* PC = GetController();
	if (!PC || !GetWorld() || !GetWorld()->HasBegunPlay()) { return; }
	if (PC->HasAuthority() && IsValid(ActiveCart) && ActiveCart->GetPusher()!=PC->GetPawn()) { ActiveCart=nullptr; PC->ForceNetUpdate(); }
	if (APawn* PossessedPawn = PC->GetPawn())
	{
		if (PC->HasAuthority())
		{
			UMallPlayerVitalityComponent::FindOrCreateForPawn(PossessedPawn);
		}
		const bool bSeatedOnBench = AMallRestBench::FindBenchOccupiedBy(PossessedPawn) != nullptr;
		const bool bWantFirstPerson = !bSeatedOnBench;
		if ((PossessedPawn != LastConfiguredFirstPersonPawn.Get() || bLastConfiguredFirstPersonMode != bWantFirstPerson)
			&& PossessedPawn->FindComponentByClass<UCameraComponent>() != nullptr)
		{
			AMallRestBench::ApplySeatedCharacterPosture(PossessedPawn, bSeatedOnBench);
			AMallPushCart::ConfigurePawnFirstPersonView(PossessedPawn, bWantFirstPerson);
			LastConfiguredFirstPersonPawn = PossessedPawn;
			bLastConfiguredFirstPersonMode = bWantFirstPerson;
		}
	}
	if (!PC->IsLocalController() || !PC->GetLocalPlayer()) { return; }
	if (APawn* Pawn = PC->GetPawn())
	{
		const int32 CurrentSlot = AMallCartFlashlight::GetSelectedInventorySlot(Pawn);
		const bool bCurrentLight = AMallCartFlashlight::GetInventoryLightOn(Pawn);
		const bool bStateChanged = (LastSyncedPawn.Get() != Pawn) || (CurrentSlot != LastSyncedSelectedSlot) || (bCurrentLight != bLastSyncedLightOn);
		if (bStateChanged && GetWorld()->GetTimeSeconds() >= NextFlashlightSync)
		{
			LastSyncedPawn = Pawn;
			LastSyncedSelectedSlot = CurrentSlot;
			bLastSyncedLightOn = bCurrentLight;
			ServerFlashlightState(CurrentSlot, bCurrentLight);
			NextFlashlightSync = GetWorld()->GetTimeSeconds() + 0.1;
		}
	}
	if (PC->InputComponent && BoundInputComponent.Get() != PC->InputComponent)
	{
		auto Bind = [this,PC](FKey Key, void (UMallCartInteractionComponent::*Method)())
		{
			for (const FInputKeyBinding& Binding : PC->InputComponent->KeyBindings)
			{
				if (Binding.Chord.Key==Key && Binding.KeyEvent==IE_Pressed && Binding.KeyDelegate.IsBoundToObject(this)) { return; }
			}
			FInputKeyBinding& NewBinding = PC->InputComponent->BindKey(Key,IE_Pressed,this,Method);
			NewBinding.bConsumeInput = false;
		};
		Bind(EKeys::F, &UMallCartInteractionComponent::RequestPrimaryInteract);
		Bind(EKeys::E, &UMallCartInteractionComponent::RequestSecondaryInteract);
		Bind(EKeys::G, &UMallCartInteractionComponent::OnKeyDropOrRelease);
		Bind(EKeys::T, &UMallCartInteractionComponent::OnKeyToggleFlashlight);
		Bind(EKeys::C, &UMallCartInteractionComponent::RequestCycleComputerTab);
		Bind(EKeys::V, &UMallCartInteractionComponent::RequestComputerRemoteFeed);
		BoundInputComponent = PC->InputComponent;
	}
	if (!Widget)
	{
		Widget=CreateWidget<UMallCartInteractionWidget>(PC,UMallCartInteractionWidget::StaticClass());
		if (Widget) { Widget->AddToPlayerScreen(151); }
	}
	AMallPushCart* ComputerCart = GetLocalActiveComputerCart();
	if (IsValid(ComputerCart))
	{
		if (LocalViewedComputerCart.Get() != ComputerCart)
		{
			PC->SetViewTargetWithBlend(ComputerCart, 0.22f, VTBlend_Cubic);
			LocalViewedComputerCart = ComputerCart;
		}
	}
	else if (LocalViewedComputerCart.IsValid() || (!ComputerCart && PC->GetViewTarget() && PC->GetViewTarget()->IsA<AMallPushCart>()))
	{
		if (PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(PC->GetPawn(), 0.18f, VTBlend_Cubic);
		}
		LocalViewedComputerCart.Reset();
	}
	AMallPushCart* Cart=GetFocusedCart();
	FString Prompt;
	if (Cart)
	{
		Prompt=Cart->GetCartStatus() + TEXT("\n") + Cart->GetComputerSummaryText();
		if (Cart->GetPusher()==PC->GetPawn())
		{
			Prompt += Cart->IsA<AMallLightCart>()
				? TEXT("\n[W/S]: Push or Pull Safe-Zone Cart  |  [F] or [G]: Park Cart")
				: TEXT("\n[W/S]: Push or Pull Equipment Cart  |  [F] or [G]: Park Cart  |  [C]: Open Full-Screen PC");
		}
		else if (Cart->IsA<AMallLightCart>())
		{
			Prompt += TEXT("\n[F]: Push from Handle  |  [E]: Switch Safe-Zone Light / Connect Wall Charger");
		}
		else
		{
			Prompt += TEXT("\n[F]: Push Cart / Take Aimed Gear  |  [E]: Store or Take Gear  |  [C]: Full-Screen Info PC  |  [V]: Remote Cam");
		}
	}
	else if (GetFocusedCharger())
	{
		Prompt = TEXT("Cart Battery Charger  |  [E]: Connect / Disconnect Nearby Light Safe-Zone Cart");
	}
	if (Prompt.IsEmpty() && GetFocusedFlashlight()) { Prompt=TEXT("Flashlight  |  [F]: Pick Up  |  [T]: Toggle Belt Flashlight"); }
	if (Prompt.IsEmpty() && PC->GetPawn())
	{
		FVector Origin; FRotator Rotation;
		PC->GetPlayerViewPoint(Origin, Rotation);
		if (AMallRestBench* Bench = AMallRestBench::FindBenchInView(PC->GetPawn(), Origin, Rotation.Vector()))
		{
			Prompt = Bench->GetHUDPrompt(PC->GetPawn());
		}
		else if (AMallHidingSpot* Spot = AMallHidingSpot::FindHidingSpotInView(PC->GetPawn(), Origin, Rotation.Vector()))
		{
			Prompt = Spot->GetHUDPrompt(PC->GetPawn());
		}
		else if (AMallStoreVendorNPC* Vendor = AMallStoreVendorNPC::FindVendorInView(PC->GetPawn(), Origin, Rotation.Vector()))
		{
			Prompt = Vendor->GetHUDPrompt();
		}
		else if (AMallSpaHealingZone* SpaZone = AMallSpaHealingZone::FindSpaZoneInViewOrOverlapping(PC->GetPawn(), Origin, Rotation.Vector()))
		{
			Prompt = SpaZone->GetHUDPrompt(PC->GetPawn());
		}
	}
	float StaminaPct = 100.0f;
	float HealthPct = 100.0f;
	bool bSeatedRest = false;
	if (PC->GetPawn())
	{
		bSeatedRest = AMallRestBench::FindBenchOccupiedBy(PC->GetPawn()) != nullptr;
		if (const UMallPlayerVitalityComponent* Vit = PC->GetPawn()->FindComponentByClass<UMallPlayerVitalityComponent>())
		{
			StaminaPct = Vit->Stamina;
			HealthPct = Vit->Health;
			if (Vit->Health < Vit->MaxHealth - 0.5f || Vit->HasSaltWard() || Vit->HasFoodBuff() || Vit->Stamina < 85.0f || bSeatedRest)
			{
				if (!Prompt.IsEmpty())
				{
					Prompt += TEXT("\n");
				}
				Prompt += Vit->GetVitalityHUDStatus();
			}
		}
	}
	const FString Feedback=GetWorld()->GetRealTimeSeconds()<FeedbackUntil ? LastFeedback : FString();
	if (Widget)
	{
		Widget->SetVitalityState(StaminaPct, HealthPct, bSeatedRest);
		Widget->SetPrompt(Prompt, Feedback, ComputerCart);
	}
}

void UMallCartInteractionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	APlayerController* PC=GetController();
	if (PC && PC->HasAuthority())
	{
		if (IsValid(ActiveCart)) { ActiveCart->StopPushing(PC); }
		if (AMallRestBench* SeatedBench = AMallRestBench::FindBenchOccupiedBy(PC->GetPawn()))
		{
			FString Dummy;
			SeatedBench->TryStandUp(PC, Dummy);
		}
		if (GetWorld())
		{
			for (TActorIterator<AMallPushCart> It(GetWorld()); It; ++It)
			{
				if (It->IsComputerInUseBy(PC->GetPawn()))
				{
					It->StopUseComputer(PC);
				}
			}
		}
	}
	if (PC && PC->InputComponent)
	{
		PC->InputComponent->KeyBindings.RemoveAll([this](const FInputKeyBinding& Binding) { return Binding.KeyDelegate.IsBoundToObject(this); });
	}
	if (Widget) { Widget->RemoveFromParent(); Widget=nullptr; }
	Super::EndPlay(Reason);
}

bool UMallCartInteractionSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type==EWorldType::Game || Type==EWorldType::PIE; }
void UMallCartInteractionSubsystem::OnWorldBeginPlay(UWorld& World) { Super::OnWorldBeginPlay(World); AttachEndpoints(); }
TStatId UMallCartInteractionSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UMallCartInteractionSubsystem,STATGROUP_Tickables); }
void UMallCartInteractionSubsystem::AttachEndpoints()
{
	UWorld* World=GetWorld();
	if (!World || World->GetNetMode()==NM_Client) { return; }
	for (FConstPlayerControllerIterator It=World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC=It->Get();
		if (!PC) { continue; }
		if (APawn* Pawn = PC->GetPawn())
		{
			UMallPlayerVitalityComponent::FindOrCreateForPawn(Pawn);
		}
		if (PC->FindComponentByClass<UMallCartInteractionComponent>()) { continue; }
		PC->SetReplicates(true);
		UMallCartInteractionComponent* Endpoint=NewObject<UMallCartInteractionComponent>(PC,TEXT("MallCartInteraction"));
		PC->AddInstanceComponent(Endpoint);
		Endpoint->RegisterComponent();
		PC->ForceNetUpdate();
	}
}
void UMallCartInteractionSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!GetWorld() || !GetWorld()->HasBegunPlay() || GetWorld()->GetTimeSeconds()<NextRefresh) { return; }
	AttachEndpoints();
	NextRefresh=GetWorld()->GetTimeSeconds()+.5;
}
