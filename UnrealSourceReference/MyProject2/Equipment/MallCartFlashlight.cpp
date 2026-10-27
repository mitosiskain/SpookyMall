#include "Equipment/MallCartFlashlight.h"
#include "Equipment/MallGhostEquipment.h"
#include "Equipment/MallPushCart.h"
#include "Ghost/MallGhostWritingBook.h"
#include "Ghost/MallSpiritBox.h"
#include "Components/ActorComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace MallFlashlightInventory
{
	// These Blueprint public fields/functions are the project's existing inventory contract.
	UActorComponent* Inventory(const APawn* Pawn)
	{
		if (!IsValid(Pawn)) { return nullptr; }
		TInlineComponentArray<UActorComponent*> Components;
		Pawn->GetComponents(Components);
		for (UActorComponent* Component:Components)
		{
			if (Component && Component->GetClass()->GetName()==TEXT("BP_InventoryComponent_C")) { return Component; }
		}
		return nullptr;
	}
	FProperty* Member(UStruct* Struct,const TCHAR* Stem)
	{
		if (!Struct) { return nullptr; }
		const FString Prefix=FString(Stem)+TEXT("_");
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (It->GetName()==Stem || It->GetName().StartsWith(Prefix)) { return *It; }
		}
		return nullptr;
	}
	struct FSlots
	{
		UActorComponent* Component=nullptr;
		FArrayProperty* Array=nullptr;
		FStructProperty* Item=nullptr;
		FObjectPropertyBase* Icon=nullptr;
		FByteProperty* Type=nullptr;
		FIntProperty* Selected=nullptr;
		explicit FSlots(const APawn* Pawn)
		{
			Component=Inventory(Pawn);
			if (!Component) { return; }
			Array=FindFProperty<FArrayProperty>(Component->GetClass(),TEXT("Slots"));
			Selected=FindFProperty<FIntProperty>(Component->GetClass(),TEXT("SelectedIndex"));
			Item=Array?CastField<FStructProperty>(Array->Inner):nullptr;
			Icon=Item?CastField<FObjectPropertyBase>(Member(Item->Struct,TEXT("Icon"))):nullptr;
			Type=Item?CastField<FByteProperty>(Member(Item->Struct,TEXT("ItemType"))):nullptr;
		}
		bool Valid() const { return Component && Array && Item && Icon && Type && Selected && Type->Enum; }
		uint8 FlashlightType() const
		{
			if (!Valid()) { return 255; }
			for (int32 Index=0; Index<Type->Enum->NumEnums(); ++Index)
			{
				if (Type->Enum->GetDisplayNameTextByIndex(Index).ToString()==TEXT("Flashlight")) { return static_cast<uint8>(Type->Enum->GetValueByIndex(Index)); }
			}
			return 255;
		}
		bool IsFlashlight(const void* Data) const
		{
			return Valid() && FlashlightType()!=255 && Type->GetPropertyValue_InContainer(Data)==FlashlightType()
				&& Icon->GetObjectPropertyValue_InContainer(Data)!=nullptr;
		}
		int32 FirstFree() const
		{
			if (!Valid()) { return INDEX_NONE; }
			FScriptArrayHelper Slots(Array,Array->ContainerPtrToValuePtr<void>(Component));
			for (int32 I=0; I<Slots.Num(); ++I) { if (!Icon->GetObjectPropertyValue_InContainer(Slots.GetRawPtr(I))) { return I; } }
			return INDEX_NONE;
		}
		int32 FirstFlashlight() const
		{
			if (!Valid()) { return INDEX_NONE; }
			FScriptArrayHelper Slots(Array,Array->ContainerPtrToValuePtr<void>(Component));
			for (int32 I=0; I<Slots.Num(); ++I) { if (IsFlashlight(Slots.GetRawPtr(I))) { return I; } }
			return INDEX_NONE;
		}
		bool IsSelectedFlashlight() const
		{
			if (!Valid()) { return false; }
			FScriptArrayHelper Slots(Array,Array->ContainerPtrToValuePtr<void>(Component));
			const int32 Index=Selected->GetPropertyValue_InContainer(Component);
			return Slots.IsValidIndex(Index) && IsFlashlight(Slots.GetRawPtr(Index));
		}
	};
	void Notify(APawn* Pawn,UActorComponent* Component)
	{
		if (Component)
		{
			if (FMulticastDelegateProperty* Changed=FindFProperty<FMulticastDelegateProperty>(Component->GetClass(),TEXT("OnInventoryChanged")))
			{
				if (FMulticastScriptDelegate* Delegate=Changed->ContainerPtrToValuePtr<FMulticastScriptDelegate>(Component)) { Delegate->ProcessDelegate<UObject>(nullptr); }
			}
			if (FMulticastDelegateProperty* SelChanged=FindFProperty<FMulticastDelegateProperty>(Component->GetClass(),TEXT("OnSelectionChanged")))
			{
				if (FMulticastScriptDelegate* Delegate=SelChanged->ContainerPtrToValuePtr<FMulticastScriptDelegate>(Component))
				{
					const FIntProperty* SelectedProp=FindFProperty<FIntProperty>(Component->GetClass(),TEXT("SelectedIndex"));
					struct { int32 NewIndex; } Params{ SelectedProp ? SelectedProp->GetPropertyValue_InContainer(Component) : 0 };
					Delegate->ProcessDelegate<UObject>(&Params);
				}
			}
		}
		if (Pawn) { if (UFunction* Refresh=Pawn->FindFunction(TEXT("RefreshFlashlight"))) { Pawn->ProcessEvent(Refresh,nullptr); } }
	}
	void RemoveFlashlights(APawn* Pawn)
	{
		FSlots Access(Pawn);
		if (!Access.Valid()) { return; }
		FScriptArrayHelper Slots(Access.Array,Access.Array->ContainerPtrToValuePtr<void>(Access.Component));
		bool Changed=false;
		for (int32 I=0; I<Slots.Num(); ++I)
		{
			void* Data=Slots.GetRawPtr(I);
			if (Access.IsFlashlight(Data))
			{
				Access.Item->Struct->DestroyStruct(Data);
				Access.Item->Struct->InitializeStruct(Data);
				Changed=true;
			}
		}
		if (Changed)
		{
			if (FBoolProperty* On=FindFProperty<FBoolProperty>(Pawn->GetClass(),TEXT("FlashlightOn"))) { On->SetPropertyValue_InContainer(Pawn,false); }
			if (FBoolProperty* Equipped=FindFProperty<FBoolProperty>(Pawn->GetClass(),TEXT("FlashlightEquipped"))) { Equipped->SetPropertyValue_InContainer(Pawn,false); }
			TInlineComponentArray<USceneComponent*> Components; Pawn->GetComponents(Components);
			for (USceneComponent* Part:Components)
			{
				if (Part && (Part->GetFName()==TEXT("FlashlightMesh") || Part->GetFName()==TEXT("FlashlightLight"))) { Part->SetVisibility(false,true); }
			}
			Notify(Pawn,Access.Component);
		}
	}
	bool AddFlashlight(APawn* Pawn,const AMallCartFlashlight* Item)
	{
		FSlots Access(Pawn);
		if (!Access.Valid() || !Item || !Item->Icon || !FMath::IsFinite(Item->ItemValue)) { return false; }
		if (Access.FirstFlashlight()!=INDEX_NONE) { return true; }
		UFunction* Add=Access.Component->FindFunction(TEXT("AddItemTyped"));
		if (!Add || Access.FirstFree()==INDEX_NONE) { return false; }
		FStructOnScope Parameters(Add);
		auto Set=[&](const FName Name)->void* { FProperty* Property=FindFProperty<FProperty>(Add,Name); return Property?Property->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()):nullptr; };
		FTextProperty* Name=FindFProperty<FTextProperty>(Add,TEXT("DisplayName"));
		FObjectPropertyBase* Icon=FindFProperty<FObjectPropertyBase>(Add,TEXT("Icon"));
		FFloatProperty* Value=FindFProperty<FFloatProperty>(Add,TEXT("ItemValue"));
		FByteProperty* Type=FindFProperty<FByteProperty>(Add,TEXT("ItemType"));
		FBoolProperty* Added=FindFProperty<FBoolProperty>(Add,TEXT("bAdded"));
		if (!Name || !Icon || !Value || !Type || !Added || !Set(TEXT("DisplayName"))) { return false; }
		Name->SetPropertyValue_InContainer(Parameters.GetStructMemory(),Item->DisplayName);
		Icon->SetObjectPropertyValue_InContainer(Parameters.GetStructMemory(),Item->Icon);
		Value->SetPropertyValue_InContainer(Parameters.GetStructMemory(),Item->ItemValue);
		Type->SetPropertyValue_InContainer(Parameters.GetStructMemory(),Access.FlashlightType());
		Access.Component->ProcessEvent(Add,Parameters.GetStructMemory());
		Notify(Pawn,Access.Component);
		return Added->GetPropertyValue_InContainer(Parameters.GetStructMemory()) && Access.FirstFlashlight()!=INDEX_NONE;
	}
	bool SetSelected(APawn* Pawn,int32 Index)
	{
		FSlots Access(Pawn);
		if (!Access.Valid()) { return false; }
		FScriptArrayHelper Slots(Access.Array,Access.Array->ContainerPtrToValuePtr<void>(Access.Component));
		if (!Slots.IsValidIndex(Index)) { return false; }
		if (Access.Selected->GetPropertyValue_InContainer(Access.Component)==Index) { return true; }
		Access.Selected->SetPropertyValue_InContainer(Access.Component,Index);
		Notify(Pawn,Access.Component);
		return true;
	}
}

AMallCartFlashlight::AMallCartFlashlight()
{
	bReplicates=true;
	bAlwaysRelevant=true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick=true;
	PrimaryActorTick.TickInterval=.1f;
	PrimaryActorTick.TickGroup=TG_PostUpdateWork;
	SetNetUpdateFrequency(15);
	DisplayName=FText::FromString(TEXT("Flashlight"));
	ItemRoot=CreateDefaultSubobject<USceneComponent>(TEXT("ItemRoot"));
	SetRootComponent(ItemRoot);
	InteractionBounds=CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(ItemRoot);
	InteractionBounds->InitBoxExtent(FVector(12,8,8));
	InteractionBounds->SetRelativeLocation(FVector(0,0,8));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	InteractionBounds->SetGenerateOverlapEvents(false);
	InteractionBounds->SetCanEverAffectNavigation(false);
	FlashlightModel=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashlightModel"));
	FlashlightModel->SetupAttachment(ItemRoot);
	FlashlightModel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlashlightModel->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Model(TEXT("/Game/Equipment/Flashlight/Meshes/small_plastic_torch.small_plastic_torch"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> Image(TEXT("/Game/Equipment/Flashlight/Textures/base_color.base_color"));
	if (Model.Succeeded()) { FlashlightModel->SetStaticMesh(Model.Object); FlashlightModel->SetRelativeScale3D(FVector(.01)); }
	if (Image.Succeeded()) { Icon=Image.Object; }
}

void AMallCartFlashlight::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallCartFlashlight,Carrier);
	DOREPLIFETIME(AMallCartFlashlight,StoredOnCart);
	DOREPLIFETIME(AMallCartFlashlight,KnownOwners);
	DOREPLIFETIME(AMallCartFlashlight,bCarrierLightOn);
	DOREPLIFETIME(AMallCartFlashlight,CarrierSelectedSlot);
}
void AMallCartFlashlight::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) { LastWorldTransform=GetActorTransform(); bHasWorldPlacement=true; }
	ApplyPresentation();
}
AMallCartFlashlight* AMallCartFlashlight::FindCarriedBy(const APawn* Pawn)
{
	if (!IsValid(Pawn) || !Pawn->GetWorld()) { return nullptr; }
	for (TActorIterator<AMallCartFlashlight> It(Pawn->GetWorld()); It; ++It) { if (It->Carrier==Pawn) { return *It; } }
	return nullptr;
}
int32 AMallCartFlashlight::GetSelectedInventorySlot(const APawn* Pawn)
{
	MallFlashlightInventory::FSlots Access(Pawn);
	return Access.Valid()?Access.Selected->GetPropertyValue_InContainer(Access.Component):INDEX_NONE;
}
bool AMallCartFlashlight::GetInventoryLightOn(const APawn* Pawn)
{
	if (const FBoolProperty* On=IsValid(Pawn)?FindFProperty<FBoolProperty>(Pawn->GetClass(),TEXT("FlashlightOn")):nullptr) { return On->GetPropertyValue_InContainer(Pawn); }
	return false;
}
bool AMallCartFlashlight::CanPawnUseFlashlight(const APawn* Pawn)
{
	if (!IsValid(Pawn)) { return false; }
	if (FindCarriedBy(Pawn)) { return true; }
	MallFlashlightInventory::FSlots Access(Pawn);
	if (Access.Valid() && Access.FirstFlashlight() != INDEX_NONE) { return true; }
	if (const FBoolProperty* Equipped = FindFProperty<FBoolProperty>(Pawn->GetClass(), TEXT("FlashlightEquipped")))
	{
		if (Equipped->GetPropertyValue_InContainer(Pawn)) { return true; }
	}
	return false;
}
void AMallCartFlashlight::SetInventoryLightOn(APawn* Pawn, bool bLightOn)
{
	if (!IsValid(Pawn)) { return; }
	if (FBoolProperty* On = FindFProperty<FBoolProperty>(Pawn->GetClass(), TEXT("FlashlightOn")))
	{
		On->SetPropertyValue_InContainer(Pawn, bLightOn);
	}
	if (FBoolProperty* Equipped = FindFProperty<FBoolProperty>(Pawn->GetClass(), TEXT("FlashlightEquipped")))
	{
		if (bLightOn || CanPawnUseFlashlight(Pawn)) { Equipped->SetPropertyValue_InContainer(Pawn, true); }
	}
	const bool bSelected = MallFlashlightInventory::FSlots(Pawn).IsSelectedFlashlight();
	TInlineComponentArray<USceneComponent*> Parts; Pawn->GetComponents(Parts);
	for (USceneComponent* Part : Parts)
	{
		if (Part && Part->GetFName() == TEXT("FlashlightMesh")) { Part->SetVisibility(bSelected || bLightOn, true); }
		if (Part && Part->GetFName() == TEXT("FlashlightLight")) { Part->SetVisibility(bLightOn, true); }
	}
}
bool AMallCartFlashlight::TogglePawnFlashlight(APawn* Pawn)
{
	if (!CanPawnUseFlashlight(Pawn)) { return false; }
	const bool bNewLightOn = !GetInventoryLightOn(Pawn);
	SetInventoryLightOn(Pawn, bNewLightOn);
	return true;
}
bool AMallCartFlashlight::IsSelectedByCarrier() const
{
	return IsValid(Carrier) && MallFlashlightInventory::FSlots(Carrier).IsSelectedFlashlight();
}
void AMallCartFlashlight::UpdateOwnerState(APlayerController* Player,int32 SelectedSlot,bool bLightOn)
{
	if (!AMallPushCart::IsEligiblePlayer(Player) || !Player->HasAuthority()) { return; }
	APawn* Pawn = Player->GetPawn();
	if (!IsValid(Pawn)) { return; }
	MallFlashlightInventory::SetSelected(Pawn, SelectedSlot);
	if (UWorld* World = Pawn->GetWorld())
	{
		for (TActorIterator<AMallGhostWritingBook> It(World); It; ++It)
		{
			if (It->GetCarrier() == Pawn) { It->SetCarrierSelectedSlot(SelectedSlot); }
		}
		for (TActorIterator<AMallSpiritBox> It(World); It; ++It)
		{
			if (It->GetCarrier() == Pawn) { It->SetCarrierSelectedSlot(SelectedSlot); }
		}
		for (TActorIterator<AMallHandheldItem> It(World); It; ++It)
		{
			if (It->GetCarrier() == Pawn) { It->SetCarrierSelectedSlot(SelectedSlot); }
		}
	}
	AMallCartFlashlight* Item = FindCarriedBy(Pawn);
	if (!Item || !Item->HasAuthority()) { return; }
	// Belt/shoulder utility: flashlight can remain on even when another hotbar slot is selected.
	const bool NewOn = bLightOn;
	const bool Changed = Item->CarrierSelectedSlot != SelectedSlot || Item->bCarrierLightOn != NewOn;
	Item->CarrierSelectedSlot = SelectedSlot; Item->bCarrierLightOn = NewOn;
	SetInventoryLightOn(Pawn, NewOn);
	if (Changed) { Item->MirrorInventories(); Item->ForceNetUpdate(); }
}
bool AMallCartFlashlight::CanReceive(APlayerController* Player) const
{
	if (!HasAuthority() || !AMallPushCart::IsEligiblePlayer(Player) || Player->GetWorld()!=GetWorld() || FindCarriedBy(Player->GetPawn())) { return false; }
	MallFlashlightInventory::FSlots Access(Player->GetPawn());
	return Access.Valid() && Access.FirstFlashlight()==INDEX_NONE && Access.FirstFree()!=INDEX_NONE;
}
bool AMallCartFlashlight::TryPickup(APlayerController* Player)
{
	if (Carrier || StoredOnCart || !CanReceive(Player)
		|| FVector::DistSquared(Player->GetPawn()->GetActorLocation(),GetActorLocation())>FMath::Square(280.0)) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallCartFlashlightReach),true,Player->GetPawn());
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit,Player->GetPawn()->GetPawnViewLocation(),GetActorLocation()+FVector(0,0,8),ECC_Visibility,Params)
		&& Hit.GetActor()!=this) { return false; }
	if (!MallFlashlightInventory::AddFlashlight(Player->GetPawn(),this)) { return false; }
	LastWorldTransform=GetActorTransform(); bHasWorldPlacement=true;
	Carrier=Player->GetPawn(); KnownOwners.AddUnique(Carrier);
	CarrierSelectedSlot=MallFlashlightInventory::FSlots(Carrier).FirstFlashlight();
	MallFlashlightInventory::SetSelected(Carrier,CarrierSelectedSlot);
	bCarrierLightOn=false;
	OnRep_ItemState(); ForceNetUpdate();
	return true;
}
bool AMallCartFlashlight::TryDrop(APlayerController* Player)
{
	if (!HasAuthority() || !AMallPushCart::IsEligiblePlayer(Player) || Carrier != Player->GetPawn() || StoredOnCart)
	{
		return false;
	}
	APawn* Pawn = Player->GetPawn();
	const FRotator FlatYaw(0.0, Player->GetControlRotation().Yaw, 0.0);
	const FVector Forward = FlatYaw.Vector();
	FTransform DropTransform = bHasWorldPlacement ? LastWorldTransform : FTransform(FlatYaw, Pawn->GetActorLocation() + Forward * 45.0 - FVector(0.0, 0.0, 85.0));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallCartFlashlightDrop), true, this);
	Params.AddIgnoredActor(Pawn);
	for (const double ForwardOffset : {50.0, 30.0, 0.0})
	{
		const FVector ProbeStart = Pawn->GetActorLocation() + Forward * ForwardOffset + FVector(0.0, 0.0, 20.0);
		const FVector ProbeEnd = ProbeStart - FVector(0.0, 0.0, 180.0);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, ProbeStart, ProbeEnd, ECC_Visibility, Params) && Hit.ImpactNormal.Z >= 0.7f)
		{
			DropTransform = FTransform(FlatYaw, Hit.ImpactPoint + FVector(0.0, 0.0, 1.0));
			break;
		}
	}
	Carrier = nullptr;
	bCarrierLightOn = false;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorTransform(DropTransform, false, nullptr, ETeleportType::TeleportPhysics);
	LastWorldTransform = DropTransform;
	bHasWorldPlacement = true;
	OnRep_ItemState();
	ForceNetUpdate();
	return true;
}
bool AMallCartFlashlight::TryStoreOnCart_Implementation(APlayerController* Player,AMallPushCart* Cart,const FTransform& Position)
{
	if (!HasAuthority() || !AMallPushCart::IsEligiblePlayer(Player) || Carrier!=Player->GetPawn()
		|| !IsSelectedByCarrier() || StoredOnCart || !IsValid(Cart) || Cart->GetWorld()!=GetWorld()
		|| !Cart->IsParked() || !Cart->CanPlayerReach(Player) || Position.ContainsNaN() || !Position.IsValid()
		|| !Position.GetScale3D().Equals(FVector::OneVector,.001)
		|| FVector::DistSquared(Position.GetLocation(),Cart->GetActorLocation())>FMath::Square(120.0)) { return false; }
	StoredOnCart=Cart; Carrier=nullptr; bCarrierLightOn=false;
	SetActorTransform(Position,false,nullptr,ETeleportType::TeleportPhysics);
	AttachToActor(Cart,FAttachmentTransformRules::KeepWorldTransform);
	OnRep_ItemState(); ForceNetUpdate();
	return true;
}
bool AMallCartFlashlight::TryTakeFromCart_Implementation(APlayerController* Player,AMallPushCart* Cart)
{
	if (!CanReceive(Player) || !IsValid(Cart) || StoredOnCart!=Cart || !Cart->ContainsStoredItem(this)
		|| !Cart->IsParked() || !Cart->CanPlayerReach(Player)) { return false; }
	if (!MallFlashlightInventory::AddFlashlight(Player->GetPawn(),this)) { return false; }
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	StoredOnCart=nullptr; Carrier=Player->GetPawn(); KnownOwners.AddUnique(Carrier);
	CarrierSelectedSlot=MallFlashlightInventory::FSlots(Carrier).FirstFlashlight();
	MallFlashlightInventory::SetSelected(Carrier,CarrierSelectedSlot);
	bCarrierLightOn=false;
	OnRep_ItemState(); ForceNetUpdate();
	return true;
}
void AMallCartFlashlight::ReleaseFromCart_Implementation(AMallPushCart* Cart,const FTransform& Position)
{
	if (!HasAuthority() || !Cart || StoredOnCart!=Cart) { return; }
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	StoredOnCart=nullptr;
	if (bHasWorldPlacement) { SetActorTransform(LastWorldTransform,false,nullptr,ETeleportType::TeleportPhysics); }
	else if (!Position.ContainsNaN() && Position.IsValid()) { SetActorTransform(Position,false,nullptr,ETeleportType::TeleportPhysics); }
	OnRep_ItemState(); ForceNetUpdate();
}
void AMallCartFlashlight::ApplyPresentation()
{
	FlashlightModel->SetVisibility(!IsValid(Carrier),true);
	InteractionBounds->SetCollisionEnabled(!Carrier && !StoredOnCart?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
	if (StoredOnCart && GetAttachParentActor()!=StoredOnCart) { AttachToActor(StoredOnCart,FAttachmentTransformRules::KeepWorldTransform); }
	if (!StoredOnCart && GetAttachParentActor()) { DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); }
}
void AMallCartFlashlight::MirrorInventories()
{
	if (LastLocallyEquippedCarrier.Get() != Carrier || !IsValid(Carrier) || !Carrier->IsLocallyControlled())
	{
		LastLocallyEquippedCarrier.Reset();
	}
	TArray<APawn*> Owners;
	for (APawn* Pawn:KnownOwners) { if (IsValid(Pawn)) { Owners.AddUnique(Pawn); } }
	if (IsValid(Carrier)) { Owners.AddUnique(Carrier); }
	for (APawn* Pawn:Owners)
	{
		AMallCartFlashlight* Current=FindCarriedBy(Pawn);
		if (!Current) { MallFlashlightInventory::RemoveFlashlights(Pawn); continue; }
		const bool bPresent = MallFlashlightInventory::AddFlashlight(Pawn,Current);
		if (bPresent && Current == this && Pawn->IsLocallyControlled() && LastLocallyEquippedCarrier.Get() != Pawn)
		{
			const int32 Slot = MallFlashlightInventory::FSlots(Pawn).FirstFlashlight();
			if (MallFlashlightInventory::SetSelected(Pawn,Slot)) { LastLocallyEquippedCarrier = Pawn; }
		}
		// After the initial equip, preserve immediate owner input and mirror it on other copies.
		if (!Pawn->IsLocallyControlled())
		{
			MallFlashlightInventory::SetSelected(Pawn,Current->CarrierSelectedSlot);
			if (FBoolProperty* On=FindFProperty<FBoolProperty>(Pawn->GetClass(),TEXT("FlashlightOn"))) { On->SetPropertyValue_InContainer(Pawn,Current->bCarrierLightOn); }
			MallFlashlightInventory::Notify(Pawn,MallFlashlightInventory::Inventory(Pawn));
			const bool bSelected=MallFlashlightInventory::FSlots(Pawn).IsSelectedFlashlight();
			TInlineComponentArray<USceneComponent*> Parts; Pawn->GetComponents(Parts);
			for (USceneComponent* Part:Parts)
			{
				if (Part && Part->GetFName()==TEXT("FlashlightMesh")) { Part->SetVisibility(bSelected || Current->bCarrierLightOn,true); }
				if (Part && Part->GetFName()==TEXT("FlashlightLight")) { Part->SetVisibility(Current->bCarrierLightOn,true); }
			}
		}
		else
		{
			const bool bLocalLightOn = GetInventoryLightOn(Pawn);
			const bool bSelected = MallFlashlightInventory::FSlots(Pawn).IsSelectedFlashlight();
			TInlineComponentArray<USceneComponent*> Parts; Pawn->GetComponents(Parts);
			for (USceneComponent* Part : Parts)
			{
				if (Part && Part->GetFName() == TEXT("FlashlightMesh")) { Part->SetVisibility(bSelected || bLocalLightOn, true); }
				if (Part && Part->GetFName() == TEXT("FlashlightLight")) { Part->SetVisibility(bLocalLightOn, true); }
			}
		}
	}
}
void AMallCartFlashlight::RecoverWorldPlacement()
{
	if (!HasAuthority()) { return; }
	Carrier=nullptr; bCarrierLightOn=false;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (bHasWorldPlacement) { SetActorTransform(LastWorldTransform,false,nullptr,ETeleportType::TeleportPhysics); }
	OnRep_ItemState(); ForceNetUpdate();
}
void AMallCartFlashlight::OnRep_ItemState() { ApplyPresentation(); MirrorInventories(); }
void AMallCartFlashlight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && Carrier)
	{
		APlayerController* PC=Cast<APlayerController>(Carrier->GetController());
		if (!AMallPushCart::IsEligiblePlayer(PC) || PC->GetPawn()!=Carrier) { RecoverWorldPlacement(); }
	}
	if (Carrier || StoredOnCart || !KnownOwners.IsEmpty())
	{
		ApplyPresentation();
		MirrorInventories();
	}
}
void AMallCartFlashlight::EndPlay(const EEndPlayReason::Type Reason)
{
	Carrier=nullptr;
	MirrorInventories();
	Super::EndPlay(Reason);
}
