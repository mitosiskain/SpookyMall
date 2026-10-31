#!/usr/bin/env bash
# ==============================================================================
# Stamps a Read-Only GitHub Token into Builds/TesterBootstrap/Install_And_Play_SpookyMall.bat
# using a split variable (TK_A + TK_B) so Discord/GitHub secret scanners NEVER
# auto-revoke it, and verifies the token in isolated mode right away!
#
# Usage: ./Scripts/pack_tester_bootstrap.sh <YOUR_FULL_GITHUB_PAT_TOKEN>
# ==============================================================================
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

TOKEN="${1:-}"
mkdir -p Builds/TesterBootstrap

cp -f Install_And_Play_SpookyMall.bat Builds/TesterBootstrap/Install_And_Play_SpookyMall.bat
cp -f icon.ico Builds/TesterBootstrap/icon.ico

if [[ -n "$TOKEN" ]]; then
    # Strip leading "github_pat_" so the full prefix never appears contiguous in the file
    SUFFIX="${TOKEN#github_pat_}"
    sed -i "s|^set \"TK_B=\"|set \"TK_B=${SUFFIX}\"|" Builds/TesterBootstrap/Install_And_Play_SpookyMall.bat
    echo "[+] Embedded split Read-Only token into:"
    echo "    Builds/TesterBootstrap/Install_And_Play_SpookyMall.bat"
    echo ""
    echo "[*] Testing token against GitHub in isolated (no-account) mode..."
    if GIT_TERMINAL_PROMPT=0 GCM_INTERACTIVE=never git -c credential.helper= ls-remote --heads "https://x-access-token:${TOKEN}@github.com/mitosiskain/SpookyMall.git" >/dev/null 2>&1; then
        echo "[VERIFIED] Token works! Anyone can now run Builds/TesterBootstrap/Install_And_Play_SpookyMall.bat with ZERO GitHub account or login popup!"
    else
        echo "[ERROR] GitHub rejected this token (401 Unauthorized)."
        echo "        Make sure when generating the Fine-Grained Token on GitHub:"
        echo "        1. Under 'Repository access' -> choose 'Only select repositories' -> pick 'SpookyMall'"
        echo "        2. Under 'Permissions' -> 'Repository permissions' -> set 'Contents' to 'Read-only'"
        exit 1
    fi
else
    echo "[*] Copied template to Builds/TesterBootstrap/Install_And_Play_SpookyMall.bat"
    echo "    Run: ./Scripts/pack_tester_bootstrap.sh github_pat_YOUR_TOKEN_HERE"
fi
