#pragma once

#include "CoreMinimal.h"

/** Offline speech recognizer. PCM is supplied by voice capture, never a second microphone. */
class MYPROJECT2_API FMallSpeechRecognizer
{
public:
	using FOnInitialized = TFunction<void(bool bSuccess, FString Status)>;
	/** PhraseId is NAME_None for unrecognized speech; text is normalized and not logged here. */
	using FOnRecognized = TFunction<void(FName PhraseId, float Confidence, FString NormalizedText)>;

	FMallSpeechRecognizer();
	~FMallSpeechRecognizer();
	FMallSpeechRecognizer(const FMallSpeechRecognizer&) = delete;
	FMallSpeechRecognizer& operator=(const FMallSpeechRecognizer&) = delete;

	/** Starts a background worker. Both callbacks run on the game thread and stop on Shutdown. */
	void Initialize(const FString& DllDirectory, const FString& ModelDirectory,
		const TMap<FName, TArray<FString>>& PhraseAliases, FOnInitialized OnInitialized,
		float MinimumConfidence = 0.0f);
	/** Uses the bundled Whisper.cpp CPU runtime and a single English GGML model file.
	 * Confidence is an informational mean token probability, not calibrated against Vosk. */
	void InitializeWhisper(const FString& DllDirectory, const FString& ModelFile,
		const TMap<FName, TArray<FString>>& PhraseAliases, FOnInitialized OnInitialized,
		float MinimumConfidence = 0.0f);
	/** PCM16 signed mono, exactly 16 kHz, 0.25 to 8 seconds. One pending utterance maximum. */
	bool SubmitUtterance(TArray<int16> Samples, FOnRecognized OnRecognized);
	void Shutdown();
	bool IsReady() const;
	FString GetStatus() const;

private:
	class FImpl;
	TUniquePtr<FImpl> Impl;
};
