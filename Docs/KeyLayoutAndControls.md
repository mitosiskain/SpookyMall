# Megamall Anomaly — Generalized Key Layout & Control Scheme

This document serves as the single reference sheet for all keyboard and mouse bindings in **Megamall Anomaly (Godot 4)**. It is designed so you can quickly review every key's role and decide on any future binding adjustments.

---

## 1. Core Control Philosophy (Strict Separation of Duties)

To prevent key-overlap conflicts (such as accidentally activating an equipped gadget while trying to press an elevator button or push a cart), inputs are strictly divided into **6 distinct roles**:

| Input | Action ID | Strict Role | Summary |
| :--- | :--- | :--- | :--- |
| **`Left Mouse Button [LMB]`** | `primary_use` | **Use Equipped Item (Primary)** | Turns held electronics **ON / OFF** (including the **CCTV Camera Monitor** and **Video Camera**), snaps photos, fires beams, or places a held camera/canister. Also toggles power when aiming directly at a placed camera or mounted Cart CCTV Monitor. |
| **`Right Mouse Button [RMB]`** | `tool_secondary` | **Equipped Item Secondary / Alt Function** | **Cycles Camera Locations (`CAM 01 -> CAM 02...`)** on the **CCTV Camera Monitor** (in Hands or when aiming at the mounted Cart Monitor), toggles **Night-Vision ON/OFF** on a Video Camera (in hand or when aiming at a placed camera), switches **°C / °F** on Thermometer, cycles **FM Channel / Sweep** on Spirit Box & Walkie-Talkie, switches **22° / 40°** UV beam, or cycles **Pull/Push Tether** on Proton Gun. |
| **`[F]`** | `interact` | **Pick Up Item (Look-Target Prioritized)** | **Only picks up items** — including **picking up the CCTV Camera Monitor off the Push Cart into your Hands slot**, picking up tools from the Equipment Table or floor, picking up placed cameras/canisters/recorders/laser domes/wards/death crates, or taking a stored item off the Push Cart shelf. |
| **`[G]`** | `drop_item` | **Drop / Place / Mount Item** | **Drops, places, or mounts the held item** — **mounts the held CCTV Camera Monitor back onto the Push Cart's front mount** when near the cart, places tools onto the Push Cart shelf/hook, deploys a held Video Camera / Canister / Recorder / Laser Dome / Ward onto the floor, or drops the active item. |
| **`[E]`** | `secondary_action` | **World Interact (Non-Pickup Objects)** | **Reserved exclusively for world objects that cannot be picked up** — **never** intercepted by the Cart CCTV Monitor! Pressing **`[E]`** while pushing a cart **always** releases/parks the cart immediately, and pressing **`[E]`** near a cart grabs/pushes it. Also presses **Elevator `▲ UP` / `▼ DOWN` Buttons**, rotates a placed Video Camera (`+35°`), kicks open a deployed floor canister, sits on **Rest Benches / Sauna / Food Court**, or talks to **Quartermaster Chen**. |
| **`[B]`** | `push_to_talk` | **Press-to-Talk (Walkie-Talkie & Voice)** | Dedicated **3rd button** for transmitting over the **2-Way Walkie-Talkie** across the mall and activating local Proximity VoIP / Spirit Box microphone speech. |

---

## 2. Movement, Stance & Camera Keys

| Key / Button | Action ID | Function | Notes |
| :--- | :--- | :--- | :--- |
| **`[W] [A] [S] [D]`** | `move_forward`, `move_left`, `move_back`, `move_right` | Move Investigator | Also stands up automatically when seated on a Concourse Rest Bench or soaking in the Himalayan Sauna. |
| **`[Left Shift]`** | `sprint` | Sprint (`6.9 m/s`) | Automatically stands up from crouch. Disabled while pushing a cart or carrying a 2-handed heavy part. |
| **`[Space]`** | `jump` | Jump | Automatically stands up from crouch. |
| **`[C]`** *(Toggle)* / **`[Left Ctrl]`** *(Hold)* | `crouch` | **Crouch / Squat (`1.02m` Eye Height)** | Smoothly squats down (`0.65x` walk speed) so players can get eye-level and up close with the widescreen **Cart CCTV Display** or low shelves. |
| **`[T]`** | `toggle_flashlight` | Toggle Belt Flashlight ON / OFF | Always available regardless of which hand tool is equipped. |
| **`[V]`** or **`[F5]`** | `camera_perspective` | Toggle 1st-Person / 3rd-Person View | Locked to 3rd-Person Ectoplasm Form while dead until resurrected at the Himalayan Sauna. |

---

## 3. Hotbar & Inventory Keys

| Key / Button | Action ID | Function | Notes |
| :--- | :--- | :--- | :--- |
| **`[1]` `[2]` `[3]` `[4]`** | `hotbar_1` .. `hotbar_4` | Select Hotbar Slot `1–4` | If carrying a **Hands-Only** item (5th slot) or if all 4 slots are full, switching to `[1]–[4]` drops the Hands item to the floor. |
| **`Mouse Wheel Up / Down`** | *(Scroll)* | Cycle Hotbar Slots `1–4` | Cycles through the 4 primary tool slots. |
| **`[F]`** | `interact` | **Pick Up Item** | Picks up table/floor items, placed gadgets, Death Inventory Crates, or takes a highlighted stored item off the Push Cart shelf. |
| **`[G]`** | `drop_item` | **Drop / Place Held Item** | Places held item onto the Push Cart shelf/hook (if near cart), deploys placeable gear to the floor, or drops active tool. |

---

## 4. Equipped Gadget Matrix (`[LMB]` vs `[RMB]` vs `[B]`)

Every handheld gadget follows a consistent two-button mouse pattern (**`[LMB]` = Primary Use / Power**, **`[RMB]` = Secondary Mode / Alt Function**), with **`[B]`** as the third button for radio/voice transmission:

| Gadget / Item | `[LMB]` Primary Use | `[RMB]` Secondary / Alt Function | `[B]` Third Button | `[G]` Drop / Place / Mount |
| :--- | :--- | :--- | :--- | :--- |
| **Portable / Cart-Mountable CCTV Camera Monitor** *(Hands-Only Slot OR Mounted on Push Cart)* | **Turn Monitor Power ON / OFF** + log CCTV Ghost Orb / Shy Entity bounties | **Cycle Camera Locations (`CAM 01 -> CAM 02 -> CAM 03...`)** *(never rotates cameras)* | — | **Mount back onto the Push Cart's Front Display Mount** *(when near cart)*, or drop on floor |
| **Night-Vision Video Camera** *(or Assembled Tripod Rig)* | **Turn Power ON / OFF** *(displays live feed on the flip-out LCD screen on the side of the camera; or combines with a nearby Tripod Stand)* | **Toggle Night-Vision (`B&W Enhanced Light` / `Standard Color`)** *(emits zero light into the room; works in hand OR by right-clicking a placed camera!)* | — | **Place / Deploy Camera** on floor/stand (or onto Push Cart shelf/hook if at cart) |
| **Thermal Scanner (Thermometer)** | Turn Power **ON / OFF** *(chirps below `0°C` when ON)* | **Switch Celsius (`°C`) / Fahrenheit (`°F`)** | — | Place on Push Cart or drop on floor |
| **Spirit Box (FM Radio)** | Turn Power **ON / OFF** *(emits static when ON)* | **Cycle FM Channel / Sweep (`87.9 -> 91.5 -> 98.3 -> 104.7 FM`)** | Speak Voice Prompt | Place on Push Cart or drop on floor |
| **2-Way Walkie-Talkie** | Turn Power **ON / OFF** | **Cycle Radio Channel / Sweep (`CH-1 462.56 -> CH-2 462.61 -> CH-3 462.66 -> CH-4 Sweep`)** | **Press-to-Talk (`[B]`)** across the mall | Place on Push Cart or drop on floor |
| **EMF Reader (5-LED)** | Turn Power **ON / OFF** *(beeps when ON; silent when OFF)* | Calibrate / Test EMF Beep | — | Place on Push Cart or drop active on floor |
| **UV Blacklight** | Turn Power **ON / OFF** | **Switch `22°` Focused (`9.5m`) / `40°` Flood (`6.2m`) Beam** | — | Place on Push Cart or drop on floor |
| **Evidence Photo Camera** | **Snap Xenon Flash Photo** (`$30–$100` + `4.0s` Ghost Stun) | — | — | Place on Push Cart or drop on floor |
| **Light Remote Gun (`60m`)** | Toggle aimed ceiling light fixture (`100%` / `45%` / `OFF`) or Light Cart | Toggle aimed ceiling light fixture or Light Cart | — | Place on Push Cart or drop on floor |
| **Proton Ray Gun** | **Hold `[LMB]`**: Fire Proton Stream (damages, slows & tethers ghost) | **Cycle Wrangle Tether Distance (`3.2m Tether -> 1.8m Pull -> 5.0m Push`)** | — | Place on Push Cart or drop on floor |
| **Empty Ectoplasm Canister** | Place Canister on floor (`CLOSED` state) | **Cycle Capture Circle Radius (`1.5m -> 2.2m -> 3.2m`)** | — | Place Canister on floor (`CLOSED` state) |
| **Paranormal Audio Recorder** | Record High-Gain EVP sample | **Play Back EVP Tape (`Forward` / `Reversed` toggle)** | — | Place Recorder on floor to auto-record |
| **SLS Depth Camera** | Turn Power **ON / OFF** & scan stick-figure posture | Re-scan stick-figure posture | — | Place on Push Cart or drop on floor |
| **Parabolic Microphone (`30m`)** | Turn Power **ON / OFF** & scan directional dB | Scan directional dB | — | Place on Push Cart or drop on floor |
| **UV Chalk Spray** | Spray glowing UV wayfinding stencil | Cycle stencil symbol (`Arrow` / `Check` / `Skull`) | — | Place on Push Cart or drop on floor |
| **Death Inventory Crate** *(Hands)* | Open Crate & spill sealed items onto floor | Open Crate & spill sealed items onto floor | — | Drop sealed crate onto floor |

---

## 5. World Object Interactions (`[E]` Reserved for Non-Pickup Objects)

Because **`[E]`** never triggers equipped hand gadgets, you can safely press **`[E]`** while holding any tool:

| World Object / Station | `[E]` World Interact Action | Related Keys |
| :--- | :--- | :--- |
| **8-Slot Equipment Push Cart** | **Grab / Push / Release Cart** *(100% reliable — never blocked by the Cart CCTV Monitor!)* | **`[F]`** to pick up the **Cart CCTV Monitor** into your **Hands slot** (or take a stored shelf item); **`[G]`** to mount the **CCTV Monitor** back onto the cart (or place a tool on the shelf) |
| **Cart Front CCTV Monitor** *(when mounted at front of Push Cart)* | *(Does NOT use `[E]` so cart pushing is never blocked!)* | **`[F]`** to **Pick Up Monitor into Hands**; **`[G]`** near cart to **Mount Monitor back on Cart**; **`[LMB]`** for Power ON/OFF; **`[RMB]`** to Cycle Camera Locations (`CAM 01 -> CAM 02...`) |
| **18,000 lm Mobile Light Cart** | **Grab / Push / Release Cart** *(or look up at the floodlight lamp + **`[E]`** to toggle the `3.25m` Circle of Safety light ON/OFF)* | Can also toggle light from `60m` away using the **Light Remote Gun (`[LMB]`)** |
| **Service Freight Elevator** | **Press Left `▲ UP BUTTON` (`+1 Floor`) or Right `▼ DOWN BUTTON` (`-1 Floor`)**, or call elevator at floor pedestal | Optional express keys inside cab: `PageUp`/`Up` (`+1`), `PageDown`/`Down` (`-1`), `0`/`Home` (Floor 1) |
| **Deployed Ectoplasm Canister** *(on floor)* | **Kick & Open Canister** (`18s–40s` active suction vortex) | **`[F]`** to pick the canister back up (filled or empty) |
| **Placed Video Camera** *(on floor/tripod)* | **Rotate / Pan Camera (`+35°`)** *(done directly on the camera, never from the cart!)* | **`[LMB]`** on camera to turn **ON/OFF**; **`[RMB]`** on camera to toggle **B&W Night-Vision ON/OFF**; **`[F]`** to pick camera up |
| **Quartermaster Chen (Trader Shop)** | Open Trader Shop UI & automatically sell carried Filled Canisters | Keys `[1]–[9]`, `[0]` inside shop menu; `[Esc]` to close |
| **Concourse Rest Benches** | Sit down in 3rd-Person View (`+24 Stamina/s`, `+8 HP/s`) | Press `WASD` to stand up |
| **West Himalayan Sauna Biome** | Soak in 3rd-Person View (`+60s` Pink Salt Ward & lights Soul-Anchor) or **Resurrect** from Ectoplasm Form | Press `WASD` to stand up |
| **North Night-Market Food Court** | Sit & eat selected meal (`5.0s` seated meal bar -> `90s` buff) | Press `WASD` after meal finishes to stand up |
| **Fitting Rooms & Hiding Spots** | Enter / Exit Hiding Spot (`Undetected` if no ghost within `5.0m` has LoS) | Press `WASD` to exit |
| **Escalator Breaker Dais** | Insert carried Relay Fuse Crate or toggle Escalator (`Motorized` / `Stalled Walkable`) | — |

---

## 6. Menus, Journal & Multiplayer Keys

| Key | Action ID | Function |
| :--- | :--- | :--- |
| **`[Tab]`** | `toggle_journal` | Open / Close Investigator Case File, 6-Store Evidence Ledger & Lore Journal |
| **`[J]`** | `toggle_voice_menu` | Open / Close On-Screen Voice Command Menu (`[1]–[7]` spoken Spirit Box phrases) |
| **`[P]`** | `toggle_coop_menu` | Open / Close Co-op Multiplayer Lobby, Invite Code & UPnP Menu |
| **`[F8]`** | `coop_host_quick` | Quick-Host Co-op Server (`UDP 7777`) |
| **`[F9]`** | `coop_join_quick` | Quick-Join Local Co-op Server (`127.0.0.1:7777`) |
| **`[Esc]`** | `ui_cancel` | Close active modal menu or toggle mouse cursor capture |
