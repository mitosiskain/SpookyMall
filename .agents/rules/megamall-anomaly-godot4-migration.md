---
trigger: always_on
description: Master context, asset catalog, and Unreal-to-Godot 4 migration blueprint for Megamall Anomaly (formerly MyProject2).
---

# Megamall Anomaly — Master Project Context & Godot 4 Migration Rule

Whenever working in this workspace, you have full access to the complete design bible, downloaded 3D assets (`.glb`, `.gltf`, `.fbx`, `.blend`), audio files, voice models, and reference C++ source code from the original Unreal Engine 5.8 prototype (`MyProject2`).

## 1. Strategic Direction (Decided 2026-10-27)
- **Target Engine:** **Godot 4 (Forward+ Vulkan Renderer)** running natively on **Arch Linux** (with full cross-platform compatibility for Windows/Linux co-op).
- **Why We Migrated from Unreal 5.8 to Godot 4:**
  - Plain-text scenes (`.tscn`), resources (`.tres`), and GDScript (`.gd`) allow the AI agent to generate and wire up entire levels, UI, physics, lighting, and AI without binary `.uasset` friction or heavy compile times.
  - **Visual Style:** Low-poly / stylized 3D using our existing `.glb` / `.gltf` / `.fbx` / `.blend` assets.
  - **Lighting Requirements:** Convincing atmospheric lighting that can be **turned on/off, dimmed (`100%` / `45%` / `OFF`), and flickered dynamically** by players and ghosts (using Godot 4 `OmniLight3D` / `SpotLight3D` + `VoxelGI` / `LightmapGI` mixed mode + `WorldEnvironment` Volumetric Fog & SSIL).
  - **Camera System:** Primarily **First-Person** with smooth `SpringArm3D` transitions to **Third-Person** for contextual actions (such as sitting on Concourse Rest Benches, soaking in Spa baths, or special interactions).
  - **Co-op Multiplayer & Physically-Based VoIP:**
    - Co-op multiplayer (`MultiplayerAPI` / `ENetMultiplayerPeer` / Steam).
    - **Physically-Based Proximity VoIP & Room Reverberation:** Each teammate's voice (`AudioStreamMicrophone` -> `AudioStreamPlayer3D`) is processed in the 3D physics environment:
      1. **Real-Time Physics Raycast Room Reverb:** Multi-directional raycasts measure local room volume (tight fitting room/closet vs. drifting concourse hallway vs. massive Oval Atrium) and drive dynamic `AudioEffectReverb` / `AudioEffectDelay`.
      2. **Wall & Door Occlusion + NavMesh Acoustic Pathing:** Physics raycasts between speaker and listener count intervening walls/closed shutters to apply `AudioEffectLowPassFilter` muffling, while `NavigationServer3D` path distances allow voices to travel naturally out open store entrances and down hallways.
      3. **AI Ghost Hearing & Voice Commands:** The AI Ghost hears players talking/shouting on mic in real time, and local offline speech recognition (`VoiceModels/Vosk` or `ThirdParty/Whisper`) detects spoken Spirit Box / Ghost prompts (*"Where are you?"*, *"Who are you?"*, *"How old are you?"*, *"Write in the book"*, *"Leave me alone"*).

---

## 2. Directory Map of This Workspace

### A. Design Documentation (`Docs/`)
Read these first before implementing any feature:
- `Docs/GameDesignBible.md` — **Master Source of Truth**: Core gameplay loop, 6 phases (Safe Hub -> Cart Logistics & Doorway Seal -> Investigation & Cash Economy -> 4-Phase Ghost Hunt Cycle -> Spa/Food Sanctuary Recovery -> Escalator Relay Fuse Progression), 3-Layer Story & Hidden Lore (Elena Vance, Arcadia Oval Megamall, Geodesic Ark Canopy), Store Archetypes, and Master Backlog.
- `Docs/GhostHuntingEquipmentAndContractModels.md` — Complete specifications for all **8 Investigation Tools** (`EMF Reader`, `UV Blacklight`, `UV Evidence Print`, `Thermal Scanner`, `Motion Sensor`, `Photo Camera`, `Spirit Box`, `Ghost Writing Book`), `Belt Flashlight`, `Light Remote Gun`, `Equipment Push Cart`, `Mobile Light Cart`, plus CC0 asset provenance and future 3D model specs.
- `Docs/MallAssetRegister.md` — Catalog of all mall architectural and retail prop assets.
- `Docs/SpiritBox.md` & `Docs/VoiceRecognition.md` — Spirit Box FM channels (`87.9`, `91.5`, `98.3`, `104.7 FM`), voice prompt phrases, and Vosk/Whisper integration details.
- `Docs/BookInteractions.md`, `Docs/Escalator.md`, `Docs/InteractionAuthoring.md`, `Docs/AdjustmentGuide.md`
- `Docs/MallAtriumTest.md`, `Docs/MallOvalTest.md`, `Docs/MallStoreChaseTest.md` — Layout dimensions and verification specs.

### B. Downloaded Free / CC0 3D Assets & Audio (`SourceAssets/` & `ExternalAssets/`)
All of these formats (`.glb`, `.gltf`, `.blend`, `.fbx`, `.wav`) import **natively into Godot 4**:
1. **`SourceAssets/Equipment/`**:
   - `PolyHavenCC0/`:
     - `retro_multimeter` (`.gltf` / `.fbx`) -> **EMF Reader**
     - `signal_flashlight` (`.gltf` / `.fbx`) -> **UV Blacklight**
     - `Camera_01` (`.gltf` / `.fbx`) -> **Evidence Photo Camera**
     - `portable_cassette_player` (`.gltf` / `.fbx`) -> **Spirit Box**
   - `Prepared/`:
     - `SM_EMFReader.fbx`, `SM_UVLight.fbx`, `SM_PhotoCamera.fbx`, `SM_SpiritBox_CC0.fbx`, `SM_Thermometer.fbx`, `SM_MotionSensor.fbx`, `SM_UVFootprint.fbx`
2. **`SourceAssets/Carts/`**:
   - `Prepared/EquipmentUtilityCart.blend` & `SM_EquipmentUtilityCart.fbx` -> **8-Slot Equipment Push Cart**
   - `WorkLamp.glb` / `SM_CartWorkLampFull.fbx` / `SM_CartWorkLampHead.fbx` -> **18,000 lm Mobile Light Cart**
3. **`SourceAssets/GhostSounds/`**:
   - Ghost audio assets, EVP responses, and preparation scripts (`prepare_ghost_sounds.py`).
4. **`ExternalAssets/Escalator/` & `ExternalAssets/Mall/Escalator450/` & `Escalator850/`**:
   - `concourse_escalator.glb`, `MallEscalator.blend`, `MallEscalator450.blend`, `MallEscalator850.blend`, shell & step meshes.
5. **`ExternalAssets/Mall/PolyHaven/`**:
   - `decorative_book_set_01` (`.blend` + `SM_BookHardcover02.fbx`, `SM_BookSoftcover04.fbx`) -> **Ghost Writing Books & Poltergeist Props**
   - `modular_street_seating` (`SM_MallBench01.fbx`) -> **3rd-Person Rest Benches**
   - `pachira_aquatica_01` & `potted_plant_01` -> **Atrium & Concourse Planters**
   - `steel_frame_shelves_01` (`SM_RetailShelves01.fbx`) -> **Store Shelving**
6. **`ExternalAssets/Mall/RetailKit/`**:
   - `GLB/` (`25159.glb` through `25413.glb` — **over 110 native `.glb` low/mid-poly retail, mannequin, fitting room, checkout, bench, food court, sauna, and concourse models!**)
   - `ImportReady/` — Named `.fbx` and `.blend` meshes (`SM_Retail_25257_FemaleMannequin`, `SM_Retail_25284_FittingCubicleClosed`, `SM_Bath_GlassSaunaPavilion`, `SM_Bath_SteamingOnsenPool`, `SM_Retail_25373_FoodCourtTableSet`, etc.).
7. **`ExternalAssets/Mall/Surfaces/`**:
   - PBR surface textures for mall floors, walls, tiles, and ceiling materials.
8. **`ExternalAssets/Mall/UserImports/`**:
   - `bistrot_table_and_chair.glb`, `railing.glb`, `restaurant_table_and_chairs.glb`, `MallBalconyModule_400.blend`, `MallBalconyStraight_400.blend`.

### C. Original C++ Reference Code (`UnrealSourceReference/MyProject2/`)
When porting any mechanic to Godot 4 GDScript, consult the exact formulas, distances, timers, and state machines already written and verified in `UnrealSourceReference/MyProject2/`:
- `Equipment/MallGhostEquipment.h/.cpp` — Exact detection radii, angles, photo star scoring (`$30–$100`), 4s flash stun, UV footprint reveal, and book writing progress.
- `Equipment/MallPushCart.h/.cpp`, `MallLightCart.h/.cpp`, `MallStoreCartDock.h/.cpp`, `MallCartCharger.h/.cpp` — Cart physics, 8-slot storage, doorway `Sealed` / `Unsealed` / `Dormant` states, and `18,000 lm` safety cone (`55°` half-angle, `6.5 m` length).
- `Ghost/MallGhostInteractionComponent.h/.cpp`, `MallGhostLightComponent.h/.cpp`, `MallGhostSoundComponent.h/.cpp`, `MallGhostStateTree.h/.cpp` — 4-phase ghost cycle (`Initial 30-45s` -> `VisibleWarning 3s` -> `VisibleHunt 20s` -> `Cooldown 40-60s`).
- `Environment/MallHidingSpot.h/.cpp`, `MallSpaAndFoodSystem.h/.cpp`, `MallStoreVendorNPC.h/.cpp`, `MallEscalatorFuse.h/.cpp` — Hiding spots with witness detection, Spa/Food buffs, Quartermaster Chen vendor, and Escalator Relay Fuse progression.
- `Environment/MallCurvedAtriumRing.h/.cpp`, `MallSplineCorridor.h/.cpp`, `MallRetailStoreUnit.h/.cpp` — Procedural Oval Atrium, drifting hallways, and 9 store archetypes.
- `Voice/MallVoiceSubsystem.h/.cpp`, `MallSpeechRecognizer.h/.cpp`, `MallMultiplayerSubsystem.h/.cpp` — Voice recognition & multiplayer architecture.

### D. Previous Antigravity Chat Logs & Artifacts (`AntigravityChatHistory/`)
- `AntigravityChatHistory/brain/` — Contains all prior conversation folders, `.md` artifacts (`task.md`, `implementation_plan.md`, `walkthrough.md`), and `.system_generated/logs/transcript.jsonl` files from the Windows sessions.
