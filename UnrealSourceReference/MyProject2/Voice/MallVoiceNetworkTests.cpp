#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Voice/MallVoiceComponent.h"
#include "Voice/MallGhostVoiceReceiver.h"
#include "Ghost/MallGhostInteractionComponent.h"
#include "Voice/MallVoiceSettings.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameMapsSettings.h"
#include "Interfaces/VoiceCodec.h"
#include "Interfaces/VoiceCapture.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/NetTestHelpers.h"
#include "UObject/UnrealType.h"
#include "VoiceModule.h"

namespace MallVoiceNetworkTest
{
    constexpr int32 SampleRate = 16000;
    constexpr int32 SamplesPerFrame = SampleRate / 50;
    const FName PawnTag(TEXT("MallVoiceNetworkFixturePawn"));
    const FName GhostTag(TEXT("MallVoiceNetworkFixtureGhost"));

    // A device-free capture object verifies cleanup without touching microphone hardware.
    class FFakeVoiceCapture final : public IVoiceCapture
    {
    public:
        bool bStopped = false;
        virtual bool Init(const FString&, int32, int32) override { return true; }
        virtual void Shutdown() override { bStopped = true; }
        virtual bool Start() override { bStopped = false; return true; }
        virtual void Stop() override { bStopped = true; }
        virtual bool ChangeDevice(const FString&, int32, int32) override { return true; }
        virtual bool IsCapturing() override { return !bStopped; }
        virtual EVoiceCaptureState::Type GetCaptureState(uint32& Available) const override
        {
            Available = 0;
            return EVoiceCaptureState::NoData;
        }
        virtual int32 GetBufferSize() const override { return SamplesPerFrame * sizeof(int16); }
        virtual void DumpState() const override { }
    };

    // Modify only in-memory defaults, and restore them after the temporary network worlds close.
    struct FRestoreVoiceSettings
    {
        UMallVoiceSettings* Settings = GetMutableDefault<UMallVoiceSettings>();
        bool bEnabled = Settings->bEnabled;
        bool bSpeech = Settings->bSpeechRecognitionEnabled;
        bool bShowStatus = Settings->bShowVoiceStatus;
        float MaximumDistance = Settings->MaxVoiceDistanceCm;
        float FullVolumeDistance = Settings->FullVolumeDistanceCm;
        FString DefaultMap = UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Client);
        FString DefaultGameMode = UGameMapsSettings::GetGlobalDefaultGameMode();
        TSubclassOf<UGameViewportClient> ViewportClass = GEngine->GameViewportClientClass;
        bool bCompatibleViewport = false;

        FRestoreVoiceSettings()
        {
            Settings->bEnabled = true;
            // This test supplies a phrase ID separately; the offline recognizer has its own tests.
            Settings->bSpeechRecognitionEnabled = false;
            Settings->bShowVoiceStatus = false;
            Settings->MaxVoiceDistanceCm = 2000.0f;
            Settings->FullVolumeDistanceCm = 200.0f;
            // The engine client helper briefly browses the default map before connecting.
            // Keep that temporary browse out of unrelated gameplay/UI Blueprints as well.
            UGameMapsSettings::SetGameDefaultMap(TEXT("/Engine/Maps/Entry"));
            UGameMapsSettings::SetGlobalDefaultGameMode(TEXT("/Script/Engine.GameMode"));
            // CommonUI is enabled in this project. Use its actual compatible viewport in
            // the temporary worlds instead of suppressing its validation diagnostics.
            UClass* CompatibleViewport = LoadClass<UGameViewportClient>(nullptr,
                TEXT("/Script/CommonUI.CommonGameViewportClient"));
            bCompatibleViewport = CompatibleViewport != nullptr;
            if (CompatibleViewport) { GEngine->GameViewportClientClass = CompatibleViewport; }
        }

        ~FRestoreVoiceSettings()
        {
            Settings->bEnabled = bEnabled;
            Settings->bSpeechRecognitionEnabled = bSpeech;
            Settings->bShowVoiceStatus = bShowStatus;
            Settings->MaxVoiceDistanceCm = MaximumDistance;
            Settings->FullVolumeDistanceCm = FullVolumeDistance;
            UGameMapsSettings::SetGameDefaultMap(DefaultMap);
            UGameMapsSettings::SetGlobalDefaultGameMode(DefaultGameMode);
            GEngine->GameViewportClientClass = ViewportClass;
        }
    };

    void FreezeFixturePawns(UWorld* World)
    {
        // These are isolated Entry worlds containing only this test's controlled characters.
        // Actor tags are not replicated, so freeze the client-side characters by class too.
        for (TActorIterator<ACharacter> It(World); It; ++It)
        {
            It->SetActorTickEnabled(false);
            It->GetCharacterMovement()->DisableMovement();
            It->GetCharacterMovement()->SetComponentTickEnabled(false);
        }
    }

    ACharacter* SpawnPossessedCharacter(APlayerController* PC, const FVector& Location)
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ACharacter* Character = PC->GetWorld()->SpawnActor<ACharacter>(Location, FRotator::ZeroRotator, Spawn);
        if (Character)
        {
            Character->Tags.AddUnique(PawnTag);
            Character->GetCapsuleComponent()->SetCapsuleSize(34.0f, 90.0f);
            Character->SetReplicates(true);
            Character->SetReplicateMovement(true);
            Character->GetCharacterMovement()->DisableMovement();
            Character->GetCharacterMovement()->SetComponentTickEnabled(false);
            PC->Possess(Character);
            Character->ForceNetUpdate();
            PC->ForceNetUpdate();
        }
        return Character;
    }

    bool SetGhostNumber(AActor* Ghost, const FName Name, double Value)
    {
        FNumericProperty* Property = FindFProperty<FNumericProperty>(Ghost->GetClass(), Name);
        if (!Property || !Property->IsFloatingPoint()) { return false; }
        Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Ghost), Value);
        return true;
    }

    ACharacter* FindFixtureGhost(UWorld* World, UClass* GhostClass)
    {
        for (TActorIterator<ACharacter> It(World); It; ++It)
        {
            if (It->IsA(GhostClass)) { return *It; }
        }
        return nullptr;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallVoiceNetworkTest, "Mall.Voice.NetworkTransport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallVoiceNetworkTest::RunTest(const FString& Parameters)
{
    using namespace MallVoiceNetworkTest;
    FRestoreVoiceSettings RestoreSettings;
    if (!TestTrue(TEXT("CommonUI-compatible temporary viewport class loads"), RestoreSettings.bCompatibleViewport)) { return false; }
    // The engine helper uses real IP connections and replication, with no editor assets modified.
    UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"), TEXT("/Script/Engine.GameMode"));
    if (!TestTrue(TEXT("Temporary server map loaded"), Worlds.Server.IsLoaded())
        || !TestTrue(TEXT("First real IP client connects"), Worlds.CreateAndConnectClient())
        || !TestTrue(TEXT("Second real IP client connects"), Worlds.CreateAndConnectClient()))
    {
        return false;
    }
    Worlds.PreTickAllDelegate.AddLambda([&Worlds]()
    {
        FreezeFixturePawns(Worlds.Server.GetWorld());
        for (UE::Net::FTestWorldInstance& Client : Worlds.Clients) { FreezeFixturePawns(Client.GetWorld()); }
    });

    APlayerController* ServerPC[2] = {};
    APlayerController* ClientPC[2] = {};
    UMallVoiceComponent* ServerVoice[2] = {};
    UMallVoiceComponent* ClientVoice[2] = {};
    const bool bEndpointsReady = Worlds.TickUntil([&]()
    {
        bool bReady = true;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            ServerPC[Index] = Worlds.GetServerPlayerControllerOfClient(Index);
            ClientPC[Index] = Worlds.Clients[Index].GetWorld()->GetFirstPlayerController();
            ServerVoice[Index] = ServerPC[Index] ? ServerPC[Index]->FindComponentByClass<UMallVoiceComponent>() : nullptr;
            ClientVoice[Index] = ClientPC[Index] ? ClientPC[Index]->FindComponentByClass<UMallVoiceComponent>() : nullptr;
            bReady &= ServerPC[Index] && ClientPC[Index] && ServerVoice[Index] && ClientVoice[Index]
                && ServerPC[Index]->PlayerState && ClientPC[Index]->PlayerState;
        }
        return bReady;
    }, 180);
    if (!TestTrue(TEXT("Subsystem-created owner endpoints replicate to both clients"), bEndpointsReady)) { return false; }
    for (int32 Index = 0; Index < 2; ++Index)
    {
        TestTrue(TEXT("Server endpoint owns the authoritative controller"), ServerPC[Index]->HasAuthority());
        TestTrue(TEXT("Client endpoint owns its local controller"), ClientPC[Index]->IsLocalController() && !ClientPC[Index]->HasAuthority());
        TestEqual(TEXT("Replicated PlayerState identifies the actual owner"), ClientPC[Index]->PlayerState->GetPlayerId(),
            ServerPC[Index]->PlayerState->GetPlayerId());
        TestTrue(TEXT("Transport component is replicated"), ServerVoice[Index]->GetIsReplicated());
        int32 ControllerCount = 0;
        for (FConstPlayerControllerIterator It = Worlds.Clients[Index].GetWorld()->GetPlayerControllerIterator(); It; ++It) { ++ControllerCount; }
        TestEqual(TEXT("Client receives only its own PlayerController endpoint"), ControllerCount, 1);
    }

    ACharacter* Pawn[2] = {
        SpawnPossessedCharacter(ServerPC[0], FVector(4950.0, -400.0, 90.0)),
        SpawnPossessedCharacter(ServerPC[1], FVector(5050.0, -400.0, 90.0))
    };
    if (!TestNotNull(TEXT("First controlled test character"), Pawn[0])
        || !TestNotNull(TEXT("Second controlled test character"), Pawn[1])) { return false; }
    if (!TestTrue(TEXT("Both fixture pawns and their PlayerStates replicate to both clients"), Worlds.TickUntil([&]()
    {
        for (int32 WorldIndex = 0; WorldIndex < 2; ++WorldIndex)
        {
            int32 FoundFixturePawns = 0;
            for (TActorIterator<ACharacter> It(Worlds.Clients[WorldIndex].GetWorld()); It; ++It)
            {
                APlayerState* State = It->GetPlayerState();
                if (State && State->GetPawn() == *It
                    && (State->GetPlayerId() == ServerPC[0]->PlayerState->GetPlayerId()
                        || State->GetPlayerId() == ServerPC[1]->PlayerState->GetPlayerId()))
                {
                    ++FoundFixturePawns;
                }
            }
            const ACharacter* Owned = Cast<ACharacter>(ClientPC[WorldIndex]->GetPawn());
            if (FoundFixturePawns != 2 || !Owned || Owned->GetPlayerState() != ClientPC[WorldIndex]->PlayerState)
            {
                return false;
            }
        }
        return true;
    }, 120))) { return false; }
    Worlds.TickAll(4);
    // Re-enable recognition on the authoritative config before exercising any phrase guard.
    RestoreSettings.Settings->bSpeechRecognitionEnabled = true;

    int32 DecodedFrames[2] = {};
    bool bIdentityCorrect = true;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        ServerVoice[Index]->OnServerVoiceActivity.AddLambda([&, Index](APlayerController* Speaker, float RMS)
        {
            ++DecodedFrames[Index];
            bIdentityCorrect &= Speaker == ServerPC[Index] && RMS >= 0.004f;
        });
        ClientVoice[Index]->Encoder = FVoiceModule::Get().CreateVoiceEncoder(SampleRate, 1, EAudioEncodeHint::VoiceEncode_Voice);
        if (!TestTrue(TEXT("Real client Opus encoder exists"), ClientVoice[Index]->Encoder.IsValid())) { return false; }
        ClientVoice[Index]->Encoder->SetBitrate(24000);
        ClientVoice[Index]->ServerSetTalking(true);
    }
    Worlds.TickAll(4);

    // This friend-only injection replaces microphone hardware with a generated tone. It uses
    // the shipping encoder/send path and real RPCs; all server validation remains active.
    auto SendFrames = [&](int32 ClientIndex, int32 FrameCount)
    {
        TArray<int16> Samples;
        Samples.SetNumUninitialized(SamplesPerFrame * FrameCount);
        for (int32 Index = 0; Index < Samples.Num(); ++Index)
        {
            Samples[Index] = static_cast<int16>(FMath::Sin(Index * 2.0 * PI * 440.0 / SampleRate) * 10000.0);
        }
        ClientVoice[ClientIndex]->PendingPCM.Append(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
        ClientVoice[ClientIndex]->EncodePendingAudio(false);
    };

    const int32 InitialListenerPackets = ClientVoice[1]->ReceivedPackets;
    SendFrames(0, 3);
    const bool bNearReceived = Worlds.TickUntil([&]()
    {
        return DecodedFrames[0] >= 3 && ClientVoice[1]->ReceivedPackets >= InitialListenerPackets + 3;
    }, 120);
    if (!bNearReceived)
    {
        AddInfo(FString::Printf(TEXT("Near transport fixture: sent=%d decoded=%d received=%d talking=%d ownerPawn=%s listenerPawn=%s ownedIdentity=%d"),
            ClientVoice[0]->SentPackets, DecodedFrames[0], ClientVoice[1]->ReceivedPackets - InitialListenerPackets,
            ServerVoice[0]->bServerTalking, *ServerPC[0]->GetPawn()->GetActorLocation().ToString(),
            *ServerPC[1]->GetPawn()->GetActorLocation().ToString(), bIdentityCorrect));
    }
    if (!TestTrue(TEXT("Near listener receives real client-to-server-to-client Opus frames"), bNearReceived)) { return false; }
    TestTrue(TEXT("Server voice activity uses actual decoded RMS and owning identity"), bIdentityCorrect);

    Pawn[1]->SetActorLocation(FVector(7450.0, -400.0, 90.0));
    Pawn[1]->ForceNetUpdate();
    Worlds.TickAll(8);
    const int32 FarListenerPackets = ClientVoice[1]->ReceivedPackets;
    const int32 FramesBeforeFar = DecodedFrames[0];
    SendFrames(0, 3);
    if (!TestTrue(TEXT("Server still decodes speaker while listener is beyond 20m"), Worlds.TickUntil([&]()
    {
        return DecodedFrames[0] >= FramesBeforeFar + 3;
    }, 120))) { return false; }
    Worlds.TickAll(8);
    TestTrue(TEXT("Far fixture is actually beyond configured voice range"),
        FVector::Dist(Pawn[0]->GetActorLocation(), Pawn[1]->GetActorLocation()) > 2000.0);
    TestEqual(TEXT("Out-of-range listener receives no new audio packets"), ClientVoice[1]->ReceivedPackets, FarListenerPackets);

    Pawn[1]->SetActorLocation(FVector(5050.0, -400.0, 90.0));
    Pawn[1]->ForceNetUpdate();
    Worlds.TickAll(8);
    const int32 ReversePackets = ClientVoice[0]->ReceivedPackets;
    SendFrames(1, 2);
    if (!TestTrue(TEXT("Reverse direction also traverses both real RPC hops"), Worlds.TickUntil([&]()
    {
        return DecodedFrames[1] >= 2 && ClientVoice[0]->ReceivedPackets >= ReversePackets + 2;
    }, 120))) { return false; }

    // Voice settings must not control the ghost's existing no-push collision policy.
    RestoreSettings.Settings->bEnabled = false;
    UClass* GhostClass = LoadClass<ACharacter>(nullptr,
        TEXT("/Game/AI/StoreGhost/BP_StoreGhost.BP_StoreGhost_C"));
    if (!TestNotNull(TEXT("Actual store ghost Blueprint class loads"), GhostClass)) { return false; }
    const FTransform GhostTransform(FRotator::ZeroRotator, FVector(4950.0, 0.0, 92.15));
    ACharacter* Ghost = Worlds.Server.GetWorld()->SpawnActorDeferred<ACharacter>(GhostClass, GhostTransform,
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestNotNull(TEXT("Actual Blueprint ghost can be spawned in temporary world"), Ghost)) { return false; }
    Ghost->Tags.AddUnique(TEXT("StoreGhostPrototype"));
    Ghost->Tags.AddUnique(GhostTag);
    Ghost->SetActorTickEnabled(false);
    Ghost->GetCharacterMovement()->DisableMovement();
    Ghost->GetCharacterMovement()->SetComponentTickEnabled(false);
    // Deliberately start at the old CDO response; the native receiver must relax it on every peer.
    Ghost->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Ghost->SetReplicates(true);
    Ghost->SetReplicateMovement(true);
    const bool bConfigured = SetGhostNumber(Ghost, TEXT("StoreMinX"), 3950.0)
        && SetGhostNumber(Ghost, TEXT("StoreMaxX"), 5950.0)
        && SetGhostNumber(Ghost, TEXT("StoreMinY"), -816.6666667)
        && SetGhostNumber(Ghost, TEXT("StoreMaxY"), 816.6666667)
        && SetGhostNumber(Ghost, TEXT("StoreFloorZ"), 0.0);
    UGameplayStatics::FinishSpawningActor(Ghost, GhostTransform);
    Ghost->SetActorTickEnabled(false);
    Ghost->SetActorTransform(GhostTransform);
    Ghost->ForceNetUpdate();
    if (!TestTrue(TEXT("Ghost fixture uses authored ground-floor store bounds"), bConfigured)) { return false; }

    UMallGhostVoiceReceiver* ServerReceiver = nullptr;
    UMallGhostVoiceReceiver* ClientReceiver[2] = {};
    ACharacter* ClientGhost[2] = {};
    if (!TestTrue(TEXT("Native receiver dynamically attaches and replicates to both ghost peers"), Worlds.TickUntil([&]()
    {
        ServerReceiver = Ghost->FindComponentByClass<UMallGhostVoiceReceiver>();
        UMallGhostInteractionComponent* State = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
        if (State) { State->bAutomaticHunts = false; }
        bool bReady = ServerReceiver != nullptr && State != nullptr;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            ClientGhost[Index] = FindFixtureGhost(Worlds.Clients[Index].GetWorld(), GhostClass);
            ClientReceiver[Index] = ClientGhost[Index] ? ClientGhost[Index]->FindComponentByClass<UMallGhostVoiceReceiver>() : nullptr;
            bReady &= ClientReceiver[Index] != nullptr;
        }
        return bReady;
    }, 180))) { return false; }
    TestEqual(TEXT("Server ghost ignores Pawn collisions after receiver BeginPlay"),
        Ghost->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
    for (int32 Index = 0; Index < 2; ++Index)
    {
        TestEqual(TEXT("Replicated ghost peer also ignores Pawn collisions"),
            ClientGhost[Index]->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
        TestFalse(TEXT("Client ghost does not own authority"), ClientGhost[Index]->HasAuthority());
    }
    RestoreSettings.Settings->bEnabled = true;

    const int32 InitialReactions = ServerReceiver->ReactionCount;
    const int32 FramesBeforePhrase = DecodedFrames[0];
    SendFrames(0, 1);
    if (!TestTrue(TEXT("Phrase speaker has recent actual server-decoded voice"), Worlds.TickUntil([&]()
    {
        return DecodedFrames[0] > FramesBeforePhrase;
    }, 90))) { return false; }
    ClientVoice[0]->SubmitRecognizedPhrase(TEXT("where_are_you"), 0.1f);
    if (!TestTrue(TEXT("Low-confidence matched phrase RPC reaches the authoritative ghost and replicates accepted feedback"), Worlds.TickUntil([&]()
    {
        return ServerReceiver->ReactionCount == InitialReactions + 1
            && ClientVoice[0]->LastServerPhrase == TEXT("where_are_you") && ClientVoice[0]->LastServerPhraseAccepted
            && ClientReceiver[0]->ReactionCount == InitialReactions + 1 && ClientReceiver[1]->ReactionCount == InitialReactions + 1;
    }, 120))) { return false; }
    TestTrue(TEXT("Ghost records the actual possessed speaker, not player index zero"), ServerReceiver->LastSpeaker == Pawn[0]);
    for (int32 Index = 0; Index < 2; ++Index)
    {
        TestEqual(TEXT("Ghost phrase telemetry replicates to both clients"), ClientReceiver[Index]->LastPhrase, FName(TEXT("where_are_you")));
    }

    // The second owner has a fresh phrase cooldown. Its outside-store rejection therefore
    // exercises ghost eligibility after the transport accepts actual recent voice and the ID.
    Pawn[1]->SetActorLocation(FVector(3900.0, 0.0, 90.0));
    Pawn[1]->ForceNetUpdate();
    Worlds.TickAll(6);
    int32 RoutedOutsidePhrases = 0;
    ServerVoice[1]->OnServerPhrase.AddLambda([&](APlayerController* Speaker, FName, float)
    {
        if (Speaker == ServerPC[1]) { ++RoutedOutsidePhrases; }
    });
    const int32 FramesBeforeOutside = DecodedFrames[1];
    SendFrames(1, 1);
    if (!TestTrue(TEXT("Outside speaker still passes actual audio transport"), Worlds.TickUntil([&]()
    {
        return DecodedFrames[1] > FramesBeforeOutside;
    }, 90))) { return false; }
    ClientVoice[1]->SubmitRecognizedPhrase(TEXT("where_are_you"), 1.0f);
    if (!TestTrue(TEXT("Outside-store phrase reaches routing and returns rejected owner feedback"), Worlds.TickUntil([&]()
    {
        return RoutedOutsidePhrases == 1 && ClientVoice[1]->LastServerPhrase == TEXT("where_are_you")
            && !ClientVoice[1]->LastServerPhraseAccepted && ClientVoice[1]->LastServerFeedback.Contains(TEXT("did not react"));
    }, 120))) { return false; }
    TestEqual(TEXT("Outside-store speaker does not add a ghost reaction"), ServerReceiver->ReactionCount, InitialReactions + 1);
    TestTrue(TEXT("All actual RMS events kept the server-derived speaker identity"), bIdentityCorrect);

    // Exercise local disabling independently of the server's relay guard. A packet that was
    // correctly delivered earlier must be rejected even if already in flight when disabled.
    APlayerState* RemoteSpeaker = nullptr;
    for (TActorIterator<APlayerState> It(Worlds.Clients[0].GetWorld()); It; ++It)
    {
        if (It->GetPlayerId() == ServerPC[1]->PlayerState->GetPlayerId()) { RemoteSpeaker = *It; break; }
    }
    if (!TestNotNull(TEXT("Disabled-client fixture has the mapped remote speaker"), RemoteSpeaker)) { return false; }
    TArray<int16> Tone;
    Tone.SetNumUninitialized(SamplesPerFrame);
    for (int32 Index = 0; Index < Tone.Num(); ++Index)
    {
        Tone[Index] = static_cast<int16>(FMath::Sin(Index * 2.0 * PI * 440.0 / SampleRate) * 10000.0);
    }
    TArray<uint8> InFlightPacket;
    InFlightPacket.SetNumUninitialized(1024);
    uint32 InFlightBytes = InFlightPacket.Num();
    ClientVoice[1]->Encoder->Encode(reinterpret_cast<const uint8*>(Tone.GetData()), Tone.Num() * sizeof(int16),
        InFlightPacket.GetData(), InFlightBytes);
    InFlightPacket.SetNum(InFlightBytes);
    const FVector_NetQuantize RemoteLocation(RemoteSpeaker->GetPawn()->GetPawnViewLocation());
    ClientVoice[0]->ClientReceiveVoice_Implementation(RemoteSpeaker, RemoteLocation, InFlightPacket, ++ClientVoice[1]->SendSequence);
    if (!TestTrue(TEXT("Enabled client has a live queued remote stream before disable"), !ClientVoice[0]->RemoteStreams.IsEmpty())) { return false; }

    const int32 ReceivedBeforeDisable = ClientVoice[0]->ReceivedPackets;
    const int32 SentBeforeDisable = ClientVoice[0]->SentPackets;
    const FString FeedbackBeforeDisable = ClientVoice[0]->LastServerFeedback;
    const TSharedPtr<FFakeVoiceCapture> FakeCapture = MakeShared<FFakeVoiceCapture>();
    ClientVoice[0]->Capture = FakeCapture;
    ClientVoice[0]->bPushToTalk = true;
    ClientVoice[0]->MicrophoneRMS = 0.25f;
    ClientVoice[0]->PendingPCM.Append(reinterpret_cast<const uint8*>(Tone.GetData()), Tone.Num() * sizeof(int16));
    RestoreSettings.Settings->bEnabled = false;
    Worlds.TickAll(1);
    TestTrue(TEXT("Disabling stops the capture device on the next tick"), FakeCapture->bStopped);
    TestFalse(TEXT("Disabled client releases capture"), ClientVoice[0]->Capture.IsValid());
    TestFalse(TEXT("Disabled client releases push-to-talk"), ClientVoice[0]->IsPushToTalkActive());
    TestTrue(TEXT("Disabled client discards pending microphone PCM"), ClientVoice[0]->PendingPCM.IsEmpty());
    TestEqual(TEXT("Disabling does not flush another microphone packet"), ClientVoice[0]->SentPackets, SentBeforeDisable);
    TestTrue(TEXT("Disabling clears all queued remote voice streams"), ClientVoice[0]->RemoteStreams.IsEmpty());
    ClientVoice[0]->ClientReceiveVoice_Implementation(RemoteSpeaker, RemoteLocation, InFlightPacket, ++ClientVoice[1]->SendSequence);
    TestEqual(TEXT("Disabled client rejects an otherwise valid in-flight voice packet"), ClientVoice[0]->ReceivedPackets, ReceivedBeforeDisable);
    TestTrue(TEXT("Rejected packet cannot recreate a disabled playback queue"), ClientVoice[0]->RemoteStreams.IsEmpty());
    ClientVoice[0]->SetPushToTalk(true);
    TestFalse(TEXT("Disabled client cannot open capture through the public PTT API"), ClientVoice[0]->Capture.IsValid());
    TestFalse(TEXT("Disabled client cannot enter PTT"), ClientVoice[0]->IsPushToTalkActive());
    ClientVoice[0]->SubmitRecognizedPhrase(TEXT("come_here"), 1.0f);
    TestEqual(TEXT("Disabled local phrase submission is rejected before changing submission status"),
        ClientVoice[0]->LastServerFeedback, FeedbackBeforeDisable);
    Worlds.TickAll(4);
    RestoreSettings.Settings->bEnabled = true;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        TestFalse(TEXT("Synthetic network test never opened a microphone"), ClientVoice[Index]->Capture.IsValid());
        ClientVoice[Index]->ServerSetTalking(false);
    }
    Worlds.TickAll(4);
    return true;
}

#endif
