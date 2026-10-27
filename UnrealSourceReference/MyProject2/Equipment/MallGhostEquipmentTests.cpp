#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Equipment/MallGhostEquipment.h"
#include "Equipment/MallLightCart.h"
#include "Equipment/MallPushCart.h"
#include "Ghost/MallBookInteractionComponent.h"
#include "Ghost/MallGhostDisturbanceProp.h"
#include "Ghost/MallGhostInteractionComponent.h"
#include "Ghost/MallGhostWritingBook.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

namespace MallGhostEquipmentTestHelpers
{
	AStaticMeshActor* SpawnFloor(UWorld* World, const FVector& Center)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(Center - FVector(0.0, 0.0, 25.0), FRotator::ZeroRotator, Params);
		if (Floor && Cube)
		{
			UStaticMeshComponent* Comp = Floor->GetStaticMeshComponent();
			Comp->SetMobility(EComponentMobility::Static);
			Comp->SetStaticMesh(Cube);
			Comp->SetWorldScale3D(FVector(40.0, 40.0, 0.5));
			Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Comp->SetCollisionObjectType(ECC_WorldStatic);
			Comp->SetCollisionResponseToAllChannels(ECR_Block);
		}
		return Floor;
	}

	ACharacter* SpawnGhost(UWorld* World, const FVector& Location)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACharacter* Ghost = World->SpawnActor<ACharacter>(Location, FRotator::ZeroRotator, Params);
		if (Ghost)
		{
			Ghost->Tags.Add(TEXT("StoreGhostPrototype"));
			UMallGhostInteractionComponent* GhostComp = NewObject<UMallGhostInteractionComponent>(Ghost, TEXT("GhostInteraction"));
			Ghost->AddInstanceComponent(GhostComp);
			GhostComp->RegisterComponent();
			GhostComp->bAutomaticHunts = false;
			GhostComp->bAutomaticDisturbances = false;
			GhostComp->SetGhostMode(EMallGhostMode::InvisibleDisturbance);
		}
		return Ghost;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMallGhostEquipmentTest,
	"Mall.Equipment.GhostHuntingTools",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallGhostEquipmentTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	if (!TestNotNull(TEXT("Test world created"), World))
	{
		return false;
	}
	FEditorScriptExecutionGuard ScriptGuard;

	MallGhostEquipmentTestHelpers::SpawnFloor(World, FVector::ZeroVector);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APlayerController* PC = World->SpawnActor<APlayerController>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	ACharacter* PlayerPawn = World->SpawnActor<ACharacter>(FVector(0.0, 0.0, 92.0), FRotator::ZeroRotator, SpawnParams);
	if (!TestNotNull(TEXT("PlayerController spawned"), PC) || !TestNotNull(TEXT("PlayerPawn spawned"), PlayerPawn))
	{
		return false;
	}
	PlayerPawn->GetCapsuleComponent()->SetCapsuleSize(34.0f, 90.0f);
	PlayerPawn->GetCharacterMovement()->DisableMovement();
	PC->Possess(PlayerPawn);

	UMallBookInteractionComponent* Interaction = NewObject<UMallBookInteractionComponent>(PC, TEXT("BookInteraction"));
	PC->AddInstanceComponent(Interaction);
	Interaction->RegisterComponent();

	// 1. Test EMF Reader pickup, power toggle, mute toggle, and EMF 1..5 detection
	AMallEMFReader* EMFReader = World->SpawnActor<AMallEMFReader>(FVector(120.0, 0.0, 2.0), FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("EMF Reader spawned"), EMFReader);
	TestTrue(TEXT("EMF Reader picked up by player"), EMFReader->TryPickup(PC));
	TestTrue(TEXT("Interaction adopts EMF Reader"), Interaction->AdoptCartItem(EMFReader));
	TestEqual(TEXT("EMF Reader carrier is player pawn"), EMFReader->GetCarrier(), static_cast<APawn*>(PlayerPawn));
	TestEqual(TEXT("EMF Reader starts at Level 0 when off"), EMFReader->CurrentEMFLevel, 0);

	TestTrue(TEXT("EMF Reader powers on via primary use"), EMFReader->TryPrimaryUse(PC));
	TestTrue(TEXT("EMF Reader is powered"), EMFReader->IsPowered());
	EMFReader->Tick(0.1f);
	TestEqual(TEXT("EMF Reader reads baseline Level 1 with no ghost nearby"), EMFReader->CurrentEMFLevel, 1);

	TestTrue(TEXT("EMF Reader toggles mute via secondary use"), EMFReader->TrySecondaryUse(PC));
	TestTrue(TEXT("EMF Reader is muted"), EMFReader->bAudioMuted);

	// Spawn an invisible store ghost within EMF-5 range (180 cm away)
	ACharacter* Ghost = MallGhostEquipmentTestHelpers::SpawnGhost(World, FVector(180.0, 0.0, 92.0));
	TestNotNull(TEXT("Store Ghost spawned"), Ghost);
	EMFReader->Tick(0.1f);
	TestEqual(TEXT("EMF Reader reaches Level 5 near EMF-5 store ghost"), EMFReader->CurrentEMFLevel, 5);
	TestTrue(TEXT("EMF Reader latches bEMF5Detected evidence"), EMFReader->bEMF5Detected);

	// Place EMF Reader on the floor sohands are free for next tool
	TestTrue(TEXT("EMF Reader placed on floor"), EMFReader->TryPlace(PC, FTransform(FRotator::ZeroRotator, FVector(90.0, -60.0, 1.0))));
	Interaction->ClearStaleHeldItems();

	// 2. Test UV Blacklight & UV Evidence Print reveal
	AMallUVEvidence* UVPrint = World->SpawnActor<AMallUVEvidence>(FVector(220.0, 0.0, 1.0), FRotator::ZeroRotator, SpawnParams);
	AMallUVLight* UVLight = World->SpawnActor<AMallUVLight>(FVector(110.0, 20.0, 2.0), FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("UV Evidence spawned"), UVPrint);
	TestNotNull(TEXT("UV Blacklight spawned"), UVLight);
	TestFalse(TEXT("UV Print starts unlit"), UVPrint->bIlluminatedByUV);
	TestTrue(TEXT("UV Print mesh starts hidden in game"), UVPrint->EvidenceMesh->bHiddenInGame != 0);

	TestTrue(TEXT("UV Blacklight picked up"), UVLight->TryPickup(PC));
	TestTrue(TEXT("Interaction adopts UV Blacklight"), Interaction->AdoptCartItem(UVLight));
	TestTrue(TEXT("UV Blacklight powered on"), UVLight->TryPrimaryUse(PC));
	PC->SetControlRotation(FRotator(-15.0, 0.0, 0.0));
	UVLight->Tick(0.1f);
	TestTrue(TEXT("UV Print is illuminated by UV Blacklight cone"), UVPrint->bIlluminatedByUV);
	TestTrue(TEXT("UV Print records discovery"), UVPrint->bEverDiscovered);
	TestFalse(TEXT("UV Print mesh is visible while illuminated"), UVPrint->EvidenceMesh->bHiddenInGame != 0);
	TestTrue(TEXT("UV Blacklight reports illuminated prints (authored + dynamic ghost footprint)"), UVLight->IlluminatedPrintCount >= 1);

	TestTrue(TEXT("UV Blacklight switches to Wide Flood mode on secondary use"), UVLight->TrySecondaryUse(PC));
	TestTrue(TEXT("UV Blacklight is in Wide Flood mode"), UVLight->bWideFloodMode);
	TestTrue(TEXT("UV Blacklight placed on floor"), UVLight->TryPlace(PC, FTransform(FRotator::ZeroRotator, FVector(90.0, -90.0, 1.0))));
	Interaction->ClearStaleHeldItems();

	// 3. Test Thermal Scanner (Thermometer) cold & freezing readings
	AMallThermometer* Thermometer = World->SpawnActor<AMallThermometer>(FVector(110.0, -20.0, 2.0), FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("Thermometer spawned"), Thermometer);
	TestTrue(TEXT("Thermometer picked up"), Thermometer->TryPickup(PC));
	TestTrue(TEXT("Interaction adopts Thermometer"), Interaction->AdoptCartItem(Thermometer));
	TestTrue(TEXT("Thermometer powered on"), Thermometer->TryPrimaryUse(PC));
	PC->SetControlRotation(FRotator::ZeroRotator);
	Thermometer->Tick(0.1f);
	TestTrue(TEXT("Thermometer reads sub-zero temperature near freezing ghost"), Thermometer->CurrentTemperatureCelsius < 0.0f);
	TestTrue(TEXT("Thermometer latches bFreezingDetected"), Thermometer->bFreezingDetected);
	TestTrue(TEXT("Thermometer toggles Fahrenheit via secondary use"), Thermometer->TrySecondaryUse(PC));
	TestTrue(TEXT("Thermometer formats in Fahrenheit"), Thermometer->FormatTemperature(0.0f).Contains(TEXT("32.0 F")));
	TestTrue(TEXT("Thermometer placed on floor"), Thermometer->TryPlace(PC, FTransform(FRotator::ZeroRotator, FVector(70.0, -90.0, 1.0))));
	Interaction->ClearStaleHeldItems();

	// 4. Test Motion Sensor detecting invisible ghost
	AMallMotionSensor* MotionSensor = World->SpawnActor<AMallMotionSensor>(FVector(100.0, -35.0, 2.0), FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("Motion Sensor spawned"), MotionSensor);
	TestTrue(TEXT("Motion Sensor powered/armed while placed"), MotionSensor->TryPrimaryUse(PC));
	MotionSensor->Tick(0.1f);
	TestTrue(TEXT("Motion Sensor detects invisible ghost in forward cone"), MotionSensor->bMotionDetected);
	TestTrue(TEXT("Motion Sensor increments TriggerCount"), MotionSensor->TriggerCount >= 1);

	// 5. Test Photo Camera scoring visible ghost & UV print and storing on AMallPushCart
	AMallPhotoCamera* Camera = World->SpawnActor<AMallPhotoCamera>(FVector(80.0, 20.0, 2.0), FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("Photo Camera spawned"), Camera);
	TestTrue(TEXT("Photo Camera picked up"), Camera->TryPickup(PC));
	TestTrue(TEXT("Interaction adopts Photo Camera"), Interaction->AdoptCartItem(Camera));

	// Aim at the invisible ghost at 180cm -> 4-Star Ectoplasmic Distortion (or UV print -> 4-Star)
	PC->SetControlRotation(FRotator::ZeroRotator);
	TestTrue(TEXT("Photo Camera snaps first photo"), Camera->TryPrimaryUse(PC));
	TestEqual(TEXT("Photo Camera increments PhotosTaken to 1"), Camera->PhotosTaken, 1);
	TestEqual(TEXT("Photo Camera scores 4 stars on invisible ghost / UV print"), Camera->LastPhotoStars, 4);

	// Switch ghost to VisibleHunt and snap a 5-Star photo after cooldown
	if (UMallGhostInteractionComponent* GhostComp = Ghost->FindComponentByClass<UMallGhostInteractionComponent>())
	{
		GhostComp->SetGhostMode(EMallGhostMode::VisibleHunt);
	}
	Camera->FlashCooldownSeconds = 0.0f;
	TestTrue(TEXT("Photo Camera snaps second photo of visible ghost"), Camera->TryPrimaryUse(PC));
	TestEqual(TEXT("Photo Camera scores 5 stars on visible ghost"), Camera->LastPhotoStars, 5);
	TestEqual(TEXT("Photo Camera awards $100 for 5-star ghost photo"), Camera->LastPhotoReward, 100);

	// 6. Test storing and retrieving equipment on AMallPushCart
	AMallPushCart* Cart = World->SpawnActor<AMallPushCart>(FVector(0.0, 140.0, 46.0), FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("Equipment Cart spawned"), Cart);
	Cart->StorageCapacity = 8;
	Cart->SetFreePhysicsEnabled(false);
	TestTrue(TEXT("Equipment Cart is parked"), Cart->IsParked());
	TestTrue(TEXT("Equipment Cart is reachable by player"), Cart->CanPlayerReach(PC));
	TestTrue(TEXT("Photo Camera deposits onto AMallPushCart"), Cart->TryDeposit(PC, Camera));
	Interaction->ClearStaleHeldItems();
	TestNull(TEXT("Photo Camera carrier cleared while stored on cart"), Camera->GetCarrier());
	TestEqual(TEXT("Photo Camera StoredOnCart points to cart"), Camera->StoredOnCart.Get(), Cart);

	TestTrue(TEXT("Photo Camera retrieves from AMallPushCart"), Cart->TryRetrieve(PC));
	TestEqual(TEXT("Photo Camera carrier restored to player pawn"), Camera->GetCarrier(), static_cast<APawn*>(PlayerPawn));

	// 7. Test First-Person Pawn configuration & Shared Cart Computer Screen (Journal, Photo Log & Remote Cams)
	TestTrue(TEXT("ConfigurePawnFirstPersonView succeeds on PlayerPawn"), AMallPushCart::ConfigurePawnFirstPersonView(PlayerPawn, true));
	TestTrue(TEXT("PlayerPawn uses controller yaw in First-Person"), PlayerPawn->bUseControllerRotationYaw != 0);
	TestFalse(TEXT("PlayerPawn disables orient-to-movement in First-Person"), PlayerPawn->GetCharacterMovement()->bOrientRotationToMovement != 0);
	TestTrue(TEXT("PlayerPawn tagged MallFirstPersonPawn"), PlayerPawn->ActorHasTag(TEXT("MallFirstPersonPawn")));

	Cart->SyncComputerFromWorldEquipment();
	TestTrue(TEXT("Cart Computer auto-detects EMF-5, UV Prints, Freezing Temps & Motion"),
		(Cart->SharedEvidenceMask & (EMallGhostEvidenceBit::EMFLevel5 | EMallGhostEvidenceBit::UVFingerprints | EMallGhostEvidenceBit::FreezingTemps | EMallGhostEvidenceBit::MotionDetected))
		== (EMallGhostEvidenceBit::EMFLevel5 | EMallGhostEvidenceBit::UVFingerprints | EMallGhostEvidenceBit::FreezingTemps | EMallGhostEvidenceBit::MotionDetected));
	TestTrue(TEXT("Cart Computer deduces Frost Wraith from EMF-5 + UV + Freezing"),
		Cart->DeducedGhostArchetype.Contains(TEXT("FROST WRAITH")));
	TestEqual(TEXT("Cart Computer syncs 2 captured photos from team camera"), Cart->SyncedPhotoCount, 2);
	TestEqual(TEXT("Cart Computer syncs 5-star best photo rating"), Cart->SyncedBestPhotoStars, 5);
	TestTrue(TEXT("Cart Computer syncs cumulative photo payout"), Cart->SyncedTotalPhotoReward >= 160);

	TestEqual(TEXT("Cart Computer starts on Tab 0 (Evidence Journal)"), static_cast<int32>(Cart->ActiveComputerTab), 0);
	TestTrue(TEXT("Player cycles Cart Computer to Tab 1 (Saved Photos)"), Cart->TryCycleComputerTab(PC));
	TestEqual(TEXT("Cart Computer is on Tab 1"), static_cast<int32>(Cart->ActiveComputerTab), 1);
	TestTrue(TEXT("Player cycles Cart Computer to Tab 2 (Remote Cams & Sensors)"), Cart->TryCycleComputerTab(PC));
	TestEqual(TEXT("Cart Computer is on Tab 2"), static_cast<int32>(Cart->ActiveComputerTab), 2);
	TestTrue(TEXT("Player cycles Remote Sensor/Camera Feed on Cart Computer"), Cart->TryCycleRemoteFeed(PC, false));
	TestTrue(TEXT("Cart Computer remote feed status is populated"), !Cart->SyncedRemoteFeedStatus.IsEmpty());

	// 8. Test Cart Role Split (Light/Battery Safe-Zone Cart vs Equipment & Info-Screen Cart) & Exclusive 1-Player Full-Screen Lock
	AMallLightCart* LightCart = World->SpawnActor<AMallLightCart>(FVector(-140.0, 140.0, 46.0), FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("Light Safe-Zone Cart spawned"), LightCart);
	TestEqual(TEXT("Light Safe-Zone Cart has 0 equipment storage capacity"), LightCart->StorageCapacity, 0);
	TestFalse(TEXT("Light Safe-Zone Cart has no computer terminal"), LightCart->bHasComputerTerminal);
	TestTrue(TEXT("Equipment Cart has computer terminal"), Cart->bHasComputerTerminal);
	TestFalse(TEXT("Light Safe-Zone Cart rejects TryBeginUseComputer"), LightCart->TryBeginUseComputer(PC));

	APlayerController* PC2 = World->SpawnActor<APlayerController>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	ACharacter* PlayerPawn2 = World->SpawnActor<ACharacter>(FVector(-60.0, 140.0, 92.0), FRotator::ZeroRotator, SpawnParams);
	if (PC2 && PlayerPawn2)
	{
		PlayerPawn2->GetCapsuleComponent()->SetCapsuleSize(34.0f, 90.0f);
		PlayerPawn2->GetCharacterMovement()->DisableMovement();
		PC2->Possess(PlayerPawn2);
	}
	TestNotNull(TEXT("Second player spawned for exclusive computer test"), PlayerPawn2);
	TestTrue(TEXT("Player 1 enters Full-Screen Cart Computer Mode"), Cart->TryBeginUseComputer(PC));
	TestTrue(TEXT("Cart reports Computer in use by Player 1"), Cart->IsComputerInUseBy(PlayerPawn));
	TestFalse(TEXT("Player 2 is blocked from entering Cart Computer while Player 1 is on the screen"), Cart->TryBeginUseComputer(PC2));
	TestFalse(TEXT("Player 2 is blocked from cycling Cart Computer tab while Player 1 is on the screen"), Cart->TryCycleComputerTab(PC2));
	TestTrue(TEXT("Player 1 exits Full-Screen Cart Computer Mode"), Cart->StopUseComputer(PC));
	TestFalse(TEXT("Cart reports Computer no longer in use"), Cart->IsComputerInUse());
	TestTrue(TEXT("Player 2 can now enter Full-Screen Cart Computer Mode after Player 1 exits"), Cart->TryBeginUseComputer(PC2));
	TestTrue(TEXT("Player 2 exits Full-Screen Cart Computer Mode"), Cart->StopUseComputer(PC2));

	// 9. Test Physical 3D Light Fixture (AMallLightFixture) & Debugging Light Remote Gun (AMallLightRemoteGun)
	TestTrue(TEXT("Deposit Photo Camera back onto Equipment Cart to free hands"), Cart->TryDeposit(PC, Camera));
	Interaction->ClearStaleHeldItems();

	AMallLightFixture* CeilingFixture = World->SpawnActor<AMallLightFixture>(FVector(320.0, 0.0, 260.0), FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("AMallLightFixture spawned"), CeilingFixture);
	CeilingFixture->ConfigureFixture(TEXT("Atrium Canopy Fixture #1"), 480.0f, 1350.0f, FLinearColor(1.0f, 0.94f, 0.85f), true);
	TestNotNull(TEXT("Light Fixture has 3D housing mesh"), CeilingFixture->FixtureHousingMesh->GetStaticMesh().Get());
	TestNotNull(TEXT("Light Fixture has 3D glowing diffuser bulb mesh"), CeilingFixture->GlowingBulbMesh->GetStaticMesh().Get());
	TestTrue(TEXT("Light Fixture starts powered at 100% Well-Lit"), CeilingFixture->bLightPowered && CeilingFixture->BrightnessMode == 2);
	TestTrue(TEXT("Light Fixture uses large SourceRadius for smooth gray-space falloff"), CeilingFixture->FixturePointLight->SourceRadius >= 100.0f);

	AMallLightRemoteGun* RemoteGun = World->SpawnActor<AMallLightRemoteGun>(FVector(70.0, 0.0, 2.0), FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("AMallLightRemoteGun spawned"), RemoteGun);
	TestTrue(TEXT("Player picks up Light Remote Gun"), RemoteGun->TryPickup(PC));
	TestTrue(TEXT("Interaction adopts Light Remote Gun"), Interaction->AdoptCartItem(RemoteGun));

	// Aim from PlayerPawn Eye toward CeilingFixture at (320, 0, 260)
	const FRotator AimAtFixture = (CeilingFixture->GetActorLocation() - PlayerPawn->GetPawnViewLocation()).Rotation();
	PC->SetControlRotation(AimAtFixture);
	RemoteGun->Tick(0.1f);
	TestTrue(TEXT("Light Remote Gun locks onto aimed 3D Ceiling Fixture"), RemoteGun->AimedTargetTitle.Contains(TEXT("Atrium Canopy Fixture #1")));
	TestTrue(TEXT("Light Remote Gun LMB toggles aimed fixture OFF"), RemoteGun->TryPrimaryUse(PC));
	TestFalse(TEXT("Ceiling Fixture is now OFF"), CeilingFixture->bLightPowered);
	TestNearlyEqual(TEXT("Ceiling Fixture point light intensity is 0 when OFF"), CeilingFixture->FixturePointLight->Intensity, 0.0f);

	TestTrue(TEXT("Light Remote Gun LMB toggles aimed fixture back ON (100%)"), RemoteGun->TryPrimaryUse(PC));
	TestTrue(TEXT("Ceiling Fixture is back ON"), CeilingFixture->bLightPowered);
	TestEqual(TEXT("Ceiling Fixture BrightnessMode is 2 (100% Well-Lit)"), static_cast<int32>(CeilingFixture->BrightnessMode), 2);

	TestTrue(TEXT("Light Remote Gun E cycles aimed fixture to Gray-Space Dim (45%)"), RemoteGun->TrySecondaryUse(PC));
	TestEqual(TEXT("Ceiling Fixture BrightnessMode is 1 (45% Gray-Space Dim)"), static_cast<int32>(CeilingFixture->BrightnessMode), 1);
	TestNearlyEqual(TEXT("Ceiling Fixture point light intensity is 45% in Gray-Space mode"), CeilingFixture->FixturePointLight->Intensity, 480.0f * 0.45f, 1.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
