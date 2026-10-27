#include "Environment/MallSpaAndFoodSystem.h"

#include "Equipment/MallLightCart.h"
#include "Equipment/MallPushCart.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
	UMaterialInterface* LoadBaseBlockoutMaterial()
	{
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Blockouts/Materials/LDA/M_LDA_Blockout.M_LDA_Blockout")))
		{
			return Mat;
		}
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
}

// ============================================================================
// UMallPlayerVitalityComponent
// ============================================================================

UMallPlayerVitalityComponent::UMallPlayerVitalityComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UMallPlayerVitalityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMallPlayerVitalityComponent, Health);
	DOREPLIFETIME(UMallPlayerVitalityComponent, MaxHealth);
	DOREPLIFETIME(UMallPlayerVitalityComponent, Stamina);
	DOREPLIFETIME(UMallPlayerVitalityComponent, MaxStamina);
	DOREPLIFETIME(UMallPlayerVitalityComponent, BioEnergy);
	DOREPLIFETIME(UMallPlayerVitalityComponent, HimalayanSaltWardSeconds);
	DOREPLIFETIME(UMallPlayerVitalityComponent, ActiveFoodBuff);
	DOREPLIFETIME(UMallPlayerVitalityComponent, FoodBuffRemainingSeconds);
	DOREPLIFETIME(UMallPlayerVitalityComponent, bIsSeatedResting);
}

UMallPlayerVitalityComponent* UMallPlayerVitalityComponent::FindOrCreateForPawn(APawn* Pawn)
{
	if (!IsValid(Pawn))
	{
		return nullptr;
	}
	if (UMallPlayerVitalityComponent* Existing = Pawn->FindComponentByClass<UMallPlayerVitalityComponent>())
	{
		return Existing;
	}
	if (Pawn->HasAuthority())
	{
		UMallPlayerVitalityComponent* Created = NewObject<UMallPlayerVitalityComponent>(Pawn, TEXT("MallPlayerVitality"));
		Pawn->AddInstanceComponent(Created);
		Created->RegisterComponent();
		return Created;
	}
	return nullptr;
}

void UMallPlayerVitalityComponent::ApplySpaHealing(float HealAmount, float StaminaAmount, float SaltWardDurationSeconds)
{
	Health = FMath::Clamp(Health + HealAmount, 0.0f, MaxHealth);
	Stamina = FMath::Clamp(Stamina + StaminaAmount, 0.0f, MaxStamina);
	HimalayanSaltWardSeconds = FMath::Max(HimalayanSaltWardSeconds, SaltWardDurationSeconds);
	if (ActiveFoodBuff == EMallFoodBuffType::None && SaltWardDurationSeconds > 0.0f)
	{
		ActiveFoodBuff = EMallFoodBuffType::HimalayanSalt_ThermalWard;
		FoodBuffRemainingSeconds = FMath::Max(FoodBuffRemainingSeconds, SaltWardDurationSeconds);
	}
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->Tags.AddUnique(TEXT("SaltWardProtected"));
	}
	SyncPawnBlueprintHealth();
	UpdateMovementBuffs();
}

void UMallPlayerVitalityComponent::ApplyFoodConsumption(
	EMallFoodBuffType BuffType,
	float HealAmount,
	float StaminaAmount,
	float EnergyAmount,
	float BuffDurationSeconds,
	float CartBatteryBoostFraction)
{
	Health = FMath::Clamp(Health + HealAmount, 0.0f, MaxHealth);
	Stamina = FMath::Clamp(Stamina + StaminaAmount, 0.0f, MaxStamina);
	BioEnergy = FMath::Clamp(BioEnergy + EnergyAmount, 0.0f, 100.0f);
	ActiveFoodBuff = BuffType;
	FoodBuffRemainingSeconds = FMath::Max(FoodBuffRemainingSeconds, BuffDurationSeconds);

	if (AActor* OwnerActor = GetOwner())
	{
		if (BuffType == EMallFoodBuffType::MatchaBoba_SpectralStealth)
		{
			OwnerActor->Tags.AddUnique(TEXT("SpectralStealthBuff"));
		}
		// Discharge Bio-Thermal Energy into any nearby Light Cart within 850 cm!
		if (CartBatteryBoostFraction > 0.0f && OwnerActor->GetWorld())
		{
			for (TActorIterator<AMallLightCart> It(OwnerActor->GetWorld()); It; ++It)
			{
				AMallLightCart* LightCart = *It;
				if (IsValid(LightCart) && FVector::DistSquared(OwnerActor->GetActorLocation(), LightCart->GetActorLocation()) <= FMath::Square(850.0f))
				{
					LightCart->BatteryFraction = FMath::Clamp(LightCart->BatteryFraction + CartBatteryBoostFraction, 0.0f, 1.0f);
					if (BuffType == EMallFoodBuffType::BentoRation_CartOvercharge)
					{
						LightCart->ConeLengthCm = FMath::Max(LightCart->ConeLengthCm, 820.0f);
					}
					LightCart->ForceNetUpdate();
				}
			}
		}
	}

	SyncPawnBlueprintHealth();
	UpdateMovementBuffs();
}

float UMallPlayerVitalityComponent::ApplyGhostChillDamage(float RawDamage)
{
	if (RawDamage <= 0.0f)
	{
		return 0.0f;
	}
	// Himalayan Pink Salt Ward absorbs 75% of ghost chill damage!
	const float EffectiveDamage = (HimalayanSaltWardSeconds > 0.0f) ? (RawDamage * 0.25f) : RawDamage;
	Health = FMath::Clamp(Health - EffectiveDamage, 5.0f, MaxHealth);
	SyncPawnBlueprintHealth();
	return EffectiveDamage;
}

float UMallPlayerVitalityComponent::GetGhostDetectionDistanceMultiplier() const
{
	if (ActiveFoodBuff == EMallFoodBuffType::MatchaBoba_SpectralStealth && FoodBuffRemainingSeconds > 0.0f)
	{
		return 0.70f;
	}
	if (HimalayanSaltWardSeconds > 0.0f)
	{
		return 0.85f;
	}
	return 1.0f;
}

FString UMallPlayerVitalityComponent::GetVitalityStatusSummary() const
{
	FString BuffStr = TEXT("No Active Buff");
	if (HimalayanSaltWardSeconds > 0.0f)
	{
		BuffStr = FString::Printf(TEXT("Himalayan Salt Ward (%.0fs)"), HimalayanSaltWardSeconds);
	}
	else if (FoodBuffRemainingSeconds > 0.0f)
	{
		switch (ActiveFoodBuff)
		{
		case EMallFoodBuffType::SpicyHotPot_SprintSurge:
			BuffStr = FString::Printf(TEXT("Hot Pot Sprint Surge +25%% (%.0fs)"), FoodBuffRemainingSeconds);
			break;
		case EMallFoodBuffType::MatchaBoba_SpectralStealth:
			BuffStr = FString::Printf(TEXT("Matcha Spectral Stealth -30%% (%.0fs)"), FoodBuffRemainingSeconds);
			break;
		case EMallFoodBuffType::BentoRation_CartOvercharge:
			BuffStr = FString::Printf(TEXT("Bento Cart Overcharge (%.0fs)"), FoodBuffRemainingSeconds);
			break;
		default:
			break;
		}
	}
	if (bIsSeatedResting)
	{
		BuffStr = TEXT("SEATED ON BENCH [3RD-PERSON REST: +24% Stamina/s]");
	}
	return FString::Printf(TEXT("HP: %.0f/%.0f | Stamina: %.0f%% | Bio-Energy: %.0f%% | %s"),
		Health, MaxHealth, Stamina, BioEnergy, *BuffStr);
}

void UMallPlayerVitalityComponent::SyncPawnBlueprintHealth()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}
	if (FNumericProperty* HealthProp = FindFProperty<FNumericProperty>(OwnerActor->GetClass(), TEXT("Health")))
	{
		void* ValuePtr = HealthProp->ContainerPtrToValuePtr<void>(OwnerActor);
		if (HealthProp->IsFloatingPoint())
		{
			HealthProp->SetFloatingPointPropertyValue(ValuePtr, Health);
		}
	}
}

void UMallPlayerVitalityComponent::UpdateMovementBuffs()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->GetCharacterMovement())
	{
		return;
	}
	if (AMallPushCart::IsPawnPushingCart(Character))
	{
		return;
	}
	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!bCapturedBaseSpeed && MoveComp->MaxWalkSpeed > 250.0f)
	{
		BaseWalkSpeed = MoveComp->MaxWalkSpeed;
		bCapturedBaseSpeed = true;
	}
	float SpeedMultiplier = 1.0f;
	if (ActiveFoodBuff == EMallFoodBuffType::SpicyHotPot_SprintSurge && FoodBuffRemainingSeconds > 0.0f)
	{
		SpeedMultiplier = 1.25f;
	}
	else if (HimalayanSaltWardSeconds > 0.0f)
	{
		SpeedMultiplier = 1.15f;
	}
	// Exhaustion penalty when Stamina drops below 12%
	if (Stamina <= 12.0f)
	{
		SpeedMultiplier *= 0.68f;
	}
	MoveComp->MaxWalkSpeed = BaseWalkSpeed * SpeedMultiplier;
}

void UMallPlayerVitalityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (HimalayanSaltWardSeconds > 0.0f)
	{
		HimalayanSaltWardSeconds = FMath::Max(0.0f, HimalayanSaltWardSeconds - DeltaTime);
		if (HimalayanSaltWardSeconds <= 0.0f)
		{
			OwnerActor->Tags.Remove(TEXT("SaltWardProtected"));
		}
	}
	if (FoodBuffRemainingSeconds > 0.0f)
	{
		FoodBuffRemainingSeconds = FMath::Max(0.0f, FoodBuffRemainingSeconds - DeltaTime);
		if (FoodBuffRemainingSeconds <= 0.0f)
		{
			ActiveFoodBuff = EMallFoodBuffType::None;
			OwnerActor->Tags.Remove(TEXT("SpectralStealthBuff"));
		}
	}

	APawn* Pawn = Cast<APawn>(OwnerActor);
	AMallRestBench* SeatedBench = AMallRestBench::FindBenchOccupiedBy(Pawn);
	if (IsValid(SeatedBench))
	{
		bIsSeatedResting = true;
		Stamina = FMath::Min(MaxStamina, Stamina + DeltaTime * SeatedBench->SeatedStaminaRegenPerSecond);
		if (Health < MaxHealth)
		{
			Health = FMath::Min(MaxHealth, Health + DeltaTime * SeatedBench->SeatedHealthRegenPerSecond);
			SyncPawnBlueprintHealth();
		}
	}
	else
	{
		bIsSeatedResting = false;
		const float Speed2D = OwnerActor->GetVelocity().Size2D();
		if (Speed2D > 125.0f && !AMallSpaHealingZone::IsPawnInsideAnySanctuary(Pawn))
		{
			float DrainPerSec = (Speed2D > 420.0f) ? 8.2f : 5.0f;
			if (AMallPushCart::IsPawnPushingCart(Pawn))
			{
				DrainPerSec += 2.6f;
			}
			if (ActiveFoodBuff == EMallFoodBuffType::SpicyHotPot_SprintSurge && FoodBuffRemainingSeconds > 0.0f)
			{
				DrainPerSec *= 0.55f;
			}
			Stamina = FMath::Max(0.0f, Stamina - DrainPerSec * DeltaTime);
		}
		else
		{
			// Slow passive recovery when standing still; sitting on a bench is 10x faster!
			Stamina = FMath::Min(MaxStamina, Stamina + DeltaTime * 2.4f);
		}
	}

	UpdateMovementBuffs();
}

// ============================================================================
// AMallSpaHealingZone
// ============================================================================

AMallSpaHealingZone::AMallSpaHealingZone()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	ZoneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ZoneRoot"));
	SetRootComponent(ZoneRoot);

	HealingVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("HealingVolume"));
	HealingVolume->SetupAttachment(ZoneRoot);
	HealingVolume->SetBoxExtent(FVector(240.0f, 240.0f, 130.0f));
	HealingVolume->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	HealingVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HealingVolume->SetCollisionObjectType(ECC_WorldDynamic);
	HealingVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	HealingVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HealingVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	SaltCrystalClusterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SaltCrystalClusterMesh"));
	SaltCrystalClusterMesh->SetupAttachment(ZoneRoot);
	SaltCrystalClusterMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 32.0f));
	SaltCrystalClusterMesh->SetRelativeScale3D(FVector(1.15f, 1.15f, 0.55f));
	SaltCrystalClusterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SaltCrystalClusterMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		SaltCrystalClusterMesh->SetStaticMesh(SphereFinder.Object);
	}

	SanctuaryTitleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SanctuaryTitleText"));
	SanctuaryTitleText->SetupAttachment(ZoneRoot);
	SanctuaryTitleText->SetRelativeLocation(FVector(0.0f, 0.0f, 210.0f));
	SanctuaryTitleText->SetHorizontalAlignment(EHTA_Center);
	SanctuaryTitleText->SetVerticalAlignment(EVRTA_TextCenter);
	SanctuaryTitleText->SetWorldSize(20.0f);
	SanctuaryTitleText->SetTextRenderColor(FColor(255, 175, 195));

	MineralGlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MineralGlowLight"));
	MineralGlowLight->SetupAttachment(ZoneRoot);
	MineralGlowLight->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	MineralGlowLight->SetIntensityUnits(ELightUnits::Lumens);
	MineralGlowLight->SetIntensity(650.0f);
	MineralGlowLight->SetAttenuationRadius(750.0f);
	MineralGlowLight->SetSourceRadius(45.0f);
	MineralGlowLight->SetCastShadows(false);
	MineralGlowLight->SetLightColor(FLinearColor(1.0f, 0.48f, 0.58f));
}

void AMallSpaHealingZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildSanctuaryVisuals();
}

void AMallSpaHealingZone::BeginPlay()
{
	Super::BeginPlay();
	RebuildSanctuaryVisuals();
}

void AMallSpaHealingZone::RebuildSanctuaryVisuals()
{
	FLinearColor CrystalColor(0.95f, 0.42f, 0.55f, 1.0f);
	FColor TextCol(255, 175, 195);
	FString TypeSubtitle = TEXT("HIMALAYAN PINK SALT HEALING BATH (+HP/s & GHOST WARD)");

	if (SanctuaryType == EMallSpaSanctuaryType::JadeGeothermalOnsen)
	{
		CrystalColor = FLinearColor(0.08f, 0.85f, 0.72f, 1.0f);
		TextCol = FColor(110, 250, 225);
		TypeSubtitle = TEXT("JADE GEOTHERMAL ONSEN SANCTUARY (+HP/s & STAMINA)");
	}
	else if (SanctuaryType == EMallSpaSanctuaryType::HinokiCedarSauna)
	{
		CrystalColor = FLinearColor(0.98f, 0.48f, 0.14f, 1.0f);
		TextCol = FColor(255, 195, 115);
		TypeSubtitle = TEXT("HINOKI CEDAR & SALT SAUNA (+HP/s & THERMAL WARD)");
	}

	if (SaltCrystalClusterMesh)
	{
		if (UMaterialInterface* BaseMat = LoadBaseBlockoutMaterial())
		{
			SaltCrystalMID = UMaterialInstanceDynamic::Create(BaseMat, this);
			if (SaltCrystalMID)
			{
				SaltCrystalMID->SetVectorParameterValue(TEXT("Color"), CrystalColor);
				SaltCrystalMID->SetScalarParameterValue(TEXT("Roughness"), 0.15f);
				SaltCrystalClusterMesh->SetMaterial(0, SaltCrystalMID);
			}
		}
	}

	if (MineralGlowLight)
	{
		MineralGlowLight->SetLightColor(CrystalColor);
	}

	if (SanctuaryTitleText)
	{
		SanctuaryTitleText->SetTextRenderColor(TextCol);
		SanctuaryTitleText->SetText(FText::FromString(FString::Printf(TEXT("%s\n[%s]"), *SanctuaryDisplayName, *TypeSubtitle)));
	}
}

bool AMallSpaHealingZone::IsPawnInsideSanctuary(const APawn* Pawn) const
{
	if (!IsValid(Pawn) || !HealingVolume)
	{
		return false;
	}
	const FVector LocalPos = GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
	const FVector Ext = HealingVolume->GetUnscaledBoxExtent();
	return FMath::Abs(LocalPos.X) <= Ext.X + 60.0f
		&& FMath::Abs(LocalPos.Y) <= Ext.Y + 60.0f
		&& FMath::Abs(LocalPos.Z - HealingVolume->GetRelativeLocation().Z) <= Ext.Z + 80.0f;
}

bool AMallSpaHealingZone::TryInteractiveSoak(APlayerController* Player, FString& OutFeedback)
{
	if (!Player || !Player->HasAuthority())
	{
		return false;
	}
	APawn* Pawn = Player->GetPawn();
	if (!IsValid(Pawn))
	{
		return false;
	}
	if (FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(480.0f))
	{
		OutFeedback = TEXT("Step closer to the mineral bath to soak.");
		return false;
	}

	if (UMallPlayerVitalityComponent* Vitality = UMallPlayerVitalityComponent::FindOrCreateForPawn(Pawn))
	{
		Vitality->ApplySpaHealing(InteractiveBurstHeal, 100.0f, SaltWardDurationSeconds);
		OutFeedback = FString::Printf(TEXT("Soaked in %s! Restored +%.0f HP, 100%% Stamina & %.0fs Himalayan Pink Salt Ward! (%s)"),
			*SanctuaryDisplayName,
			InteractiveBurstHeal,
			SaltWardDurationSeconds,
			*Vitality->GetVitalityStatusSummary());
		return true;
	}
	return false;
}

FString AMallSpaHealingZone::GetHUDPrompt(const APawn* ViewerPawn) const
{
	if (ViewerPawn)
	{
		if (const UMallPlayerVitalityComponent* Vitality = ViewerPawn->FindComponentByClass<UMallPlayerVitalityComponent>())
		{
			return FString::Printf(TEXT("%s\nF: Deep Mineral Soak (+%.0f HP & %.0fs Salt Ward) | %s"),
				*SanctuaryDisplayName,
				InteractiveBurstHeal,
				SaltWardDurationSeconds,
				*Vitality->GetVitalityStatusSummary());
		}
	}
	return FString::Printf(TEXT("%s\nStep into the bath or press F to heal (+%.0f HP) & gain Himalayan Pink Salt Ward!"),
		*SanctuaryDisplayName,
		InteractiveBurstHeal);
}

void AMallSpaHealingZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (IsValid(Pawn) && IsPawnInsideSanctuary(Pawn))
		{
			if (UMallPlayerVitalityComponent* Vitality = UMallPlayerVitalityComponent::FindOrCreateForPawn(Pawn))
			{
				Vitality->ApplySpaHealing(PassiveHealPerSecond * DeltaSeconds, 25.0f * DeltaSeconds, SaltWardDurationSeconds);
			}
		}
	}
}

AMallSpaHealingZone* AMallSpaHealingZone::FindSpaZoneInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction)
{
	if (!IsValid(Pawn) || !Pawn->GetWorld())
	{
		return nullptr;
	}
	if (AMallSpaHealingZone* Inside = FindSpaZoneContainingPawn(Pawn))
	{
		return Inside;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallSpaZoneFocus), true, Pawn);
	FHitResult Hit;
	if (Pawn->GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 450.0f, ECC_Visibility, Params))
	{
		if (AMallSpaHealingZone* Zone = Cast<AMallSpaHealingZone>(Hit.GetActor()))
		{
			if (FVector::DistSquared(Pawn->GetActorLocation(), Zone->GetActorLocation()) <= FMath::Square(480.0f))
			{
				return Zone;
			}
		}
	}
	return nullptr;
}

AMallSpaHealingZone* AMallSpaHealingZone::FindSpaZoneContainingPawn(const APawn* Pawn)
{
	if (!IsValid(Pawn) || !Pawn->GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<AMallSpaHealingZone> It(Pawn->GetWorld()); It; ++It)
	{
		if (It->IsPawnInsideSanctuary(Pawn))
		{
			return *It;
		}
	}
	return nullptr;
}

// ============================================================================
// AMallFoodRationItem
// ============================================================================

AMallFoodRationItem::AMallFoodRationItem()
{
	DisplayName = FText::FromString(TEXT("Chongqing Hot Pot Ration"));
	ItemValue = 20.0f;
	bPowered = true;
	PlacementHalfExtent = FVector(10.0f, 10.0f, 8.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded() && DeviceMesh)
	{
		DeviceMesh->SetStaticMesh(CylinderMesh.Object);
		DeviceMesh->SetRelativeScale3D(FVector(0.24f, 0.24f, 0.12f));
	}

	BowlContentsMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BowlContentsMesh"));
	BowlContentsMesh->SetupAttachment(ItemRoot);
	BowlContentsMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 11.5f));
	BowlContentsMesh->SetRelativeScale3D(FVector(0.21f, 0.21f, 0.03f));
	BowlContentsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BowlContentsMesh->SetCanEverAffectNavigation(false);
	if (CylinderMesh.Succeeded())
	{
		BowlContentsMesh->SetStaticMesh(CylinderMesh.Object);
	}
}

void AMallFoodRationItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallFoodRationItem, MealBuffType);
	DOREPLIFETIME(AMallFoodRationItem, ServingsRemaining);
}

void AMallFoodRationItem::ConfigureMealType(EMallFoodBuffType NewType)
{
	MealBuffType = NewType;
	switch (MealBuffType)
	{
	case EMallFoodBuffType::SpicyHotPot_SprintSurge:
		DisplayName = FText::FromString(TEXT("Chongqing Hot Pot Ration"));
		break;
	case EMallFoodBuffType::MatchaBoba_SpectralStealth:
		DisplayName = FText::FromString(TEXT("Geothermal Matcha Boba"));
		break;
	case EMallFoodBuffType::BentoRation_CartOvercharge:
	default:
		DisplayName = FText::FromString(TEXT("Bio-Battery Bento Ration"));
		break;
	}
	UpdateDeviceVisuals();
}

void AMallFoodRationItem::OnRep_FoodState()
{
	UpdateDeviceVisuals();
}

bool AMallFoodRationItem::TryPrimaryUse(APlayerController* Player)
{
	if (!HasAuthority() || !IsValid(Player) || !Player->HasAuthority() || ServingsRemaining <= 0 || StoredOnCart)
	{
		return false;
	}
	APawn* Pawn = Player->GetPawn();
	if (!IsValid(Pawn) || Pawn->GetWorld() != GetWorld())
	{
		return false;
	}
	if (Carrier && Carrier != Pawn)
	{
		return false;
	}
	if (!Carrier && FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(280.0))
	{
		return false;
	}

	UMallPlayerVitalityComponent* Vitality = UMallPlayerVitalityComponent::FindOrCreateForPawn(Pawn);
	if (!Vitality)
	{
		return false;
	}

	Vitality->ApplyFoodConsumption(
		MealBuffType,
		HealPerServing,
		StaminaPerServing,
		40.0f,
		BuffDurationSeconds,
		CartBatteryBoostPerServing);

	--ServingsRemaining;
	bPowered = (ServingsRemaining > 0);
	LastConsumeFeedback = FString::Printf(
		TEXT("Consumed %s! +%.0f HP, +%.0f%% Stamina, +%.0f%% Cart Battery Charge! [%s] (%d serving%s left)"),
		*DisplayName.ToString(),
		HealPerServing,
		StaminaPerServing,
		CartBatteryBoostPerServing * 100.0f,
		*Vitality->GetVitalityStatusSummary(),
		ServingsRemaining,
		ServingsRemaining == 1 ? TEXT("") : TEXT("s"));

	UpdateDeviceVisuals();
	ForceNetUpdate();
	return true;
}

bool AMallFoodRationItem::TrySecondaryUse(APlayerController* Player)
{
	if (!CanPlayerControlDevice(Player))
	{
		return false;
	}
	if (MealBuffType == EMallFoodBuffType::SpicyHotPot_SprintSurge)
	{
		ConfigureMealType(EMallFoodBuffType::MatchaBoba_SpectralStealth);
	}
	else if (MealBuffType == EMallFoodBuffType::MatchaBoba_SpectralStealth)
	{
		ConfigureMealType(EMallFoodBuffType::BentoRation_CartOvercharge);
	}
	else
	{
		ConfigureMealType(EMallFoodBuffType::SpicyHotPot_SprintSurge);
	}
	ForceNetUpdate();
	return true;
}

void AMallFoodRationItem::UpdateDeviceVisuals()
{
	FLinearColor BrothColor(0.95f, 0.22f, 0.08f, 1.0f);
	FColor TextCol(255, 185, 95);
	FString BuffTag = TEXT("HOT POT: +25% SPEED & +45% CART BATTERY");

	if (MealBuffType == EMallFoodBuffType::MatchaBoba_SpectralStealth)
	{
		BrothColor = FLinearColor(0.18f, 0.88f, 0.42f, 1.0f);
		TextCol = FColor(120, 255, 165);
		BuffTag = TEXT("MATCHA BOBA: -30% GHOST SIGHT & +45% BATTERY");
	}
	else if (MealBuffType == EMallFoodBuffType::BentoRation_CartOvercharge)
	{
		BrothColor = FLinearColor(0.95f, 0.68f, 0.12f, 1.0f);
		TextCol = FColor(255, 225, 110);
		BuffTag = TEXT("BIO-BENTO: +40 HP & CART OVERCHARGE");
	}

	if (BowlContentsMesh)
	{
		BowlContentsMesh->SetVisibility(ServingsRemaining > 0);
		if (!BrothMID)
		{
			if (UMaterialInterface* BaseMat = LoadBaseBlockoutMaterial())
			{
				BrothMID = UMaterialInstanceDynamic::Create(BaseMat, this);
				BowlContentsMesh->SetMaterial(0, BrothMID);
			}
		}
		if (BrothMID)
		{
			BrothMID->SetVectorParameterValue(TEXT("Color"), BrothColor);
		}
	}

	if (ReadoutText)
	{
		if (ServingsRemaining <= 0)
		{
			ReadoutText->SetText(FText::FromString(TEXT("RATION: EMPTY")));
			ReadoutText->SetTextRenderColor(FColor(130, 130, 130));
		}
		else
		{
			ReadoutText->SetText(FText::FromString(FString::Printf(TEXT("%s [%d LEFT]"), *BuffTag, ServingsRemaining)));
			ReadoutText->SetTextRenderColor(TextCol);
		}
	}

	if (StatusLight)
	{
		StatusLight->SetLightColor(BrothColor);
		StatusLight->SetIntensity(ServingsRemaining > 0 ? 180.0f : 0.0f);
	}
}

FString AMallFoodRationItem::GetHUDPrompt(bool bIsHeld) const
{
	return FString::Printf(TEXT("%s [%d servings]  |  LMB: eat & charge cart  |  E: switch blend  |  F: %s%s"),
		*DisplayName.ToString(),
		ServingsRemaining,
		bIsHeld ? TEXT("place / store on cart") : TEXT("pick up"),
		bIsHeld ? TEXT("  |  G: drop") : TEXT(""));
}

FString AMallFoodRationItem::GetHUDDetail() const
{
	if (ServingsRemaining <= 0)
	{
		return TEXT("Empty ration bowl. Pick up a fresh meal in the North Night-Market Food Court.");
	}
	return TEXT("Click LMB to eat: restores +40 HP, +65% Stamina, grants a 90s combat buff, and recharges nearby Light Cart batteries by +45%!");
}

FString AMallFoodRationItem::GetPickupFeedback() const
{
	return FString::Printf(TEXT("%s held in hand! Click LMB to eat (+Stamina/HP + Cart Battery Charge + Combat Buff)."), *DisplayName.ToString());
}

FString AMallFoodRationItem::GetPrimaryUseFeedback() const
{
	return LastConsumeFeedback.IsEmpty()
		? FString::Printf(TEXT("Consumed %s! Stamina, Health & nearby Cart Battery boosted!"), *DisplayName.ToString())
		: LastConsumeFeedback;
}

FString AMallFoodRationItem::GetSecondaryUseFeedback() const
{
	return FString::Printf(TEXT("Ration switched to %s."), *DisplayName.ToString());
}

// ============================================================================
// AMallRestBench
// ============================================================================

AMallRestBench::AMallRestBench()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	BenchRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BenchRoot"));
	SetRootComponent(BenchRoot);

	BenchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BenchMesh"));
	BenchMesh->SetupAttachment(BenchRoot);
	BenchMesh->SetCollisionProfileName(TEXT("BlockAll"));
	BenchMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultBenchFinder(
		TEXT("/Game/Environment/Mall/Models/PolyHaven/SM_MallBench01.SM_MallBench01"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackBenchFinder(
		TEXT("/Game/Environment/Mall/Models/Retail/SM_Retail_25366_ConcourseBench.SM_Retail_25366_ConcourseBench"));
	if (DefaultBenchFinder.Succeeded())
	{
		BenchMesh->SetStaticMesh(DefaultBenchFinder.Object);
	}
	else if (FallbackBenchFinder.Succeeded())
	{
		BenchMesh->SetStaticMesh(FallbackBenchFinder.Object);
	}

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(BenchRoot);
	InteractionBounds->InitBoxExtent(FVector(115.0f, 52.0f, 50.0f));
	InteractionBounds->SetRelativeLocation(FVector(0.0f, 0.0f, 46.0f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionBounds->SetGenerateOverlapEvents(false);
	InteractionBounds->SetCanEverAffectNavigation(false);
}

void AMallRestBench::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMallRestBench, OccupantPawn);
}

void AMallRestBench::ApplySeatedCharacterPosture(APawn* Pawn, bool bSeated)
{
	ACharacter* Character = Cast<ACharacter>(Pawn);
	if (!IsValid(Character))
	{
		return;
	}
	if (USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		// Lower the character skeletal mesh by 28 cm so the hips rest naturally on the bench seat surface in 3rd-person view
		Mesh->SetRelativeLocation(bSeated ? FVector(0.0f, 0.0f, -118.0f) : FVector(0.0f, 0.0f, -90.0f));
		if (bSeated)
		{
			Mesh->UnHideBoneByName(TEXT("neck_01"));
			Mesh->UnHideBoneByName(TEXT("head"));
		}
	}
	if (bSeated)
	{
		Character->Tags.AddUnique(TEXT("SeatedOnBench"));
	}
	else
	{
		Character->Tags.Remove(TEXT("SeatedOnBench"));
	}
}

void AMallRestBench::OnRep_OccupantPawn(APawn* PreviousOccupant)
{
	if (IsValid(PreviousOccupant) && PreviousOccupant != OccupantPawn)
	{
		ApplySeatedCharacterPosture(PreviousOccupant, false);
		AMallPushCart::ConfigurePawnFirstPersonView(PreviousOccupant, true);
	}
	if (IsValid(OccupantPawn))
	{
		ApplySeatedCharacterPosture(OccupantPawn, true);
		AMallPushCart::ConfigurePawnFirstPersonView(OccupantPawn, false);
	}
}

bool AMallRestBench::TrySitDown(APlayerController* Player, FString& OutFeedback)
{
	if (!HasAuthority() || !IsValid(Player) || !Player->HasAuthority())
	{
		return false;
	}
	ACharacter* Character = Cast<ACharacter>(Player->GetPawn());
	if (!IsValid(Character))
	{
		return false;
	}
	if (IsOccupied() && OccupantPawn != Character)
	{
		OutFeedback = TEXT("This bench is currently occupied by another survivor.");
		return false;
	}
	if (FVector::DistSquared(Character->GetActorLocation(), GetActorLocation()) > FMath::Square(340.0f))
	{
		OutFeedback = TEXT("Step closer to the bench to sit down.");
		return false;
	}

	// If seated on another bench, stand up from it first
	if (AMallRestBench* OtherBench = FindBenchOccupiedBy(Character))
	{
		if (OtherBench != this)
		{
			FString Dummy;
			OtherBench->TryStandUp(Player, Dummy);
		}
	}

	// Release any pushed cart before sitting down
	if (GetWorld())
	{
		for (TActorIterator<AMallPushCart> It(GetWorld()); It; ++It)
		{
			if (It->GetPusher() == Character)
			{
				It->StopPushing(Player);
			}
		}
	}

	OccupantEntryTransform = Character->GetActorTransform();
	const FVector LocalApproach = GetActorTransform().InverseTransformPosition(Character->GetActorLocation());
	OccupantSeatSideSign = (LocalApproach.Y >= 0.0f) ? 1.0f : -1.0f;

	const FVector SeatWorldPos = GetActorTransform().TransformPosition(
		FVector(0.0f, OccupantSeatSideSign * FMath::Abs(SeatLocalOffset.Y), SeatLocalOffset.Z));
	const float OutwardYaw = FRotator::NormalizeAxis(GetActorRotation().Yaw + (OccupantSeatSideSign >= 0.0f ? 90.0f : -90.0f));

	if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}

	OccupantPawn = Character;
	SitStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Character->SetActorLocationAndRotation(
		SeatWorldPos,
		FRotator(0.0f, OutwardYaw, 0.0f),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	ApplySeatedCharacterPosture(Character, true);
	AMallPushCart::ConfigurePawnFirstPersonView(Character, false);

	// Angle the 3rd-person orbit camera so the player sees their seated character resting on the bench
	Player->SetControlRotation(FRotator(-14.0f, FRotator::NormalizeAxis(OutwardYaw + 150.0f), 0.0f));

	if (UMallPlayerVitalityComponent* Vitality = UMallPlayerVitalityComponent::FindOrCreateForPawn(Character))
	{
		Vitality->bIsSeatedResting = true;
	}

	ForceNetUpdate();
	OutFeedback = FString::Printf(
		TEXT("Seated on %s [3rd-Person Rest View]! Recovering +%.0f%% Stamina/s & +%.0f HP/s. Press [F], [E], [G], or Move to stand up."),
		*BenchDisplayName,
		SeatedStaminaRegenPerSecond,
		SeatedHealthRegenPerSecond);
	return true;
}

bool AMallRestBench::TryStandUp(APlayerController* Player, FString& OutFeedback)
{
	if (!HasAuthority() || !IsValid(OccupantPawn))
	{
		return false;
	}
	if (Player && Player->GetPawn() && Player->GetPawn() != OccupantPawn)
	{
		return false;
	}

	APawn* SeatedPawn = OccupantPawn;
	OccupantPawn = nullptr;

	const FVector StandWorldPos = GetActorTransform().TransformPosition(
		FVector(0.0f, OccupantSeatSideSign * FMath::Max(78.0f, FMath::Abs(StandUpLocalOffset.Y)), StandUpLocalOffset.Z));
	const float OutwardYaw = FRotator::NormalizeAxis(GetActorRotation().Yaw + (OccupantSeatSideSign >= 0.0f ? 90.0f : -90.0f));

	SeatedPawn->SetActorLocationAndRotation(
		StandWorldPos,
		FRotator(0.0f, OutwardYaw, 0.0f),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	ApplySeatedCharacterPosture(SeatedPawn, false);
	AMallPushCart::ConfigurePawnFirstPersonView(SeatedPawn, true);

	if (APlayerController* SeatedPC = Cast<APlayerController>(SeatedPawn->GetController()))
	{
		SeatedPC->SetControlRotation(FRotator(0.0f, OutwardYaw, 0.0f));
	}
	if (UMallPlayerVitalityComponent* Vitality = SeatedPawn->FindComponentByClass<UMallPlayerVitalityComponent>())
	{
		Vitality->bIsSeatedResting = false;
	}

	ForceNetUpdate();
	OutFeedback = TEXT("Stood up from bench. Switched back to First-Person View.");
	return true;
}

bool AMallRestBench::TryToggleSit(APlayerController* Player, FString& OutFeedback)
{
	if (!IsValid(Player) || !IsValid(Player->GetPawn()))
	{
		return false;
	}
	if (IsOccupiedBy(Player->GetPawn()))
	{
		return TryStandUp(Player, OutFeedback);
	}
	return TrySitDown(Player, OutFeedback);
}

void AMallRestBench::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !IsValid(OccupantPawn))
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(OccupantPawn);
	if (!IsValid(Character) || Character->IsActorBeingDestroyed())
	{
		OccupantPawn = nullptr;
		ForceNetUpdate();
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now > SitStartTimeSeconds + 0.55)
	{
		const FVector InputVec = Character->GetLastMovementInputVector();
		if (InputVec.SizeSquared() > 0.25f || Character->bPressedJump)
		{
			FString Feedback;
			TryStandUp(Cast<APlayerController>(Character->GetController()), Feedback);
			return;
		}
	}

	// Keep the character cleanly anchored on the bench seat while resting
	const FVector SeatWorldPos = GetActorTransform().TransformPosition(
		FVector(0.0f, OccupantSeatSideSign * FMath::Abs(SeatLocalOffset.Y), SeatLocalOffset.Z));
	const float OutwardYaw = FRotator::NormalizeAxis(GetActorRotation().Yaw + (OccupantSeatSideSign >= 0.0f ? 90.0f : -90.0f));
	if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}
	Character->SetActorLocationAndRotation(
		SeatWorldPos,
		FRotator(0.0f, OutwardYaw, 0.0f),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

FString AMallRestBench::GetHUDPrompt(const APawn* ViewerPawn) const
{
	float StaminaPct = 100.0f;
	if (ViewerPawn)
	{
		if (const UMallPlayerVitalityComponent* Vit = ViewerPawn->FindComponentByClass<UMallPlayerVitalityComponent>())
		{
			StaminaPct = Vit->Stamina;
		}
	}
	if (IsOccupiedBy(ViewerPawn))
	{
		return FString::Printf(
			TEXT("%s  [SEATED — 3RD-PERSON REST VIEW]  |  Stamina: %.0f%%\n[F] / [E] / [G] or [W/A/S/D]: Stand Up (Return to 1st-Person View)  |  Mouse: Orbit 3rd-Person Camera"),
			*BenchDisplayName,
			StaminaPct);
	}
	return FString::Printf(
		TEXT("%s  |  Stamina: %.0f%%\n[F] or [E]: Sit & Rest on Bench (Switches to 3rd-Person View & Recovers +%.0f%% Stamina/s)"),
		*BenchDisplayName,
		StaminaPct,
		SeatedStaminaRegenPerSecond);
}

AMallRestBench* AMallRestBench::FindBenchOccupiedBy(const APawn* Pawn)
{
	if (!IsValid(Pawn) || !Pawn->GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<AMallRestBench> It(Pawn->GetWorld()); It; ++It)
	{
		if (It->IsOccupiedBy(Pawn))
		{
			return *It;
		}
	}
	return nullptr;
}

AMallRestBench* AMallRestBench::FindBenchInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction)
{
	if (!IsValid(Pawn) || !Pawn->GetWorld())
	{
		return nullptr;
	}
	if (AMallRestBench* Occupied = FindBenchOccupiedBy(Pawn))
	{
		return Occupied;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MallRestBenchFocus), true, Pawn);
	FHitResult Hit;
	if (Pawn->GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 340.0f, ECC_Visibility, Params))
	{
		if (AMallRestBench* Bench = Cast<AMallRestBench>(Hit.GetActor()))
		{
			if (FVector::DistSquared(Pawn->GetActorLocation(), Bench->GetActorLocation()) <= FMath::Square(340.0f))
			{
				return Bench;
			}
		}
	}
	// Fallback cone check for low benches when looking slightly forward
	AMallRestBench* BestBench = nullptr;
	float BestDistSq = FMath::Square(260.0f);
	const FVector SafeDir = Direction.GetSafeNormal();
	for (TActorIterator<AMallRestBench> It(Pawn->GetWorld()); It; ++It)
	{
		AMallRestBench* Bench = *It;
		if (!IsValid(Bench))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), Bench->GetActorLocation());
		if (DistSq > BestDistSq)
		{
			continue;
		}
		const FVector ToBench = (Bench->GetActorLocation() + FVector(0.0f, 0.0f, 38.0f) - Origin).GetSafeNormal();
		if (FVector::DotProduct(SafeDir, ToBench) >= 0.72f)
		{
			BestDistSq = DistSq;
			BestBench = Bench;
		}
	}
	return BestBench;
}
