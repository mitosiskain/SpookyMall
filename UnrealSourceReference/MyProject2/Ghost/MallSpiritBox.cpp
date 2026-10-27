#include "Ghost/MallSpiritBox.h"
#include "Ghost/MallBookInteractionComponent.h"
#include "Equipment/MallHotbarInventory.h"
#include "Equipment/MallPushCart.h"

#include "Ghost/MallGhostAudio.h"
#include "Ghost/MallGhostInteractionComponent.h"
#include "Ghost/MallGhostWritingBook.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace MallSpiritBox
{
	constexpr double ReachCm = 280.0;
	constexpr double GhostRangeCm = 1500.0;
	constexpr double SpeakerRangeCm = 600.0;
	constexpr double ResponseInterval = 4.0;

	bool IsAlive(const APawn* Pawn)
	{
		if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed()) { return false; }
		if (const FNumericProperty* Health = FindFProperty<FNumericProperty>(Pawn->GetClass(), TEXT("Health")))
		{
			const void* Value = Health->ContainerPtrToValuePtr<void>(Pawn);
			const double Amount = Health->IsFloatingPoint() ? Health->GetFloatingPointPropertyValue(Value)
				: static_cast<double>(Health->GetSignedIntPropertyValue(Value));
			if (!FMath::IsFinite(Amount) || Amount <= 0.0) { return false; }
		}
		const FBoolProperty* Dead = FindFProperty<FBoolProperty>(Pawn->GetClass(), TEXT("bIsDead"));
		return !Dead || !Dead->GetPropertyValue_InContainer(Pawn);
	}
}

AMallSpiritBox::AMallSpiritBox()
{
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(20.0f);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.04f;

	RadioRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RadioRoot"));
	RadioRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(RadioRoot);
	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(RadioRoot);
	InteractionBounds->SetRelativeLocation(FVector(0.0, 0.0, 15.0));
	InteractionBounds->InitBoxExtent(FVector(11.0, 7.0, 15.0));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionBounds->SetGenerateOverlapEvents(false);
	InteractionBounds->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	auto AddPart = [this](const TCHAR* Name, const FVector& Position, const FVector& Scale)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(RadioRoot);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetStaticMesh(Cube.Object);
		Part->SetRelativeLocation(Position);
		Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false);
		return Part;
	};
	Casing = AddPart(TEXT("Casing"), FVector(0.0, 0.0, 9.0), FVector(0.22, 0.14, 0.18));
	Antenna = AddPart(TEXT("Antenna"), FVector(-7.0, 0.0, 24.0), FVector(0.008, 0.008, 0.12));
	AddPart(TEXT("TopKnob"), FVector(5.0, 0.0, 19.0), FVector(0.035, 0.035, 0.02));
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FName Name(*FString::Printf(TEXT("SpeakerGrille%d"), Index));
		AddPart(*Name.ToString(), FVector(11.2, 0.0, 4.0 + Index * 1.7), FVector(0.008, 0.115, 0.005));
	}

	auto AddText = [this](const TCHAR* Name, const FVector& Position, const float Size)
	{
		UTextRenderComponent* Text = CreateDefaultSubobject<UTextRenderComponent>(Name);
		Text->SetupAttachment(RadioRoot);
		Text->SetRelativeLocation(Position);
		Text->SetHorizontalAlignment(EHTA_Center);
		Text->SetVerticalAlignment(EVRTA_TextCenter);
		Text->SetWorldSize(Size);
		Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Text->SetCanEverAffectNavigation(false);
		Text->SetCastShadow(false);
		return Text;
	};
	DeviceLabel = AddText(TEXT("DeviceLabel"), FVector(11.65, 0.0, 14.8), 1.9f);
	DeviceLabel->SetText(FText::FromString(TEXT("SPIRIT BOX")));
	DeviceLabel->SetTextRenderColor(FColor(20, 25, 25));
	PowerIndicator = AddText(TEXT("PowerIndicator"), FVector(11.7, 0.0, 12.5), 1.4f);
	ResponseSubtitle = AddText(TEXT("ResponseSubtitle"), FVector(12.0, 0.0, 32.0), 3.0f);
	ResponseSubtitle->SetTextRenderColor(FColor(150, 255, 180));
	ResponseSubtitle->SetVisibility(false);
	PowerLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PowerLight"));
	PowerLight->SetupAttachment(RadioRoot);
	PowerLight->SetRelativeLocation(FVector(13.0, 0.0, 13.0));
	PowerLight->SetLightColor(FLinearColor(0.05f, 1.0f, 0.15f));
	PowerLight->SetAttenuationRadius(22.0f);
	PowerLight->SetCastShadows(false);
	PowerLight->SetIntensity(0.0f);

	FSoundAttenuationSettings Attenuation;
	Attenuation.bAttenuate = true;
	Attenuation.bSpatialize = true;
	Attenuation.bEnableOcclusion = false;
	Attenuation.bEnableListenerFocus = false;
	Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
	Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Attenuation.AttenuationShapeExtents = FVector(200.0, 0.0, 0.0);
	Attenuation.FalloffDistance = 1000.0f;
	auto AddAudio = [this, &Attenuation](const TCHAR* Name, const float Volume)
	{
		UAudioComponent* Audio = CreateDefaultSubobject<UAudioComponent>(Name);
		Audio->SetupAttachment(RadioRoot);
		Audio->bAutoActivate = false;
		Audio->bAutoDestroy = false;
		Audio->bAllowSpatialization = true;
		Audio->bIsUISound = false;
		Audio->bOverrideAttenuation = true;
		Audio->SetAttenuationOverrides(Attenuation);
		Audio->SetVolumeMultiplier(Volume);
		return Audio;
	};
	ScanAudio = AddAudio(TEXT("ScanAudio"), 0.04f);
	ResponseAudio = AddAudio(TEXT("ResponseAudio"), 0.65f);
	// Both the static and spoken radio answers travel with the device.
	ResponseAudio->SetUsingAbsoluteLocation(false);
	ResponseAudio->SetAttenuationOverrides(MallGhostAudio::SpatialAttenuation());

	static ConstructorHelpers::FObjectFinderOptional<USoundBase> Placeholder(
		TEXT("/Game/Equipment/SpiritBox/Audio/SW_SpiritBox_Placeholder.SW_SpiritBox_Placeholder"));
	static ConstructorHelpers::FObjectFinderOptional<USoundBase> Scan(
		TEXT("/Game/Equipment/SpiritBox/Audio/SW_SpiritBox_Static.SW_SpiritBox_Static"));
	ScanSound = Scan.Get();
	const FName Phrases[] = {TEXT("where_are_you"), TEXT("who_are_you"), TEXT("how_old_are_you")};
	const TCHAR* Subtitles[] = {TEXT("I am here."), TEXT("I am Evelyn."), TEXT("I was thirty-two.")};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Phrases); ++Index)
	{
		FMallSpiritBoxResponse& Entry = Responses.AddDefaulted_GetRef();
		Entry.PhraseId = Phrases[Index];
		Entry.Sound = Placeholder.Get(); // Replace each slot independently in the Blueprint.
		Entry.Subtitle = Subtitles[Index];
	}
	ChannelNames = {TEXT("87.9 FM"), TEXT("91.5 AM"), TEXT("104.3 EVP")};

	DisplayName = FText::FromString(TEXT("Spirit Box"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> RadioIcon(TEXT("/Game/Equipment/Carts/Textures/T_CartLamp_basecolor.T_CartLamp_basecolor"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> FallbackIcon(TEXT("/Game/Equipment/Flashlight/Textures/base_color.base_color"));
	if (RadioIcon.Succeeded()) { Icon = RadioIcon.Object; }
	else if (FallbackIcon.Succeeded()) { Icon = FallbackIcon.Object; }
}

void AMallSpiritBox::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallSpiritBox, Carrier);
	DOREPLIFETIME(AMallSpiritBox, StoredOnCart);
	DOREPLIFETIME(AMallSpiritBox, bPowered);
	DOREPLIFETIME(AMallSpiritBox, ChannelIndex);
	DOREPLIFETIME(AMallSpiritBox, ResponseCount);
	DOREPLIFETIME(AMallSpiritBox, LastPhraseId);
	DOREPLIFETIME(AMallSpiritBox, LastSubtitle);
	DOREPLIFETIME(AMallSpiritBox, LastResponseLocation);
	DOREPLIFETIME(AMallSpiritBox, CarriedHotbarSlot);
	DOREPLIFETIME(AMallSpiritBox, CarrierSelectedSlot);
}

void AMallSpiritBox::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshPresentation();
}

void AMallSpiritBox::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && !Carrier)
	{
		LastPlacedTransform = GetActorTransform();
		bHasLastPlacedTransform = true;
	}
	ScanAudio->OnAudioFinished.AddUniqueDynamic(this, &AMallSpiritBox::OnScanFinished);
	ResponseAudio->OnAudioFinished.AddUniqueDynamic(this, &AMallSpiritBox::OnResponseFinished);
	RefreshPresentation();
	RefreshAudio();
}

void AMallSpiritBox::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearCarrier();
	if (LastSyncedCarrier.IsValid())
	{
		MallHotbarInventory::RemoveItem(LastSyncedCarrier.Get(), DisplayName, LastSyncedSlot);
		LastSyncedCarrier.Reset();
		LastSyncedSlot = INDEX_NONE;
	}
	bPowered = false;
	bResponseActive = false;
	ScanAudio->OnAudioFinished.RemoveDynamic(this, &AMallSpiritBox::OnScanFinished);
	ResponseAudio->OnAudioFinished.RemoveDynamic(this, &AMallSpiritBox::OnResponseFinished);
	ScanAudio->Stop();
	ResponseAudio->Stop();
	Super::EndPlay(EndPlayReason);
}

void AMallSpiritBox::Tick(float DeltaSeconds)
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
			RefreshPresentation();
		}
	}
	if (ResponseSubtitle->IsVisible() && GetWorld()->GetTimeSeconds() >= SubtitleUntil)
	{
		ResponseSubtitle->SetVisibility(false);
	}
}

bool AMallSpiritBox::IsSelectedByCarrier() const
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

void AMallSpiritBox::SetCarrierSelectedSlot(int32 NewSelectedSlot)
{
	if (CarrierSelectedSlot != NewSelectedSlot)
	{
		CarrierSelectedSlot = NewSelectedSlot;
		RefreshPresentation();
		if (HasAuthority()) { ForceNetUpdate(); }
	}
}

void AMallSpiritBox::SyncHotbarState()
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

APawn* AMallSpiritBox::GetCarrier() const
{
	return IsValid(Carrier) ? Carrier.Get() : nullptr;
}

bool AMallSpiritBox::IsPlayerEligible(APlayerController* Player) const
{
	const APawn* Pawn = IsValid(Player) ? Player->GetPawn() : nullptr;
	return HasAuthority() && GetWorld() && IsValid(Player) && !Player->IsActorBeingDestroyed()
		&& Player->HasAuthority() && Player->GetWorld() == GetWorld()
		&& MallSpiritBox::IsAlive(Pawn) && Pawn->HasAuthority()
		&& Pawn->GetWorld() == GetWorld() && Pawn->GetController() == Player;
}

bool AMallSpiritBox::CanReachPlacedBox(APlayerController* Player) const
{
	if (!IsPlayerEligible(Player) || Carrier || StoredOnCart
		|| FVector::DistSquared(Player->GetPawn()->GetActorLocation(), GetActorLocation()) > FMath::Square(MallSpiritBox::ReachCm))
	{
		return false;
	}
	FVector Eye;
	FRotator View;
	Player->GetPlayerViewPoint(Eye, View);
	if (Eye.ContainsNaN()) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallSpiritBoxReach), true, this);
	Params.AddIgnoredActor(Player->GetPawn());
	FHitResult Hit;
	const FVector Target = GetActorLocation() + FVector(0.0, 0.0, 12.0);
	const FVector PawnEye = Player->GetPawn()->GetPawnViewLocation();
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params)
		|| (!PawnEye.ContainsNaN() && !GetWorld()->LineTraceSingleByChannel(Hit, PawnEye, Target, ECC_Visibility, Params));
}


bool AMallSpiritBox::TryStoreOnCart_Implementation(APlayerController* Player, AMallPushCart* Cart, const FTransform& WorldTransform)
{
	if (!IsPlayerEligible(Player) || Carrier != Player->GetPawn() || StoredOnCart || !IsValid(Cart)
		|| !Cart->HasAuthority() || Cart->GetWorld() != GetWorld() || !Cart->IsParked()
		|| !Cart->CanPlayerReach(Player) || WorldTransform.ContainsNaN()
		|| !WorldTransform.IsValid() || !WorldTransform.GetScale3D().Equals(FVector::OneVector, .001)
		|| FVector::DistSquared(WorldTransform.GetLocation(), Cart->GetActorLocation()) > FMath::Square(120.0)) { return false; }
	ClearCarrier();
	StoredOnCart = Cart;
	bPowered = false;
	OnRep_Power();
	SetActorTransform(WorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
	AttachToActor(Cart, FAttachmentTransformRules::KeepWorldTransform);
	OnRep_InteractionState();
	ForceNetUpdate();
	return true;
}

bool AMallSpiritBox::TryTakeFromCart_Implementation(APlayerController* Player, AMallPushCart* Cart)
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
	Carrier->OnDestroyed.AddUniqueDynamic(this, &AMallSpiritBox::OnCarrierDestroyed);
	Player->OnDestroyed.AddUniqueDynamic(this, &AMallSpiritBox::OnCarrierDestroyed);
	FollowCarrier();
	OnRep_InteractionState();
	Endpoint->AdoptCartItem(this);
	ForceNetUpdate();
	return true;
}

void AMallSpiritBox::ReleaseFromCart_Implementation(AMallPushCart* Cart, const FTransform& WorldTransform)
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


bool AMallSpiritBox::TryPickup(APlayerController* Player)
{
	if (!CanReachPlacedBox(Player)) { return false; }
	APawn* Pawn = Player->GetPawn();
	if (!MallHotbarInventory::CanAcceptItem(Pawn, DisplayName)) { return false; }
	for (TActorIterator<AMallSpiritBox> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->Carrier == Pawn) { return false; }
	}
	if (!MallHotbarInventory::HasInventory(Pawn))
	{
		for (TActorIterator<AMallGhostWritingBook> It(GetWorld()); It; ++It)
		{
			if (It->GetCarrier() == Pawn) { return false; }
		}
	}
	LastPlacedTransform = GetActorTransform();
	bHasLastPlacedTransform = true;
	const int32 FreeSlot = MallHotbarInventory::HasInventory(Pawn) ? MallHotbarInventory::FindFreeSlot(Pawn, DisplayName) : 0;
	CarriedHotbarSlot = FreeSlot;
	CarrierSelectedSlot = FreeSlot;
	Carrier = Pawn;
	CarrierController = Player;
	Carrier->OnDestroyed.AddUniqueDynamic(this, &AMallSpiritBox::OnCarrierDestroyed);
	Player->OnDestroyed.AddUniqueDynamic(this, &AMallSpiritBox::OnCarrierDestroyed);
	FollowCarrier();
	OnRep_InteractionState();
	ForceNetUpdate();
	return true;
}

bool AMallSpiritBox::ValidatePlacement(APlayerController* Player, const FTransform& Requested, FTransform& Supported) const
{
	if (!IsPlayerEligible(Player) || Requested.ContainsNaN() || !Requested.IsValid()
		|| !Requested.GetScale3D().Equals(FVector::OneVector, 0.001)
		|| FVector::DistSquared(Player->GetPawn()->GetActorLocation(), Requested.GetLocation()) > FMath::Square(MallSpiritBox::ReachCm))
	{
		return false;
	}
	const FRotator Rotation = Requested.Rotator().GetNormalized();
	if (FMath::Abs(Rotation.Pitch) > 5.0 || FMath::Abs(Rotation.Roll) > 5.0) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallSpiritBoxPlace), true, this);
	Params.AddIgnoredActor(Player->GetPawn());
	auto FindSupport = [this, &Params](const FVector& Point, FHitResult& Hit)
	{
		return GetWorld()->LineTraceSingleByChannel(Hit, Point + FVector(0.0, 0.0, 20.0), Point - FVector(0.0, 0.0, 25.0), ECC_Visibility, Params)
			&& Hit.ImpactNormal.Z >= 0.7 && IsValid(Hit.GetComponent())
			&& !Hit.GetComponent()->IsSimulatingPhysics() && !Cast<APawn>(Hit.GetActor())
			&& !Cast<AMallGhostWritingBook>(Hit.GetActor()) && !Cast<AMallSpiritBox>(Hit.GetActor());
	};
	FHitResult Center;
	if (!FindSupport(Requested.GetLocation(), Center) || FMath::Abs(Center.ImpactPoint.Z - Requested.GetLocation().Z) > 8.0) { return false; }
	const FRotator FlatRotation(0.0, Rotation.Yaw, 0.0);
	for (const double X : {-11.0, 11.0})
	{
		for (const double Y : {-7.0, 7.0})
		{
			FHitResult Corner;
			if (!FindSupport(Requested.GetLocation() + FlatRotation.RotateVector(FVector(X, Y, 0.0)), Corner)
				|| FMath::Abs(Corner.ImpactPoint.Z - Center.ImpactPoint.Z) > 2.0) { return false; }
		}
	}
	Supported = FTransform(FlatRotation, FVector(Requested.GetLocation().X, Requested.GetLocation().Y, Center.ImpactPoint.Z + 0.3), FVector::OneVector);
	// Check the complete upright volume, including the antenna, independently of the client's preview.
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects;
	for (const ECollisionChannel Channel : {ECC_WorldStatic, ECC_WorldDynamic, ECC_Pawn, ECC_PhysicsBody}) { Objects.AddObjectTypesToQuery(Channel); }
	GetWorld()->OverlapMultiByObjectType(Overlaps, Supported.TransformPosition(FVector(0.0, 0.0, 15.1)),
		FlatRotation.Quaternion(), Objects, FCollisionShape::MakeBox(FVector(10.8, 6.8, 14.8)), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const UPrimitiveComponent* Component = Overlap.GetComponent();
		if (IsValid(Component) && (Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block
			|| Component->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block || Component->IsSimulatingPhysics()))
		{
			return false;
		}
	}
	FVector Eye;
	FRotator View;
	Player->GetPlayerViewPoint(Eye, View);
	FHitResult Obstruction;
	return !Eye.ContainsNaN() && !GetWorld()->LineTraceSingleByChannel(Obstruction, Eye,
		Supported.GetLocation() + FVector(0.0, 0.0, 12.0), ECC_Visibility, Params);
}

bool AMallSpiritBox::TryPlace(APlayerController* Player, const FTransform& Placement)
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

bool AMallSpiritBox::TrySetPowered(APlayerController* Player, const bool bNewPowered)
{
	if (!IsPlayerEligible(Player)) { return false; }
	if (Carrier)
	{
		if (Carrier != Player->GetPawn() || CarrierController.Get() != Player) { return false; }
	}
	else if (!CanReachPlacedBox(Player)) { return false; }
	bPowered = bNewPowered;
	OnRep_Power();
	BP_OnPowerChanged(bPowered);
	ForceNetUpdate();
	return true;
}

FString AMallSpiritBox::GetCurrentChannelName() const
{
	if (ChannelNames.IsEmpty()) { return TEXT("87.9 FM"); }
	const int32 SafeIndex = ((ChannelIndex % ChannelNames.Num()) + ChannelNames.Num()) % ChannelNames.Num();
	return ChannelNames[SafeIndex];
}

bool AMallSpiritBox::TryCycleChannel(APlayerController* Player)
{
	if (!IsPlayerEligible(Player)) { return false; }
	if (Carrier)
	{
		if (Carrier != Player->GetPawn() || CarrierController.Get() != Player) { return false; }
	}
	else if (!CanReachPlacedBox(Player)) { return false; }
	const int32 Count = FMath::Max(1, ChannelNames.Num());
	ChannelIndex = (ChannelIndex + 1) % Count;
	if (!bPowered)
	{
		bPowered = true;
		OnRep_Power();
		BP_OnPowerChanged(true);
	}
	OnRep_Channel();
	BP_OnChannelChanged(ChannelIndex, GetCurrentChannelName());
	ForceNetUpdate();
	return true;
}

void AMallSpiritBox::FollowCarrier()
{
	if (!IsValid(Carrier)) { return; }
	FRotator ControlRot = Carrier->GetActorRotation();
	if (AController* Controller = Carrier->GetController())
	{
		ControlRot = Controller->GetControlRotation();
	}
	const FRotator FirstPersonRot(FMath::ClampAngle(ControlRot.Pitch, -50.0f, 50.0f), ControlRot.Yaw, 0.0);
	const FRotator FacingPlayer = (FirstPersonRot.Quaternion() * FRotator(10.0, 180.0, 0.0).Quaternion()).Rotator();
	FVector EyeOrigin = Carrier->GetPawnViewLocation();
	if (EyeOrigin.ContainsNaN() || EyeOrigin.IsZero()) { EyeOrigin = Carrier->GetActorLocation() + FVector(0.0, 0.0, 64.0); }
	SetActorLocationAndRotation(EyeOrigin + FirstPersonRot.RotateVector(FVector(38.0, 16.0, -14.0)),
		FacingPlayer, false, nullptr, ETeleportType::TeleportPhysics);
}

void AMallSpiritBox::ClearCarrier()
{
	if (IsValid(Carrier))
	{
		Carrier->OnDestroyed.RemoveDynamic(this, &AMallSpiritBox::OnCarrierDestroyed);
		MallHotbarInventory::RemoveItem(Carrier, DisplayName, CarriedHotbarSlot);
	}
	if (LastSyncedCarrier.IsValid())
	{
		MallHotbarInventory::RemoveItem(LastSyncedCarrier.Get(), DisplayName, LastSyncedSlot);
		LastSyncedCarrier.Reset();
		LastSyncedSlot = INDEX_NONE;
	}
	if (APlayerController* Player = CarrierController.Get()) { Player->OnDestroyed.RemoveDynamic(this, &AMallSpiritBox::OnCarrierDestroyed); }
	Carrier = nullptr;
	CarriedHotbarSlot = INDEX_NONE;
	CarrierController.Reset();
}

void AMallSpiritBox::RestoreLastPlacement()
{
	if (!HasAuthority()) { return; }
	ClearCarrier();
	if (bHasLastPlacedTransform) { SetActorTransform(LastPlacedTransform, false, nullptr, ETeleportType::TeleportPhysics); }
	bPowered = false;
	OnRep_InteractionState();
	OnRep_Power();
	ForceNetUpdate();
}

void AMallSpiritBox::OnCarrierDestroyed(AActor* DestroyedActor)
{
	RestoreLastPlacement();
}

void AMallSpiritBox::OnRep_InteractionState()
{
	RefreshPresentation();
}

void AMallSpiritBox::OnRep_Power()
{
	RefreshPresentation();
	RefreshAudio();
}

void AMallSpiritBox::OnRep_Channel()
{
	RefreshPresentation();
	RefreshAudio();
}

void AMallSpiritBox::RefreshPresentation()
{
	SyncHotbarState();
	const bool bShowVisuals = (!Carrier || IsSelectedByCarrier());
	InteractionBounds->SetCollisionEnabled(Carrier || StoredOnCart ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
	if (RadioRoot)
	{
		for (USceneComponent* Child : RadioRoot->GetAttachChildren())
		{
			if (!Child || Child == InteractionBounds || Child == ResponseSubtitle || Child == ScanAudio || Child == ResponseAudio)
			{
				continue;
			}
			if (Child->IsA<UStaticMeshComponent>() || Child->IsA<UTextRenderComponent>())
			{
				Child->SetVisibility(bShowVisuals);
			}
		}
	}
	const FString ChannelLabel = GetCurrentChannelName();
	PowerIndicator->SetText(FText::FromString(bPowered ? FString::Printf(TEXT("ON | %s"), *ChannelLabel) : TEXT("POWER OFF")));
	PowerIndicator->SetTextRenderColor(bPowered ? FColor(55, 255, 95) : FColor(40, 50, 45));
	PowerLight->SetIntensity(bShowVisuals && bPowered ? 160.0f : 0.0f);
}

void AMallSpiritBox::RefreshAudio()
{
	if (!bPowered)
	{
		bResponseActive = false;
		ScanAudio->Stop();
		ResponseAudio->Stop();
		ResponseSubtitle->SetVisibility(false);
		return;
	}
	const float PitchByChannel = 0.94f + (FMath::Max(0, ChannelIndex) % 3) * 0.06f;
	ScanAudio->SetPitchMultiplier(PitchByChannel);
	if (!bResponseActive && GetNetMode() != NM_DedicatedServer && IsValid(ScanSound) && !ScanAudio->IsPlaying())
	{
		ScanAudio->SetSound(ScanSound);
		ScanAudio->Play();
	}
}

void AMallSpiritBox::OnScanFinished()
{
	// The default wave loops seamlessly. This also supports replacing it with a non-looping recording.
	if (bPowered && !IsActorBeingDestroyed()) { RefreshAudio(); }
}

void AMallSpiritBox::OnResponseFinished()
{
	if (ResponseAudio->IsPlaying()) { return; }
	bResponseActive = false;
	if (bPowered && !IsActorBeingDestroyed()) { RefreshAudio(); }
}

bool AMallSpiritBox::TryRespond(AActor* Ghost, APlayerController* Speaker, const FName Phrase)
{
	if (!IsPlayerEligible(Speaker) || StoredOnCart || !bPowered || !IsValid(Ghost) || Ghost->IsActorBeingDestroyed()
		|| !Ghost->HasAuthority() || Ghost->GetWorld() != GetWorld() || Phrase.IsNone())
	{
		return false;
	}
	const UMallGhostInteractionComponent* Interaction = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
	const APawn* Pawn = Speaker->GetPawn();
	if (!Interaction || !Interaction->IsInvisible() || !Interaction->CanPerformDisturbance()
		|| !UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, Ghost)
		|| !UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, this)
		|| !UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, Pawn)
		|| FVector::DistSquared(Ghost->GetActorLocation(), GetActorLocation()) > FMath::Square(MallSpiritBox::GhostRangeCm)
		|| FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(MallSpiritBox::SpeakerRangeCm))
	{
		return false;
	}
	const FVector SourceLocation = GetActorLocation();
	if (SourceLocation.ContainsNaN()) { return false; }
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastResponseTime < ResponseCooldownSeconds) { return false; }
	TArray<const FMallSpiritBoxResponse*> Variants;
	for (const FMallSpiritBoxResponse& Entry : Responses)
	{
		if (Entry.PhraseId == Phrase && IsValid(Entry.Sound) && !Entry.Sound->IsLooping()
			&& FMath::IsFinite(Entry.Sound->GetDuration()) && Entry.Sound->GetDuration() > 0.0f)
		{
			Variants.Add(&Entry);
		}
	}
	if (Variants.IsEmpty()) { return false; }
	int32& Next = NextVariantIndex.FindOrAdd(Phrase);
	const FMallSpiritBoxResponse& Chosen = *Variants[Next % Variants.Num()];
	Next = (Next + 1) % Variants.Num();
	LastResponseTime = Now;
	ResponseCooldownSeconds = FMath::Max(MallSpiritBox::ResponseInterval, static_cast<double>(Chosen.Sound->GetDuration()) + 0.2);
	LastPhraseId = Phrase;
	LastSubtitle = Chosen.Subtitle.Left(256);
	LastResponseLocation = SourceLocation;
	++ResponseCount;
	MulticastPlayResponse(Chosen.Sound, LastSubtitle, Phrase, ResponseCount, LastResponseLocation);
	ForceNetUpdate();
	return true;
}

void AMallSpiritBox::MulticastPlayResponse_Implementation(USoundBase* Sound, const FString& Subtitle, const FName Phrase,
	const int32 Serial, const FVector_NetQuantize SourceLocation)
{
	if (!IsValid(Sound) || Serial <= LastPlayedResponseSerial || SourceLocation.ContainsNaN()) { return; }
	LastPlayedResponseSerial = Serial;
	++LocalResponsePlaybackCount;
	ResponseAudio->Stop();
	// Repair inherited/placed component overrides too. Use the local replicated
	// radio transform, rather than pinning playback to the server's old snapshot.
	ResponseAudio->SetUsingAbsoluteLocation(false);
	ResponseAudio->SetRelativeLocation(FVector::ZeroVector);
	// No OnRep handler plays responses, so late joiners see the state without hearing an old recording.
	if (GetNetMode() == NM_DedicatedServer) { return; }
	bResponseActive = true;
	ScanAudio->Stop();
	ResponseAudio->SetSound(Sound);
	ResponseAudio->Play();
	ResponseSubtitle->SetText(FText::FromString(Subtitle));
	ResponseSubtitle->SetVisibility(!Subtitle.IsEmpty());
	SubtitleUntil = GetWorld()->GetTimeSeconds() + FMath::Clamp(static_cast<double>(Sound->GetDuration()), 3.0, 10.0);
}
