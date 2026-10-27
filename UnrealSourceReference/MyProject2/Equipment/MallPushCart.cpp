#include "Equipment/MallPushCart.h"
#include "Equipment/MallCartItem.h"
#include "Equipment/MallCartFlashlight.h"
#include "Equipment/MallGhostEquipment.h"
#include "Equipment/MallHotbarInventory.h"
#include "Ghost/MallGhostWritingBook.h"
#include "Ghost/MallSpiritBox.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodyInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

AMallPushCart::AMallPushCart()
{
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30.0f);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	CartBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("CartBounds"));
	SetRootComponent(CartBounds);
	CartBounds->InitBoxExtent(FVector(55, 36, 44));
	CartBounds->SetMobility(EComponentMobility::Movable);
	CartBounds->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CartBounds->SetCollisionObjectType(ECC_GameTraceChannel1);
	CartBounds->SetCollisionResponseToAllChannels(ECR_Block);
	CartBounds->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CartBounds->SetGenerateOverlapEvents(false);
	CartBounds->SetIsReplicated(true);
	CartBounds->bReplicatePhysicsToAutonomousProxy = true;
	CartBounds->BodyInstance.DOFMode = EDOFMode::SixDOF;
	CartBounds->BodyInstance.bLockXRotation = true;
	CartBounds->BodyInstance.bLockYRotation = true;
	CartBounds->BodyInstance.bLockZRotation = false;
	CartBounds->BodyInstance.bLockRotation = false;
	CartBounds->BodyInstance.bUseCCD = true;
	CartBounds->CanCharacterStepUpOn = ECB_No;
	CartBounds->SetCanEverAffectNavigation(true);
	// The mall uses dynamic modifiers: moving carts must not leave baked blockers behind.
	CartBounds->bDynamicObstacle = true;
	CartVisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CartVisualRoot"));
	CartVisualRoot->SetupAttachment(CartBounds);
	CartModel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CartModel"));
	CartModel->SetupAttachment(CartVisualRoot);
	CartModel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CartModel->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	auto Part = [this](const TCHAR* Name, UStaticMesh* Mesh, FVector Position, FVector Scale, FRotator Rotation = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(CartVisualRoot);
		Component->SetStaticMesh(Mesh);
		Component->SetRelativeLocation(Position);
		Component->SetRelativeScale3D(Scale);
		Component->SetRelativeRotation(Rotation);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		ProceduralParts.Add(Component);
	};
	if (Cube.Succeeded())
	{
		Part(TEXT("LowerDeck"), Cube.Object, FVector(0,0,-15), FVector(1.04,.66,.035));
		Part(TEXT("UpperDeck"), Cube.Object, FVector(0,0,35), FVector(1.04,.66,.035));
		Part(TEXT("LeftRearPost"), Cube.Object, FVector(-47,-29,7), FVector(.035,.035,.94));
		Part(TEXT("RightRearPost"), Cube.Object, FVector(-47,29,7), FVector(.035,.035,.94));
		Part(TEXT("LeftFrontPost"), Cube.Object, FVector(47,-29,3), FVector(.035,.035,.56));
		Part(TEXT("RightFrontPost"), Cube.Object, FVector(47,29,3), FVector(.035,.035,.56));
		Part(TEXT("PushHandle"), Cube.Object, FVector(-47,0,54), FVector(.045,.67,.045));
	}
	if (Cylinder.Succeeded())
	{
		Part(TEXT("WheelFL"), Cylinder.Object, FVector(40,-29,-36), FVector(.16,.16,.055), FRotator(90,0,0));
		Part(TEXT("WheelFR"), Cylinder.Object, FVector(40,29,-36), FVector(.16,.16,.055), FRotator(90,0,0));
		Part(TEXT("WheelRL"), Cylinder.Object, FVector(-40,-29,-36), FVector(.16,.16,.055), FRotator(90,0,0));
		Part(TEXT("WheelRR"), Cylinder.Object, FVector(-40,29,-36), FVector(.16,.16,.055), FRotator(90,0,0));
	}

	// Prominent Rugged Shared Team Computer Monitor mounted above the rear handle console (-X facing operator)
	ComputerBaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ComputerBaseMesh"));
	ComputerBaseMesh->SetupAttachment(CartVisualRoot);
	ComputerBaseMesh->SetRelativeLocation(FVector(-43.0, 0.0, 50.0));
	ComputerBaseMesh->SetRelativeScale3D(FVector(0.16, 0.28, 0.30));
	ComputerBaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ComputerBaseMesh->SetCanEverAffectNavigation(false);
	ComputerBaseMesh->SetCastShadow(false);

	ComputerScreenMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ComputerScreenMesh"));
	ComputerScreenMesh->SetupAttachment(CartVisualRoot);
	ComputerScreenMesh->SetRelativeLocation(FVector(-47.5, 0.0, 74.0));
	ComputerScreenMesh->SetRelativeRotation(FRotator(-10.0, 0.0, 0.0));
	ComputerScreenMesh->SetRelativeScale3D(FVector(0.055, 0.64, 0.44));
	ComputerScreenMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ComputerScreenMesh->SetCanEverAffectNavigation(false);
	ComputerScreenMesh->SetCastShadow(false);

	if (Cube.Succeeded())
	{
		ComputerBaseMesh->SetStaticMesh(Cube.Object);
		ComputerScreenMesh->SetStaticMesh(Cube.Object);
	}

	const FRotator ScreenTextRot(10.0, 180.0, 0.0);
	ComputerHeaderText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ComputerHeaderText"));
	ComputerHeaderText->SetupAttachment(CartVisualRoot);
	ComputerHeaderText->SetRelativeLocation(FVector(-51.0, 0.0, 86.5));
	ComputerHeaderText->SetRelativeRotation(ScreenTextRot);
	ComputerHeaderText->SetHorizontalAlignment(EHTA_Center);
	ComputerHeaderText->SetVerticalAlignment(EVRTA_TextCenter);
	ComputerHeaderText->SetWorldSize(3.6f);
	ComputerHeaderText->SetTextRenderColor(FColor(85, 255, 215));
	ComputerHeaderText->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ComputerBodyText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ComputerBodyText"));
	ComputerBodyText->SetupAttachment(CartVisualRoot);
	ComputerBodyText->SetRelativeLocation(FVector(-51.0, 0.0, 74.0));
	ComputerBodyText->SetRelativeRotation(ScreenTextRot);
	ComputerBodyText->SetHorizontalAlignment(EHTA_Center);
	ComputerBodyText->SetVerticalAlignment(EVRTA_TextCenter);
	ComputerBodyText->SetWorldSize(3.0f);
	ComputerBodyText->SetTextRenderColor(FColor(195, 250, 225));
	ComputerBodyText->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ComputerFooterText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ComputerFooterText"));
	ComputerFooterText->SetupAttachment(CartVisualRoot);
	ComputerFooterText->SetRelativeLocation(FVector(-51.0, 0.0, 61.5));
	ComputerFooterText->SetRelativeRotation(ScreenTextRot);
	ComputerFooterText->SetHorizontalAlignment(EHTA_Center);
	ComputerFooterText->SetVerticalAlignment(EVRTA_TextCenter);
	ComputerFooterText->SetWorldSize(2.7f);
	ComputerFooterText->SetTextRenderColor(FColor(255, 220, 95));
	ComputerFooterText->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ComputerScreenLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ComputerScreenLight"));
	ComputerScreenLight->SetupAttachment(CartVisualRoot);
	ComputerScreenLight->SetRelativeLocation(FVector(-55.0, 0.0, 74.0));
	ComputerScreenLight->SetLightColor(FLinearColor(0.18f, 0.95f, 0.78f));
	ComputerScreenLight->SetIntensity(110.0f);
	ComputerScreenLight->SetAttenuationRadius(95.0f);
	ComputerScreenLight->SetCastShadows(false);
	ComputerScreenLight->MaxDrawDistance = 2400.0f;
	ComputerScreenLight->MaxDistanceFadeRange = 500.0f;

	ComputerViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ComputerViewCamera"));
	ComputerViewCamera->SetupAttachment(CartVisualRoot);
	ComputerViewCamera->SetRelativeLocation(FVector(-94.0, 0.0, 78.0));
	ComputerViewCamera->SetRelativeRotation(FRotator(-6.0, 0.0, 0.0));
	ComputerViewCamera->SetFieldOfView(54.0f);
	ComputerViewCamera->bUsePawnControlRotation = false;
}

bool AMallPushCart::IsBeingPushed() const { return IsValid(Pusher.Get()); }

bool AMallPushCart::IsSettled() const { return !IsBeingPushed() && (!bEnableFreePhysics || bSettled); }

void AMallPushCart::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallPushCart, Pusher);
	DOREPLIFETIME(AMallPushCart, StoredItems);
	DOREPLIFETIME(AMallPushCart, PushSpeed);
	DOREPLIFETIME(AMallPushCart, bEnableFreePhysics);
	DOREPLIFETIME(AMallPushCart, bSettled);
	DOREPLIFETIME(AMallPushCart, ComputerOperator);
	DOREPLIFETIME(AMallPushCart, ActiveComputerTab);
	DOREPLIFETIME(AMallPushCart, SharedEvidenceMask);
	DOREPLIFETIME(AMallPushCart, AutoDetectedEvidenceMask);
	DOREPLIFETIME(AMallPushCart, ManualEvidenceMask);
	DOREPLIFETIME(AMallPushCart, DeducedGhostArchetype);
	DOREPLIFETIME(AMallPushCart, SyncedPhotoCount);
	DOREPLIFETIME(AMallPushCart, SyncedBestPhotoStars);
	DOREPLIFETIME(AMallPushCart, SyncedTotalPhotoReward);
	DOREPLIFETIME(AMallPushCart, SyncedLastPhotoSubject);
	DOREPLIFETIME(AMallPushCart, SelectedRemoteFeedIndex);
	DOREPLIFETIME(AMallPushCart, SyncedRemoteFeedStatus);
}

void AMallPushCart::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	for (UStaticMeshComponent* Part : ProceduralParts) { if (Part) { Part->SetVisibility(bShowProceduralCart); } }
	UpdateComputerVisuals();
}

void AMallPushCart::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		StoredItems.SetNum(FMath::Clamp(StorageCapacity, 0, 8));
		SyncComputerFromWorldEquipment();
	}
	SettledLocation = GetActorLocation();
	SettledYaw = GetActorRotation().Yaw;
	RefreshPhysicsMode();
	UpdateComputerVisuals();
}

void AMallPushCart::SetFreePhysicsEnabled(bool bEnabled)
{
	if (!HasAuthority()) { return; }
	bEnableFreePhysics = bEnabled;
	StablePhysicsSeconds = 0.0f;
	SetSettled(!bEnabled && !Pusher);
	RefreshPhysicsMode();
	ForceNetUpdate();
}

void AMallPushCart::OnRep_Settled() { OnPushStateChanged(); }

void AMallPushCart::SetSettled(bool bNewSettled)
{
	if (!HasAuthority() || bSettled == bNewSettled) { return; }
	bSettled = bNewSettled;
	if (bSettled) { SettledLocation = GetActorLocation(); SettledYaw = GetActorRotation().Yaw; }
	OnPushStateChanged();
	ForceNetUpdate();
}

void AMallPushCart::RefreshPhysicsMode()
{
	if (!CartBounds || !HasActorBegunPlay()) { return; }
	CartBounds->SetCollisionObjectType(ECC_GameTraceChannel1);
	const bool bShouldSimulate = bEnableFreePhysics && !IsBeingPushed() && !IsActorBeingDestroyed();
	const bool bChanged = CartBounds->IsSimulatingPhysics() != bShouldSimulate;
	if (bChanged || !bPhysicsConfigured)
	{
		// Existing Blueprint BodyInstance overrides may already be simulating before BeginPlay.
		bPhysicsConfigured = true;
		// Existing Blueprint BodyInstance overrides may predate the native physics defaults.
		CartBounds->BodyInstance.bLockTranslation = false;
		CartBounds->BodyInstance.bLockXTranslation = false;
		CartBounds->BodyInstance.bLockYTranslation = false;
		CartBounds->BodyInstance.bLockZTranslation = false;
		CartBounds->BodyInstance.bLockRotation = false;
		CartBounds->BodyInstance.bLockXRotation = true;
		CartBounds->BodyInstance.bLockYRotation = true;
		CartBounds->BodyInstance.bLockZRotation = false;
		if (bChanged)
		{
			if (CartBounds->IsSimulatingPhysics())
			{
				CartBounds->SetPhysicsLinearVelocity(FVector::ZeroVector);
				CartBounds->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			}
			CartBounds->SetSimulatePhysics(bShouldSimulate);
		}
		if (bShouldSimulate)
		{
			CartBounds->SetConstraintMode(EDOFMode::SixDOF);
			CartBounds->SetUseCCD(true);
			CartBounds->SetMaxDepenetrationVelocity(NAME_None,120.0f);
		}
		if (bShouldSimulate && HasAuthority()) { StablePhysicsSeconds = 0.0f; SetSettled(false); }
	}
	if (!bShouldSimulate) { return; }
	const float Mass = FMath::IsFinite(CartMassKg) ? FMath::Clamp(CartMassKg,20.0f,500.0f) : 100.0f;
	const float Linear = FMath::IsFinite(FreeLinearDamping) ? FMath::Clamp(FreeLinearDamping,.1f,20.0f) : 3.0f;
	const float Angular = FMath::IsFinite(FreeAngularDamping) ? FMath::Clamp(FreeAngularDamping,.1f,20.0f) : 5.0f;
	if (bChanged || !FMath::IsNearlyEqual(Mass,AppliedMassKg)) { CartBounds->SetMassOverrideInKg(NAME_None,Mass,true); AppliedMassKg = Mass; }
	if (bChanged || !FMath::IsNearlyEqual(Linear,AppliedLinearDamping)) { CartBounds->SetLinearDamping(Linear); AppliedLinearDamping = Linear; }
	if (bChanged || !FMath::IsNearlyEqual(Angular,AppliedAngularDamping)) { CartBounds->SetAngularDamping(Angular); AppliedAngularDamping = Angular; }
	CartBounds->SetEnableGravity(true);
	CartBounds->SetPhysicsMaxAngularVelocityInDegrees(FMath::IsFinite(MaxBumpAngularSpeedDegrees) ? FMath::Clamp(MaxBumpAngularSpeedDegrees,10.0f,180.0f) : 80.0f);
}

void AMallPushCart::UpdateFreePhysics(float DeltaSeconds)
{
	if (!HasAuthority()) { return; }
	if (!bEnableFreePhysics || !CartBounds->IsSimulatingPhysics()) { SetSettled(true); return; }
	FVector Velocity = CartBounds->GetPhysicsLinearVelocity();
	FVector Angular = CartBounds->GetPhysicsAngularVelocityInDegrees();
	if (Velocity.ContainsNaN() || Angular.ContainsNaN())
	{
		CartBounds->SetPhysicsLinearVelocity(FVector::ZeroVector);
		CartBounds->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		SetSettled(false);
		return;
	}
	const float SpeedLimit = FMath::IsFinite(MaxBumpSpeedCmPerSecond) ? FMath::Clamp(MaxBumpSpeedCmPerSecond,30.0f,600.0f) : 180.0f;
	const FVector Horizontal = FVector(Velocity.X,Velocity.Y,0).GetClampedToMaxSize(SpeedLimit);
	if (!Horizontal.Equals(FVector(Velocity.X,Velocity.Y,0),.01))
	{
		Velocity.X = Horizontal.X; Velocity.Y = Horizontal.Y;
		CartBounds->SetPhysicsLinearVelocity(Velocity);
	}
	const float AngularLimit = FMath::IsFinite(MaxBumpAngularSpeedDegrees) ? FMath::Clamp(MaxBumpAngularSpeedDegrees,10.0f,180.0f) : 80.0f;
	const FVector LimitedAngular(0,0,FMath::Clamp(Angular.Z,-static_cast<double>(AngularLimit),static_cast<double>(AngularLimit)));
	if (!Angular.Equals(LimitedAngular,.01)) { Angular = LimitedAngular; CartBounds->SetPhysicsAngularVelocityInDegrees(Angular); }
	const double Speed = Velocity.Size();
	const double TurnSpeed = FMath::Abs(Angular.Z);
	if (bSettled)
	{
		if (Speed > 12.0 || TurnSpeed > 10.0 || FVector::DistSquared(GetActorLocation(),SettledLocation) > FMath::Square(3.0)
			|| FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,static_cast<double>(SettledYaw))) > 2.0)
		{
			StablePhysicsSeconds = 0.0f;
			SetSettled(false);
		}
		return;
	}
	if (Speed <= 3.0 && TurnSpeed <= 3.0) { StablePhysicsSeconds += FMath::Clamp(DeltaSeconds,0.0f,.1f); }
	else { StablePhysicsSeconds = 0.0f; }
	const float Duration = FMath::IsFinite(SettleDurationSeconds) ? FMath::Clamp(SettleDurationSeconds,.1f,3.0f) : .6f;
	if (StablePhysicsSeconds >= Duration)
	{
		SetSettled(true);
		CartBounds->PutRigidBodyToSleep();
	}
}

AMallPushCart* AMallPushCart::FindCartInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction)
{
	if (!IsValid(Pawn) || !Pawn->GetWorld() || Origin.ContainsNaN() || Direction.ContainsNaN()
		|| FMath::Abs(Direction.SizeSquared()-1.0) > .05) { return nullptr; }
	UWorld* World = Pawn->GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallCartFocus), false, Pawn);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetClass()->ImplementsInterface(UMallCartItem::StaticClass())
			&& IMallCartItem::Execute_GetCartItemCarrier(*It) == Pawn) { Params.AddIgnoredActor(*It); }
	}
	auto VisibleCart = [&](AActor* Candidate) -> AMallPushCart*
	{
		for (int32 Depth = 0; IsValid(Candidate) && Depth < 3; ++Depth)
		{
			if (AMallPushCart* Cart = Cast<AMallPushCart>(Candidate))
			{
				if (FVector::DistSquared(Pawn->GetActorLocation(),Cart->GetActorLocation()) > FMath::Square(280.0)) { return nullptr; }
				for (const FVector& Eye : {Origin,Pawn->GetPawnViewLocation()})
				{
					FVector Point;
					if (Cart->CartBounds->GetClosestPointOnCollision(Eye,Point) < 0) { Point = Cart->GetActorLocation(); }
					FHitResult Obstruction;
					if (World->LineTraceSingleByChannel(Obstruction,Eye,Point,ECC_Visibility,Params)
						&& Obstruction.GetActor() != Cart && (!Obstruction.GetActor() || Obstruction.GetActor()->GetAttachParentActor() != Cart)) { return nullptr; }
				}
				return Cart;
			}
			Candidate = Candidate->GetAttachParentActor();
		}
		return nullptr;
	};
	FHitResult Exact;
	if (World->LineTraceSingleByChannel(Exact,Origin,Origin+Direction*1000.0,ECC_Visibility,Params))
	{
		if (AMallPushCart* Cart = VisibleCart(Exact.GetActor())) { return Cart; }
	}
	TArray<FHitResult> Hits;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	World->SweepMultiByObjectType(Hits,Origin,Origin+Direction*1000.0,FQuat::Identity,Objects,FCollisionShape::MakeSphere(38.0),Params);
	for (const FHitResult& Hit : Hits) { if (AMallPushCart* Cart = VisibleCart(Hit.GetActor())) { return Cart; } }
	return nullptr;
}

bool AMallPushCart::IsEligiblePlayer(const APlayerController* Player)
{
	const ACharacter* Pawn = IsValid(Player) ? Cast<ACharacter>(Player->GetPawn()) : nullptr;
	if (!IsValid(Pawn) || !Player->HasAuthority() || !Pawn->HasAuthority() || Player->IsActorBeingDestroyed()
		|| Pawn->IsActorBeingDestroyed() || Pawn->GetController() != Player || Pawn->GetWorld() != Player->GetWorld()) { return false; }
	if (const FNumericProperty* Health = FindFProperty<FNumericProperty>(Pawn->GetClass(), TEXT("Health")))
	{
		const void* Data = Health->ContainerPtrToValuePtr<void>(Pawn);
		const double Value = Health->IsFloatingPoint() ? Health->GetFloatingPointPropertyValue(Data) : static_cast<double>(Health->GetSignedIntPropertyValue(Data));
		if (!FMath::IsFinite(Value) || Value <= 0) { return false; }
	}
	if (const FBoolProperty* Dead = FindFProperty<FBoolProperty>(Pawn->GetClass(), TEXT("bIsDead")))
	{
		if (Dead->GetPropertyValue_InContainer(Pawn)) { return false; }
	}
	return true;
}

bool AMallPushCart::CanPlayerReach(APlayerController* Player) const
{
	if (!HasAuthority() || !GetWorld() || !IsEligiblePlayer(Player) || Player->GetWorld() != GetWorld()
		|| FVector::DistSquared(Player->GetPawn()->GetActorLocation(), GetActorLocation()) > FMath::Square(280.0)) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallCartReach), true, Player->GetPawn());
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->ImplementsInterface(UMallCartItem::StaticClass())
			&& IMallCartItem::Execute_GetCartItemCarrier(*It) == Player->GetPawn())
		{
			Params.AddIgnoredActor(*It);
		}
	}
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Player->GetPawn()->GetPawnViewLocation(), GetActorLocation(), ECC_Visibility, Params)
		|| Hit.GetActor() == this || (Hit.GetActor() && Hit.GetActor()->GetAttachParentActor() == this);
}

bool AMallPushCart::CanPlayerPushFromHandle(const APlayerController* Player) const
{
	const ACharacter* Character = IsValid(Player) ? Cast<ACharacter>(Player->GetPawn()) : nullptr;
	if (!IsValid(Character) || Character->GetWorld() != GetWorld()
		|| FVector::DistSquared(Character->GetActorLocation(), GetActorLocation()) > FMath::Square(280.0)) { return false; }
	const FVector Relative = GetActorTransform().InverseTransformPosition(Character->GetActorLocation());
	const float MinDist = FMath::IsFinite(HandleMinDistanceCm) ? FMath::Clamp(HandleMinDistanceCm, 40.0f, 120.0f) : 50.0f;
	const float MaxDist = FMath::IsFinite(HandleMaxDistanceCm) ? FMath::Clamp(HandleMaxDistanceCm, 120.0f, 260.0f) : 210.0f;
	const float HalfWidth = FMath::IsFinite(HandleHalfWidthCm) ? FMath::Clamp(HandleHalfWidthCm, 50.0f, 160.0f) : 100.0f;
	return Relative.X <= -MinDist && Relative.X >= -MaxDist && FMath::Abs(Relative.Y) <= HalfWidth && FMath::Abs(Relative.Z) <= 120.0;
}

int32 AMallPushCart::FindAimedStoredItemSlot(const FVector& Origin, const FVector& Direction) const
{
	if (!GetWorld() || Origin.ContainsNaN() || Direction.ContainsNaN() || FMath::Abs(Direction.SizeSquared() - 1.0) > 0.05)
	{
		return INDEX_NONE;
	}
	int32 BestSlot = INDEX_NONE;
	double BestDistSq = FMath::Square(28.0);
	for (int32 Index = 0; Index < StoredItems.Num(); ++Index)
	{
		const AActor* Item = StoredItems[Index];
		if (!IsValid(Item)) { continue; }
		const FVector ItemPos = Item->GetActorLocation() + FVector(0.0, 0.0, 6.0);
		const FVector Projected = FMath::ClosestPointOnSegment(ItemPos, Origin, Origin + Direction * 400.0);
		const double DistSq = FVector::DistSquared(ItemPos, Projected);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestSlot = Index;
		}
	}
	return BestSlot;
}

bool AMallPushCart::HasStoredItems() const
{
	for (const AActor* Item : StoredItems)
	{
		if (IsValid(Item)) { return true; }
	}
	return false;
}

bool AMallPushCart::IsPawnPushingCart(const APawn* Pawn)
{
	if (!IsValid(Pawn) || !Pawn->GetWorld()) { return false; }
	for (TActorIterator<AMallPushCart> It(Pawn->GetWorld()); It; ++It)
	{
		if (It->Pusher == Pawn) { return true; }
	}
	return false;
}

bool AMallPushCart::TryBeginPush(APlayerController* Player)
{
	if (!CanPlayerReach(Player) || Pusher) { return false; }
	ACharacter* Character = Cast<ACharacter>(Player->GetPawn());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || !Movement->IsMovingOnGround() || Character->GetAttachParentActor()) { return false; }
	for (TActorIterator<AMallPushCart> It(GetWorld()); It; ++It) { if (It->Pusher == Character) { return false; } }
	// The handle is on -X. Avoid snapping a player from the far side or through a shelf.
	if (!CanPlayerPushFromHandle(Player)) { return false; }
	const float MinSafeDist = (CartBounds ? static_cast<float>(CartBounds->GetScaledBoxExtent().X) : 55.0f)
		+ (Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.0f) + 6.0f;
	PushDistance = FMath::Clamp(static_cast<float>(FVector::Dist2D(Character->GetActorLocation(), GetActorLocation())), FMath::Max(95.0f, MinSafeDist), 165.0f);
	if (IsValid(ComputerOperator))
	{
		ComputerOperator = nullptr;
		ComputerOperatorController.Reset();
		UpdateComputerVisuals();
	}
	Pusher = Character;
	PusherController = Player;
	SetSettled(false);
	RefreshPhysicsMode();
	ApplyPusherSettings();
	OnPushStateChanged();
	BP_OnPushStarted(Character);
	ForceNetUpdate();
	return true;
}

bool AMallPushCart::StopPushing(APlayerController* Player)
{
	if (!HasAuthority() || !IsValid(Player) || !Pusher || Pusher->GetController() != Player) { return false; }
	EndPush();
	return true;
}

void AMallPushCart::EndPush()
{
	if (!HasAuthority()) { return; }
	ACharacter* OldPusher = Pusher;
	RestorePusherSettings();
	Pusher = nullptr;
	PusherController.Reset();
	StablePhysicsSeconds = 0.0f;
	SetSettled(!bEnableFreePhysics);
	RefreshPhysicsMode();
	OnPushStateChanged();
	if (OldPusher) { BP_OnPushStopped(OldPusher); }
	ForceNetUpdate();
}

void AMallPushCart::OnRep_Pusher()
{
	RestorePusherSettings();
	RefreshPhysicsMode();
	ApplyPusherSettings();
	OnPushStateChanged();
}

void AMallPushCart::ApplyPusherSettings()
{
	if (!IsValid(Pusher)) { return; }
	UCharacterMovementComponent* Movement = Pusher->GetCharacterMovement();
	if (!Movement) { return; }
	if (ConfiguredPusher.Get() != Pusher)
	{
		RestorePusherSettings();
		ConfiguredPusher = Pusher;
		SavedWalkSpeed = Movement->MaxWalkSpeed;
		SavedCrouchedSpeed = Movement->MaxWalkSpeedCrouched;
		bSavedControllerYaw = Pusher->bUseControllerRotationYaw;
		bSavedOrientToMovement = Movement->bOrientRotationToMovement;
		bSavedControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
		AppliedWalkSpeed = SavedWalkSpeed;
		AppliedCrouchedSpeed = SavedCrouchedSpeed;
	}
	// Prevent the pusher's pre-physics capsule sweep from colliding with the rear of CartBounds
	// before FollowPusher() ticks in TG_PostPhysics.
	Pusher->MoveIgnoreActorAdd(this);
	if (UCapsuleComponent* Capsule = Pusher->GetCapsuleComponent())
	{
		Capsule->IgnoreActorWhenMoving(this, true);
	}
	if (CartBounds)
	{
		CartBounds->IgnoreActorWhenMoving(Pusher, true);
	}
	// An unrelated movement effect may change speeds while pushing. Retain that new baseline.
	if (!FMath::IsNearlyEqual(Movement->MaxWalkSpeed, AppliedWalkSpeed)) { SavedWalkSpeed = Movement->MaxWalkSpeed; }
	if (!FMath::IsNearlyEqual(Movement->MaxWalkSpeedCrouched, AppliedCrouchedSpeed)) { SavedCrouchedSpeed = Movement->MaxWalkSpeedCrouched; }
	const float Limit = FMath::IsFinite(PushSpeed) ? FMath::Clamp(PushSpeed, 25.0f, 600.0f) : 200.0f;
	AppliedWalkSpeed = FMath::Min(SavedWalkSpeed, Limit);
	AppliedCrouchedSpeed = FMath::Min(SavedCrouchedSpeed, Limit);
	Movement->MaxWalkSpeed = AppliedWalkSpeed;
	Movement->MaxWalkSpeedCrouched = AppliedCrouchedSpeed;
	Pusher->bUseControllerRotationYaw = true;
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = false;
}

void AMallPushCart::RestorePusherSettings()
{
	ACharacter* Previous = ConfiguredPusher.Get();
	if (IsValid(Previous))
	{
		Previous->MoveIgnoreActorRemove(this);
		if (UCapsuleComponent* Capsule = Previous->GetCapsuleComponent())
		{
			Capsule->IgnoreActorWhenMoving(this, false);
		}
		if (CartBounds)
		{
			CartBounds->IgnoreActorWhenMoving(Previous, false);
		}
		if (UCharacterMovementComponent* Movement = Previous->GetCharacterMovement())
		{
			if (FMath::IsNearlyEqual(Movement->MaxWalkSpeed, AppliedWalkSpeed)) { Movement->MaxWalkSpeed = SavedWalkSpeed; }
			if (FMath::IsNearlyEqual(Movement->MaxWalkSpeedCrouched, AppliedCrouchedSpeed)) { Movement->MaxWalkSpeedCrouched = SavedCrouchedSpeed; }
			if (!Movement->bOrientRotationToMovement) { Movement->bOrientRotationToMovement = bSavedOrientToMovement; }
			if (!Movement->bUseControllerDesiredRotation) { Movement->bUseControllerDesiredRotation = bSavedControllerDesiredRotation; }
		}
		if (Previous->bUseControllerRotationYaw) { Previous->bUseControllerRotationYaw = bSavedControllerYaw; }
	}
	ConfiguredPusher.Reset();
}

void AMallPushCart::OnPushStateChanged() {}

bool AMallPushCart::HasSafeFloor(const FVector& Centre, const FQuat& Rotation) const
{
	if (!GetWorld()) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallCartFloor), false, this);
	if (Pusher) { Params.AddIgnoredActor(Pusher); }
	for (AActor* Item : StoredItems) { if (Item) { Params.AddIgnoredActor(Item); } }
	for (const double X : {-45.0, 45.0})
	{
		for (const double Y : {-28.0, 28.0})
		{
			const FVector Corner = Centre + Rotation.RotateVector(FVector(X,Y,0));
			FHitResult Hit;
			if (!GetWorld()->LineTraceSingleByChannel(Hit, Corner + FVector(0,0,-30), Corner + FVector(0,0,-65), ECC_WorldStatic, Params)
				|| !Hit.GetComponent() || Hit.GetComponent()->IsSimulatingPhysics() || Cast<APawn>(Hit.GetActor())
				|| Hit.ImpactNormal.Z < .94 || FMath::Abs(Hit.ImpactPoint.Z - (Centre.Z - 45)) > 8) { return false; }
		}
	}
	return true;
}

void AMallPushCart::FollowPusher(float DeltaSeconds)
{
	if (!Pusher || !GetWorld()) { return; }
	ApplyPusherSettings();
	const float Step = FMath::Clamp(DeltaSeconds, 0.0f, .1f);
	const float Turn = FMath::IsFinite(TurnSpeedDegrees) ? FMath::Clamp(TurnSpeedDegrees, 30.0f, 180.0f) : 95.0f;
	const FRotator DesiredRotation(0, Pusher->GetActorRotation().Yaw, 0);
	const FRotator NewRotation = FMath::RInterpConstantTo(GetActorRotation(), DesiredRotation, Step, Turn);
	const FVector Forward = NewRotation.Vector();
	FVector Desired = Pusher->GetActorLocation() + Forward * PushDistance;
	Desired.Z = Pusher->GetActorLocation().Z - Pusher->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 45;
	const float Limit = FMath::IsFinite(PushSpeed) ? FMath::Clamp(PushSpeed, 25.0f, 600.0f) : 200.0f;
	const FVector Translation = (Desired - GetActorLocation()).GetClampedToMaxSize(Limit * Step);
	const FVector NewPosition = GetActorLocation() + Translation;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallCartMovement), false, this);
	if (Pusher) { Params.AddIgnoredActor(Pusher); }
	for (AActor* Item : StoredItems) { if (Item) { Params.AddIgnoredActor(Item); } }
	auto HoldPusherBehindCart = [&]()
	{
		Pusher->GetCharacterMovement()->StopMovementImmediately();
		const FVector CartToPusher = Pusher->GetActorLocation() - GetActorLocation();
		const double BackProjection = FVector::DotProduct(CartToPusher, -Forward);
		if (BackProjection < PushDistance - 1.0)
		{
			FVector ClampedPusher = GetActorLocation() - Forward * PushDistance;
			ClampedPusher.Z = Pusher->GetActorLocation().Z;
			Pusher->SetActorLocation(ClampedPusher, true);
		}
	};
	// Rotation sweep is not provided by Actor movement: reject an occupied final orientation explicitly.
	const bool bClear = !GetWorld()->OverlapBlockingTestByChannel(NewPosition, NewRotation.Quaternion(), ECC_GameTraceChannel1,
		FCollisionShape::MakeBox(CartBounds->GetScaledBoxExtent() * .99), Params);
	if (!bClear || !HasSafeFloor(NewPosition, NewRotation.Quaternion()))
	{
		HoldPusherBehindCart();
		return;
	}
	FHitResult Hit;
	SetActorLocationAndRotation(NewPosition, NewRotation, true, &Hit, ETeleportType::None);
	if (Hit.bBlockingHit) { HoldPusherBehindCart(); }
}

FTransform AMallPushCart::GetSlotWorldTransform(int32 SlotIndex) const
{
	const int32 Index = FMath::Clamp(SlotIndex, 0, 7);
	const FVector Offset((Index % 2) ? 26 : -26, ((Index / 2) % 2) ? 17 : -17, Index < 4 ? 47 : -20);
	return FTransform(GetActorRotation(), GetActorTransform().TransformPosition(Offset), FVector::OneVector);
}

bool AMallPushCart::ContainsStoredItem(const AActor* Item) const
{
	return IsValid(Item) && StoredItems.Contains(Item);
}

void AMallPushCart::ReconcileStorage()
{
	if (!HasAuthority()) { return; }
	bool bChanged = false;
	for (TObjectPtr<AActor>& Item : StoredItems)
	{
		if (Item && (!IsValid(Item) || !Item->GetClass()->ImplementsInterface(UMallCartItem::StaticClass())
			|| IMallCartItem::Execute_GetCartStorage(Item) != this)) { Item = nullptr; bChanged = true; }
	}
	if (bChanged) { ForceNetUpdate(); }
}

bool AMallPushCart::TryDeposit(APlayerController* Player, AActor* Item)
{
	if (!CanPlayerReach(Player) || !IsParked() || !IsValid(Item) || Item->GetWorld() != GetWorld() || !Item->HasAuthority()
		|| !Item->GetClass()->ImplementsInterface(UMallCartItem::StaticClass())
		|| IMallCartItem::Execute_GetCartItemCarrier(Item) != Player->GetPawn()
		|| IMallCartItem::Execute_GetCartStorage(Item)) { return false; }
	ReconcileStorage();
	const int32 Capacity = FMath::Clamp(StorageCapacity, 0, 8);
	// Do not shrink an occupied rack if a designer lowers capacity at runtime.
	if (StoredItems.Num() < Capacity) { StoredItems.SetNum(Capacity); }
	int32 Free = INDEX_NONE;
	for (int32 Index=0; Index < Capacity; ++Index) { if (!StoredItems[Index]) { Free=Index; break; } }
	if (Free == INDEX_NONE) { return false; }
	const FTransform Position = GetSlotWorldTransform(Free);
	if (!IMallCartItem::Execute_TryStoreOnCart(Item, Player, this, Position)
		|| IMallCartItem::Execute_GetCartStorage(Item) != this || IMallCartItem::Execute_GetCartItemCarrier(Item)) { return false; }
	StoredItems[Free] = Item;
	Item->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	OnRep_Storage();
	SyncComputerFromWorldEquipment();
	BP_OnItemStored(Item, Free);
	Item->ForceNetUpdate();
	ForceNetUpdate();
	return true;
}

bool AMallPushCart::TryRetrieve(APlayerController* Player, int32 SlotIndex)
{
	if (!CanPlayerReach(Player) || !IsParked()) { return false; }
	ReconcileStorage();
	// Characters with a 4-slot hotbar can carry multiple items across free slots; bare pawns hold one item.
	if (!MallHotbarInventory::HasInventory(Player->GetPawn()))
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (!It->IsA<AMallCartFlashlight>() && It->GetClass()->ImplementsInterface(UMallCartItem::StaticClass())
				&& IMallCartItem::Execute_GetCartItemCarrier(*It) == Player->GetPawn()) { return false; }
		}
	}
	auto TakeSlot = [this, Player](int32 Index)
	{
		if (!StoredItems.IsValidIndex(Index) || !IsValid(StoredItems[Index])) { return false; }
		AActor* Item = StoredItems[Index];
		if (!IMallCartItem::Execute_TryTakeFromCart(Item, Player, this)
			|| IMallCartItem::Execute_GetCartItemCarrier(Item) != Player->GetPawn()
			|| IMallCartItem::Execute_GetCartStorage(Item)) { return false; }
		StoredItems[Index] = nullptr;
		SyncComputerFromWorldEquipment();
		BP_OnItemRetrieved(Item, Cast<ACharacter>(Player->GetPawn()));
		Item->ForceNetUpdate();
		ForceNetUpdate();
		return true;
	};
	if (SlotIndex != INDEX_NONE) { return TakeSlot(SlotIndex); }
	// A spare inventory flashlight must not hide usable evidence tools beneath it.
	// Rejected adapters leave the original item and slot unchanged.
	for (int32 Index = StoredItems.Num() - 1; Index >= 0; --Index)
	{
		if (TakeSlot(Index)) { return true; }
	}
	return false;
}

void AMallPushCart::OnRep_Storage()
{
	for (int32 Index=0; Index<StoredItems.Num(); ++Index)
	{
		AActor* Item = StoredItems[Index];
		if (!IsValid(Item) || !Item->GetClass()->ImplementsInterface(UMallCartItem::StaticClass())
			|| IMallCartItem::Execute_GetCartStorage(Item) != this) { continue; }
		Item->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		Item->SetActorTransform(GetSlotWorldTransform(Index), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

bool AMallPushCart::TryUse(APlayerController* Player) { return false; }

FString AMallPushCart::GetCartStatus() const
{
	int32 Count = 0;
	for (AActor* Item : StoredItems) { if (IsValid(Item)) { ++Count; } }
	return FString::Printf(TEXT("%s | %d/%d items"), *CartName, Count, FMath::Max(StoredItems.Num(), FMath::Clamp(StorageCapacity,0,8)));
}

void AMallPushCart::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshPhysicsMode();
	if (Pusher) { ApplyPusherSettings(); }
	if (!HasAuthority()) { return; }
	ReconcileStorage();
	if (bHasComputerTerminal)
	{
		if (IsValid(ComputerOperator))
		{
			APlayerController* OpPC = ComputerOperatorController.Get();
			if (!IsEligiblePlayer(OpPC) || OpPC->GetPawn() != ComputerOperator || IsBeingPushed()
				|| FVector::DistSquared(ComputerOperator->GetActorLocation(), GetActorLocation()) > FMath::Square(290.0))
			{
				ComputerOperator = nullptr;
				ComputerOperatorController.Reset();
				UpdateComputerVisuals();
				ForceNetUpdate();
			}
		}
		const float SyncInterval = IsValid(ComputerOperator) ? 0.35f : 1.25f;
		ComputerSyncAccumulator += FMath::Clamp(DeltaSeconds, 0.0f, 0.25f);
		if (ComputerSyncAccumulator >= SyncInterval)
		{
			ComputerSyncAccumulator = 0.0f;
			SyncComputerFromWorldEquipment();
		}
	}
	if (!Pusher) { UpdateFreePhysics(DeltaSeconds); return; }
	APlayerController* PC = PusherController.Get();
	UCharacterMovementComponent* Movement = IsValid(Pusher) ? Pusher->GetCharacterMovement() : nullptr;
	if (!IsEligiblePlayer(PC) || PC->GetPawn() != Pusher || !Movement || !Movement->IsMovingOnGround()
		|| FVector::DistSquared(Pusher->GetActorLocation(), GetActorLocation()) > FMath::Square(260.0))
	{
		EndPush(); return;
	}
	FollowPusher(DeltaSeconds);
}

void AMallPushCart::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HasAuthority())
	{
		ComputerOperator = nullptr;
		ComputerOperatorController.Reset();
		EndPush();
		for (int32 Index=0; Index<StoredItems.Num(); ++Index)
		{
			AActor* Item = StoredItems[Index];
			if (!IsValid(Item) || !Item->GetClass()->ImplementsInterface(UMallCartItem::StaticClass())) { continue; }
			FCollisionQueryParams Params(SCENE_QUERY_STAT(MallCartRelease), true, this);
			for (AActor* Stored : StoredItems) { if (Stored) { Params.AddIgnoredActor(Stored); } }
			const FVector Offset(-72.0 + (Index % 4) * 48.0, Index < 4 ? -65.0 : 65.0, 40.0);
			const FVector Probe = GetActorTransform().TransformPosition(Offset);
			FHitResult Floor;
			FTransform Release = GetSlotWorldTransform(Index);
			if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Floor, Probe, Probe-FVector(0,0,250), ECC_WorldStatic, Params)
				&& Floor.ImpactNormal.Z > .94 && Floor.GetComponent() && !Floor.GetComponent()->IsSimulatingPhysics())
			{
				Release = FTransform(FRotator(0,GetActorRotation().Yaw,0),Floor.ImpactPoint+FVector(0,0,.5),FVector::OneVector);
			}
			IMallCartItem::Execute_ReleaseFromCart(Item, this, Release);
		}
	}
	RestorePusherSettings();
	Super::EndPlay(Reason);
}

bool AMallPushCart::ConfigurePawnFirstPersonView(APawn* Pawn, bool bEnableFirstPerson)
{
	ACharacter* Character = Cast<ACharacter>(Pawn);
	if (!IsValid(Character))
	{
		return false;
	}

	Character->BaseEyeHeight = 64.0f;
	Character->bUseControllerRotationYaw = bEnableFirstPerson;
	Character->bUseControllerRotationPitch = false;
	Character->bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = !bEnableFirstPerson;
		Movement->bUseControllerDesiredRotation = false;
	}

	if (USpringArmComponent* CameraBoom = Character->FindComponentByClass<USpringArmComponent>())
	{
		CameraBoom->TargetArmLength = bEnableFirstPerson ? 0.0f : 250.0f;
		CameraBoom->bDoCollisionTest = !bEnableFirstPerson;
		CameraBoom->bUsePawnControlRotation = true;
		CameraBoom->bEnableCameraLag = false;
		CameraBoom->bEnableCameraRotationLag = false;
		CameraBoom->SocketOffset = FVector::ZeroVector;
		CameraBoom->TargetOffset = FVector::ZeroVector;
		CameraBoom->SetRelativeLocation(bEnableFirstPerson ? FVector(16.0, 0.0, 64.0) : FVector(0.0, 0.0, 58.0));
	}

	if (UCameraComponent* FollowCamera = Character->FindComponentByClass<UCameraComponent>())
	{
		FollowCamera->bUsePawnControlRotation = true;
		FollowCamera->SetFieldOfView(bEnableFirstPerson ? 92.0f : 86.0f);
		FollowCamera->SetRelativeLocation(FVector::ZeroVector);
		FollowCamera->SetRelativeRotation(FRotator::ZeroRotator);
	}

	if (USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		if (bEnableFirstPerson && Character->IsLocallyControlled())
		{
			Mesh->bCastHiddenShadow = true;
			if (Mesh->GetBoneIndex(TEXT("neck_01")) != INDEX_NONE)
			{
				Mesh->HideBoneByName(TEXT("neck_01"), PBO_None);
			}
			else if (Mesh->GetBoneIndex(TEXT("head")) != INDEX_NONE)
			{
				Mesh->HideBoneByName(TEXT("head"), PBO_None);
			}
		}
		else if (!bEnableFirstPerson)
		{
			Mesh->UnHideBoneByName(TEXT("neck_01"));
			Mesh->UnHideBoneByName(TEXT("head"));
		}
	}

	if (bEnableFirstPerson)
	{
		Character->Tags.AddUnique(TEXT("MallFirstPersonPawn"));
	}
	else
	{
		Character->Tags.Remove(TEXT("MallFirstPersonPawn"));
	}
	return true;
}

void AMallPushCart::OnRep_ComputerState()
{
	UpdateComputerVisuals();
}

FString AMallPushCart::DeduceGhostTypeFromEvidence(uint8 EvidenceMask)
{
	using namespace EMallGhostEvidenceBit;
	struct FGhostArchetypeEntry
	{
		const TCHAR* Name;
		uint8 RequiredMask;
	};
	static const FGhostArchetypeEntry Archetypes[] = {
		{ TEXT("FROST WRAITH"), (EMFLevel5 | UVFingerprints | FreezingTemps) },
		{ TEXT("REVENANT"),     (EMFLevel5 | FreezingTemps  | GhostWriting) },
		{ TEXT("POLTERGEIST"),  (EMFLevel5 | UVFingerprints | SpiritBox) },
		{ TEXT("BANSHEE"),      (UVFingerprints | FreezingTemps | MotionDetected) },
		{ TEXT("JINN"),         (EMFLevel5 | SpiritBox | MotionDetected) },
		{ TEXT("PHANTOM"),      (UVFingerprints | SpiritBox | GhostWriting) },
		{ TEXT("SHADE"),        (FreezingTemps | GhostWriting | MotionDetected) }
	};

	const uint8 CleanMask = EvidenceMask & 0x3F;
	if (CleanMask == 0)
	{
		return TEXT("UNDETERMINED (0/3 Evidence)");
	}

	// Exact 3-evidence match first
	for (const FGhostArchetypeEntry& Entry : Archetypes)
	{
		if (CleanMask == Entry.RequiredMask)
		{
			return FString::Printf(TEXT("CONFIRMED: %s"), Entry.Name);
		}
	}

	// Superset match (e.g. 3+ evidence bits collected where an archetype's 3 signature evidences are all present)
	for (const FGhostArchetypeEntry& Entry : Archetypes)
	{
		if ((CleanMask & Entry.RequiredMask) == Entry.RequiredMask)
		{
			return FString::Printf(TEXT("CONFIRMED: %s"), Entry.Name);
		}
	}

	// Subset candidate filtering (1 or 2 evidence bits)
	TArray<FString> Candidates;
	for (const FGhostArchetypeEntry& Entry : Archetypes)
	{
		if ((Entry.RequiredMask & CleanMask) == CleanMask)
		{
			Candidates.Add(Entry.Name);
		}
	}
	if (Candidates.Num() == 1)
	{
		return FString::Printf(TEXT("LIKELY: %s"), *Candidates[0]);
	}
	if (Candidates.Num() > 1)
	{
		return FString::Printf(TEXT("CANDIDATES: %s"), *FString::Join(Candidates, TEXT(" / ")));
	}
	return TEXT("CONFIRMED: ELDER SPECTER");
}

void AMallPushCart::SyncComputerFromWorldEquipment()
{
	if (!HasAuthority() || !GetWorld() || !bHasComputerTerminal)
	{
		return;
	}

	using namespace EMallGhostEvidenceBit;
	uint8 NewAutoMask = 0;

	for (TActorIterator<AMallEMFReader> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && (It->bEMF5Detected || It->PeakEMFLevel >= 5 || It->CurrentEMFLevel >= 5))
		{
			NewAutoMask |= EMFLevel5;
		}
	}
	for (TActorIterator<AMallUVLight> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && (It->TotalPrintsRevealed > 0 || It->IlluminatedPrintCount > 0))
		{
			NewAutoMask |= UVFingerprints;
		}
	}
	for (TActorIterator<AMallUVEvidence> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && (It->bEverDiscovered || It->bIlluminatedByUV))
		{
			NewAutoMask |= UVFingerprints;
		}
	}
	for (TActorIterator<AMallThermometer> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && (It->bFreezingDetected || It->CurrentTemperatureCelsius <= 0.0f))
		{
			NewAutoMask |= FreezingTemps;
		}
	}
	for (TActorIterator<AMallSpiritBox> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->ResponseCount > 0)
		{
			NewAutoMask |= SpiritBox;
		}
	}
	for (TActorIterator<AMallGhostWritingBook> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->IsWritten())
		{
			NewAutoMask |= GhostWriting;
		}
	}
	for (TActorIterator<AMallMotionSensor> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && (It->bMotionDetected || It->TriggerCount > 0))
		{
			NewAutoMask |= MotionDetected;
		}
	}

	int32 TotalPhotos = 0;
	int32 BestStars = 0;
	int32 TotalReward = 0;
	FString LatestSubject = TEXT("No Photos Logged");
	for (TActorIterator<AMallPhotoCamera> It(GetWorld()); It; ++It)
	{
		const AMallPhotoCamera* Cam = *It;
		if (!IsValid(Cam)) { continue; }
		TotalPhotos += Cam->PhotosTaken;
		TotalReward += Cam->TotalPhotoValue;
		if (Cam->LastPhotoStars >= BestStars && Cam->PhotosTaken > 0)
		{
			BestStars = Cam->LastPhotoStars;
			if (!Cam->LastPhotoSubject.IsEmpty())
			{
				LatestSubject = Cam->LastPhotoSubject;
			}
		}
	}

	// Gather deployed remote cameras & sensors (placed in world or stored)
	TArray<FString> RemoteFeeds;
	int32 CamIdx = 1;
	for (TActorIterator<AMallPhotoCamera> It(GetWorld()); It; ++It)
	{
		const AMallPhotoCamera* Cam = *It;
		if (!IsValid(Cam)) { continue; }
		const TCHAR* ModeTag = (!Cam->GetCarrier() && !Cam->StoredOnCart) ? TEXT("DEPLOYED CAM") : TEXT("TEAM CAM");
		RemoteFeeds.Add(FString::Printf(TEXT("%s #%d: %d/%d shots | %d* %s ($%d)"),
			ModeTag, CamIdx++, FMath::Max(0, Cam->MaxPhotos - Cam->PhotosTaken), Cam->MaxPhotos,
			Cam->LastPhotoStars, Cam->LastPhotoSubject.IsEmpty() ? TEXT("Ready") : *Cam->LastPhotoSubject, Cam->TotalPhotoValue));
	}
	int32 MotionIdx = 1;
	for (TActorIterator<AMallMotionSensor> It(GetWorld()); It; ++It)
	{
		const AMallMotionSensor* Sensor = *It;
		if (!IsValid(Sensor)) { continue; }
		RemoteFeeds.Add(FString::Printf(TEXT("MOTION #%d: %s | %d triggers"),
			MotionIdx++, !Sensor->IsPowered() ? TEXT("OFF") : (Sensor->bMotionDetected ? TEXT("MOTION ALERT!") : TEXT("ARMED")), Sensor->TriggerCount));
	}
	int32 EMFIdx = 1;
	for (TActorIterator<AMallEMFReader> It(GetWorld()); It; ++It)
	{
		const AMallEMFReader* EMF = *It;
		if (!IsValid(EMF)) { continue; }
		RemoteFeeds.Add(FString::Printf(TEXT("EMF #%d: %s (Peak LVL %d%s)"),
			EMFIdx++, EMF->IsPowered() ? *FString::Printf(TEXT("LVL %d"), EMF->CurrentEMFLevel) : TEXT("OFF"),
			EMF->PeakEMFLevel, EMF->bEMF5Detected ? TEXT(" | EMF-5") : TEXT("")));
	}

	FString NewRemoteStatus = TEXT("No Remote Cameras/Sensors Deployed");
	if (RemoteFeeds.Num() > 0)
	{
		const int32 SafeIdx = ((SelectedRemoteFeedIndex % RemoteFeeds.Num()) + RemoteFeeds.Num()) % RemoteFeeds.Num();
		NewRemoteStatus = FString::Printf(TEXT("[%d/%d] %s"), SafeIdx + 1, RemoteFeeds.Num(), *RemoteFeeds[SafeIdx]);
	}

	const uint8 NewSharedMask = (NewAutoMask | ManualEvidenceMask) & 0x3F;
	const FString NewDeduction = DeduceGhostTypeFromEvidence(NewSharedMask);

	const bool bChanged = (AutoDetectedEvidenceMask != NewAutoMask)
		|| (SharedEvidenceMask != NewSharedMask)
		|| (DeducedGhostArchetype != NewDeduction)
		|| (SyncedPhotoCount != TotalPhotos)
		|| (SyncedBestPhotoStars != BestStars)
		|| (SyncedTotalPhotoReward != TotalReward)
		|| (SyncedLastPhotoSubject != LatestSubject)
		|| (SyncedRemoteFeedStatus != NewRemoteStatus);

	AutoDetectedEvidenceMask = NewAutoMask;
	SharedEvidenceMask = NewSharedMask;
	DeducedGhostArchetype = NewDeduction;
	SyncedPhotoCount = TotalPhotos;
	SyncedBestPhotoStars = BestStars;
	SyncedTotalPhotoReward = TotalReward;
	SyncedLastPhotoSubject = LatestSubject;
	SyncedRemoteFeedStatus = NewRemoteStatus;

	if (bChanged)
	{
		UpdateComputerVisuals();
		ForceNetUpdate();
	}
}

bool AMallPushCart::IsComputerInUse() const
{
	return bHasComputerTerminal && IsValid(ComputerOperator.Get());
}

bool AMallPushCart::IsComputerInUseBy(const APawn* Pawn) const
{
	return bHasComputerTerminal && IsValid(Pawn) && ComputerOperator == Pawn;
}

bool AMallPushCart::TryBeginUseComputer(APlayerController* Player)
{
	if (!bHasComputerTerminal || !CanPlayerReach(Player))
	{
		return false;
	}
	ACharacter* Character = Cast<ACharacter>(Player->GetPawn());
	if (!IsValid(Character))
	{
		return false;
	}
	// Exclusive to one player at a time!
	if (IsValid(ComputerOperator) && ComputerOperator != Character)
	{
		return false;
	}
	if (Pusher == Character)
	{
		EndPush();
	}
	else if (IsBeingPushed())
	{
		return false;
	}
	// Release any other cart computer this player might have been using
	for (TActorIterator<AMallPushCart> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->ComputerOperator == Character)
		{
			It->StopUseComputer(Player);
		}
	}
	ComputerOperator = Character;
	ComputerOperatorController = Player;
	SyncComputerFromWorldEquipment();
	UpdateComputerVisuals();
	ForceNetUpdate();
	return true;
}

bool AMallPushCart::StopUseComputer(APlayerController* Player)
{
	if (!HasAuthority() || !IsValid(ComputerOperator))
	{
		return false;
	}
	if (Player && Player->GetPawn() != ComputerOperator && ComputerOperatorController.Get() != Player)
	{
		return false;
	}
	ComputerOperator = nullptr;
	ComputerOperatorController.Reset();
	UpdateComputerVisuals();
	ForceNetUpdate();
	return true;
}

bool AMallPushCart::TryToggleUseComputer(APlayerController* Player)
{
	if (!bHasComputerTerminal || !IsValid(Player))
	{
		return false;
	}
	if (IsComputerInUseBy(Player->GetPawn()))
	{
		return StopUseComputer(Player);
	}
	return TryBeginUseComputer(Player);
}

bool AMallPushCart::TryCycleComputerTab(APlayerController* Player)
{
	if (!bHasComputerTerminal || !CanPlayerReach(Player))
	{
		return false;
	}
	if (IsValid(ComputerOperator) && ComputerOperator != Player->GetPawn())
	{
		return false;
	}
	ActiveComputerTab = (ActiveComputerTab + 1) % 3;
	SyncComputerFromWorldEquipment();
	UpdateComputerVisuals();
	ForceNetUpdate();
	return true;
}

bool AMallPushCart::TryToggleEvidenceBit(APlayerController* Player, uint8 EvidenceBit)
{
	if (!bHasComputerTerminal || !CanPlayerReach(Player) || (EvidenceBit & 0x3F) == 0)
	{
		return false;
	}
	if (IsValid(ComputerOperator) && ComputerOperator != Player->GetPawn())
	{
		return false;
	}
	ManualEvidenceMask ^= (EvidenceBit & 0x3F);
	SyncComputerFromWorldEquipment();
	UpdateComputerVisuals();
	ForceNetUpdate();
	return true;
}

bool AMallPushCart::TryCycleRemoteFeed(APlayerController* Player, bool bTriggerRemoteShutter)
{
	if (!bHasComputerTerminal || !CanPlayerReach(Player))
	{
		return false;
	}
	if (IsValid(ComputerOperator) && ComputerOperator != Player->GetPawn())
	{
		return false;
	}
	if (bTriggerRemoteShutter && GetWorld())
	{
		for (TActorIterator<AMallPhotoCamera> It(GetWorld()); It; ++It)
		{
			AMallPhotoCamera* Cam = *It;
			if (IsValid(Cam) && !Cam->GetCarrier() && !Cam->StoredOnCart)
			{
				Cam->TryCapturePhoto(Player);
				break;
			}
		}
	}
	else
	{
		++SelectedRemoteFeedIndex;
	}
	SyncComputerFromWorldEquipment();
	UpdateComputerVisuals();
	ForceNetUpdate();
	return true;
}

FString AMallPushCart::GetComputerSummaryText() const
{
	if (!bHasComputerTerminal)
	{
		return TEXT("ROLE: Light & Battery Safe-Zone Cart (Info Screen & Gear Rack are on the Equipment Cart)");
	}
	const TCHAR* TabName = (ActiveComputerTab == 0) ? TEXT("JOURNAL")
		: (ActiveComputerTab == 1) ? TEXT("SAVED PHOTOS")
		: TEXT("REMOTE CAMS");
	const FString LockTag = IsValid(ComputerOperator)
		? TEXT(" [IN USE - 1 PLAYER MAX]")
		: TEXT(" [PRESS C FOR FULL-SCREEN]");
	if (ActiveComputerTab == 1)
	{
		return FString::Printf(TEXT("CART PC [%s]%s: %d Photos ($%d) | Best: %d* %s | %s"),
			TabName, *LockTag, SyncedPhotoCount, SyncedTotalPhotoReward, SyncedBestPhotoStars, *SyncedLastPhotoSubject, *DeducedGhostArchetype);
	}
	if (ActiveComputerTab == 2)
	{
		return FString::Printf(TEXT("CART PC [%s]%s: %s | %s"),
			TabName, *LockTag, *SyncedRemoteFeedStatus, *DeducedGhostArchetype);
	}
	return FString::Printf(TEXT("CART PC [%s]%s: %s | Photos: %d ($%d)"),
		TabName, *LockTag, *DeducedGhostArchetype, SyncedPhotoCount, SyncedTotalPhotoReward);
}

void AMallPushCart::UpdateComputerVisuals()
{
	const bool bShowComputer = bHasComputerTerminal && bShowProceduralCart;
	if (ComputerBaseMesh)
	{
		ComputerBaseMesh->SetVisibility(bShowComputer);
		ComputerBaseMesh->SetHiddenInGame(!bShowComputer);
	}
	if (ComputerScreenMesh)
	{
		ComputerScreenMesh->SetVisibility(bShowComputer);
		ComputerScreenMesh->SetHiddenInGame(!bShowComputer);
		if (bShowComputer && !ComputerScreenMID)
		{
			if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Blockouts/Materials/LDA/M_LDA_Blockout.M_LDA_Blockout")))
			{
				ComputerScreenMID = UMaterialInstanceDynamic::Create(BaseMat, this);
				ComputerScreenMesh->SetMaterial(0, ComputerScreenMID);
			}
		}
		if (ComputerScreenMID && bShowComputer)
		{
			const FLinearColor ScreenColor = (ActiveComputerTab == 0) ? FLinearColor(0.04f, 0.25f, 0.20f, 1.0f)
				: (ActiveComputerTab == 1) ? FLinearColor(0.22f, 0.16f, 0.04f, 1.0f)
				: FLinearColor(0.05f, 0.18f, 0.28f, 1.0f);
			ComputerScreenMID->SetVectorParameterValue(TEXT("Color"), ScreenColor);
			ComputerScreenMID->SetScalarParameterValue(TEXT("Roughness"), 0.10f);
		}
	}
	if (ComputerHeaderText)
	{
		ComputerHeaderText->SetVisibility(bShowComputer);
		ComputerHeaderText->SetHiddenInGame(!bShowComputer);
	}
	if (ComputerBodyText)
	{
		ComputerBodyText->SetVisibility(bShowComputer);
		ComputerBodyText->SetHiddenInGame(!bShowComputer);
	}
	if (ComputerFooterText)
	{
		ComputerFooterText->SetVisibility(bShowComputer);
		ComputerFooterText->SetHiddenInGame(!bShowComputer);
	}
	if (ComputerScreenLight)
	{
		ComputerScreenLight->SetVisibility(bShowComputer);
		ComputerScreenLight->SetHiddenInGame(!bShowComputer);
		ComputerScreenLight->SetIntensity(bShowComputer ? 115.0f : 0.0f);
	}
	if (!bShowComputer)
	{
		return;
	}

	using namespace EMallGhostEvidenceBit;
	if (ComputerHeaderText)
	{
		const TCHAR* HeaderStr = (ActiveComputerTab == 0) ? TEXT("=== SHARED TEAM JOURNAL [1/3] ===")
			: (ActiveComputerTab == 1) ? TEXT("=== SAVED PHOTOS & PAYOUT [2/3] ===")
			: TEXT("=== REMOTE CAMS & SENSORS [3/3] ===");
		ComputerHeaderText->SetText(FText::FromString(HeaderStr));
	}

	if (ComputerBodyText)
	{
		if (ActiveComputerTab == 0)
		{
			auto Check = [&](uint8 Bit) -> const TCHAR* { return (SharedEvidenceMask & Bit) ? TEXT("[X]") : TEXT("[ ]"); };
			const FString BodyStr = FString::Printf(
				TEXT("%s EMF-5   %s UV PRINTS   %s FREEZING\n%s SPIRIT  %s WRITING     %s MOTION"),
				Check(EMFLevel5), Check(UVFingerprints), Check(FreezingTemps),
				Check(SpiritBox), Check(GhostWriting), Check(MotionDetected));
			ComputerBodyText->SetText(FText::FromString(BodyStr));
		}
		else if (ActiveComputerTab == 1)
		{
			const FString BodyStr = FString::Printf(
				TEXT("SAVED PHOTOS: %d  |  TEAM PAYOUT: $%d\nBEST: %d* %s"),
				SyncedPhotoCount, SyncedTotalPhotoReward, SyncedBestPhotoStars, *SyncedLastPhotoSubject);
			ComputerBodyText->SetText(FText::FromString(BodyStr));
		}
		else
		{
			ComputerBodyText->SetText(FText::FromString(SyncedRemoteFeedStatus));
		}
	}

	if (ComputerFooterText)
	{
		const TCHAR* OpStatus = IsValid(ComputerOperator) ? TEXT("[OPERATOR ACTIVE]") : TEXT("[PRESS C: FULLSCREEN]");
		ComputerFooterText->SetText(FText::FromString(FString::Printf(TEXT("%s  |  %s"), *DeducedGhostArchetype, OpStatus)));
	}
}
