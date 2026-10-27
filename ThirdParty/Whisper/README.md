# Whisper runtime

Official whisper.cpp v1.9.5 (`d1be6fde11ac6e0407606b4e42fe72d34add8037`), CPU Win64 binary from the same-commit upstream `b5454` release. Public headers are from that exact commit. MIT notices for the runtime and model are preserved alongside this file.

`Win64` contains only `whisper.dll`, the two ggml core DLLs and the nine upstream CPU variants. The upstream dispatcher chooses a compatible CPU implementation; the game does not directly select an arbitrary instruction set. Other bundled command-line programs, SDL, llama and parakeet are not redistributed with the game.

The system Microsoft Visual C++ runtime and OpenMP (`VCOMP140.DLL`) are required. This project's dependency staging includes every DLL and `.txt` notice in this directory hierarchy's configured runtime/notices locations. The model is staged from `ThirdParty/Whisper/Models`. Keep it outside Content: Unreal's MetaHuman importer handles `.bin` files as assets and cannot read Whisper models.

See `Docs/VoiceRecognition.md` and `ExternalAssets/Voice/whisper_provenance.json`. Reproduce downloads with `Automation/Voice/prepare_whisper.ps1`, then regenerate the hash record with `Automation/Voice/record_whisper_provenance.py`.
