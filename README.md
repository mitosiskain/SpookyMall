# Megamall Anomaly (Godot 4.7 Vulkan)

**Co-op Liminal Paranormal Investigation, Survival Economy & Vertical Escape in the Arcadia Oval Megamall.**

---

## 1. Quickstart for Testers & Developers

### Requirements
- **Engine:** **Godot 4.3+ / 4.7** (Forward+ Vulkan Renderer, Jolt Physics)
- **OS:** Arch Linux / Linux (`x86_64`) or Windows 10/11 (`x86_64`)

### Clone & Run
```bash
git clone https://github.com/mitosiskain/SpookyMall.git
cd SpookyMall

# (Optional) Download the 466 MB Whisper speech model (SHA-1 verified from HuggingFace):
# Linux / macOS / Git Bash:
./Scripts/fetch_whisper_model.sh
# Windows PowerShell:
# powershell -ExecutionPolicy Bypass -File Automation/Voice/prepare_whisper.ps1

# Launch in Godot 4:
godot --path .
```
*(Note: The lightweight offline **Vosk** model in `VoiceModels/Vosk/vosk-model-small-en-us-0.15` and the `[J]` On-Screen Voice Command Menu work out-of-the-box without downloading Whisper.)*

---

## 2. Co-op Multiplayer Testing (`ENetMultiplayerPeer`, Up to 4 Investigators)

### Option A: 1-Command Split-Screen on a Single PC (Linux)
Launch two side-by-side `960x1040` instances (Left = Host Investigator #1, Right = Client Investigator #2 auto-joining `127.0.0.1:7777`):
```bash
./Scripts/run_coop_test.sh
```
> **Tip:** Press **`[Esc]`** to unlock your mouse cursor when switching between the two windows, then left-click inside the target window to capture the mouse.

### Option B: Across 2–4 PCs (LAN, Tailscale, ZeroTier, or Port-Forwarded UDP `7777`)
1. **Host Player:** Press **`[F8]`** in-game (or press **`[P]`** -> **Host Co-op Server on UDP Port 7777**).
2. **Client Players:** Press **`[P]`** to open the **Co-op Multiplayer Lobby**, enter the Host's LAN or Tailscale IP address (`127.0.0.1` for local), and click **Join Co-op Server** (or press **`[F9]`**).
3. You can also launch directly from the command line:
   ```bash
   # Host:
   godot --path . -- --host --port=7777

   # Client:
   godot --path . -- --join=<HOST_IP> --port=7777
   ```

---

## 3. Controls & Equipment Guide

| Key / Input | Action |
| :--- | :--- |
| **`WASD`** | Move (Walk / Climb Platform Stairs & Escalators) |
| **`Shift`** | Sprint (Disabled while pushing a cart or carrying a 2-handed heavy assembly) |
| **`Space`** | Jump |
| **`[1]` – `[4]` / Mouse Wheel** | Select **4-Slot Hotbar** Tool (`1: EMF Reader`, `2: Thermal Scanner`, `3: Spirit Box`, `4: UV Blacklight`) |
| **`Left Mouse Button` / `[R]`** | **Primary Action:** Toggle Active Gadget Power **ON / OFF** (or fire **Photo Camera** / **Light Remote** / **Proton Stream**) |
| **`[E]`** | **Secondary Action:** Toggle Wheeled Light Cart Floodlight ON/OFF / Take Item off Push Cart / Cycle Tool Mode (`°C/°F`, FM Channel, UV Beam Angle, Deploy Canister Trap) |
| **`[G]`** | **Place / Drop:** Place held item onto highlighted **8-Slot Equipment Push Cart** shelf outline, or drop/throw active item onto the floor |
| **`[F]`** | **Contextual Interact:** Push/Park Carts, Pick Up Floor Items / Death Inventory Crates, Sit at Rest Benches / Food Court / Pink Salt Spa, Use Vendor / Escalator Breaker / Store Ledger |
| **`[T]`** | Toggle Belt Flashlight ON / OFF |
| **`[J]`** | Open **On-Screen Voice Command Menu** (Speak Spirit Box & Ritual Prompts `[1]–[7]`) |
| **`[V]` (Hold)** | Push-to-Talk (3D Proximity VoIP with Real-Time Raycast Room Reverb & Wall Occlusion) |
| **`[B]` / `[F5]`** | Toggle 1st-Person / 3rd-Person Camera View (Locked to 3rd-Person Ectoplasm Form while dead) |
| **`[Tab]`** | Open Investigator Case File, Evidence Ledger & Hidden Lore Journal |
| **`[P]` / `[F8]` / `[F9]`** | Co-op Multiplayer Menu (`[P]`), Quick-Host (`[F8]`), Quick-Join (`[F9]`) |

---

## 4. Core Mechanics & Progression Summary

- **4-Slot Hotbar + "Hands" (Pseudo-5th Slot):**
  - Investigators carry up to 4 standard tools in slots `[1]–[4]`.
  - Picking up a 5th item when slots `[1]–[4]` are full—or picking up a **Hands-Only** item (**Death Inventory Crate**, **Heavy Hydraulic Assembly**, **Escalator Relay Fuse Crate**)—holds it in **Hands**. Switching back to `[1]–[4]` without an empty hotbar slot automatically drops the Hands item onto the floor.
- **Active Electronic Device Noise:**
  - Leaving an **EMF Reader**, **Spirit Box**, or **Thermal Scanner** switched **ON** emits electronic noise—even while stowed on your hotbar or thrown onto the floor! Turn gadgets **OFF (`[LMB]`)** before hiding in a Fitting Room Booth so hunting ghosts cannot track your electronics.
- **Cart Logistics & Circle of Safety:**
  - **8-Slot Equipment Push Cart:** Racks up to 8 tools on physical shelves (`[G]` to place on the green wireframe outline, `[E]` to take) and seals store doorways (`SEALED` / `DORMANT`).
  - **Wheeled Mobile Light Cart:** Projects a downward **3.25m Circle of Safety** (`[E]` to toggle ON/OFF). Store ghosts cannot enter the light circle or cross its perimeter edge—use the circle edge to block upper-floor ghosts from leaving their store!
- **Death Inventory Crate & Sauna Resurrection:**
  - Dying seals all carried items inside a **Death Inventory Crate** at your death site (unusable until opened; carried in Hands `[F]`; opening `[LMB/E]` spills items onto the floor) and locks your camera to **3rd-Person Ectoplasm Form** until you resurrect at the **South Tangquan Pink Salt Bathhouse & Sauna Shrine**.
- **Two-Stage Vertical Progression (Floor 1 -> Floor 2):**
  - **Stage 6A:** Purchase the **Escalator Relay Fuse Crate** (`$60` at Quartermaster Chen) and install it at the **Escalator Breaker Dais** to ride the motorized escalators up to Floor 2 on foot (stepped escalators cannot carry carts!).
  - **Stage 6B:** Haul the two-handed **Heavy Hydraulic Assembly** on Floor 2 to the **Service Freight Elevator Shaft** to bring your Equipment Push Cart and Mobile Light Cart up to Floor 2.

---

## 5. Repository Structure
- `Docs/` — Master Design Bible ([`GameDesignBible.md`](Docs/GameDesignBible.md)), Equipment Specs, Spirit Box & Voice Recognition Docs.
- `godot/` — Godot 4 GDScript architecture (`autoload/`, `player/`, `equipment/`, `ghost/`, `environment/`, `lighting/`).
- `scenes/main/megamall_main.tscn` — Main playable Megamall scene.
- `SourceAssets/` & `ExternalAssets/` — Normalized 1:1 meter low-poly characters, carts, equipment meshes, retail kit props, and audio.
- `Scripts/` — Co-op test launcher ([`run_coop_test.sh`](Scripts/run_coop_test.sh)), Whisper model fetcher ([`fetch_whisper_model.sh`](Scripts/fetch_whisper_model.sh)), and asset pipeline scripts.
