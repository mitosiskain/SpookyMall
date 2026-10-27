# Offline voice recognition

The game now uses Whisper `small.en` through whisper.cpp 1.9.5 on Windows CPU. It replaces the small Vosk model for normal play. Audio stays on the player's PC; there is no account, API key, cloud transcription, or second microphone capture.

## Testing in PIE

1. Wait for **Offline English voice recognition ready (Whisper)** in the voice panel.
2. Hold **V**, say one complete question, and release **V** after the last word. Quiet pauses while V is held no longer finish the phrase. A continuous turn is capped at eight seconds.
3. Wait for processing to finish before repeating the question. An initial CPU benchmark on this PC took about four seconds to decode a short recording. The model stays loaded between questions.
4. Try **where are you**, **who are you**, **how old are you**, or **write in the book**. The spirit box must be powered, and the normal store, ghost-state and distance requirements still apply. Use **F6** for existing microphone gain/AGC controls.

The UI shows the recognized text locally when the phrase is rejected. Transcript contents are not written to gameplay logs. The automation tests log only their synthetic fixture transcripts.

## Microphone gain and local diagnosis

Automatic gain now keeps a fixed speech level. The saved **Manual boost** slider applies only when automatic gain is off; F6 disables that slider while automatic gain is enabled. This prevents a saved high manual boost from also driving AGC toward excessive volume. Each new V press resets gain, detector and filter history so a previous loud pop cannot suppress the next quiet turn.

If microphone audio is also wrong in Windows Recorder or Discord, resolve the Windows/NVIDIA input-device problem first. Changing recognizers cannot restore missing speech. The game's basic filter removes rumble and reduces very low-level noise; it does not replace a working microphone or a speech-isolation filter.

An optional developer diagnostic can save the exact audio before and after the game's gain/filter processing:

1. In PIE, release V and open the Unreal console with **~**.
2. Enter **mall.MicRecordNext**, then close the console. This explicitly arms one local recording; it does not open the microphone.
3. Hold V, say one short phrase, and release V. Only that turn is saved, capped at the first eight seconds. Further turns are not recorded unless explicitly armed again.
4. Files appear under `Saved/VoiceDiagnostics/<timestamp_and_id>/`: **raw.wav**, **processed.wav**, and **metadata.json**. F6 shows the saved directory. `raw.wav` is Unreal-delivered PCM before the game's processing, so Windows or NVIDIA processing may already be present.
5. **mall.MicRecordCancel** discards an armed or active diagnostic without saving it. Ending PIE or disabling voice also discards an unfinished recording. Release V to stop normal microphone capture.

Metadata includes audio/sample duration, push-to-talk wall time, buffer gaps, reported platform sample-clock continuity, raw/processed RMS and peaks, clipping fractions and processing settings. Platform clocks cannot prove that every hardware sample was delivered. Listen to both files and compare their timing before attributing a failure to decoding.

Recording is off by default, consent is not persisted, files remain local, and nothing is uploaded. Saved diagnostics are excluded from Git. These console commands are available only in development builds; ordinary voice play does not retain microphone audio.

## Design and limits

- Mono PCM16 at 16 kHz comes from the same conditioned push-to-talk audio used by proximity chat. Conversion to float32 does not resample or discard words.
- One background worker owns each model context. Decoding uses at most four CPU threads, with one active and one pending turn. The game thread receives guarded callbacks. Shutdown cancels decoding and suppresses stale callbacks.
- The model transcribes freely, then the existing bounded whole-phrase matcher maps text to allowed ghost actions. Wrong transcripts such as “better” and “then then there” are not added as command aliases. Negations and unrelated speech remain rejected.
- Silence/DC, isolated pops and strong model no-speech results are filtered. This does not guarantee that every environmental sound is rejected or every accent is recognized. Human microphone testing remains necessary.
- A mean token probability is shown as informational confidence. It is not a calibrated accuracy score or directly comparable to Vosk's word confidence. Minimum phrase confidence remains zero.
- The server permits up to 20 seconds of recognition latency after recent voiced packets, while retaining ownership, phrase whitelist, cooldown and current store/proximity checks. Old results outside that bounded window are rejected.
- The model stays outside Content to avoid Unreal's .bin asset importer. The model download is about 466 MiB. Runtime memory is greater than that and each local game world currently owns a context; multi-client PIE therefore costs more memory. This first integration uses the CPU to avoid competing with Unreal's renderer for GPU memory.
- Windows native DLLs are retained for process lifetime, including the selected compatible CPU backend. Contexts are released by their owning workers. Mismatched headers/runtime or conflicting DLLs loaded by another plugin fail with a status message.

## Dependencies and reproduction

`Automation/Voice/prepare_whisper.ps1` downloads the official binary package, exact-version headers, notices and model, checking the published package SHA256 and model SHA1. Runtime files live under `ThirdParty/Whisper/Win64`; the model is `ThirdParty/Whisper/Models/ggml-small.en.bin`. Both are staged as loose files in packaged Win64 builds. Native binaries and the model use Git LFS. The Microsoft Visual C++ runtime, including OpenMP (`VCOMP140.DLL`), is required on Windows.

The upstream code and Whisper model use MIT licenses; the original notices are bundled in `ThirdParty/Whisper`. Exact sources and file hashes are recorded in `ExternalAssets/Voice/whisper_provenance.json`.

The previous Vosk implementation and its regression coverage remain available through the native `Initialize` method, but normal play calls `InitializeWhisper`. No silent fallback is used: a missing Whisper model reports an error rather than returning to the old recognition quality.

## Automated coverage

- `MyProject2.Voice.WhisperSpeechBackend`: actual model decoding for the three spirit-box questions, writing command, very quiet speech through cold AGC, speech mixed with noise, negation, unrelated/substitution-like words, silence, noise and a pop; validates callback lifetime and input bounds.
- `MyProject2.Voice.ConcurrentWhisperTeardown`: simultaneous contexts, canceled initialization/recognition, and a surviving context recognizing again after sibling teardown.
- `MyProject2.Voice.PushToTalkUtterance`: quiet connecting spans remain in one turn, key release finalizes once, silence/short pops are omitted, eight-second overflow preserves sample order, and resets discard unfinished speech.
- `MyProject2.Voice.Microphone.AutomaticTargetIgnoresManualBoost`: a saved +19.7 dB manual profile cannot increase the automatic target or limiting, using synthetic speech at quiet, normal and loud input levels.
- `MyProject2.Voice.MicrophonePipeline`: also verifies a quiet new PTT turn after a loud previous turn acquires gain like a fresh processor, without opening a device.
- `MyProject2.Voice.MicrophoneDiagnostics`: synthetic paired WAV sample order/format/metrics, explicit one-turn arming, cancellation and eight-second bounds. Test recordings are removed after verification.

Fixtures are generated with the installed Windows synthetic voice using `create_whisper_fixtures.ps1` and deterministic noise conditioning using `condition_whisper_fixtures.py`. These tests validate integration and specific regressions; they do not measure recognition accuracy on the player's microphone.
