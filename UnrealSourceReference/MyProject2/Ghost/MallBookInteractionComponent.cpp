#include "Ghost/MallBookInteractionComponent.h"
#include "Equipment/MallGhostEquipment.h"
#include "Equipment/MallHotbarInventory.h"
#include "Equipment/MallPushCart.h"
#include "Ghost/MallBookInteractionWidget.h"
#include "Ghost/MallGhostWritingBook.h"
#include "Ghost/MallGhostDisturbanceProp.h"
#include "Ghost/MallSpiritBox.h"
#include "Components/PrimitiveComponent.h"
#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Net/UnrealNetwork.h"
#include "UnrealClient.h"

namespace MallBookInteraction
{
	constexpr double Reach = 280.0;
	constexpr double TraceLength = 1000.0;
	bool IsSupport(const FHitResult& Hit)
	{
		return Hit.bBlockingHit && Hit.ImpactNormal.Z >= .9 && IsValid(Hit.GetComponent())
			&& !Hit.GetComponent()->IsSimulatingPhysics() && !Cast<APawn>(Hit.GetActor())
			&& !Cast<AMallGhostWritingBook>(Hit.GetActor()) && !Cast<AMallGhostDisturbanceProp>(Hit.GetActor())
			&& !Cast<AMallSpiritBox>(Hit.GetActor()) && !Cast<AMallHandheldItem>(Hit.GetActor());
	}
}

UMallBookInteractionComponent::UMallBookInteractionComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UMallBookInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UMallBookInteractionComponent, HeldBook, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMallBookInteractionComponent, HeldSpiritBox, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UMallBookInteractionComponent, HeldEquipment, COND_OwnerOnly);
}

APlayerController* UMallBookInteractionComponent::GetController() const { return Cast<APlayerController>(GetOwner()); }

void UMallBookInteractionComponent::AddIgnoredHeldActors(FCollisionQueryParams& Params) const
{
	if (HeldBook) { Params.AddIgnoredActor(HeldBook); }
	if (HeldSpiritBox) { Params.AddIgnoredActor(HeldSpiritBox); }
	for (const TObjectPtr<AMallHandheldItem>& Equip : HeldEquipment)
	{
		if (IsValid(Equip)) { Params.AddIgnoredActor(Equip); }
	}
}

bool UMallBookInteractionComponent::CanAcceptCartItem(const AActor* Item)
{
	APlayerController* PC = GetController();
	if (!PC || !PC->HasAuthority() || !PC->GetPawn()) { return false; }
	ClearStaleHeldItems();
	APawn* Pawn = PC->GetPawn();
	if (!MallHotbarInventory::HasInventory(Pawn))
	{
		return !HeldBook && !HeldSpiritBox && HeldEquipment.IsEmpty();
	}
	if (Item && Item->IsA<AMallGhostWritingBook>()) { return !HeldBook; }
	if (Item && Item->IsA<AMallSpiritBox>()) { return !HeldSpiritBox; }
	if (const AMallHandheldItem* Equip = Cast<AMallHandheldItem>(Item))
	{
		for (const TObjectPtr<AMallHandheldItem>& Existing : HeldEquipment)
		{
			if (IsValid(Existing) && Existing->GetClass() == Equip->GetClass()) { return false; }
		}
		return MallHotbarInventory::CanAcceptItem(Pawn, Equip->DisplayName);
	}
	return !HeldBook || !HeldSpiritBox || MallHotbarInventory::FindFreeSlot(Pawn) != INDEX_NONE;
}

bool UMallBookInteractionComponent::AdoptCartItem(AActor* Item)
{
	APlayerController* PC = GetController();
	if (!PC || !PC->HasAuthority() || !PC->GetPawn() || !IsValid(Item)) { return false; }
	APawn* Pawn = PC->GetPawn();
	if (AMallGhostWritingBook* ExistingBook = Cast<AMallGhostWritingBook>(Item))
	{
		if (HeldBook == ExistingBook && ExistingBook->GetCarrier() == Pawn) { return true; }
	}
	else if (AMallSpiritBox* ExistingBox = Cast<AMallSpiritBox>(Item))
	{
		if (HeldSpiritBox == ExistingBox && ExistingBox->GetCarrier() == Pawn) { return true; }
	}
	else if (AMallHandheldItem* ExistingEquip = Cast<AMallHandheldItem>(Item))
	{
		if (HeldEquipment.Contains(ExistingEquip) && ExistingEquip->GetCarrier() == Pawn) { return true; }
	}
	if (!CanAcceptCartItem(Item)) { return false; }
	AMallGhostWritingBook* Book = Cast<AMallGhostWritingBook>(Item);
	AMallSpiritBox* Box = Cast<AMallSpiritBox>(Item);
	AMallHandheldItem* Equip = Cast<AMallHandheldItem>(Item);
	if (Book && Book->GetCarrier() == Pawn) { HeldBook = Book; }
	else if (Box && Box->GetCarrier() == Pawn) { HeldSpiritBox = Box; }
	else if (Equip && Equip->GetCarrier() == Pawn) { HeldEquipment.AddUnique(Equip); }
	else { return false; }
	GetController()->ForceNetUpdate();
	return true;
}

AMallGhostWritingBook* UMallBookInteractionComponent::GetActiveHeldBook() const
{
	return IsValid(HeldBook) && HeldBook->IsSelectedByCarrier() ? HeldBook.Get() : nullptr;
}

AMallSpiritBox* UMallBookInteractionComponent::GetActiveHeldSpiritBox() const
{
	return IsValid(HeldSpiritBox) && HeldSpiritBox->IsSelectedByCarrier() ? HeldSpiritBox.Get() : nullptr;
}

AMallHandheldItem* UMallBookInteractionComponent::GetActiveHeldEquipment() const
{
	for (const TObjectPtr<AMallHandheldItem>& Equip : HeldEquipment)
	{
		if (IsValid(Equip) && Equip->IsSelectedByCarrier())
		{
			return Equip.Get();
		}
	}
	return nullptr;
}

void UMallBookInteractionComponent::ClearStaleHeldItems()
{
	APlayerController* PC = GetController();
	if (!PC || !PC->HasAuthority()) { return; }
	bool bChanged = false;
	if (HeldBook && (!IsValid(HeldBook) || HeldBook->GetCarrier() != PC->GetPawn())) { HeldBook = nullptr; bChanged = true; }
	if (HeldSpiritBox && (!IsValid(HeldSpiritBox) || HeldSpiritBox->GetCarrier() != PC->GetPawn())) { HeldSpiritBox = nullptr; bChanged = true; }
	const int32 Removed = HeldEquipment.RemoveAll([PC](const TObjectPtr<AMallHandheldItem>& Item)
	{
		return !IsValid(Item) || Item->GetCarrier() != PC->GetPawn();
	});
	if (Removed > 0) { bChanged = true; }
	if (bChanged) { PC->ForceNetUpdate(); }
}

bool UMallBookInteractionComponent::IsValidView(const FVector& Origin, const FVector& Direction) const
{
	const APlayerController* PC = GetController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return IsValid(Pawn) && Pawn->GetController() == PC && !Origin.ContainsNaN() && !Direction.ContainsNaN()
		&& FMath::Abs(Direction.SizeSquared() - 1.0) < .05
		&& FVector::DistSquared(Origin, Pawn->GetActorLocation()) <= FMath::Square(750.0)
		&& FVector::DotProduct(Direction, PC->GetControlRotation().Vector()) >= .8;
}

bool UMallBookInteractionComponent::HasPawnLineOfSight(const FVector& Target, const AActor* AllowedHit) const
{
	const APlayerController* PC = GetController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !GetWorld()) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallBookReach), true, Pawn);
	AddIgnoredHeldActors(Params);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetPawnViewLocation(), Target, ECC_Visibility, Params)
		|| (Hit.GetActor() == AllowedHit && FVector::DistSquared(Hit.ImpactPoint, Target) <= FMath::Square(8.0));
}

AMallGhostWritingBook* UMallBookInteractionComponent::FindBook(const FVector& Origin, const FVector& Direction) const
{
	if (!GetWorld() || !GetController() || !GetController()->GetPawn()) { return nullptr; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallBookFocus), true, GetController()->GetPawn());
	AddIgnoredHeldActors(Params);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * MallBookInteraction::TraceLength, ECC_Visibility, Params)) { return nullptr; }
	AMallGhostWritingBook* Book = Cast<AMallGhostWritingBook>(Hit.GetActor());
	if (!Book || Book->GetCarrier() || FVector::DistSquared(GetController()->GetPawn()->GetActorLocation(), Book->GetActorLocation()) > FMath::Square(MallBookInteraction::Reach)
		|| !HasPawnLineOfSight(Book->GetActorLocation() + FVector(0, 0, 3), Book)) { return nullptr; }
	return Book;
}

AMallGhostWritingBook* UMallBookInteractionComponent::GetFocusedBook() const
{
	APlayerController* PC = GetController();
	if (!PC || !PC->IsLocalController() || HeldBook) { return nullptr; }
	if ((HeldSpiritBox || !HeldEquipment.IsEmpty()) && !MallHotbarInventory::HasInventory(PC->GetPawn())) { return nullptr; }
	FVector Origin; FRotator Rotation; PC->GetPlayerViewPoint(Origin, Rotation);
	return FindBook(Origin, Rotation.Vector());
}

AMallSpiritBox* UMallBookInteractionComponent::FindSpiritBox(const FVector& Origin, const FVector& Direction) const
{
	const APlayerController* PC = GetController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!GetWorld() || !Pawn) { return nullptr; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallSpiritBoxFocus), true, Pawn);
	AddIgnoredHeldActors(Params);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * MallBookInteraction::TraceLength, ECC_Visibility, Params)) { return nullptr; }
	AMallSpiritBox* Box = Cast<AMallSpiritBox>(Hit.GetActor());
	if (!IsValid(Box) || Box->GetCarrier()
		|| FVector::DistSquared(Pawn->GetActorLocation(), Box->GetActorLocation()) > FMath::Square(MallBookInteraction::Reach)
		|| !HasPawnLineOfSight(Hit.ImpactPoint, Box)) { return nullptr; }
	return Box;
}

AMallSpiritBox* UMallBookInteractionComponent::GetFocusedSpiritBox() const
{
	APlayerController* PC = GetController();
	if (!PC || !PC->IsLocalController() || HeldSpiritBox) { return nullptr; }
	if ((HeldBook || !HeldEquipment.IsEmpty()) && !MallHotbarInventory::HasInventory(PC->GetPawn())) { return nullptr; }
	FVector Origin; FRotator Rotation; PC->GetPlayerViewPoint(Origin, Rotation);
	return FindSpiritBox(Origin, Rotation.Vector());
}

AMallHandheldItem* UMallBookInteractionComponent::FindEquipment(const FVector& Origin, const FVector& Direction) const
{
	const APlayerController* PC = GetController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!GetWorld() || !Pawn) { return nullptr; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallEquipmentFocus), true, Pawn);
	AddIgnoredHeldActors(Params);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * MallBookInteraction::TraceLength, ECC_Visibility, Params)) { return nullptr; }
	AMallHandheldItem* Equip = Cast<AMallHandheldItem>(Hit.GetActor());
	if (!IsValid(Equip) || Equip->GetCarrier() || Equip->StoredOnCart
		|| FVector::DistSquared(Pawn->GetActorLocation(), Equip->GetActorLocation()) > FMath::Square(MallBookInteraction::Reach)
		|| !HasPawnLineOfSight(Hit.ImpactPoint, Equip)) { return nullptr; }
	return Equip;
}

AMallHandheldItem* UMallBookInteractionComponent::GetFocusedEquipment() const
{
	APlayerController* PC = GetController();
	if (!PC || !PC->IsLocalController()) { return nullptr; }
	if ((HeldBook || HeldSpiritBox || !HeldEquipment.IsEmpty()) && !MallHotbarInventory::HasInventory(PC->GetPawn())) { return nullptr; }
	FVector Origin; FRotator Rotation; PC->GetPlayerViewPoint(Origin, Rotation);
	return FindEquipment(Origin, Rotation.Vector());
}

bool UMallBookInteractionComponent::BuildPlacement(const FVector& Origin, const FVector& Direction, FTransform& Result, FString& Failure) const
{
	APawn* Pawn = GetController() ? GetController()->GetPawn() : nullptr;
	if (!GetWorld() || !Pawn || !HeldBook) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallBookPlacement), true, Pawn);
	AddIgnoredHeldActors(Params);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * MallBookInteraction::TraceLength, ECC_Visibility, Params)
		|| !MallBookInteraction::IsSupport(Hit))
	{
		Failure = TEXT("Aim at a clear, level floor or tabletop."); return false;
	}
	if (FVector::DistSquared(Pawn->GetActorLocation(), Hit.ImpactPoint) > FMath::Square(MallBookInteraction::Reach)
		|| !HasPawnLineOfSight(Hit.ImpactPoint + FVector(0, 0, 3), Hit.GetActor()))
	{
		Failure = TEXT("Move closer to the placement surface."); return false;
	}
	const FRotator Rotation(0, GetController()->GetControlRotation().Yaw - 90, 0);
	for (double X : {-21.0, 21.0})
	{
		for (double Y : {-15.5, 15.5})
		{
			const FVector Corner = Hit.ImpactPoint + Rotation.RotateVector(FVector(X, Y, 0));
			FHitResult Support;
			if (!GetWorld()->LineTraceSingleByChannel(Support, Corner + FVector(0, 0, 8), Corner - FVector(0, 0, 8), ECC_Visibility, Params)
				|| !MallBookInteraction::IsSupport(Support) || FMath::Abs(Support.ImpactPoint.Z - Hit.ImpactPoint.Z) > 2.0)
			{
				Failure = TEXT("The whole open book needs a level surface."); return false;
			}
		}
	}
	const FVector Location = Hit.ImpactPoint + FVector(0, 0, .3);
	if (GetWorld()->OverlapBlockingTestByChannel(Location + FVector(0, 0, 3), Rotation.Quaternion(), ECC_Visibility,
		FCollisionShape::MakeBox(FVector(20.3, 15.0, 1.5)), Params))
	{
		Failure = TEXT("There is not enough space for the open book."); return false;
	}
	Result = FTransform(Rotation, Location, FVector::OneVector);
	return true;
}

bool UMallBookInteractionComponent::BuildSpiritBoxPlacement(const FVector& Origin, const FVector& Direction, FTransform& Result, FString& Failure) const
{
	APawn* Pawn = GetController() ? GetController()->GetPawn() : nullptr;
	if (!GetWorld() || !Pawn || !HeldSpiritBox) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallSpiritBoxPlacement), true, Pawn);
	AddIgnoredHeldActors(Params);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * MallBookInteraction::TraceLength, ECC_Visibility, Params)
		|| !MallBookInteraction::IsSupport(Hit))
	{
		Failure = TEXT("Aim at a clear, level floor or tabletop."); return false;
	}
	if (FVector::DistSquared(Pawn->GetActorLocation(), Hit.ImpactPoint) > FMath::Square(MallBookInteraction::Reach)
		|| !HasPawnLineOfSight(Hit.ImpactPoint + FVector(0, 0, 3), Hit.GetActor()))
	{
		Failure = TEXT("Move closer to the placement surface."); return false;
	}
	const FRotator Rotation(0, GetController()->GetControlRotation().Yaw, 0);
	// Match the radio's 22 x 14 cm footprint and 30 cm antenna clearance.
	for (double X : {-11.0, 11.0})
	{
		for (double Y : {-7.0, 7.0})
		{
			const FVector Corner = Hit.ImpactPoint + Rotation.RotateVector(FVector(X, Y, 0));
			FHitResult Support;
			if (!GetWorld()->LineTraceSingleByChannel(Support, Corner + FVector(0, 0, 8), Corner - FVector(0, 0, 8), ECC_Visibility, Params)
				|| !MallBookInteraction::IsSupport(Support) || FMath::Abs(Support.ImpactPoint.Z - Hit.ImpactPoint.Z) > 2.0)
			{
				Failure = TEXT("The spirit box needs a level surface under its whole base."); return false;
			}
		}
	}
	const FVector Location = Hit.ImpactPoint + FVector(0, 0, .3);
	if (GetWorld()->OverlapBlockingTestByChannel(Location + FVector(0, 0, 15.2), Rotation.Quaternion(), ECC_Visibility,
		FCollisionShape::MakeBox(FVector(10.8, 6.8, 14.8)), Params))
	{
		Failure = TEXT("There is not enough space for the spirit box and antenna."); return false;
	}
	Result = FTransform(Rotation, Location, FVector::OneVector);
	return true;
}

bool UMallBookInteractionComponent::BuildEquipmentPlacement(
	const AMallHandheldItem* Item,
	const FVector& Origin,
	const FVector& Direction,
	FTransform& Result,
	FString& Failure) const
{
	APawn* Pawn = GetController() ? GetController()->GetPawn() : nullptr;
	if (!GetWorld() || !Pawn || !IsValid(Item)) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallEquipmentPlacement), true, Pawn);
	AddIgnoredHeldActors(Params);
	Params.AddIgnoredActor(Item);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * MallBookInteraction::TraceLength, ECC_Visibility, Params)
		|| !MallBookInteraction::IsSupport(Hit))
	{
		Failure = TEXT("Aim at a clear, level floor or tabletop."); return false;
	}
	if (FVector::DistSquared(Pawn->GetActorLocation(), Hit.ImpactPoint) > FMath::Square(MallBookInteraction::Reach)
		|| !HasPawnLineOfSight(Hit.ImpactPoint + FVector(0, 0, 3), Hit.GetActor()))
	{
		Failure = TEXT("Move closer to the placement surface."); return false;
	}
	const FRotator Rotation(0, GetController()->GetControlRotation().Yaw, 0);
	const double HalfX = FMath::Clamp(Item->PlacementHalfExtent.X, 4.0, 25.0);
	const double HalfY = FMath::Clamp(Item->PlacementHalfExtent.Y, 4.0, 25.0);
	const double HalfZ = FMath::Clamp(Item->PlacementHalfExtent.Z, 3.0, 25.0);
	for (double X : {-HalfX, HalfX})
	{
		for (double Y : {-HalfY, HalfY})
		{
			const FVector Corner = Hit.ImpactPoint + Rotation.RotateVector(FVector(X, Y, 0));
			FHitResult Support;
			if (!GetWorld()->LineTraceSingleByChannel(Support, Corner + FVector(0, 0, 8), Corner - FVector(0, 0, 8), ECC_Visibility, Params)
				|| !MallBookInteraction::IsSupport(Support) || FMath::Abs(Support.ImpactPoint.Z - Hit.ImpactPoint.Z) > 2.5)
			{
				Failure = TEXT("This device needs a level surface under its whole base."); return false;
			}
		}
	}
	const FVector Location = Hit.ImpactPoint + FVector(0, 0, .3);
	if (GetWorld()->OverlapBlockingTestByChannel(Location + FVector(0, 0, HalfZ + 0.5), Rotation.Quaternion(), ECC_Visibility,
		FCollisionShape::MakeBox(FVector(HalfX * 0.92, HalfY * 0.92, HalfZ * 0.92)), Params))
	{
		Failure = TEXT("There is not enough clearance to place this device here."); return false;
	}
	Result = FTransform(Rotation, Location, FVector::OneVector);
	return true;
}

bool UMallBookInteractionComponent::HasHeldEvidenceItem() const
{
	return GetActiveHeldBook() != nullptr || GetActiveHeldSpiritBox() != nullptr || GetActiveHeldEquipment() != nullptr;
}

bool UMallBookInteractionComponent::HasFocusedEvidenceItem() const
{
	return GetFocusedBook() != nullptr || GetFocusedSpiritBox() != nullptr || GetFocusedEquipment() != nullptr;
}

void UMallBookInteractionComponent::RequestInteract()
{
	APlayerController* PC = GetController();
	if (!PC || !PC->IsLocalController()) { return; }
	FVector Origin; FRotator Rotation; PC->GetPlayerViewPoint(Origin, Rotation);
	ServerInteract(Origin, Rotation.Vector());
}

void UMallBookInteractionComponent::RequestToggleOpen()
{
	APlayerController* PC = GetController();
	if (!PC || !PC->IsLocalController()) { return; }
	if (GetActiveHeldBook()) { ServerToggleOpen(); return; }
	FVector Origin; FRotator Rotation; PC->GetPlayerViewPoint(Origin, Rotation);
	if (GetActiveHeldSpiritBox())
	{
		ServerToggleSpiritBox(Origin, Rotation.Vector());
		return;
	}
	if (GetActiveHeldEquipment())
	{
		ServerPrimaryUseEquipment(Origin, Rotation.Vector());
		return;
	}
	if (GetFocusedSpiritBox())
	{
		ServerToggleSpiritBox(Origin, Rotation.Vector());
		return;
	}
	if (GetFocusedEquipment())
	{
		ServerPrimaryUseEquipment(Origin, Rotation.Vector());
	}
}

void UMallBookInteractionComponent::RequestSecondaryInteract()
{
	APlayerController* PC = GetController();
	if (!PC || !PC->IsLocalController()) { return; }
	FVector Origin; FRotator Rotation; PC->GetPlayerViewPoint(Origin, Rotation);
	if (GetActiveHeldSpiritBox())
	{
		ServerCycleSpiritBoxChannel(Origin, Rotation.Vector());
		return;
	}
	if (GetActiveHeldEquipment())
	{
		ServerSecondaryUseEquipment(Origin, Rotation.Vector());
		return;
	}
	if (GetFocusedSpiritBox())
	{
		ServerCycleSpiritBoxChannel(Origin, Rotation.Vector());
		return;
	}
	if (GetFocusedEquipment())
	{
		ServerSecondaryUseEquipment(Origin, Rotation.Vector());
	}
}

bool UMallBookInteractionComponent::RequestDrop()
{
	APlayerController* PC = GetController();
	if (!PC || !PC->IsLocalController() || (!GetActiveHeldBook() && !GetActiveHeldSpiritBox() && !GetActiveHeldEquipment())) { return false; }
	ServerDropHeldItem();
	return true;
}

void UMallBookInteractionComponent::Feedback(const FString& Message) { ClientFeedback(Message); }

void UMallBookInteractionComponent::ClientFeedback_Implementation(const FString& Message)
{
	LastFeedback = Message;
	FeedbackExpiresAt = GetWorld() ? GetWorld()->GetRealTimeSeconds() + 3.0 : 0.0;
}

void UMallBookInteractionComponent::ServerInteract_Implementation(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection)
{
	if (!GetWorld() || !GetOwner()->HasAuthority() || GetWorld()->GetTimeSeconds() < NextActionTime) { return; }
	NextActionTime = GetWorld()->GetTimeSeconds() + .2;
	if (!IsValidView(ViewOrigin, ViewDirection)) { return; }
	APlayerController* PC = GetController();
	ClearStaleHeldItems();
	const bool bHasHotbar = PC && MallHotbarInventory::HasInventory(PC->GetPawn());
	const bool bHandsEmpty = !HeldBook && !HeldSpiritBox && HeldEquipment.IsEmpty();
	AMallGhostWritingBook* ActiveBook = GetActiveHeldBook();
	AMallSpiritBox* ActiveBox = GetActiveHeldSpiritBox();
	AMallHandheldItem* ActiveEquip = GetActiveHeldEquipment();

	// If aiming directly at a world pickup that can go into a free hotbar slot, prioritize picking it up.
	if (!HeldBook && (bHasHotbar || bHandsEmpty))
	{
		if (AMallGhostWritingBook* Book = FindBook(ViewOrigin, ViewDirection))
		{
			if (!Book->TryPickup(PC)) { Feedback(TEXT("Cannot pick up the book right now. Check free hotbar slots.")); return; }
			HeldBook = Book;
			Book->TrySetOpen(PC, false);
			GetOwner()->ForceNetUpdate();
			Feedback(TEXT("Book picked up. LMB: inspect | F: place open | G: drop"));
			return;
		}
	}
	if (!HeldSpiritBox && (bHasHotbar || bHandsEmpty))
	{
		if (AMallSpiritBox* Box = FindSpiritBox(ViewOrigin, ViewDirection))
		{
			if (!Box->TryPickup(PC)) { Feedback(TEXT("Cannot pick up the spirit box right now. Check free hotbar slots.")); return; }
			HeldSpiritBox = Box;
			GetOwner()->ForceNetUpdate();
			Feedback(TEXT("Spirit box picked up. LMB: switch power | E: change channel | F: place | G: drop"));
			return;
		}
	}
	if (bHasHotbar || bHandsEmpty)
	{
		if (AMallHandheldItem* Equip = FindEquipment(ViewOrigin, ViewDirection))
		{
			if (!Equip->TryPickup(PC)) { Feedback(TEXT("Cannot pick up this item right now. Check free hotbar slots.")); return; }
			HeldEquipment.AddUnique(Equip);
			GetOwner()->ForceNetUpdate();
			Feedback(Equip->GetPickupFeedback());
			return;
		}
	}

	if (ActiveBook || ActiveBox || ActiveEquip)
	{
		// A cart is a moving rack, not a static tabletop. Resolve the same broad,
		// occlusion-checked focus used by G/T before rejecting physics supports.
		if (AMallPushCart* Cart = AMallPushCart::FindCartInView(PC->GetPawn(), ViewOrigin, ViewDirection))
		{
			AActor* Item = ActiveBook ? static_cast<AActor*>(ActiveBook)
				: ActiveBox ? static_cast<AActor*>(ActiveBox)
				: static_cast<AActor*>(ActiveEquip);
			if (!Cart->TryDeposit(PC, Item))
			{
				Feedback(TEXT("Cannot store this item. Let the cart stop, then check for a free equipment slot."));
				return;
			}
			ClearStaleHeldItems();
			GetOwner()->ForceNetUpdate();
			Feedback(TEXT("Item stored on the cart. Aim at the cart and press F or E to take equipment."));
			return;
		}
	}
	if (ActiveBook)
	{
		FTransform Placement; FString Failure;
		if (!BuildPlacement(ViewOrigin, ViewDirection, Placement, Failure) || !ActiveBook->TryPlace(PC, Placement))
		{
			Feedback(Failure.IsEmpty() ? TEXT("Cannot place the book here.") : Failure); return;
		}
		AMallGhostWritingBook* Placed = ActiveBook;
		if (HeldBook == ActiveBook) { HeldBook = nullptr; }
		Placed->TrySetOpen(PC, true);
		GetOwner()->ForceNetUpdate();
		Feedback(TEXT("Book placed open. Ask the ghost a question or say: write in the book."));
		return;
	}
	if (ActiveBox)
	{
		FTransform Placement; FString Failure;
		if (!BuildSpiritBoxPlacement(ViewOrigin, ViewDirection, Placement, Failure) || !ActiveBox->TryPlace(PC, Placement))
		{
			Feedback(Failure.IsEmpty() ? TEXT("Cannot place the spirit box here.") : Failure); return;
		}
		if (HeldSpiritBox == ActiveBox) { HeldSpiritBox = nullptr; }
		GetOwner()->ForceNetUpdate();
		Feedback(TEXT("Spirit box placed. Aim at it and click LMB to switch power or press E to change channel."));
		return;
	}
	if (ActiveEquip)
	{
		FTransform Placement; FString Failure;
		if (!BuildEquipmentPlacement(ActiveEquip, ViewOrigin, ViewDirection, Placement, Failure) || !ActiveEquip->TryPlace(PC, Placement))
		{
			Feedback(Failure.IsEmpty() ? TEXT("Cannot place this device here.") : Failure); return;
		}
		HeldEquipment.Remove(ActiveEquip);
		GetOwner()->ForceNetUpdate();
		Feedback(ActiveEquip->GetPlaceFeedback());
		return;
	}
}

void UMallBookInteractionComponent::ServerToggleOpen_Implementation()
{
	if (!GetWorld() || !GetOwner()->HasAuthority() || GetWorld()->GetTimeSeconds() < NextActionTime) { return; }
	NextActionTime = GetWorld()->GetTimeSeconds() + .2;
	APlayerController* PC = GetController();
	ClearStaleHeldItems();
	AMallGhostWritingBook* ActiveBook = GetActiveHeldBook();
	if (!PC || !PC->GetPawn() || !ActiveBook || ActiveBook->GetCarrier() != PC->GetPawn() || !ActiveBook->TrySetOpen(PC, !ActiveBook->IsOpen()))
	{
		Feedback(TEXT("Select the evidence book in your hotbar before inspecting it.")); return;
	}
	Feedback(ActiveBook->IsOpen() ? TEXT("Inspecting the book. LMB: close | F: place open | G: drop") : TEXT("Book closed. Its writing is preserved."));
}

void UMallBookInteractionComponent::ServerToggleSpiritBox_Implementation(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection)
{
	if (!GetWorld() || !GetOwner()->HasAuthority() || GetWorld()->GetTimeSeconds() < NextActionTime) { return; }
	NextActionTime = GetWorld()->GetTimeSeconds() + .2;
	if (!IsValidView(ViewOrigin, ViewDirection)) { return; }
	ClearStaleHeldItems();
	// An active held book owns primary use while its slot is selected; clients cannot bypass that rule.
	if (GetActiveHeldBook()) { return; }
	APlayerController* PC = GetController();
	AMallSpiritBox* ActiveBox = GetActiveHeldSpiritBox();
	AMallSpiritBox* Box = ActiveBox ? ActiveBox : FindSpiritBox(ViewOrigin, ViewDirection);
	if (!Box) { return; }
	if (!Box->TrySetPowered(PC, !Box->IsPowered())) { Feedback(TEXT("Cannot switch the spirit box right now.")); return; }
	Feedback(Box->IsPowered() ? FString::Printf(TEXT("Spirit box on (%s). Hold V and ask: where are you?"), *Box->GetCurrentChannelName()) : TEXT("Spirit box off."));
}

void UMallBookInteractionComponent::ServerCycleSpiritBoxChannel_Implementation(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection)
{
	if (!GetWorld() || !GetOwner()->HasAuthority() || GetWorld()->GetTimeSeconds() < NextActionTime) { return; }
	NextActionTime = GetWorld()->GetTimeSeconds() + .2;
	if (!IsValidView(ViewOrigin, ViewDirection)) { return; }
	ClearStaleHeldItems();
	APlayerController* PC = GetController();
	AMallSpiritBox* ActiveBox = GetActiveHeldSpiritBox();
	AMallSpiritBox* Box = ActiveBox ? ActiveBox : FindSpiritBox(ViewOrigin, ViewDirection);
	if (!Box) { return; }
	if (!Box->TryCycleChannel(PC)) { Feedback(TEXT("Cannot change spirit box channel right now.")); return; }
	Feedback(FString::Printf(TEXT("Spirit box tuned to %s. Hold V and ask a question."), *Box->GetCurrentChannelName()));
}

void UMallBookInteractionComponent::ServerPrimaryUseEquipment_Implementation(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection)
{
	if (!GetWorld() || !GetOwner()->HasAuthority() || GetWorld()->GetTimeSeconds() < NextActionTime) { return; }
	NextActionTime = GetWorld()->GetTimeSeconds() + .2;
	if (!IsValidView(ViewOrigin, ViewDirection)) { return; }
	ClearStaleHeldItems();
	if (GetActiveHeldBook() || GetActiveHeldSpiritBox()) { return; }
	APlayerController* PC = GetController();
	AMallHandheldItem* ActiveEquip = GetActiveHeldEquipment();
	AMallHandheldItem* Equip = ActiveEquip ? ActiveEquip : FindEquipment(ViewOrigin, ViewDirection);
	if (!Equip) { return; }
	if (!Equip->TryPrimaryUse(PC))
	{
		Feedback(TEXT("Cannot use this device right now."));
		return;
	}
	Feedback(Equip->GetPrimaryUseFeedback());
}

void UMallBookInteractionComponent::ServerSecondaryUseEquipment_Implementation(FVector_NetQuantize ViewOrigin, FVector_NetQuantizeNormal ViewDirection)
{
	if (!GetWorld() || !GetOwner()->HasAuthority() || GetWorld()->GetTimeSeconds() < NextActionTime) { return; }
	NextActionTime = GetWorld()->GetTimeSeconds() + .2;
	if (!IsValidView(ViewOrigin, ViewDirection)) { return; }
	ClearStaleHeldItems();
	APlayerController* PC = GetController();
	AMallHandheldItem* ActiveEquip = GetActiveHeldEquipment();
	AMallHandheldItem* Equip = ActiveEquip ? ActiveEquip : FindEquipment(ViewOrigin, ViewDirection);
	if (!Equip) { return; }
	if (!Equip->TrySecondaryUse(PC))
	{
		Feedback(TEXT("Cannot change device mode right now."));
		return;
	}
	Feedback(Equip->GetSecondaryUseFeedback());
}

void UMallBookInteractionComponent::ServerDropHeldItem_Implementation()
{
	if (!GetWorld() || !GetOwner()->HasAuthority() || GetWorld()->GetTimeSeconds() < NextActionTime) { return; }
	NextActionTime = GetWorld()->GetTimeSeconds() + .2;
	APlayerController* PC = GetController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!PC || !Pawn) { return; }
	ClearStaleHeldItems();
	AMallGhostWritingBook* ActiveBook = GetActiveHeldBook();
	AMallSpiritBox* ActiveBox = GetActiveHeldSpiritBox();
	AMallHandheldItem* ActiveEquip = GetActiveHeldEquipment();
	if (!ActiveBook && !ActiveBox && !ActiveEquip) { return; }
	// Trace downward in front of the player (or directly beneath them) to find floor support for dropping with G.
	const FRotator FlatYaw(0.0, PC->GetControlRotation().Yaw, 0.0);
	const FVector Forward = FlatYaw.Vector();
	for (const double ForwardOffset : {55.0, 35.0, 0.0})
	{
		const FVector ProbeStart = Pawn->GetActorLocation() + Forward * ForwardOffset + FVector(0.0, 0.0, 20.0);
		const FVector ProbeEnd = ProbeStart - FVector(0.0, 0.0, 180.0);
		const FVector RayDir = (ProbeEnd - ProbeStart).GetSafeNormal();
		FTransform Placement;
		FString Failure;
		if (ActiveBook && BuildPlacement(ProbeStart, RayDir, Placement, Failure) && ActiveBook->TryPlace(PC, Placement))
		{
			AMallGhostWritingBook* Dropped = ActiveBook;
			if (HeldBook == ActiveBook) { HeldBook = nullptr; }
			Dropped->TrySetOpen(PC, false);
			GetOwner()->ForceNetUpdate();
			Feedback(TEXT("Book dropped."));
			return;
		}
		if (ActiveBox && BuildSpiritBoxPlacement(ProbeStart, RayDir, Placement, Failure) && ActiveBox->TryPlace(PC, Placement))
		{
			if (HeldSpiritBox == ActiveBox) { HeldSpiritBox = nullptr; }
			GetOwner()->ForceNetUpdate();
			Feedback(TEXT("Spirit box dropped."));
			return;
		}
		if (ActiveEquip && BuildEquipmentPlacement(ActiveEquip, ProbeStart, RayDir, Placement, Failure) && ActiveEquip->TryPlace(PC, Placement))
		{
			HeldEquipment.Remove(ActiveEquip);
			GetOwner()->ForceNetUpdate();
			Feedback(FString::Printf(TEXT("%s dropped."), *ActiveEquip->DisplayName.ToString()));
			return;
		}
	}
	Feedback(TEXT("Move to clear level floor space to drop this item."));
}

void UMallBookInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	APlayerController* PC = GetController();
	if (!PC || !GetWorld() || !GetWorld()->HasBegunPlay()) { return; }
	ClearStaleHeldItems();
	if (!PC->IsLocalController() || !PC->GetLocalPlayer()) { return; }
	ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	FViewport* Viewport = LocalPlayer && LocalPlayer->ViewportClient ? LocalPlayer->ViewportClient->Viewport : nullptr;
	if (!Viewport) { return; }
	// Bind through Unreal's input stack once per InputComponent instance.
	if (PC->InputComponent && BoundInputComponent.Get() != PC->InputComponent)
	{
		auto BindOnce = [this, PC](const FKey Key, void (UMallBookInteractionComponent::*Method)())
		{
			for (const FInputKeyBinding& Binding : PC->InputComponent->KeyBindings)
			{
				if (Binding.Chord.Key == Key && Binding.KeyEvent == IE_Pressed && Binding.KeyDelegate.IsBoundToObject(this)) { return; }
			}
			FInputKeyBinding& NewBinding = PC->InputComponent->BindKey(Key, IE_Pressed, this, Method);
			NewBinding.bConsumeInput = false;
		};
		BindOnce(EKeys::LeftMouseButton, &UMallBookInteractionComponent::RequestToggleOpen);
		BindOnce(EKeys::R, &UMallBookInteractionComponent::RequestToggleOpen);
		BoundInputComponent = PC->InputComponent;
	}
	if (!Widget)
	{
		Widget = CreateWidget<UMallBookInteractionWidget>(PC, UMallBookInteractionWidget::StaticClass());
		if (Widget) { Widget->AddToPlayerScreen(150); }
	}
	AMallGhostWritingBook* ActiveBook = GetActiveHeldBook();
	AMallSpiritBox* ActiveBox = GetActiveHeldSpiritBox();
	AMallHandheldItem* ActiveEquip = GetActiveHeldEquipment();
	AMallGhostWritingBook* Book = ActiveBook ? ActiveBook : GetFocusedBook();
	AMallSpiritBox* Box = ActiveBox ? ActiveBox : GetFocusedSpiritBox();
	AMallHandheldItem* Equip = ActiveEquip ? ActiveEquip : GetFocusedEquipment();

	FString Prompt = ActiveBook ? TEXT("Evidence book  |  LMB: open / close  |  F: place open / store on cart  |  G: drop")
		: Book ? TEXT("Evidence book  |  F: pick up") : FString();
	FString Detail = GetWorld()->GetRealTimeSeconds() < FeedbackExpiresAt ? LastFeedback : FString();
	if (Box && !Book && !ActiveEquip)
	{
		Prompt = FString::Printf(TEXT("Spirit box [%s | %s]  |  LMB: switch %s  |  E: channel  |  F: %s%s"),
			Box->IsPowered() ? TEXT("ON") : TEXT("OFF"),
			*Box->GetCurrentChannelName(),
			Box->IsPowered() ? TEXT("off") : TEXT("on"),
			ActiveBox ? TEXT("place / store on cart") : TEXT("pick up"),
			ActiveBox ? TEXT("  |  G: drop") : TEXT(""));
		if (Detail.IsEmpty())
		{
			Detail = Box->IsPowered() && Box->ResponseCount > 0 && !Box->LastSubtitle.IsEmpty()
				? FString::Printf(TEXT("Spirit box: %s"), *Box->LastSubtitle)
				: Box->IsPowered() ? TEXT("Hold V and ask: where are you? / who are you? / how old are you?") : TEXT("Click LMB to power on or press E to cycle channel.");
		}
	}
	else if (Equip && !Book && !ActiveBox)
	{
		Prompt = Equip->GetHUDPrompt(ActiveEquip != nullptr);
		if (Detail.IsEmpty())
		{
			Detail = Equip->GetHUDDetail();
		}
	}
	if (Widget) { Widget->SetBookState(Book, Prompt, Detail, IsValid(ActiveBook) && ActiveBook->IsOpen()); }
}

void UMallBookInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Widget) { Widget->RemoveFromParent(); Widget = nullptr; }
	Super::EndPlay(EndPlayReason);
}

bool UMallBookInteractionSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game || Type == EWorldType::PIE; }
void UMallBookInteractionSubsystem::OnWorldBeginPlay(UWorld& InWorld) { Super::OnWorldBeginPlay(InWorld); AttachEndpoints(); }
TStatId UMallBookInteractionSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UMallBookInteractionSubsystem, STATGROUP_Tickables); }
void UMallBookInteractionSubsystem::AttachEndpoints()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) { return; }
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || PC->FindComponentByClass<UMallBookInteractionComponent>()) { continue; }
		PC->SetReplicates(true);
		UMallBookInteractionComponent* Endpoint = NewObject<UMallBookInteractionComponent>(PC, TEXT("MallBookInteraction"));
		PC->AddInstanceComponent(Endpoint); Endpoint->RegisterComponent(); PC->ForceNetUpdate();
	}
}
void UMallBookInteractionSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!GetWorld() || !GetWorld()->HasBegunPlay() || GetWorld()->GetTimeSeconds() < NextRefreshTime) { return; }
	AttachEndpoints(); NextRefreshTime = GetWorld()->GetTimeSeconds() + .5;
}
