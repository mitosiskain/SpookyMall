#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Ghost/MallGhostSoundComponent.h"
#include "Ghost/MallGhostAudio.h"
#include "Ghost/MallGhostInteractionComponent.h"
#include "Voice/MallVoiceSettings.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Tests/NetTestHelpers.h"
#include "UObject/UnrealType.h"

namespace MallGhostSoundTest
{
	struct FRestoreSettings
	{
		UMallVoiceSettings* Settings = GetMutableDefault<UMallVoiceSettings>();
		bool bVoice = Settings->bEnabled, bSpeech = Settings->bSpeechRecognitionEnabled, bStatus = Settings->bShowVoiceStatus;
		FString Map = UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Client);
		FString Mode = UGameMapsSettings::GetGlobalDefaultGameMode();
		TSubclassOf<UGameViewportClient> Viewport = GEngine->GameViewportClientClass;
		FRestoreSettings()
		{
			// Native network fixtures never open microphone or recognition hardware.
			Settings->bEnabled = Settings->bSpeechRecognitionEnabled = Settings->bShowVoiceStatus = false;
			UGameMapsSettings::SetGameDefaultMap(TEXT("/Engine/Maps/Entry"));
			UGameMapsSettings::SetGlobalDefaultGameMode(TEXT("/Script/Engine.GameMode"));
			if (UClass* Compatible = LoadClass<UGameViewportClient>(nullptr, TEXT("/Script/CommonUI.CommonGameViewportClient")))
			{
				GEngine->GameViewportClientClass = Compatible;
			}
		}
		~FRestoreSettings()
		{
			Settings->bEnabled = bVoice; Settings->bSpeechRecognitionEnabled = bSpeech; Settings->bShowVoiceStatus = bStatus;
			UGameMapsSettings::SetGameDefaultMap(Map); UGameMapsSettings::SetGlobalDefaultGameMode(Mode);
			GEngine->GameViewportClientClass = Viewport;
		}
	};

	void FreezeCharacters(UWorld* World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			It->SetActorTickEnabled(false);
			It->GetCharacterMovement()->DisableMovement();
			It->GetCharacterMovement()->SetComponentTickEnabled(false);
		}
	}

	ACharacter* FindGhost(UWorld* World, UClass* Class)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It) { if (It->IsA(Class)) { return *It; } }
		return nullptr;
	}

	bool SetNumber(AActor* Actor, FName Name, double Value)
	{
		FNumericProperty* Property = FindFProperty<FNumericProperty>(Actor->GetClass(), Name);
		if (!Property || !Property->IsFloatingPoint()) { return false; }
		Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Actor), Value);
		return true;
	}

	bool SetChase(AActor* Ghost, const FString& State, APawn* Target)
	{
		FStrProperty* StateProperty = FindFProperty<FStrProperty>(Ghost->GetClass(), TEXT("PrototypeState"));
		FObjectPropertyBase* TargetProperty = FindFProperty<FObjectPropertyBase>(Ghost->GetClass(), TEXT("TargetPlayer"));
		if (!StateProperty || !TargetProperty) { return false; }
		StateProperty->SetPropertyValue_InContainer(Ghost, State);
		TargetProperty->SetObjectPropertyValue_InContainer(Ghost, Target);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallGhostSoundNetworkTest, "Mall.Ghost.PhysicalSoundNetwork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallGhostSoundNetworkTest::RunTest(const FString& Parameters)
{
	using namespace MallGhostSoundTest;
	FRestoreSettings Restore;
	UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"), TEXT("/Script/Engine.GameMode"));
	if (!TestTrue(TEXT("Authority world loads"), Worlds.Server.IsLoaded())
		|| !TestTrue(TEXT("First real IP peer connects"), Worlds.CreateAndConnectClient())
		|| !TestTrue(TEXT("Second real IP peer connects"), Worlds.CreateAndConnectClient())) { return false; }
	Worlds.PreTickAllDelegate.AddLambda([&]()
	{
		FreezeCharacters(Worlds.Server.GetWorld());
		for (UE::Net::FTestWorldInstance& Client : Worlds.Clients) { FreezeCharacters(Client.GetWorld()); }
	});
	UWorld* Server = Worlds.Server.GetWorld();
	APlayerController* PC[2] = {};
	if (!TestTrue(TEXT("Connected authoritative controllers exist"), Worlds.TickUntil([&]()
	{
		PC[0] = Worlds.GetServerPlayerControllerOfClient(0); PC[1] = Worlds.GetServerPlayerControllerOfClient(1);
		return PC[0] && PC[1];
	}, 120))) { return false; }
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Player[2] = {};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Player[Index] = Server->SpawnActor<ACharacter>(FVector(4800, 100 * Index, 90), FRotator::ZeroRotator, Spawn);
		if (!TestNotNull(TEXT("Possessed fixture player spawns"), Player[Index])) { return false; }
		Player[Index]->GetCapsuleComponent()->SetCapsuleSize(34.0f, 90.0f);
		Player[Index]->SetReplicates(true); Player[Index]->SetReplicateMovement(true);
		PC[Index]->Possess(Player[Index]);
	}
	UClass* GhostClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/AI/StoreGhost/BP_StoreGhost.BP_StoreGhost_C"));
	if (!TestNotNull(TEXT("Authored store ghost class loads"), GhostClass)) { return false; }
	const FTransform Home(FRotator::ZeroRotator, FVector(5050, 100, 92.15));
	ACharacter* Ghost = Server->SpawnActorDeferred<ACharacter>(GhostClass, Home, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Authored store ghost spawns"), Ghost)) { return false; }
	Ghost->Tags.AddUnique(TEXT("StoreGhostPrototype")); Ghost->SetReplicates(true); Ghost->SetReplicateMovement(true);
	const bool bStoreConfigured = SetNumber(Ghost, TEXT("StoreMinX"), 3950) && SetNumber(Ghost, TEXT("StoreMaxX"), 5950)
		&& SetNumber(Ghost, TEXT("StoreMinY"), -816.6666667) && SetNumber(Ghost, TEXT("StoreMaxY"), 816.6666667)
		&& SetNumber(Ghost, TEXT("StoreFloorZ"), 0) && SetNumber(Ghost, TEXT("CooldownUntil"), 0);
	UGameplayStatics::FinishSpawningActor(Ghost, Home); Ghost->SetActorTransform(Home); Ghost->SetActorTickEnabled(false);
	if (!TestTrue(TEXT("Ghost retains authored physical store bounds"), bStoreConfigured)) { return false; }
	UMallGhostInteractionComponent* State = nullptr;
	UMallGhostSoundComponent* Sound = nullptr;
	UMallGhostSoundComponent* Peer[2] = {};
	ACharacter* PeerGhost[2] = {};
	if (!TestTrue(TEXT("Voice-independent sound component replicates to both peers"), Worlds.TickUntil([&]()
	{
		State = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
		Sound = Ghost->FindComponentByClass<UMallGhostSoundComponent>();
		for (int32 Index = 0; Index < 2; ++Index)
		{
			PeerGhost[Index] = FindGhost(Worlds.Clients[Index].GetWorld(), GhostClass);
			Peer[Index] = PeerGhost[Index] ? PeerGhost[Index]->FindComponentByClass<UMallGhostSoundComponent>() : nullptr;
			if (!Peer[Index]) { return false; }
		}
		return State && Sound;
	}, 180))) { return false; }
	State->bAutomaticDisturbances = false;
	State->bAutomaticHunts = false;
	Sound->bAutomaticSounds = false;
	TestTrue(TEXT("Automatic audio gives the first visitor at least six seconds before a cue"), Sound->NextCueTime >= 6.0);
	Sound->NextCueTime = 0;
	TestEqual(TEXT("No spontaneous sound preceded the controlled fixture"), Sound->CueCount, 0);
	TestFalse(TEXT("Client cannot start an authoritative cue"), Peer[0]->TryPlayCue());
	TestFalse(TEXT("Invisible sound cannot authorize player harm"), UMallGhostInteractionComponent::CanGhostHunt(Ghost));
	TestTrue(TEXT("Physical sounds do not require voice capture or a spirit-box actor"), State->IsInvisible());
	if (!TestTrue(TEXT("Invisible free sound is configured"), Sound->InvisibleSounds.Num() > 0)
		|| !TestTrue(TEXT("Hunt free sound is configured"), Sound->HuntSounds.Num() > 0)) { return false; }
	USoundBase* InvisibleAsset = Sound->InvisibleSounds[0].LoadSynchronous();
	USoundBase* HuntAsset = Sound->HuntSounds[0].LoadSynchronous();
	if (!TestNotNull(TEXT("Invisible audio is an imported playable asset"), InvisibleAsset)
		|| !TestNotNull(TEXT("Hunt audio is an imported playable asset"), HuntAsset)) { return false; }
	TestTrue(TEXT("Two states use different audio assets"), InvisibleAsset != HuntAsset);
	// Keep network expectations deterministic when the shipping defaults contain
	// several randomly selected variants; every configured variant must be usable.
	for (const auto& Choice : Sound->InvisibleSounds)
	{
		USoundBase* Asset = Choice.LoadSynchronous();
		TestTrue(TEXT("Every invisible variant is a finite non-looping sound"), IsValid(Asset) && !Asset->IsLooping()
			&& FMath::IsFinite(Asset->GetDuration()) && Asset->GetDuration() > 0 && Asset->GetDuration() <= 15.0);
	}
	Sound->InvisibleSounds = {Sound->InvisibleSounds[0]};
	Player[0]->SetActorLocation(FVector(4800, 1000, 90)); Player[1]->SetActorLocation(FVector(4800, 1200, 90));
	TestFalse(TEXT("Empty store does not emit an invisible cue"), Sound->TryPlayCue());
	Player[0]->SetActorLocation(FVector(5940, 0, 90));
	TestFalse(TEXT("A player whose capsule protrudes outside the store is ineligible"), Sound->TryPlayCue());
	Player[0]->SetActorLocation(FVector(4800, 0, 940));
	TestFalse(TEXT("An upstairs player cannot trigger this ground-floor ghost"), Sound->TryPlayCue());
	Player[0]->SetActorLocation(FVector(4800, 0, 90));
	Ghost->SetActorLocation(FVector(5050, 1000, 92.15));
	TestFalse(TEXT("A ghost outside its own store cannot emit a cue"), Sound->TryPlayCue());
	Ghost->SetActorTransform(Home);
	SetNumber(Ghost, TEXT("CooldownUntil"), Server->GetTimeSeconds() + 1.0);
	TestFalse(TEXT("Blueprint recovery cooldown also blocks physical ghost cues"), Sound->TryPlayCue());
	SetNumber(Ghost, TEXT("CooldownUntil"), 0);
	State->PauseDisturbances(0.25f);
	TestFalse(TEXT("An explicit disturbance pause silences invisible physical cues"), Sound->TryPlayCue());
	Worlds.TickAll(20);
	const auto SavedInvisible = Sound->InvisibleSounds;
	Sound->InvisibleSounds.Reset();
	TestFalse(TEXT("Missing audio cannot create a cue"), Sound->TryPlayCue());
	Sound->NextCueTime = 0;
	USoundWave* Loop = NewObject<USoundWave>(Ghost, NAME_None, RF_Transient);
	Loop->Duration = 1.0f; Loop->bLooping = true;
	Sound->InvisibleSounds.Add(TSoftObjectPtr<USoundBase>(Loop));
	TestFalse(TEXT("Looping ambience is not accepted as a finite creepy cue"), Sound->TryPlayCue());
	Sound->InvisibleSounds = SavedInvisible;
	Sound->NextCueTime = 0;
	TestEqual(TEXT("Rejected states have no replicated playback history"), Sound->CueCount, 0);
	if (!TestTrue(TEXT("Nearby player permits one harmless invisible cue"), Sound->TryPlayCue())) { return false; }
	const FVector InvisibleSource = Ghost->GetPawnViewLocation();
	if (!TestTrue(TEXT("Both peers receive exactly one invisible event from the ghost position"), Worlds.TickUntil([&]()
	{
		for (UMallGhostSoundComponent* Local : {Sound, Peer[0], Peer[1]})
		{
			if (Local->CueCount != 1 || Local->LocalPlaybackCount != 1 || Local->LastCueKind != EMallGhostSoundKind::Invisible
				|| Local->LastSound != InvisibleAsset || !FVector(Local->LastSourceLocation).Equals(InvisibleSource, 1.0)) { return false; }
		}
		return true;
	}, 120))) { return false; }
	TestTrue(TEXT("Playing a creepy sound leaves the ghost invisible and unable to harm"), State->IsInvisible()
		&& !UMallGhostInteractionComponent::CanGhostHunt(Ghost) && State->DisturbanceCount == 0);
	Sound->NextCueTime = 0;
	TestFalse(TEXT("A running cue cannot overlap another cue"), Sound->TryPlayCue());
	const double SavedActiveUntil = Sound->ActiveUntil;
	Sound->bActiveCue = false;
	Sound->ActiveUntil = 0;
	Sound->NextCueTime = Server->GetTimeSeconds() + 2.0;
	TestFalse(TEXT("The rest interval still blocks a finished cue"), Sound->TryPlayCue());
	Sound->bActiveCue = true;
	Sound->ActiveUntil = FMath::Max(SavedActiveUntil, Server->GetTimeSeconds() + 5.0);
	// Null-audio automation has no device. Attach silent components to exercise
	// movement and teardown itself rather than treating IsPlaying() as evidence.
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (IsValid(Peer[Index]->ActiveAudio)) { Peer[Index]->ActiveAudio->DestroyComponent(); }
		UAudioComponent* Audio = NewObject<UAudioComponent>(PeerGhost[Index], NAME_None, RF_Transient);
		if (!TestTrue(TEXT("Each peer can attach its physical cue to its own ghost"), MallGhostAudio::ConfigureAndAttach(Audio, PeerGhost[Index]))) { return false; }
		Audio->RegisterComponent(); Peer[Index]->ActiveAudio = Audio;
	}
	Ghost->SetActorLocation(FVector(5150, 150, 92.15)); Ghost->ForceNetUpdate();
	if (!TestTrue(TEXT("Each local ghost audio follows replicated ghost movement"), Worlds.TickUntil([&]()
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			if (!IsValid(Peer[Index]->ActiveAudio) || !PeerGhost[Index]->GetActorLocation().Equals(Ghost->GetActorLocation(), 1.0)
				|| !Peer[Index]->ActiveAudio->GetComponentLocation().Equals(PeerGhost[Index]->GetPawnViewLocation(), 1.0)) { return false; }
		}
		return true;
	}, 120))) { return false; }
	State->SetGhostMode(EMallGhostMode::VisibleHunt);
	if (!TestTrue(TEXT("Mode transition removes old invisible audio on all peers"), Worlds.TickUntil([&]()
	{
		return !IsValid(Peer[0]->ActiveAudio) && !IsValid(Peer[1]->ActiveAudio);
	}, 120))) { return false; }
	Sound->NextCueTime = 0; Sound->ActiveUntil = 0;
	SetChase(Ghost, TEXT("Idle"), Player[0]);
	TestFalse(TEXT("Visible idle mode alone is not a chase sound trigger"), Sound->TryPlayCue());
	SetChase(Ghost, TEXT("Chasing"), nullptr);
	TestFalse(TEXT("A chase without an actual target cannot emit hunt audio"), Sound->TryPlayCue());
	SetChase(Ghost, TEXT("Chasing"), Player[0]);
	Player[0]->SetActorLocation(FVector(4800, 1000, 90));
	TestFalse(TEXT("A target outside the store cannot trigger hunt audio"), Sound->TryPlayCue());
	Player[0]->SetActorLocation(FVector(4800, 0, 90));
	Worlds.TickAll(20);
	Sound->NextCueTime = 0;
	SetNumber(Ghost, TEXT("CooldownUntil"), Server->GetTimeSeconds() + 1.0);
	TestFalse(TEXT("Stale Chasing text during Blueprint recovery does not authorize hunt audio"), Sound->TryPlayCue());
	SetNumber(Ghost, TEXT("CooldownUntil"), 0);
	if (!TestTrue(TEXT("Actual confined chase can emit its distinct hunt cue"), Sound->TryPlayCue())) { return false; }
	if (!TestTrue(TEXT("Both peers receive one new hunt event at the moved ghost"), Worlds.TickUntil([&]()
	{
		for (UMallGhostSoundComponent* Local : {Sound, Peer[0], Peer[1]})
		{
			if (Local->CueCount != 2 || Local->LocalPlaybackCount != 2 || Local->LastCueKind != EMallGhostSoundKind::Hunt
				|| Local->LastSound != HuntAsset || !FVector(Local->LastSourceLocation).Equals(Ghost->GetPawnViewLocation(), 1.0)) { return false; }
		}
		return true;
	}, 120))) { return false; }
	Peer[0]->MulticastCue_Implementation(HuntAsset, 0.75f, Peer[0]->LastAppliedSerial);
	Peer[0]->MulticastCue_Implementation(InvisibleAsset, 0.55f, Peer[0]->LastAppliedSerial - 1);
	TestEqual(TEXT("Duplicate and older event serials cannot replay a sound"), Peer[0]->LocalPlaybackCount, 2);
	if (!TestTrue(TEXT("Third real IP peer joins after previous events"), Worlds.CreateAndConnectClient())) { return false; }
	UMallGhostSoundComponent* Late = nullptr;
	if (!TestTrue(TEXT("Late join receives history but never replays old ghost sounds"), Worlds.TickUntil([&]()
	{
		ACharacter* LateGhost = FindGhost(Worlds.Clients[2].GetWorld(), GhostClass);
		Late = LateGhost ? LateGhost->FindComponentByClass<UMallGhostSoundComponent>() : nullptr;
		return Late && Late->CueCount == 2 && Late->LastCueKind == EMallGhostSoundKind::Hunt && Late->LastSound == HuntAsset
			&& FVector(Late->LastSourceLocation).Equals(FVector(Sound->LastSourceLocation), 1.0)
			&& Late->LocalPlaybackCount == 0 && !IsValid(Late->ActiveAudio);
	}, 180))) { return false; }
	Worlds.TickAll(20);
	TestEqual(TEXT("Further ticks do not replay the event on the first peer"), Peer[0]->LocalPlaybackCount, 2);
	TestEqual(TEXT("Further ticks do not replay old events for late join"), Late->LocalPlaybackCount, 0);
	State->SetGhostMode(EMallGhostMode::InvisibleDisturbance);
	TestFalse(TEXT("Ending the hunt restores harmless state after physical sounds"), UMallGhostInteractionComponent::CanGhostHunt(Ghost));
	Worlds.TickAll(20);
	Sound->bAutomaticSounds = true;
	Sound->NextCueTime = Server->GetTimeSeconds() + 0.75;
	Worlds.TickAll(20);
	TestEqual(TEXT("Automatic scheduling waits until its deadline"), Sound->CueCount, 2);
	if (!TestTrue(TEXT("Automatic invisible cue is delivered once, including to the late joiner"), Worlds.TickUntil([&]()
	{
		return Sound->CueCount == 3 && Peer[0]->CueCount == 3 && Peer[1]->CueCount == 3 && Late->CueCount == 3
			&& Peer[0]->LocalPlaybackCount == 3 && Peer[1]->LocalPlaybackCount == 3 && Late->LocalPlaybackCount == 1
			&& Late->LastCueKind == EMallGhostSoundKind::Invisible;
	}, 120))) { return false; }
	Sound->bAutomaticSounds = false;
	return true;
}

#endif
