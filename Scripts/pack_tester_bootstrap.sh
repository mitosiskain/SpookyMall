#!/usr/bin/env bash
# ==============================================================================
# Stamps a Read-Only GitHub Token into a local tester package inside
# Builds/TesterBootstrap/ (git-ignored so GitHub secret scanning never revokes it!)
# Usage: ./Scripts/pack_tester_bootstrap.sh <YOUR_READ_ONLY_GITHUB_PAT>
# ==============================================================================
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

TOKEN="${1:-}"
mkdir -p Builds/TesterBootstrap

cp -f Install_And_Play_SpookyMall.bat Builds/TesterBootstrap/Install_And_Play_SpookyMall.bat
cp -f icon.ico Builds/TesterBootstrap/icon.ico

if [[ -n "$TOKEN" ]]; then
    sed -i "s|^set \"GITHUB_READ_TOKEN=\"|set \"GITHUB_READ_TOKEN=${TOKEN}\"|" Builds/TesterBootstrap/Install_And_Play_SpookyMall.bat
    echo "[+] Embedded Read-Only token into Builds/TesterBootstrap/Install_And_Play_SpookyMall.bat"
else
    echo "[*] Packaged without embedded token (edit line 11 of Builds/TesterBootstrap/Install_And_Play_SpookyMall.bat or pass token as arg)."
fi

python3 - << 'PYEOF'
import zipfile
with zipfile.ZipFile("Builds/TesterBootstrap/SpookyMall_Tester_Installer.zip", "w", zipfile.ZIP_DEFLATED) as zf:
    zf.write("Builds/TesterBootstrap/Install_And_Play_SpookyMall.bat", "Install_And_Play_SpookyMall.bat")
    zf.write("Builds/TesterBootstrap/icon.ico", "icon.ico")
PYEOF

echo "[SUCCESS] Ready to send to friends on Discord:"
echo "  - Single file: Builds/TesterBootstrap/Install_And_Play_SpookyMall.bat"
echo "  - Or ZIP file: Builds/TesterBootstrap/SpookyMall_Tester_Installer.zip"
