#!/usr/bin/env bash
# Megamall Anomaly — Local 2-Player Co-op Split-Screen Test Launcher (Arch Linux / Godot 4)
# Launches two side-by-side 960x1040 Godot 4 windows on the same PC:
#   - Left Window:  Investigator #1 (ENet Co-op Host on UDP port 7777)
#   - Right Window: Investigator #2 (ENet Co-op Client auto-joining 127.0.0.1:7777)

set -e
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_DIR"

PORT="${1:-7777}"

echo "=================================================================="
echo " MEGAMALL ANOMALY — LAUNCHING 2-PLAYER LOCAL CO-OP TEST (UDP :$PORT)"
echo " • Window 1 (Left):  HOST (Investigator #1)"
echo " • Window 2 (Right): CLIENT (Investigator #2 -> 127.0.0.1:$PORT)"
echo " • Tip: Press [Esc] to unlock mouse when switching between windows!"
echo "=================================================================="

cleanup() {
    echo "Shutting down Co-op test instances..."
    kill "$HOST_PID" "$CLIENT_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# 1. Launch Host Window on the Left (960x1040 at 0,20)
godot --path "$PROJECT_DIR" --position 0,20 --resolution 960x1040 -- --host --port="$PORT" &
HOST_PID=$!

# 2. Launch Client Window on the Right (960x1040 at 960,20)
godot --path "$PROJECT_DIR" --position 960,20 --resolution 960x1040 -- --join=127.0.0.1 --port="$PORT" &
CLIENT_PID=$!

wait "$HOST_PID" "$CLIENT_PID"
