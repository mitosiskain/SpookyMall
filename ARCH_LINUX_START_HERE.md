# Megamall Anomaly — Arch Linux Handoff & Quickstart Guide

Everything from your Windows sessions—including all **12 Design Documents**, **110+ `.glb`/`.gltf`/`.fbx`/`.blend` 3D assets**, **Ghost Audio & Vosk/Whisper Voice Models**, **C++ Reference Code**, and **Antigravity Chat History**—has been packaged inside this folder (`E:\ai`).

---

## How to Open This Project in Antigravity on Arch Linux

### Step 1: Copy or Mount on Arch Linux
When you boot into Arch Linux, your `E:` drive (`junk drive but it's fast`) can be mounted directly, or you can copy the `ai` folder into your native Linux home directory for maximum SSD speed and clean Linux file permissions:

```bash
# Example once your E: drive is mounted (e.g., at /run/media/$USER/junk\ drive\ but\ it\'s\ fast):
cp -r "/run/media/$USER/junk drive but it's fast/ai" ~/megamall-anomaly
```
*(Or you can open the `ai` folder directly off the mounted drive!)*

### Step 2: Open the Folder as a Project in Antigravity
1. Launch **Antigravity** on Arch Linux.
2. Click **Projects** (or **Open Folder**) and select `~/megamall-anomaly` (or `/path/to/ai`).
3. Start a **New Conversation** inside that project.

### Step 3: Automatic AI Memory Restoration (`.agents/rules/`)
You do **not** need to re-explain the game!
Because this folder contains **`.agents/rules/megamall-anomaly-godot4-migration.md`**, Antigravity on Arch Linux **automatically loads our entire project context into the AI's memory** on every message:
- It knows the full **Megamall Anomaly** design (`Docs/GameDesignBible.md`).
- It knows we switched from Unreal Engine 5.8 to **Godot 4 (Vulkan Forward+)** on Arch Linux.
- It knows our exact new requirements:
  - **Low-Poly 3D + Toggleable/Flickering Lighting (`100%` / `45%` / `OFF`)**
  - **Co-op Multiplayer**
  - **First-Person View with Smooth Third-Person Camera Transitions** (sitting on rest benches, spa baths, etc.)
  - **Physically-Based Proximity VoIP & Room Reverberation** (real-time physics raycast room sizing, wall/door low-pass muffling, NavMesh hallway sound propagation, and AI ghosts that hear player mics)
- It knows where all your downloaded **CC0 3D models (`ExternalAssets/` & `SourceAssets/`)**, **Voice Models (`VoiceModels/Vosk/`)**, and **Original C++ Reference Files (`UnrealSourceReference/`)** live.

### Step 4: What to Say in Your First Arch Linux Chat
Once you open the project in Antigravity on Arch Linux, just type:
> *"We're on Arch Linux now! Check `.agents/rules/megamall-anomaly-godot4-migration.md` and `Docs/GameDesignBible.md`, and let's initialize the Godot 4 project right here."*

---

## Optional: Restoring Raw Windows Chat Logs into `~/.gemini/antigravity`
If you ever want to browse the raw Windows chat artifacts (`task.md`, `walkthrough.md`, `transcript.jsonl`) from earlier sessions, they are all backed up inside:
- `AntigravityChatHistory/brain/`
- `AntigravityChatHistory/conversations/`
