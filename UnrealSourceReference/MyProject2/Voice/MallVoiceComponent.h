#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "Voice/MallMicrophoneProcessor.h"
#include "MallVoiceComponent.generated.h"

class APlayerController;
class APlayerState;
class IVoiceCapture;
class IVoiceEncoder;
class IVoiceDecoder;
class UAudioComponent;
class USoundWaveProcedural;

// Game-thread PCM16 mono at 16000 Hz. The array is borrowed only for the duration of the callback.
DECLARE_MULTICAST_DELEGATE_TwoParams(FMallLocalVoicePCM, const TArray<int16>&, bool);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMallServerVoiceActivity, APlayerController*, float);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FMallServerVoicePhrase, APlayerController*, FName, float);

USTRUCT()
struct FMallRemoteVoiceStream
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TObjectPtr<USoundWaveProcedural> Wave;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> Audio;

    UPROPERTY(Transient)
    TWeakObjectPtr<APlayerState> Speaker;

    TSharedPtr<IVoiceDecoder> Decoder;
    FVector LastSpeakerLocation = FVector::ZeroVector;
    double LastPacketTime = -1000.0;
    double PlaybackReadyTime = -1000.0;
    uint16 LastSequence = 0;
    bool bHasSequence = false;
};

/** Owner-only transport component: the server creates one on each PlayerController. */
UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent))
class MYPROJECT2_API UMallVoiceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMallVoiceComponent();

    UFUNCTION(BlueprintCallable, Category="Mall Voice")
    void SetPushToTalk(bool bPressed);

    UFUNCTION(BlueprintPure, Category="Mall Voice")
    bool IsPushToTalkActive() const { return bPushToTalk; }

    // A client supplies only a phrase ID; the owning controller and hearing position are server-derived.
    UFUNCTION(BlueprintCallable, Category="Mall Voice")
    void SubmitRecognizedPhrase(FName PhraseId, float Confidence);

    /** The authoritative ghost routing code reports whether any eligible ghost accepted the phrase. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall Voice")
    void ReportPhraseResult(FName PhraseId, bool bAccepted);

    void SetVoiceDistances(float InMaximumDistanceCm, float InFullVolumeDistanceCm);

    UPROPERTY(BlueprintReadOnly, Transient, Category="Mall Voice")
    FString Status = TEXT("Ready - hold V to speak");

    UPROPERTY(BlueprintReadOnly, Transient, Category="Mall Voice")
    float MicrophoneRMS = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Mall Voice")
    float RawMicrophoneRMS = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Mall Voice")
    float AppliedMicrophoneGainDb = 0.0f;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Mall Voice")
    bool bMicrophoneClipping = false;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Mall Voice")
    int32 SentPackets = 0;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Mall Voice")
    int32 ReceivedPackets = 0;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Mall Voice")
    FName LastServerPhrase;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Mall Voice")
    bool LastServerPhraseAccepted = false;

    UPROPERTY(BlueprintReadOnly, Transient, Category="Mall Voice")
    FString LastServerFeedback;

    FMallLocalVoicePCM OnLocalPCM;
    FMallServerVoiceActivity OnServerVoiceActivity;
    FMallServerVoicePhrase OnServerPhrase;

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FMallVoiceNetworkTest;
    friend class FMallMicrophonePipelineTest;
#endif

    UFUNCTION(Server, Reliable)
    void ServerSetTalking(bool bTalking);

    UFUNCTION(Server, Unreliable)
    void ServerReceiveVoice(const TArray<uint8>& Packet, uint16 Sequence);

    UFUNCTION(Client, Unreliable)
    void ClientReceiveVoice(APlayerState* Speaker, FVector_NetQuantize SpeakerLocation, const TArray<uint8>& Packet, uint16 Sequence);

    UFUNCTION(Server, Reliable)
    void ServerSubmitPhrase(FName PhraseId, float Confidence);

    UFUNCTION(Client, Reliable)
    void ClientReportPhraseResult(FName PhraseId, bool bAccepted);

    bool StartCapture();
    void PollCapture();
    void FeedCapturedSamples(TArray<int16>& Samples);
    void ResetMicrophoneMeter();
    void BeginMicrophoneTurn();
    void UpdateMicrophoneMeter(float DeltaTime);
    void EncodePendingAudio(bool bFlush);
    void RefreshPlayback();
    void StopPlayback();
    void StopLocalVoiceForDisable();
    APlayerController* GetPlayerController() const;

    TSharedPtr<IVoiceCapture> Capture;
    TSharedPtr<IVoiceEncoder> Encoder;
    TSharedPtr<IVoiceDecoder> ServerDecoder;
    TArray<uint8> PendingPCM;
    FMallMicrophoneProcessor MicrophoneProcessor;
    float MicrophonePeakAge = 0.0f;
    float MicrophoneClipRemaining = 0.0f;

    UPROPERTY(Transient)
    TMap<int32, FMallRemoteVoiceStream> RemoteStreams;

    float MaximumDistanceCm = 2000.0f;
    float FullVolumeDistanceCm = 200.0f;
    bool bPushToTalk = false;
    bool bServerTalking = false;
    bool bHasServerSequence = false;
    uint16 SendSequence = 0;
    uint16 LastServerSequence = 0;
    double LastServerPacketTime = -1000.0;
    double LastServerSpeechTime = -1000.0;
    double LastServerTalkEndTime = -1000.0;
    double LastPhraseTime = -1000.0;
    double LastFeedbackTime = -1000.0;
    double PacketTokenTime = 0.0;
    double PacketTokens = 12.0;
};
