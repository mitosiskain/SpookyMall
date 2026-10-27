#include "Voice/MallVoiceComponent.h"
#include "Voice/MallVoiceSettings.h"
#include "Voice/MallMicrophoneSettings.h"
#include "Voice/MallMicrophoneDiagnostics.h"
#include "HAL/IConsoleManager.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/VoiceCapture.h"
#include "Interfaces/VoiceCodec.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "VoiceModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogMallVoice, Log, All);

namespace MallVoiceTransport
{
    constexpr int32 SampleRate = 16000;
    constexpr int32 SamplesPerFrame = SampleRate / 50;
    constexpr int32 BytesPerFrame = SamplesPerFrame * sizeof(int16);
    constexpr int32 MaximumPacketBytes = 1024;
    // The engine Opus decoder reserves space for its maximum possible 120 ms output.
    constexpr int32 DecodeBufferBytes = 11520;
    constexpr double PacketRate = 60.0;
    constexpr double PacketBurst = 12.0;
    constexpr float SpeechThreshold = 0.004f;
    // CPU Whisper decoding and the single pending utterance can outlast five seconds.
    // Keep the window bounded; ownership, recent voiced packets, whitelist,
    // cooldown and current ghost/store distance checks still apply server-side.
    constexpr double RecognitionGraceSeconds = 20.0;

    bool IsNewerSequence(uint16 Candidate, uint16 Previous)
    {
        const uint16 Difference = Candidate - Previous;
        return Difference != 0 && Difference < 32768;
    }

    bool IsValidPacket(const TArray<uint8>& Packet)
    {
        // UE 5.8 IVoiceEncoder wraps Opus in frame count, generation, and one uint16 offset.
        // Restrict the protocol to exactly one frame, so clients cannot amplify decode work.
        if (Packet.Num() < 5 || Packet.Num() > MaximumPacketBytes || Packet[0] != 1)
        {
            return false;
        }
        uint16 PayloadLength = 0;
        FMemory::Memcpy(&PayloadLength, Packet.GetData() + 2, sizeof(PayloadLength));
        return PayloadLength > 0 && PayloadLength == Packet.Num() - 4;
    }

    float CalculateRMS(const uint8* PCM, int32 ByteCount)
    {
        const int32 SampleCount = ByteCount / sizeof(int16);
        double Sum = 0.0;
        for (int32 Index = 0; Index < SampleCount; ++Index)
        {
            int16 Sample;
            FMemory::Memcpy(&Sample, PCM + Index * sizeof(int16), sizeof(Sample));
            const double Normalized = static_cast<double>(Sample) / 32768.0;
            Sum += Normalized * Normalized;
        }
        return SampleCount > 0 ? FMath::Sqrt(Sum / SampleCount) : 0.0f;
    }

    bool IsAllowedPhrase(FName PhraseId)
    {
        return PhraseId == TEXT("where_are_you") || PhraseId == TEXT("give_me_a_sign")
            || PhraseId == TEXT("come_here") || PhraseId == TEXT("leave_me_alone")
            || PhraseId == TEXT("write_in_book") || PhraseId == TEXT("who_are_you")
            || PhraseId == TEXT("how_old_are_you");
    }

    FVector MouthLocation(const APawn* Pawn)
    {
        return Pawn ? Pawn->GetPawnViewLocation() : FVector::ZeroVector;
    }
}

UMallVoiceComponent::UMallVoiceComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

APlayerController* UMallVoiceComponent::GetPlayerController() const
{
    return Cast<APlayerController>(GetOwner());
}

void UMallVoiceComponent::SetVoiceDistances(float InMaximumDistanceCm, float InFullVolumeDistanceCm)
{
    if (FMath::IsFinite(InMaximumDistanceCm) && FMath::IsFinite(InFullVolumeDistanceCm))
    {
        MaximumDistanceCm = FMath::Clamp(InMaximumDistanceCm, 100.0f, 10000.0f);
        FullVolumeDistanceCm = FMath::Clamp(InFullVolumeDistanceCm, 0.0f, MaximumDistanceCm - 1.0f);
    }
}

bool UMallVoiceComponent::StartCapture()
{
    if (!GetDefault<UMallVoiceSettings>()->bEnabled)
    {
        StopLocalVoiceForDisable();
        return false;
    }
    FVoiceModule& Voice = FVoiceModule::Get();
    if (!Voice.IsVoiceEnabled())
    {
        Status = TEXT("Voice disabled: enable [Voice] bEnabled and restart");
        return false;
    }
    if (!Encoder.IsValid())
    {
        Encoder = Voice.CreateVoiceEncoder(MallVoiceTransport::SampleRate, 1, EAudioEncodeHint::VoiceEncode_Voice);
        if (!Encoder.IsValid())
        {
            Status = TEXT("Opus encoder unavailable");
            return false;
        }
        Encoder->SetBitrate(24000);
        Encoder->SetVBR(true);
        Encoder->SetComplexity(5);
    }
    Encoder->Reset();
    // This prototype owns capture processing. Never discard quiet speech or apply
    // a second gain stage before our shared recognizer/network microphone path.
    for (const TCHAR* Name : {TEXT("voice.SilenceDetectionThreshold"), TEXT("voice.MicNoiseGateThreshold"), TEXT("voice.MicInputGain")})
    {
        if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
        {
            const float Value = FCString::Strcmp(Name, TEXT("voice.MicInputGain")) == 0 ? 1.0f : 0.0f;
            if (Variable->GetFloat() != Value) { Variable->Set(Value, ECVF_SetByConsole); }
        }
    }
    // A fresh capture object prevents old buffered audio leaking across PTT presses.
    Capture = Voice.CreateVoiceCapture(TEXT(""), MallVoiceTransport::SampleRate, 1);
    if (!Capture.IsValid() || !Capture->Start())
    {
        Capture.Reset();
        Status = TEXT("Microphone unavailable: check Windows default input and microphone access");
        UE_LOG(LogMallVoice, Warning, TEXT("Unable to start default microphone capture"));
        return false;
    }
    BeginMicrophoneTurn();
    FMallMicrophoneDiagnostics::BeginTurn(this);
    return true;
}

void UMallVoiceComponent::SetPushToTalk(bool bPressed)
{
    APlayerController* PC = GetPlayerController();
    if (!PC || !PC->IsLocalController())
    {
        return;
    }
    if (!GetDefault<UMallVoiceSettings>()->bEnabled)
    {
        StopLocalVoiceForDisable();
        return;
    }
    if (bPushToTalk == bPressed) { return; }
    if (bPressed)
    {
        if (!StartCapture())
        {
            return;
        }
        bPushToTalk = true;
        ServerSetTalking(true);
        Status = TEXT("Talking - release V to finish");
    }
    else
    {
        PollCapture();
        if (!bPushToTalk)
        {
            // PollCapture already closed a failed device and ended this utterance.
            return;
        }
        EncodePendingAudio(true);
        bPushToTalk = false;
        if (Capture.IsValid())
        {
            Capture->Stop();
            Capture.Reset();
        }
        const TArray<int16> Empty;
        OnLocalPCM.Broadcast(Empty, true);
        ServerSetTalking(false);
        FMallMicrophoneDiagnostics::EndTurn(this, TEXT("Push-to-talk ended"));
        Status = TEXT("Ready - hold V to speak");
    }
}

void UMallVoiceComponent::PollCapture()
{
    if (!Capture.IsValid() || !bPushToTalk)
    {
        return;
    }
    uint32 Available = 0;
    const EVoiceCaptureState::Type State = Capture->GetCaptureState(Available);
    if (State == EVoiceCaptureState::Error || State == EVoiceCaptureState::UnInitialized)
    {
        // End the local utterance and close the device without recursively polling it again.
        bPushToTalk = false;
        Capture->Stop();
        Capture.Reset();
        PendingPCM.Reset();
        ResetMicrophoneMeter();
        const TArray<int16> Empty;
        OnLocalPCM.Broadcast(Empty, true);
        ServerSetTalking(false);
        FMallMicrophoneDiagnostics::EndTurn(this, TEXT("Capture error"));
        Status = TEXT("Microphone capture error - check the input device");
        return;
    }
    if (State != EVoiceCaptureState::Ok || Available == 0)
    {
        return;
    }
    // Bound delivered data independently of the hardware ring. Windows' ring is
    // only 0.5 seconds: a long game-thread hitch can lose data before this guard.
    if (Available > MallVoiceTransport::SampleRate * sizeof(int16) * 2)
    {
        Capture->Stop();
        Capture.Reset();
        bPushToTalk = false;
        PendingPCM.Reset();
        ResetMicrophoneMeter();
        const TArray<int16> Empty;
        OnLocalPCM.Broadcast(Empty, true);
        ServerSetTalking(false);
        FMallMicrophoneDiagnostics::EndTurn(this, TEXT("Capture data bound exceeded"));
        Status = TEXT("Microphone buffer overflow - press V again");
        return;
    }
    TArray<uint8> Bytes;
    Bytes.SetNumUninitialized(Available);
    uint32 Written = 0;
    uint64 FirstSampleClock = 0;
    Capture->GetVoiceData(Bytes.GetData(), Available, Written, FirstSampleClock);
    Written = FMath::Min(Written, Available) & ~1u;
    if (Written == 0)
    {
        return;
    }
    TArray<int16> Samples;
    Samples.SetNumUninitialized(Written / sizeof(int16));
    FMemory::Memcpy(Samples.GetData(), Bytes.GetData(), Written);
    FMallMicrophoneDiagnostics::RecordSampleClock(this, FirstSampleClock, Samples.Num());
    FeedCapturedSamples(Samples);
    // Drop stale queued chat after hitches instead of delivering old speech seconds later.
    constexpr int32 MaximumPendingBytes = MallVoiceTransport::BytesPerFrame * 12;
    if (PendingPCM.Num() > MaximumPendingBytes)
    {
        PendingPCM.RemoveAt(0, PendingPCM.Num() - MaximumPendingBytes, EAllowShrinking::No);
    }
    EncodePendingAudio(false);
}

void UMallVoiceComponent::FeedCapturedSamples(TArray<int16>& Samples)
{
    FMallMicrophoneDiagnostics::RecordRaw(this, Samples);
    const FMallMicrophoneOptions Options = GetDefault<UMallMicrophoneSettings>()->GetOptions();
    const FMallMicrophoneLevels Levels = MicrophoneProcessor.Process(Samples, Options);
    FMallMicrophoneDiagnostics::RecordProcessed(this, Samples, Options, Levels);
    RawMicrophoneRMS = Levels.RawRMS;
    AppliedMicrophoneGainDb = Levels.AppliedGainDb;
    if (Levels.ProcessedRMS >= MicrophoneRMS)
    {
        MicrophoneRMS = Levels.ProcessedRMS;
        MicrophonePeakAge = 0.0f;
    }
    if (Levels.bClipping) { MicrophoneClipRemaining = 1.0f; }
    bMicrophoneClipping = MicrophoneClipRemaining > 0.0f;
    OnLocalPCM.Broadcast(Samples, false);
    PendingPCM.Append(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
}

void UMallVoiceComponent::BeginMicrophoneTurn()
{
    PendingPCM.Reset();
    // No PCM arrives while PTT is released, so wall-time silence cannot reset the
    // AGC detector. Each new capture must acquire quiet speech independently.
    MicrophoneProcessor.Reset();
    ResetMicrophoneMeter();
}

void UMallVoiceComponent::ResetMicrophoneMeter()
{
    MicrophoneRMS = 0.0f;
    RawMicrophoneRMS = 0.0f;
    AppliedMicrophoneGainDb = 0.0f;
    bMicrophoneClipping = false;
    MicrophonePeakAge = 0.0f;
    MicrophoneClipRemaining = 0.0f;
}

void UMallVoiceComponent::UpdateMicrophoneMeter(float DeltaTime)
{
    const float Step = FMath::IsFinite(DeltaTime) ? FMath::Max(0.0f, DeltaTime) : 0.0f;
    const float PreviousAge = MicrophonePeakAge;
    MicrophonePeakAge += Step;
    const float DecayTime = FMath::Max(0.0f, MicrophonePeakAge - 0.12f) - FMath::Max(0.0f, PreviousAge - 0.12f);
    MicrophoneRMS *= FMath::Exp(-DecayTime / 0.25f);
    MicrophoneClipRemaining = FMath::Max(0.0f, MicrophoneClipRemaining - Step);
    bMicrophoneClipping = MicrophoneClipRemaining > 0.0f;
}

void UMallVoiceComponent::EncodePendingAudio(bool bFlush)
{
    if (!Encoder.IsValid())
    {
        PendingPCM.Reset();
        return;
    }
    if (bFlush && PendingPCM.Num() > 0 && PendingPCM.Num() < MallVoiceTransport::BytesPerFrame)
    {
        PendingPCM.AddZeroed(MallVoiceTransport::BytesPerFrame - PendingPCM.Num());
    }
    int32 Consumed = 0;
    while (PendingPCM.Num() - Consumed >= MallVoiceTransport::BytesPerFrame)
    {
        TArray<uint8> Packet;
        Packet.SetNumUninitialized(MallVoiceTransport::MaximumPacketBytes);
        uint32 EncodedBytes = Packet.Num();
        Encoder->Encode(PendingPCM.GetData() + Consumed, MallVoiceTransport::BytesPerFrame, Packet.GetData(), EncodedBytes);
        Consumed += MallVoiceTransport::BytesPerFrame;
        Packet.SetNum(FMath::Min<int32>(EncodedBytes, MallVoiceTransport::MaximumPacketBytes), EAllowShrinking::No);
        if (MallVoiceTransport::IsValidPacket(Packet))
        {
            ServerReceiveVoice(Packet, ++SendSequence);
            ++SentPackets;
        }
    }
    if (Consumed > 0)
    {
        PendingPCM.RemoveAt(0, Consumed, EAllowShrinking::No);
    }
}

void UMallVoiceComponent::ServerSetTalking_Implementation(bool bTalking)
{
    bServerTalking = bTalking;
    if (!bTalking)
    {
        LastServerTalkEndTime = FPlatformTime::Seconds();
    }
}

void UMallVoiceComponent::ServerReceiveVoice_Implementation(const TArray<uint8>& Packet, uint16 Sequence)
{
    APlayerController* SpeakerPC = GetPlayerController();
    APawn* SpeakerPawn = SpeakerPC ? SpeakerPC->GetPawn() : nullptr;
    if (!GetDefault<UMallVoiceSettings>()->bEnabled || !bServerTalking || !SpeakerPawn
        || !SpeakerPC->PlayerState || !MallVoiceTransport::IsValidPacket(Packet))
    {
        return;
    }
    if (bHasServerSequence && !MallVoiceTransport::IsNewerSequence(Sequence, LastServerSequence))
    {
        return;
    }
    const double Now = FPlatformTime::Seconds();
    PacketTokens = FMath::Min(MallVoiceTransport::PacketBurst,
        PacketTokens + FMath::Max(0.0, Now - PacketTokenTime) * MallVoiceTransport::PacketRate);
    PacketTokenTime = Now;
    if (PacketTokens < 1.0)
    {
        return;
    }
    PacketTokens -= 1.0;
    if (!ServerDecoder.IsValid())
    {
        ServerDecoder = FVoiceModule::Get().CreateVoiceDecoder(MallVoiceTransport::SampleRate, 1);
    }
    if (!ServerDecoder.IsValid())
    {
        return;
    }
    TArray<uint8> PCM;
    PCM.SetNumUninitialized(MallVoiceTransport::DecodeBufferBytes);
    uint32 DecodedBytes = PCM.Num();
    ServerDecoder->Decode(Packet.GetData(), Packet.Num(), PCM.GetData(), DecodedBytes);
    // A hostile packet can ask Opus for a longer frame; never relay that amplification.
    if (DecodedBytes != MallVoiceTransport::BytesPerFrame)
    {
        return;
    }
    bHasServerSequence = true;
    LastServerSequence = Sequence;
    LastServerPacketTime = Now;
    const float RMS = MallVoiceTransport::CalculateRMS(PCM.GetData(), DecodedBytes);
    if (RMS >= MallVoiceTransport::SpeechThreshold)
    {
        LastServerSpeechTime = Now;
        OnServerVoiceActivity.Broadcast(SpeakerPC, RMS);
    }
    const FVector Location = MallVoiceTransport::MouthLocation(SpeakerPawn);
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* ListenerPC = It->Get();
        APawn* ListenerPawn = ListenerPC ? ListenerPC->GetPawn() : nullptr;
        if (!ListenerPawn || ListenerPC == SpeakerPC
            || FVector::DistSquared(Location, MallVoiceTransport::MouthLocation(ListenerPawn)) > FMath::Square(MaximumDistanceCm))
        {
            continue;
        }
        if (UMallVoiceComponent* ListenerVoice = ListenerPC->FindComponentByClass<UMallVoiceComponent>())
        {
            ListenerVoice->ClientReceiveVoice(SpeakerPC->PlayerState, Location, Packet, Sequence);
        }
    }
}

void UMallVoiceComponent::ClientReceiveVoice_Implementation(APlayerState* Speaker, FVector_NetQuantize SpeakerLocation,
    const TArray<uint8>& Packet, uint16 Sequence)
{
    APlayerController* PC = GetPlayerController();
    if (!GetDefault<UMallVoiceSettings>()->bEnabled)
    {
        StopLocalVoiceForDisable();
        return;
    }
    if (!PC || !PC->IsLocalController() || !Speaker || !MallVoiceTransport::IsValidPacket(Packet))
    {
        return;
    }
    FMallRemoteVoiceStream& Stream = RemoteStreams.FindOrAdd(Speaker->GetPlayerId());
    if (Stream.Speaker.IsValid() && Stream.Speaker.Get() != Speaker)
    {
        if (Stream.Audio)
        {
            Stream.Audio->Stop();
            Stream.Audio->DestroyComponent();
        }
        Stream = FMallRemoteVoiceStream();
    }
    if (Stream.bHasSequence && !MallVoiceTransport::IsNewerSequence(Sequence, Stream.LastSequence))
    {
        return;
    }
    if (!Stream.Decoder.IsValid())
    {
        Stream.Decoder = FVoiceModule::Get().CreateVoiceDecoder(MallVoiceTransport::SampleRate, 1);
    }
    if (!Stream.Decoder.IsValid())
    {
        return;
    }
    if (!Stream.Wave)
    {
        Stream.Wave = NewObject<USoundWaveProcedural>(this);
        Stream.Wave->SetSampleRate(MallVoiceTransport::SampleRate);
        Stream.Wave->NumChannels = 1;
        Stream.Wave->SampleByteSize = sizeof(int16);
        Stream.Wave->Duration = INDEFINITELY_LOOPING_DURATION;
        Stream.Wave->SoundGroup = SOUNDGROUP_Voice;
        Stream.Wave->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
        Stream.Audio = NewObject<UAudioComponent>(GetOwner());
        Stream.Audio->bAutoActivate = false;
        Stream.Audio->bAutoDestroy = false;
        Stream.Audio->bAllowSpatialization = true;
        Stream.Audio->bIsUISound = false;
        Stream.Audio->bOverrideAttenuation = true;
        FSoundAttenuationSettings Attenuation;
        Attenuation.bAttenuate = true;
        Attenuation.bSpatialize = true;
        Attenuation.bEnableOcclusion = false;
        Attenuation.bEnableListenerFocus = false;
        Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
        Attenuation.AttenuationShape = EAttenuationShape::Sphere;
        Attenuation.AttenuationShapeExtents = FVector(FullVolumeDistanceCm, 0.0, 0.0);
        Attenuation.FalloffDistance = MaximumDistanceCm - FullVolumeDistanceCm;
        Stream.Audio->SetAttenuationOverrides(Attenuation);
        Stream.Audio->SetSound(Stream.Wave);
        Stream.Audio->RegisterComponent();
    }
    TArray<uint8> PCM;
    PCM.SetNumUninitialized(MallVoiceTransport::DecodeBufferBytes);
    uint32 DecodedBytes = PCM.Num();
    Stream.Decoder->Decode(Packet.GetData(), Packet.Num(), PCM.GetData(), DecodedBytes);
    if (DecodedBytes != MallVoiceTransport::BytesPerFrame)
    {
        return;
    }
    if (Stream.Wave->GetAvailableAudioByteCount() > MallVoiceTransport::BytesPerFrame * 12)
    {
        Stream.Wave->ResetAudio();
    }
    Stream.Speaker = Speaker;
    Stream.LastSpeakerLocation = SpeakerLocation;
    const double Now = FPlatformTime::Seconds();
    if (Now - Stream.LastPacketTime > 0.5)
    {
        Stream.PlaybackReadyTime = Now + 0.08;
    }
    Stream.LastPacketTime = Now;
    Stream.LastSequence = Sequence;
    Stream.bHasSequence = true;
    Stream.Audio->SetWorldLocation(SpeakerLocation);
    Stream.Wave->QueueAudio(PCM.GetData(), DecodedBytes);
    if (!Stream.Audio->IsPlaying() && Now >= Stream.PlaybackReadyTime)
    {
        Stream.Audio->Play();
    }
    ++ReceivedPackets;
}

void UMallVoiceComponent::SubmitRecognizedPhrase(FName PhraseId, float Confidence)
{
    const APlayerController* PC = GetPlayerController();
    const UMallVoiceSettings* Settings = GetDefault<UMallVoiceSettings>();
    if (Settings->bEnabled && Settings->bSpeechRecognitionEnabled && PC && PC->IsLocalController()
        && MallVoiceTransport::IsAllowedPhrase(PhraseId) && FMath::IsFinite(Confidence))
    {
        LastServerFeedback = TEXT("Waiting for the ghost's response...");
        ServerSubmitPhrase(PhraseId, FMath::Clamp(Confidence, 0.0f, 1.0f));
    }
}

void UMallVoiceComponent::ServerSubmitPhrase_Implementation(FName PhraseId, float Confidence)
{
    const double Now = FPlatformTime::Seconds();
    APlayerController* PC = GetPlayerController();
    const UMallVoiceSettings* Settings = GetDefault<UMallVoiceSettings>();
    if (!PC || !PC->GetPawn() || !MallVoiceTransport::IsAllowedPhrase(PhraseId)
        || !FMath::IsFinite(Confidence) || Confidence < FMath::Max(0.0f, Settings->MinimumPhraseConfidence) || Confidence > 1.0f)
    {
        return;
    }
    if (!Settings->bEnabled || !Settings->bSpeechRecognitionEnabled || Now - LastPhraseTime < 1.0
        || Now - LastServerPacketTime > MallVoiceTransport::RecognitionGraceSeconds
        || Now - LastServerSpeechTime > MallVoiceTransport::RecognitionGraceSeconds
        || (!bServerTalking && Now - LastServerTalkEndTime > MallVoiceTransport::RecognitionGraceSeconds))
    {
        ReportPhraseResult(PhraseId, false);
        return;
    }
    LastPhraseTime = Now;
    if (OnServerPhrase.IsBound())
    {
        OnServerPhrase.Broadcast(PC, PhraseId, Confidence);
    }
    else
    {
        ReportPhraseResult(PhraseId, false);
    }
}

void UMallVoiceComponent::ReportPhraseResult(FName PhraseId, bool bAccepted)
{
    const APlayerController* PC = GetPlayerController();
    const double Now = FPlatformTime::Seconds();
    if (!PC || !PC->HasAuthority() || !MallVoiceTransport::IsAllowedPhrase(PhraseId) || Now - LastFeedbackTime < 0.5)
    {
        return;
    }
    LastFeedbackTime = Now;
    ClientReportPhraseResult(PhraseId, bAccepted);
}

void UMallVoiceComponent::ClientReportPhraseResult_Implementation(FName PhraseId, bool bAccepted)
{
    LastServerPhrase = PhraseId;
    LastServerPhraseAccepted = bAccepted;
    const FString Phrase = PhraseId.ToString().Replace(TEXT("_"), TEXT(" "));
    LastServerFeedback = bAccepted
        ? FString::Printf(TEXT("Ghost heard: %s"), *Phrase)
        : FString::Printf(TEXT("Ghost did not react: %s (check the store, distance, or cooldown)"), *Phrase);
}

void UMallVoiceComponent::RefreshPlayback()
{
    if (!GetDefault<UMallVoiceSettings>()->bEnabled)
    {
        StopLocalVoiceForDisable();
        return;
    }
    const double Now = FPlatformTime::Seconds();
    for (auto It = RemoteStreams.CreateIterator(); It; ++It)
    {
        FMallRemoteVoiceStream& Stream = It.Value();
        if (!Stream.Speaker.IsValid())
        {
            if (Stream.Audio)
            {
                Stream.Audio->Stop();
                Stream.Audio->DestroyComponent();
            }
            It.RemoveCurrent();
            continue;
        }
        if (Stream.Audio)
        {
            if (const APawn* Pawn = Stream.Speaker->GetPawn())
            {
                Stream.Audio->SetWorldLocation(MallVoiceTransport::MouthLocation(Pawn));
            }
            if (Now - Stream.LastPacketTime > 0.5 && Stream.Audio->IsPlaying())
            {
                Stream.Audio->Stop();
            }
            else if (!Stream.Audio->IsPlaying() && Now - Stream.LastPacketTime <= 0.5
                && Now >= Stream.PlaybackReadyTime && Stream.Wave->GetAvailableAudioByteCount() > 0)
            {
                // An 80 ms initial queue absorbs packet/tick jitter, including brief utterances.
                Stream.Audio->Play();
            }
        }
    }
}

void UMallVoiceComponent::StopPlayback()
{
    for (auto& Pair : RemoteStreams)
    {
        if (Pair.Value.Wave)
        {
            Pair.Value.Wave->ResetAudio();
        }
        if (Pair.Value.Audio)
        {
            Pair.Value.Audio->Stop();
            Pair.Value.Audio->DestroyComponent();
        }
    }
    RemoteStreams.Reset();
}

void UMallVoiceComponent::StopLocalVoiceForDisable()
{
    FMallMicrophoneDiagnostics::Cancel(this);
    const bool bWasTalking = bPushToTalk;
    bPushToTalk = false;
    if (Capture.IsValid())
    {
        Capture->Stop();
        Capture.Reset();
    }
    // Disabling must discard buffered microphone audio instead of flushing another packet.
    PendingPCM.Reset();
    MicrophoneProcessor.Reset();
    ResetMicrophoneMeter();
    if (bWasTalking)
    {
        const TArray<int16> Empty;
        OnLocalPCM.Broadcast(Empty, true);
        ServerSetTalking(false);
    }
    StopPlayback();
    Status = TEXT("Voice disabled in Project Settings");
}

void UMallVoiceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    const APlayerController* PC = GetPlayerController();
    if (PC && PC->IsLocalController())
    {
        if (!GetDefault<UMallVoiceSettings>()->bEnabled)
        {
            StopLocalVoiceForDisable();
            return;
        }
        if (Status == TEXT("Voice disabled in Project Settings"))
        {
            Status = TEXT("Ready - hold V to speak");
        }
        PollCapture();
        UpdateMicrophoneMeter(DeltaTime);
        RefreshPlayback();
    }
}

void UMallVoiceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FMallMicrophoneDiagnostics::Cancel(this);
    if (Capture.IsValid())
    {
        Capture->Stop();
        Capture.Reset();
    }
    bPushToTalk = false;
    Encoder.Reset();
    ServerDecoder.Reset();
    PendingPCM.Reset();
    StopPlayback();
    OnLocalPCM.Clear();
    OnServerVoiceActivity.Clear();
    OnServerPhrase.Clear();
    Super::EndPlay(EndPlayReason);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallVoiceCodecTest, "Mall.Voice.CodecAndPacketBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallVoiceCodecTest::RunTest(const FString& Parameters)
{
    using namespace MallVoiceTransport;
    FVoiceModule& Voice = FVoiceModule::Get();
    TSharedPtr<IVoiceEncoder> TestEncoder = Voice.CreateVoiceEncoder(SampleRate, 1, EAudioEncodeHint::VoiceEncode_Voice);
    TSharedPtr<IVoiceDecoder> TestDecoder = Voice.CreateVoiceDecoder(SampleRate, 1);
    if (!TestTrue(TEXT("Configured Opus encoder is available"), TestEncoder.IsValid())
        || !TestTrue(TEXT("Configured Opus decoder is available"), TestDecoder.IsValid()))
    {
        return false;
    }
    TArray<int16> Samples;
    Samples.SetNumUninitialized(SamplesPerFrame);
    for (int32 Index = 0; Index < Samples.Num(); ++Index)
    {
        Samples[Index] = static_cast<int16>(FMath::Sin(Index * 2.0 * PI * 440.0 / SampleRate) * 8000.0);
    }
    TArray<uint8> Packet;
    Packet.SetNumUninitialized(MaximumPacketBytes);
    uint32 EncodedBytes = Packet.Num();
    TestEncoder->Encode(reinterpret_cast<const uint8*>(Samples.GetData()), BytesPerFrame, Packet.GetData(), EncodedBytes);
    Packet.SetNum(EncodedBytes);
    TestTrue(TEXT("Actual engine-encoded frame passes the transport guard"), IsValidPacket(Packet));
    TArray<uint8> Decoded;
    Decoded.SetNumUninitialized(DecodeBufferBytes);
    uint32 DecodedBytes = Decoded.Num();
    TestDecoder->Decode(Packet.GetData(), Packet.Num(), Decoded.GetData(), DecodedBytes);
    TestEqual(TEXT("Real codec roundtrip produces one 20 ms PCM16 frame"), DecodedBytes, static_cast<uint32>(BytesPerFrame));
    TestTrue(TEXT("Decoded tone has nonzero energy"), CalculateRMS(Decoded.GetData(), DecodedBytes) > SpeechThreshold);

    const TArray<uint8> ValidPacket = Packet;
    Packet.SetNum(3);
    TestFalse(TEXT("Truncated header is rejected before decoder access"), IsValidPacket(Packet));
    Packet = ValidPacket;
    Packet[0] = 2;
    TestFalse(TEXT("Multiple frames cannot amplify per-packet decode work"), IsValidPacket(Packet));
    Packet = ValidPacket;
    Packet[2] = 0xff;
    Packet[3] = 0xff;
    TestFalse(TEXT("Offset past packet data is rejected"), IsValidPacket(Packet));
    Packet.SetNumZeroed(MaximumPacketBytes + 1);
    TestFalse(TEXT("Oversized payload is rejected"), IsValidPacket(Packet));
    TestTrue(TEXT("Sequence wraps forward"), IsNewerSequence(0, 65535));
    TestFalse(TEXT("Repeated packet is rejected"), IsNewerSequence(42, 42));
    TestFalse(TEXT("Out of order packet is rejected"), IsNewerSequence(41, 42));
    TestTrue(TEXT("Known phrase is allowed"), IsAllowedPhrase(TEXT("come_here")));
    TestTrue(TEXT("Book writing phrase is allowed"), IsAllowedPhrase(TEXT("write_in_book")));
    TestTrue(TEXT("Ghost identity question is allowed"), IsAllowedPhrase(TEXT("who_are_you")));
    TestTrue(TEXT("Ghost age question is allowed"), IsAllowedPhrase(TEXT("how_old_are_you")));
    TestTrue(TEXT("Ghost location question is allowed"), IsAllowedPhrase(TEXT("where_are_you")));
    TestTrue(TEXT("Ghost sign request is allowed"), IsAllowedPhrase(TEXT("give_me_a_sign")));
    TestTrue(TEXT("Ghost pause request is allowed"), IsAllowedPhrase(TEXT("leave_me_alone")));
    TestFalse(TEXT("A natural-language alias cannot bypass canonical phrase validation"), IsAllowedPhrase(TEXT("what is your name")));
    TestFalse(TEXT("A negated canonical phrase is rejected"), IsAllowedPhrase(TEXT("do_not_write_in_book")));
    TestFalse(TEXT("Arbitrary client phrase is rejected"), IsAllowedPhrase(TEXT("delete_everything")));
    return true;
}
#endif
