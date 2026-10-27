#pragma once

#include "CoreMinimal.h"

/** Decoded text and an informational mean token probability, not a calibrated correctness score. */
struct FMallWhisperTranscript
{
	FString Text;
	float Confidence = 0.0f;
	bool bValid = true;
};

/** Native CPU backend. Every method, including destruction, belongs to the speech worker. */
class FMallWhisperBackend final
{
public:
	FMallWhisperBackend();
	~FMallWhisperBackend();
	FMallWhisperBackend(const FMallWhisperBackend&) = delete;
	FMallWhisperBackend& operator=(const FMallWhisperBackend&) = delete;

	bool Initialize(const FString& DllDirectory, const FString& ModelFile,
		const TAtomic<bool>& bStopping, FString& OutError);
	FMallWhisperTranscript Recognize(TConstArrayView<int16> Samples, const TAtomic<bool>& bStopping);
	void Shutdown();

private:
	class FImpl;
	TUniquePtr<FImpl> Impl;
};
