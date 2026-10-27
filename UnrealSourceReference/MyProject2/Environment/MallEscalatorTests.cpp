#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "MallEscalator.h"
#include "Environment/MallCurvedAtriumRing.h"
#include "Environment/MallEscalatorFuse.h"
#include "Environment/MallHidingSpot.h"
#include "Environment/MallRetailStoreUnit.h"
#include "Environment/MallSpaAndFoodSystem.h"
#include "Environment/MallSplineCorridor.h"
#include "Environment/MallStoreVendorNPC.h"
#include "Equipment/MallLightCart.h"
#include "ProceduralMeshComponent.h"
#include "Voice/MallVoiceSettings.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/NetTestHelpers.h"

namespace MallEscalatorTestInternal
{
	struct FRestore
	{
		UMallVoiceSettings* Voice = GetMutableDefault<UMallVoiceSettings>();
		bool Enabled = Voice->bEnabled, Speech = Voice->bSpeechRecognitionEnabled, Status = Voice->bShowVoiceStatus;
		FString Map = UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Client);
		FString Mode = UGameMapsSettings::GetGlobalDefaultGameMode();
		TSubclassOf<UGameViewportClient> Viewport = GEngine->GameViewportClientClass;
		FRestore()
		{
			Voice->bEnabled = Voice->bSpeechRecognitionEnabled = Voice->bShowVoiceStatus = false;
			UGameMapsSettings::SetGameDefaultMap(TEXT("/Engine/Maps/Entry"));
			UGameMapsSettings::SetGlobalDefaultGameMode(TEXT("/Script/Engine.GameMode"));
			if (UClass* Compatible = LoadClass<UGameViewportClient>(nullptr, TEXT("/Script/CommonUI.CommonGameViewportClient")))
			{
				GEngine->GameViewportClientClass = Compatible;
			}
		}
		~FRestore()
		{
			Voice->bEnabled = Enabled;
			Voice->bSpeechRecognitionEnabled = Speech;
			Voice->bShowVoiceStatus = Status;
			UGameMapsSettings::SetGameDefaultMap(Map);
			UGameMapsSettings::SetGlobalDefaultGameMode(Mode);
			GEngine->GameViewportClientClass = Viewport;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMallEscalatorFloorTraversalAndRepairTest,
	"Mall.Environment.EscalatorFloorTraversalAndRepair",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallEscalatorFloorTraversalAndRepairTest::RunTest(const FString& Parameters)
{
	MallEscalatorTestInternal::FRestore Restore;
	UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"), TEXT("/Script/Engine.GameMode"));
	if (!TestTrue(TEXT("Server fixture loads"), Worlds.Server.IsLoaded()) ||
		!TestTrue(TEXT("Client connects"), Worlds.CreateAndConnectClient()))
	{
		return false;
	}

	UWorld* World = Worlds.Server.GetWorld();
	if (!TestNotNull(TEXT("Server world valid"), World))
	{
		return false;
	}

	auto SpawnFloorSlab = [&](const FVector& Center, const FVector& Extent) -> AActor*
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Slab = World->SpawnActor<AActor>(Center, FRotator::ZeroRotator, SpawnParams);
		if (!Slab)
		{
			return nullptr;
		}
		UBoxComponent* Box = NewObject<UBoxComponent>(Slab);
		Box->InitBoxExtent(Extent);
		Box->SetMobility(EComponentMobility::Static);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Slab->SetRootComponent(Box);
		Box->SetWorldLocation(Center);
		Box->RegisterComponent();
		return Slab;
	};

	// Spawn Ground Floor slab (top surface at Z = 0) and Floor 2 slab (top surface at Z = 850, edge at Y = -340)
	SpawnFloorSlab(FVector(-500.0f, -1460.0f, -20.0f), FVector(600.0f, 300.0f, 20.0f));
	SpawnFloorSlab(FVector(-500.0f, -40.0f, 830.0f), FVector(600.0f, 300.0f, 20.0f));

	FActorSpawnParameters EscSpawn;
	EscSpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AMallEscalator* UpEsc = World->SpawnActor<AMallEscalator>(
		FVector(-500.0f, -1460.0f, 0.0f),
		FRotator(0.0f, 90.0f, 0.0f),
		EscSpawn);
	if (!TestNotNull(TEXT("Spawned Up Escalator"), UpEsc))
	{
		return false;
	}

	UpEsc->bUseProceduralShell = true;
	UpEsc->Rise = 850.0f;
	UpEsc->Run = 980.0f;
	UpEsc->LandingLength = 140.0f;
	UpEsc->BeltWidth = 115.0f;
	UpEsc->TravelSpeed = 220.0f;
	UpEsc->SpeedChangeTime = 0.0f;
	UpEsc->ReverseDirection = false;
	UpEsc->SetMaintenanceState(EEscalatorMaintenanceState::Operational);
	UpEsc->RebuildEscalator();

	FActorSpawnParameters CharSpawn;
	CharSpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Rider = World->SpawnActor<ACharacter>(
		FVector(-500.0f, -1420.0f, 94.0f),
		FRotator(0.0f, 90.0f, 0.0f),
		CharSpawn);
	if (!TestNotNull(TEXT("Spawned Rider Character"), Rider))
	{
		return false;
	}

	Rider->GetCapsuleComponent()->SetCapsuleSize(34.0f, 90.0f);
	UCharacterMovementComponent* MoveComp = Rider->GetCharacterMovement();
	MoveComp->SetMovementMode(MOVE_Walking);
	MoveComp->FindFloor(Rider->GetCapsuleComponent()->GetComponentLocation(), MoveComp->CurrentFloor, false);
	MoveComp->AdjustFloorHeight();

	TestTrue(TEXT("Rider starts grounded on lower landing"), MoveComp->IsMovingOnGround());

	// Tick the escalator and world for 9 seconds at 60 Hz with ZERO player input; escalator must carry the rider up to Floor 2 (Z >= 850)!
	const float Dt = 1.0f / 60.0f;
	for (int32 Step = 0; Step < 540; ++Step)
	{
		UpEsc->Tick(Dt);
	}

	const FVector FinalPos = Rider->GetActorLocation();
	const float CapsuleHalfH = Rider->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float FinalFootZ = FinalPos.Z - CapsuleHalfH;

	TestTrue(
		*FString::Printf(TEXT("Rider was carried to Floor 2 (FootZ=%.1f, Y=%.1f)"), FinalFootZ, FinalPos.Y),
		FinalFootZ >= 840.0f && FinalPos.Y >= -320.0f);

	// Test BrokenBlocked -> Breaker Reset progression
	AMallEscalator* BrokenEsc = World->SpawnActor<AMallEscalator>(
		FVector(500.0f, -1460.0f, 0.0f),
		FRotator(0.0f, 90.0f, 0.0f),
		EscSpawn);
	BrokenEsc->Rise = 850.0f;
	BrokenEsc->Run = 980.0f;
	BrokenEsc->LinkedBreakerTag = TEXT("Circuit_TestEast");
	BrokenEsc->SetMaintenanceState(EEscalatorMaintenanceState::BrokenBlocked);
	TestTrue(TEXT("Escalator starts BrokenBlocked"), BrokenEsc->IsBrokenBlocked());
	TestEqual(TEXT("BrokenBlocked belt speed is zero"), BrokenEsc->GetCurrentBeltSpeed(), 0.0f);

	AMallEscalatorBreakerPanel* Breaker = World->SpawnActor<AMallEscalatorBreakerPanel>(
		FVector(650.0f, -1460.0f, 130.0f),
		FRotator::ZeroRotator,
		EscSpawn);
	Breaker->LinkedBreakerTag = TEXT("Circuit_TestEast");
	FString Feedback;
	const bool bResetOk = Breaker->TryResetBreaker(nullptr, Feedback);
	TestTrue(TEXT("Breaker reset succeeded"), bResetOk);
	TestFalse(TEXT("Escalator is no longer BrokenBlocked after breaker reset"), BrokenEsc->IsBrokenBlocked());

	// Verify North-South Oval Orientation (RadiusY > RadiusX), Monumental Arboretum, Five-Biome Store Archetypes & Boundary Seals
	AMallCurvedAtriumRing* Atrium = World->SpawnActor<AMallCurvedAtriumRing>(FVector::ZeroVector, FRotator::ZeroRotator, EscSpawn);
	if (TestNotNull(TEXT("Atrium spawned"), Atrium))
	{
		TestTrue(TEXT("Oval Island Major Axis is oriented North-South (Y > X)"), Atrium->OvalIslandRadiusY > Atrium->OvalIslandRadiusX);
		TestTrue(TEXT("Atrium Void Major Axis is oriented North-South (Y > X)"), Atrium->AtriumRadiusY > Atrium->AtriumRadiusX);
		TestTrue(TEXT("Monumental Arboretum & Hot Springs enabled"), Atrium->bBuildMonumentalArboretumAndHotSprings);
		TestTrue(TEXT("Outer perimeter enclosure enabled"), Atrium->bEncloseOuterPerimeterWithHallwayPortals);
		for (const auto& Portal : Atrium->BalustradePortals)
		{
			TestEqual(TEXT("Balustrade portals only open on Floor 3 (UpperFloorIndex 2) where escalators dock"), Portal.TargetUpperFloorIndex, 2);
		}
	}

	AMallRetailStoreUnit* BathhouseStore = World->SpawnActor<AMallRetailStoreUnit>(FVector(0.0f, 5000.0f, 0.0f), FRotator::ZeroRotator, EscSpawn);
	if (TestNotNull(TEXT("Bathhouse/Sauna Biome Store spawned"), BathhouseStore))
	{
		TestFalse(TEXT("Store neighbor pass-through doors default to sealed (false)"), BathhouseStore->bOpenNeighborPassThroughDoors);
		TestFalse(TEXT("Store left neighbor door defaults to sealed (false)"), BathhouseStore->bOpenLeftNeighborDoor);
		TestFalse(TEXT("Store right neighbor door defaults to sealed (false)"), BathhouseStore->bOpenRightNeighborDoor);
		TestFalse(TEXT("Store rear pass-through door defaults to sealed (false)"), BathhouseStore->bOpenRearPassThroughDoor);
		BathhouseStore->StoreArchetype = EMallStoreArchetype::BathhouseSaunaShowers;
		BathhouseStore->RebuildStoreUnit();
		TestTrue(TEXT("Bathhouse/Sauna store generates solid 3D shell"), BathhouseStore->StoreShellMesh && BathhouseStore->StoreShellMesh->GetNumSections() > 0);
	}

	AMallSplineCorridor* Corridor = World->SpawnActor<AMallSplineCorridor>(FVector(-2300.0f, -3850.0f, 0.0f), FRotator(0.0f, -90.0f, 0.0f), EscSpawn);
	if (TestNotNull(TEXT("Spline Corridor spawned"), Corridor))
	{
		Corridor->NumBays = 4;
		Corridor->OpenPortalBayIndices = { 1, 2 };
		Corridor->bCapFarEndForFutureExpansion = true;
		Corridor->RebuildSplineCorridor();
		TestTrue(TEXT("Corridor generates solid 3D manifold sections"), Corridor->CorridorMesh && Corridor->CorridorMesh->GetNumSections() > 0);
	}

	// Verify 2-Story Multi-Floor Anchor Store generation (Floor 2 Mezzanine + Walkable Internal Staircase)
	AMallRetailStoreUnit* AnchorStore = World->SpawnActor<AMallRetailStoreUnit>(FVector(0.0f, 9000.0f, 0.0f), FRotator::ZeroRotator, EscSpawn);
	if (TestNotNull(TEXT("2-Story Multi-Floor Anchor Store spawned"), AnchorStore))
	{
		AnchorStore->bIsMultiFloorAnchorStore = true;
		AnchorStore->UpperFloorHeightCm = 850.0f;
		AnchorStore->StoreWidthCm = 1100.0f;
		AnchorStore->StoreDepthCm = 1100.0f;
		AnchorStore->RebuildStoreUnit();
		TestTrue(TEXT("Anchor store generates 2-story solid 3D shell"), AnchorStore->StoreShellMesh && AnchorStore->StoreShellMesh->GetNumSections() > 0);
	}

	// Verify Store Vendor NPC catalog cycling & dispensing and Hiding Spot concealment
	APlayerController* ServerPC = World->GetFirstPlayerController();
	APawn* ServerPawn = ServerPC ? ServerPC->GetPawn() : nullptr;
	if (TestNotNull(TEXT("Server PlayerController & Pawn valid"), ServerPawn))
	{
		const FVector PawnPos = ServerPawn->GetActorLocation();
		AMallStoreVendorNPC* Vendor = World->SpawnActor<AMallStoreVendorNPC>(PawnPos + FVector(150.0f, 0.0f, 0.0f), FRotator::ZeroRotator, EscSpawn);
		if (TestNotNull(TEXT("Store Vendor NPC spawned"), Vendor))
		{
			FString VendorMsg;
			const EMallVendorItemType InitialItem = Vendor->GetCurrentCatalogItem();
			TestTrue(TEXT("Vendor catalog cycles via E"), Vendor->TryCycleCatalogItem(ServerPC, VendorMsg));
			TestTrue(TEXT("Vendor catalog index advanced"), Vendor->GetCurrentCatalogItem() != InitialItem);
			TestTrue(TEXT("Vendor dispenses ghost-hunting item via F"), Vendor->TryPurchaseOrDispense(ServerPC, VendorMsg));
			TestEqual(TEXT("Vendor dispensed count incremented"), Vendor->TotalItemsDispensed, 1);
		}

		AMallHidingSpot* HideSpot = World->SpawnActor<AMallHidingSpot>(PawnPos + FVector(0.0f, 120.0f, 0.0f), FRotator::ZeroRotator, EscSpawn);
		if (TestNotNull(TEXT("Mall Hiding Spot spawned"), HideSpot))
		{
			FString HideMsg;
			TestFalse(TEXT("Player starts unconcealed"), AMallHidingSpot::IsPlayerHiddenAndSafe(ServerPawn));
			TestTrue(TEXT("Player enters hiding spot via F"), HideSpot->TryToggleHide(ServerPC, HideMsg));
			TestTrue(TEXT("Player is concealed from ghosts while hidden"), AMallHidingSpot::IsPlayerHiddenAndSafe(ServerPawn));
			TestTrue(TEXT("Player exits hiding spot via F"), HideSpot->TryToggleHide(ServerPC, HideMsg));
			TestFalse(TEXT("Player is unconcealed after stepping out"), AMallHidingSpot::IsPlayerHiddenAndSafe(ServerPawn));
		}

		// Verify Spa Healing Zone (Himalayan Pink Salt Bath) & Food Court Ration / Cart Battery Recharge
		UMallPlayerVitalityComponent* Vit = UMallPlayerVitalityComponent::FindOrCreateForPawn(ServerPawn);
		if (TestNotNull(TEXT("Player Vitality Component created"), Vit))
		{
			Vit->Health = 45.0f;
			Vit->Stamina = 30.0f;
			AMallSpaHealingZone* SaltBath = World->SpawnActor<AMallSpaHealingZone>(PawnPos, FRotator::ZeroRotator, EscSpawn);
			if (TestNotNull(TEXT("Himalayan Pink Salt Bath Spa Zone spawned"), SaltBath))
			{
				SaltBath->SanctuaryType = EMallSpaSanctuaryType::HimalayanPinkSaltBath;
				FString SpaMsg;
				TestTrue(TEXT("Interactive soak in Himalayan Pink Salt Bath succeeds"), SaltBath->TryInteractiveSoak(ServerPC, SpaMsg));
				TestTrue(TEXT("Salt Bath heals player Health"), Vit->Health > 45.0f);
				TestTrue(TEXT("Salt Bath grants Himalayan Pink Salt Ward buff"), Vit->HasSaltWard());
			}

			// Spawn a depleted Light Cart nearby and consume a Food Ration to recharge its battery and boost Stamina
			AMallLightCart* LightCart = World->SpawnActor<AMallLightCart>(PawnPos + FVector(120.0f, -120.0f, 46.0f), FRotator::ZeroRotator, EscSpawn);
			if (TestNotNull(TEXT("Light Cart spawned near player"), LightCart))
			{
				LightCart->BatteryFraction = 0.25f;
				AMallFoodRationItem* FoodItem = World->SpawnActor<AMallFoodRationItem>(PawnPos + FVector(80.0f, 0.0f, 2.0f), FRotator::ZeroRotator, EscSpawn);
				if (TestNotNull(TEXT("Food Ration Item spawned"), FoodItem))
				{
					Vit->Health = 60.0f;
					Vit->Stamina = 40.0f;
					TestTrue(TEXT("Food Ration consumed via primary use"), FoodItem->TryPrimaryUse(ServerPC));
					TestTrue(TEXT("Food Ration restores Health"), Vit->Health > 60.0f);
					TestTrue(TEXT("Food Ration restores Stamina"), Vit->Stamina > 40.0f);
					TestTrue(TEXT("Food Ration grants active combat/energy buff"), Vit->HasFoodBuff());
					TestTrue(TEXT("Food Ration recharges nearby Light Cart battery"), LightCart->BatteryFraction > 0.25f);
				}
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
