#!/usr/bin/env bash
# Megamall Anomaly — Whisper Model Fetcher & SHA-1 Verifier (Linux / macOS / Git Bash)
# Downloads ThirdParty/Whisper/Models/ggml-small.en.bin (466 MB) from HuggingFace
# and verifies its official SHA-1 checksum so the 466 MB binary stays out of Git.

set -e
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODEL_DIR="$PROJECT_DIR/ThirdParty/Whisper/Models"
MODEL_FILE="$MODEL_DIR/ggml-small.en.bin"
MODEL_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin"
EXPECTED_SHA1="db8a495a91d927739e50b3fc1cc4c6b8f6c2d022"

mkdir -p "$MODEL_DIR"

if [ -f "$MODEL_FILE" ]; then
    echo "Checking existing Whisper model at $MODEL_FILE..."
    ACTUAL_SHA1="$(sha1sum "$MODEL_FILE" | awk '{print $1}')"
    if [ "$ACTUAL_SHA1" = "$EXPECTED_SHA1" ]; then
        echo "OK: ggml-small.en.bin is already present and SHA-1 verified ($EXPECTED_SHA1)."
        exit 0
    else
        echo "WARNING: SHA-1 mismatch ($ACTUAL_SHA1 != $EXPECTED_SHA1). Re-downloading..."
        rm -f "$MODEL_FILE"
    fi
fi

echo "Downloading official Whisper model (ggml-small.en.bin, ~466 MB)..."
curl -L --progress-bar "$MODEL_URL" -o "$MODEL_FILE"

ACTUAL_SHA1="$(sha1sum "$MODEL_FILE" | awk '{print $1}')"
if [ "$ACTUAL_SHA1" != "$EXPECTED_SHA1" ]; then
    echo "ERROR: SHA-1 verification failed! Expected $EXPECTED_SHA1, got $ACTUAL_SHA1"
    exit 1
fi

echo "SUCCESS: ggml-small.en.bin downloaded and verified ($EXPECTED_SHA1)."
