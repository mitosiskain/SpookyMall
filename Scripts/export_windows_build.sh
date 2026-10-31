#!/usr/bin/env bash
# ==============================================================================
# SpookyMall (Megamall Anomaly) — Standalone Windows .exe Exporter (Arch Linux)
# Exports Builds/Windows/SpookyMall.exe + SpookyMall.pck and packages a ready-to-share
# Builds/SpookyMall_Windows_x64.zip for Windows testers (zero Git/Godot required).
# ==============================================================================
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

GODOT_VER_FULL="$(godot --version | head -n 1)"
# e.g., "4.7.2.stable.official.ed1daf0bf" -> "4.7.2.stable" and "4.7.2-stable"
VER_NUM="$(echo "$GODOT_VER_FULL" | cut -d'.' -f1-3)"
VER_CHANNEL="$(echo "$GODOT_VER_FULL" | cut -d'.' -f4)"
TEMPLATE_DIR="$HOME/.local/share/godot/export_templates/${VER_NUM}.${VER_CHANNEL}"
TEMPLATE_WIN_EXE="$TEMPLATE_DIR/windows_release_x86_64.exe"

echo "============================================================"
echo "  SPOOKYMALL : STANDALONE WINDOWS .EXE EXPORT BUILDER"
echo "  Detected Godot Version: ${VER_NUM}.${VER_CHANNEL}"
echo "============================================================"

# 1. Ensure Godot Export Templates are installed for this Godot version
if [[ ! -f "$TEMPLATE_WIN_EXE" ]]; then
    echo "[1/3] Godot export templates not found at:"
    echo "      $TEMPLATE_DIR"
    echo "      Downloading official Godot ${VER_NUM}-${VER_CHANNEL} export templates..."
    mkdir -p "$TEMPLATE_DIR"
    TMP_TPZ="/tmp/Godot_v${VER_NUM}-${VER_CHANNEL}_export_templates.tpz"
    DOWNLOAD_URL="https://github.com/godotengine/godot/releases/download/${VER_NUM}-${VER_CHANNEL}/Godot_v${VER_NUM}-${VER_CHANNEL}_export_templates.tpz"
    if ! curl -fL --progress-bar -o "$TMP_TPZ" "$DOWNLOAD_URL"; then
        echo "[!] Trying tuxfamily /godot-builds mirror..."
        DOWNLOAD_URL="https://github.com/godotengine/godot-builds/releases/download/${VER_NUM}-${VER_CHANNEL}/Godot_v${VER_NUM}-${VER_CHANNEL}_export_templates.tpz"
        curl -fL --progress-bar -o "$TMP_TPZ" "$DOWNLOAD_URL"
    fi
    echo "      Extracting export templates into $TEMPLATE_DIR..."
    unzip -q -j "$TMP_TPZ" "templates/windows_*" "templates/linux_*" -d "$TEMPLATE_DIR"
    rm -f "$TMP_TPZ"
    echo "      Export templates installed!"
else
    echo "[1/3] Found Godot export templates at: $TEMPLATE_DIR"
fi

# 2. Export Windows Desktop build
mkdir -p "$PROJECT_ROOT/Builds/Windows"
echo "[2/3] Exporting standalone Windows build -> Builds/Windows/SpookyMall.exe..."
godot --headless --path "$PROJECT_ROOT" --export-release "Windows Desktop" "$PROJECT_ROOT/Builds/Windows/SpookyMall.exe"

# Copy icon and create quick-host / quick-join helper .bat files inside Builds/Windows
cp -f "$PROJECT_ROOT/icon.ico" "$PROJECT_ROOT/Builds/Windows/icon.ico"

cat > "$PROJECT_ROOT/Builds/Windows/Host_Coop_Server.bat" << 'EOF'
@echo off
cd /d "%~dp0"
start "" "SpookyMall.exe" --host
EOF

# 3. Package into a single shareable .zip if zip/python3 is available
echo "[3/3] Packaging Builds/SpookyMall_Windows_x64.zip..."
python3 - << 'PYEOF'
import os, zipfile

build_dir = "Builds/Windows"
zip_path = "Builds/SpookyMall_Windows_x64.zip"
with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
    for root, _, files in os.walk(build_dir):
        for f in files:
            full_path = os.path.join(root, f)
            rel_path = os.path.relpath(full_path, "Builds")
            zf.write(full_path, os.path.join("SpookyMall_Windows", os.path.basename(full_path)))
print(f"Created {zip_path} ({os.path.getsize(zip_path) / (1024*1024):.1f} MB)")
PYEOF

echo "============================================================"
echo "  SUCCESS! Standalone Windows build is ready:"
echo "  - Folder: Builds/Windows/SpookyMall.exe"
echo "  - Shareable ZIP: Builds/SpookyMall_Windows_x64.zip"
echo "============================================================"
