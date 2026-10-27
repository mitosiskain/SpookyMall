# Ghost-Hunting Equipment & 3D Model Contracting Guide

This document covers:
1. **Usable Player Equipment & Ghost Detection Systems** implemented in C++ and Blueprints under `/Game/Equipment/...`.
2. **Sourced Free CC0 3D Assets** downloaded and prepared in `SourceAssets/Equipment/`.
3. **Contract Out 3D Model List & Artist Specifications** for custom/bespoke models and tiered upgrades you can hand directly to a freelance 3D artist.

---

## 1. Usable Player Equipment & Detection Systems

All handheld equipment shares a unified server-authoritative interaction and 4-slot hotbar contract (`BP_InventoryComponent` via [`MallHotbarInventory.h`](../Source/MyProject2/Equipment/MallHotbarInventory.h), [`UMallBookInteractionComponent`](../Source/MyProject2/Ghost/MallBookInteractionComponent.h), [`UMallCartInteractionComponent`](../Source/MyProject2/Equipment/MallCartInteractionComponent.h), and [`AMallHandheldItem`](../Source/MyProject2/Equipment/MallGhostEquipment.h)).

### Unified Player Controls

| Input | Action |
| --- | --- |
| **F** | **Primary Interact:** Pick up aimed world item into a free hotbar slot, place the currently selected held item onto a level floor/tabletop, store/retrieve on a parked `AMallPushCart`, or push/park a cart from its rear handle |
| **Left Mouse Button (LMB)** or **R** | **Primary Use:** Toggle device power (`ON`/`OFF`), open/close the Ghost Writing Book, or fire the Photo Camera shutter + xenon flash |
| **E** | **Secondary Use / Mode Toggle:** Cycle device mode (EMF audio mute, UV Focused/Flood beam, Thermometer `°C`/`°F` units, Motion Sensor Chime/Silent mode, Photo Camera log review, Spirit Box frequency channel), toggle a Light Cart's floodlight, or transfer items with an Equipment Cart |
| **G** | **Drop / Release:** Drop the currently selected hotbar item onto the floor in front of the player (or release a pushed cart) |
| **1 / 2 / 3 / 4** or **Mouse Wheel** | **Hotbar Slot Selection:** Switch active held tool across the 4-slot hotbar |
| **T** | **Belt Flashlight Toggle:** Toggle the carried flashlight (`AMallCartFlashlight`) on/off even while holding another evidence tool in the active hand |
| **V (Hold)** | **Push-to-Talk Voice:** Speak questions (`Where are you?`, `Who are you?`, `How old are you?`, `Write in the book`) into the Spirit Box or Haunted Store |

### Equipment Roster & Ghost Detection Mechanics

| Item | Blueprint Path | Native C++ Class | Ghost Detection System & Player Utility |
| --- | --- | --- | --- |
| **EMF Reader** | `/Game/Equipment/EMFReader/BP_MallEMFReader` | `AMallEMFReader` | **5-Level Electromagnetic Field Detector:**<br>• **Level 1 (Green):** Baseline when powered on.<br>• **Level 2 (Yellow-Green):** Within `1200 cm` of an active ghost or inside its haunted store.<br>• **Level 3 (Yellow):** Within `650 cm` of the ghost, within `10s` of a ghost disturbance, or near a disturbed prop / written book / active spirit box.<br>• **Level 4 (Orange):** Ghost within `360 cm` or manifesting in `VisibleWarning` / `VisibleHunt`.<br>• **Level 5 (Red — EMF-5 Evidence):** Ghost with EMF-5 capability within `260 cm` or during a close disturbance; latches `bEMF5Detected = true`. Scales audio beep rate with level (`E` mutes audio). |
| **UV Blacklight** | `/Game/Equipment/UVLight/BP_MallUVLight` | `AMallUVLight` | **Ultraviolet Evidence Spotlight:**<br>• Shines a deep-violet UV cone (`E` toggles **Focused 22° / 9.5 m** vs. **Wide Flood 40° / 6.2 m**).<br>• Reveals invisible `AMallUVEvidence` footprints and handprints via line-of-sight cone checks (`bIlluminatedByUV = true`, glowing fluorescent green with `2.5s` phosphorescent afterglow).<br>• Automatically spawns dynamic `AMallUVEvidence` footprints/handprints when an active store ghost disturbs props or moves near a powered UV scan. |
| **UV Evidence Print** | `/Game/Equipment/UVLight/BP_MallUVEvidence` | `AMallUVEvidence` | **Hidden Footprint / Handprint Actor:**<br>• Completely hidden under normal lighting (`EvidenceMesh->bHiddenInGame = true`).<br>• Glows bright green and increments the player's discovered print counter when hit by any active `AMallUVLight` beam (held or placed). |
| **Thermal Scanner** | `/Game/Equipment/Thermometer/BP_MallThermometer` | `AMallThermometer` | **Digital IR & Ambient Cold-Spot Scanner:**<br>• Reads `~20.5°C` (`68.9°F`) in normal mall corridors.<br>• Drops to `~5.5°C` (`41.9°F`) inside an active haunted store or when aiming the IR ray (`850 cm`) into the haunted room.<br>• Plunges below `0.0°C` down to `-7.8°C` (`18.0°F`) when within `420 cm` of a freezing ghost or aimed within `24°` of the entity, latching **Freezing Temperatures** (`bFreezingDetected = true`). `E` toggles `°C` / `°F`. |
| **Motion Sensor** | `/Game/Equipment/MotionSensor/BP_MallMotionSensor` | `AMallMotionSensor` | **Infrared Presence & Crossing Sensor:**<br>• Can be carried or placed (`F`) on floors/tables facing a store aisle (`480 cm` forward cone + `200 cm` proximity).<br>• Detects `BP_StoreGhost` **even while the ghost is invisible (`EMallGhostMode::InvisibleDisturbance`)**, firing a bright cyan/green strobe (`StatusLight`), playing an alert chime (`E` toggles Silent Strobe mode), and incrementing `TriggerCount`. |
| **Photo Camera** | `/Game/Equipment/PhotoCamera/BP_MallPhotoCamera` | `AMallPhotoCamera` | **Paranormal Evidence Camera:**<br>• `LMB` / `R` fires a high-intensity xenon flash (`XenonFlashLight`) and evaluates line-of-sight targets inside a `35°` half-angle frustum (`1200 cm` range, 10 exposures).<br>• **5 Stars (`$100`):** Manifested Store Ghost (`VisibleWarning` / `VisibleHunt`) — also pauses ghost disturbances for `4.0s` when photographed within `6.5 m`!<br>• **4 Stars (`$65` / `$60`):** UV-Illuminated Footprint/Handprint, EMF-5 Reading, or close Invisible Ghost Ectoplasmic Distortion.<br>• **3 Stars (`$50` / `$40`):** Written Ghost Writing Book or EMF Level 4 spike.<br>• **2 Stars (`$30`):** DisturbedPoltergeist Prop (`AMallGhostDisturbanceProp`). |
| **Spirit Box** | `/Game/Equipment/SpiritBox/BP_MallSpiritBox` | `AMallSpiritBox` | **EVP Radio Scanner:**<br>• `LMB` / `R` toggles power; `E` cycles frequency channels (`87.9 FM`, `91.5 FM`, `98.3 FM`, `104.7 FM`).<br>• Responds to spoken voice questions (`V`) inside the ghost's store when tuned to the ghost's required channel. |
| **Ghost Writing Book** | `/Game/Equipment/GhostBook/BP_MallGhostWritingBook` | `AMallGhostWritingBook` | **Placeable Evidence Book:**<br>• `LMB` / `R` opens/closes held inspection view; `F` places open on a floor/counter.<br>• Ghost writes progressive ink across the page over `4.0s` when prompted or disturbed. |
| **Flashlight** | `/Game/Equipment/Flashlight/BP_MallCartFlashlight` | `AMallCartFlashlight` | **Personal Belt / Hand Torch:**<br>• Picked up with `F`; toggled at any time with `T` even while another hotbar slot (`1–4`) is selected. |

---

## 2. Sourced Free CC0 3D Assets

The following CC0 1.0 Public Domain 3D models were sourced from Poly Haven (`https://polyhaven.com`) and OpenGameArt/Wikimedia, normalized in Blender (`SourceAssets/Equipment/prepare_equipment_models.py`) to Unreal Engine centimeter scale (`+X` forward, `+Z` up, bottom-center pivot at `Z = 0`), and imported into `/Game/Equipment/...`:

| Equipment Item | Imported Static Mesh | Source Asset & URL | License | Dimensions (cm) | Triangles |
| --- | --- | --- | --- | --- | --- |
| **EMF Reader** | `/Game/Equipment/EMFReader/Meshes/SM_EMFReader` | Poly Haven `retro_multimeter` (`https://polyhaven.com/a/retro_multimeter`) | CC0 1.0 Public Domain | `16.0 × 10.0 × 6.5` | 8,948 |
| **UV Blacklight** | `/Game/Equipment/UVLight/Meshes/SM_UVLight` | Poly Haven `signal_flashlight` (`https://polyhaven.com/a/signal_flashlight`) | CC0 1.0 Public Domain | `20.0 × 6.5 × 6.5` | 9,730 |
| **Photo Camera** | `/Game/Equipment/PhotoCamera/Meshes/SM_PhotoCamera` | Poly Haven `Camera_01` (`https://polyhaven.com/a/Camera_01`) | CC0 1.0 Public Domain | `14.5 × 13.5 × 10.5` | 26,987 |
| **Spirit Box (CC0 Option)** | `/Game/Equipment/SpiritBox/Meshes/SM_SpiritBox_CC0` | Poly Haven `portable_cassette_player` (`https://polyhaven.com/a/portable_cassette_player`) | CC0 1.0 Public Domain | `22.0 × 14.0 × 9.5` | 13,307 |
| **Flashlight** | `/Game/Equipment/Flashlight/Meshes/small_plastic_torch` | `Small plastic torch.stl` (`SourceAssets/Flashlight/`) | CC0 1.0 Public Domain | `16.0 × 4.5 × 4.5` | 3,420 |
| **Thermal Scanner** | `/Game/Equipment/Thermometer/Meshes/SM_Thermometer` | Project-original Blender mesh (`prepare_equipment_models.py`) | Project Original | `16.5 × 6.2 × 13.5` | 692 |
| **Motion Sensor** | `/Game/Equipment/MotionSensor/Meshes/SM_MotionSensor` | Project-original Blender mesh (`prepare_equipment_models.py`) | Project Original | `12.5 × 19.5 × 14.5` | 476 |
| **UV Footprint** | `/Game/Equipment/UVLight/Meshes/SM_UVFootprint` | Project-original Blender mesh (`prepare_equipment_models.py`) | Project Original | `24.0 × 14.5 × 0.4` | 288 |
| **Equipment Cart** | `/Game/Equipment/Carts/SM_EquipmentUtilityCart` | Project-original Blender mesh (`create_utility_cart.py`) | Project Original | `116.1 × 65.4 × 110.1` | 4,052 |

Full provenance records and SHA256 checksums are stored in:
- [`SourceAssets/Equipment/PolyHavenCC0/provenance.json`](../SourceAssets/Equipment/PolyHavenCC0/provenance.json)
- [`SourceAssets/Equipment/Prepared/metadata.json`](../SourceAssets/Equipment/Prepared/metadata.json)

---

## 3. Needed 3D Models to Contract Out (Outsourcing Specification Sheet)

While the sourced CC0 and procedural models make every item immediately playable and recognizable in PIE, contracting out custom 3D models will give the game a cohesive visual identity, support first-person/third-person hand animations, and provide Tier 1 / Tier 2 / Tier 3 upgrade progression.

### Global Technical Standards for Contracted 3D Artists

- **Target Engine:** Unreal Engine 5.8 (`1 unit = 1 cm`).
- **Coordinate System:** `+X` Forward (lens/beam/barrel pointing along `+X`), `+Y` Right, `+Z` Up.
- **Pivot Convention:**
  - Placeable / tabletop items: Pivot at **bottom-center (`Z = 0`)** so surface placement traces sit flush on tables, floors, and cart shelves.
  - Include a named socket `GripSocket` positioned at the right-hand palm grip location (`hand_r` attachment).
- **File Format:** `.fbx` (2020 binary) + source `.blend` file.
- **Textures:** 2048×2048 (2K) 8-bit PNG PBR Metal/Roughness workflow:
  - `T_<Asset>_D` (BaseColor, sRGB)
  - `T_<Asset>_N` (Normal Map, DirectX `-Y` green channel, Linear)
  - `T_<Asset>_ORM` (Packed Linear: `R` = Ambient Occlusion, `G` = Roughness, `B` = Metallic)
  - `T_<Asset>_E` (Emissive Mask, sRGB — for LEDs, backlit screens, and indicators)

### Priority 1: Bespoke Ghost-Hunting Equipment Models (Drop-In Replacements)

| # | Asset Name to Contract | Replaces / Upgrades | Target Dimensions (X × Y × Z cm) | Max Triangles | Required Material Slots / Sub-Elements | Required Bones / Sockets |
| --- | --- | --- | --- | --- | --- | --- |
| **1** | **`SK_GhostWritingBook`** (Skeletal Mesh) | `AMallGhostWritingBook` primitive book parts | Closed: `21 × 30 × 3.5`<br>Open: `42 × 30 × 2.8` | 3,500 | 1. `M_BookCover` (leather cover & spine)<br>2. `M_BookPage_Left` (`0..1` UV mapped flat page)<br>3. `M_BookPage_Right` (`0..1` UV mapped flat page for progressive `WritingProgress` ink reveal) | `root`, `spine`, `cover_left`, `cover_right`, `page_turn_01`, `GripSocket`. Include `Anim_Book_Open`, `Anim_Book_Close`, `Anim_Book_IdleOpen`. |
| **2** | **`SM_EMFReader_K2`** (Bespoke 5-LED Meter) | `SM_EMFReader` (`BP_MallEMFReader`) | `17 × 8.5 × 4.5` | 4,500 | 1. `M_EMF_Body` (molded ABS plastic + rubber grip + label decals)<br>2. `M_EMF_LED_1` through `M_EMF_LED_5` (or 5 separate LED dome submeshes so `LEDMeshes[0..4]` can toggle emissive per level) | `GripSocket` (thumb over power button), `ScreenSocket` (for `ReadoutText`). |
| **3** | **`SM_Thermometer_IRGun`** (Bespoke IR Scanner) | `SM_Thermometer` (`BP_MallThermometer`) | `16.5 × 6.0 × 14.5` | 4,000 | 1. `M_Thermo_Body` (industrial yellow/black housing & trigger)<br>2. `M_Thermo_Screen` (dedicated flat rear LCD quad facing `-X` with clean `0..1` UVs for digital readout) | `GripSocket`, `LaserMuzzleSocket` (`+X` IR barrel tip), `ScreenSocket` (`-X` rear LCD). |
| **4** | **`SM_MotionSensor_Tripod`** (Bespoke IR Sensor) | `SM_MotionSensor` (`BP_MallMotionSensor`) | `14 × 20 × 16` (tabletop)<br>Optional extendable legs | 4,500 | 1. `M_Motion_Body` (matte housing + folding tripod legs)<br>2. `M_Motion_Fresnel` (translucent white IR sensor window)<br>3. `M_Motion_Strobe` (top emissive cyan/green alert dome) | `GripSocket`, `BeamOriginSocket` (`+X` center of Fresnel lens), `StrobeSocket`. |
| **5** | **`SM_SpiritBox_PSB`** (Bespoke Sweep Radio) | `AMallSpiritBox` / `SM_SpiritBox_CC0` | `20 × 12 × 28` (including whip antenna) | 5,000 | 1. `M_SpiritBox_Body` (rugged radio casing, speaker grille, knobs)<br>2. `M_SpiritBox_LCD` (orange/green backlit frequency display window)<br>3. `M_SpiritBox_LED` (power/response LED) | `GripSocket`, `AntennaTipSocket`, `SpeakerSocket`. |
| **6** | **`SM_UVLight_Tactical`** (Bespoke UV Blacklight) | `SM_UVLight` (`BP_MallUVLight`) | `21 × 6.5 × 6.5` | 4,000 | 1. `M_UV_Body` (anodized dark-violet/black aluminum barrel + knurled grip)<br>2. `M_UV_Lens` (deep violet multi-LED reflector & front glass) | `GripSocket`, `LensSocket` (`+X` spotlight origin). |
| **7** | **`SM_PhotoCamera_Evidence`** (Bespoke Camera) | `SM_PhotoCamera` (`BP_MallPhotoCamera`) | `15 × 13.5 × 10.5` | 6,500 | 1. `M_Camera_Body` (body, dials, viewfinder)<br>2. `M_Camera_Lens` (coated glass objective)<br>3. `M_Camera_Flash` (xenon flash diffuser)<br>4. `M_Camera_LCD` (rear preview screen quad facing `-X`) | `GripSocket` (right hand grip), `SupportHandSocket` (left hand under lens), `LensSocket` (`+X`), `FlashSocket` (`+X`). |
| **8** | **`SM_UVPrints_DecalPack`** (Handprints & Footprints) | `SM_UVFootprint` (`BP_MallUVEvidence`) | Footprints: `26 × 12 × 0.2`<br>Handprints: `18 × 16 × 0.2` | 500 each | 1. `M_UV_Fluorescent` (alpha-masked texture atlas with left/right boot treads, bare footprints, 5-finger handprints, and claw drag marks) | Bottom-center pivot (`Z = 0`). |

### Priority 2: Planned Combat, Extraction & Utility Gear (Mall Expansion Plan)

| # | Asset Name to Contract | Planned System Role | Target Dimensions (X × Y × Z cm) | Max Triangles | Required Material Slots & Details | Required Bones / Sockets |
| --- | --- | --- | --- | --- | --- | --- |
| **9** | **`SM_ProtonRayGun_T1` / `T2` / `T3`** + **`SM_ProtonPack`** | Parody Proton Ray Gun (`AMallProtonRayGun`) for trapping/weakening store ghosts & stunning hallway gremlins | Wand: `68 × 18 × 22`<br>Backpack: `38 × 16 × 52` | Wand: 8,000<br>Pack: 10,000 | • **Tier 1 (Mall Scrap):** Duct-taped vacuum/microwave cyclotron.<br>• **Tier 2 (Industrial Arc):** Heavy copper coils & analog heat gauge.<br>• **Tier 3 (Quantum Collider):** Sleek chrome/neon containment emitters.<br>Separate `M_Proton_EmissiveVents` for overheat glow. | `Grip_R_Socket` (rear trigger handle), `Grip_L_Socket` (foregrip), `MuzzleSocket` (`+X` beam origin), `CableAttachSocket` (rear of wand & bottom of pack). |
| **10** | **`SK_EctoplasmCanister`** | Ghost Trap / Ectoplasm Extraction Canister (`AMallEctoplasmCanister`) | `32 × 16 × 14` | 5,500 | 1. `M_Canister_Body` (hazard-striped metal trap box)<br>2. `M_Canister_Glass` (side ectoplasm fill-level tube)<br>3. `M_Canister_Pedal` (activation foot switch & indicator LED) | `root`, `door_left`, `door_right` (hinged top trap doors that open `90°` when active), `TrapVortexSocket` (`+Z` center). |
| **11** | **`SM_MallLightCart_Generator`** | Bespoke mesh for `BP_MallLightCart` (`AMallLightCart`) + `BP_MallCartCharger` | Cart: `110 × 72 × 145`<br>Charger: `36 × 22 × 48` | Cart: 9,500<br>Charger: 3,000 | 1. `M_LightCart_Frame` (tubular steel frame, casters, battery housing)<br>2. `M_LightCart_Floodlight` (telescoping mast + dual halogen/LED floodlight heads)<br>3. `M_LightCart_BatteryBar` (4-segment charge bar)<br>4. `M_WallCharger` (industrial yellow 220V wall charger box) | `HandleSocket` (`-X` rear push bar), `FloodlightSocket` (`+X` top mast), `ChargerPlugSocket`. |

### Priority 3: Haunted Store & Mall Entities

| # | Asset Name to Contract | Planned System Role | Target Dimensions | Max Triangles | Required Material Slots & Rigging |
| --- | --- | --- | --- | --- | --- |
| **12** | **`SK_StoreGhost_Clerk` & `SK_StoreGhost_Mannequin`** | Replaces Manny placeholder in `BP_StoreGhost` | `180 cm` tall humanoid | 18,000–25,000 | Rigged to **UE5 Mannequin (`SK_Mannequin`)** skeleton so existing locomotion and chase animations retarget cleanly. Include translucent/dissolve-ready PBR materials with emissive eyes/veins for `VisibleWarning` and `VisibleHunt` phases. |
| **13** | **`SK_HallwayGremlin`** | Fast mall concourse nuisance enemy (`BP_HallwayGremlin`) | `95 cm` tall hunched creature | 12,000 | Custom or UE5-compatible rig with `Idle`, `Scamper_Run`, `Swipe_Attack`, `Flash_Stunned` (triggered by Photo Camera flash or UV beam), and `Flee` animations. |
