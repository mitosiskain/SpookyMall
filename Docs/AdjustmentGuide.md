# Human adjustment guide

This is the central checklist for settings that need playtesting and art placement. Unreal uses **centimetres** for distances, **centimetres/second** for speed, and **seconds** for durations. Change one group at a time, test in PIE, then stop PIE and save the change in the editor. Changes made to runtime actors during PIE disappear when PIE ends.

## Where to change values

For one placed object, select it in the **World Outliner** and change its **Details** properties. For all future instances of a cart or tool, open its Blueprint, choose **Class Defaults**, change the value, **Compile**, and **Save**. A placed actor with an instance override keeps that override even if the Blueprint default changes; use the property's reset arrow to inherit the default again.

Native class fields appear under **Mall** in Details. Native code does not need recompiling for ordinary editable values. Changing native classes, replicated fields, or RPC definitions does require a native build with the editor closed. New meshes, material assignments, sounds, and Blueprint defaults can be edited with Unreal open.

## Equipment cart and doorway rules

The equipment cart carries the existing item actors. A stored book retains its writing. Stored evidence is inactive: the book closes and cannot receive writing, and the spirit box switches off. **F while carrying a book or radio and aiming at the cart now stores it on the rack**, just like T, rather than leaving it at a fixed world position. F still places evidence normally on floors and tables. Retrieve the item to use its normal **F** pickup/placement and **R** inspection/power controls again.

The prototype cart controls are **G** to push/release, **T** to transfer equipment while aiming at a parked cart, and **H** while aiming at a light cart to use its power/charger action. Ordinary T retrieval takes the last occupied rack slot that the player can accept, skipping spare flashlights if the player already owns one; choosing individual slots through a polished inventory UI remains future work. A native flashlight on the floor uses **T** for pickup and the existing hotbar/flashlight controls afterwards. Book and radio floor pickup remains **F**. Near an outlet, aim at the light cart and press H; aiming only at the outlet does not operate the current cart endpoint.

T first stores a held book or spirit box, then a flashlight in the selected hotbar slot. To retrieve evidence while you already own a flashlight, select an **empty hotbar slot** first (for example **2**), then aim at the cart and press T. Otherwise T deposits the selected flashlight. Use **E** to toggle the selected flashlight, and **R** to open a retrieved book or power a retrieved spirit box.

If a cart is deleted during play, the book and radio recover to their last validated placement rather than disappearing or resetting their evidence. This is a development recovery policy, not a finished destruction/drop animation.

The doorway dock identifies one ghost store by its actor tag, such as `East_F0_02`:

- **Across the doorway and released:** the cart establishes the store seal. Ghost investigation inside the store remains possible.
- **Being pushed, off the doorway, or incorrectly aligned:** the seal is open. An active ghost can pursue players outside its store on the same floor.
- **Fully inside its assigned store:** the equipment cart makes the store dormant. The ghost is invisible and harmless, and does not provide writing, spirit-box answers, disturbances, or hunts. Taking the cart out restores the normal initial activity grace period.
- The light cart provides its own powered safety area. It does not replace the doorway equipment cart or make a store dormant.

The seal is a gameplay boundary for the assigned store, including its connecting passages. The cart is also a physical obstacle. Door widths, adjoining store passages, and a player's route after parking need a human layout pass; do not leave a team trapped behind its own cart.

In the saved test map, the equipment cart starts across the east-middle ground-floor store entrance at **(3950, 0, 45) cm**, with its long axis along the doorway. Matching glazed infill narrows the opening to about **1.4 m**. The light cart starts nearby at **(3560, −400, 45)**. Two native flashlights are on the low blue supply tray at **(3610, −840)**. The wall charger is at **(3934, −650, 95)**, and the prototype powered extension endpoint is at **(3550, −1150, 20)**. The existing book and radio remain on their stand inside the store.

The ghost keeps its existing `StoreId:East_F0_02` tag and also has the matching `East_F0_02` alias plus the opt-in `MallCartBoundary` tag. Copy all required settings when authoring another store. Existing ghosts without the opt-in retain the earlier confinement behavior.

| Setting | Starting value | How to tune |
| --- | --- | --- |
| Cart **Push Speed** | 200 cm/s (2 m/s) | Select the equipment or light cart, search **Push Speed**, and enter a new value. The current character's normal walking speed is 600 cm/s. Try 150–250 for a deliberate pushing pace. |
| **Turn Speed Degrees** | 95 degrees/s | Lower values make steering heavier. Check tight store corners before increasing it. |
| **Storage Capacity** | 8 on equipment cart; 0 on light cart | Maximum prototype capacity is eight. Do not increase it beyond eight without adding slot transforms and testing collision/spacing. |
| Dock **Store Tag** | `East_F0_02` | Must match the ghost's store tag. A missing match does not create a seal. |
| Dock **Assigned Cart** | Test equipment cart | Use an explicit assignment when several carts or stores exist. |
| Dock parking/facing/threshold tolerances | See placed dock Details | Tighten only after testing approach and steering from both directions. A cart merely near the entrance must not count as blocking it. |
| **Unsealed Roam Distance Cm** | 10000 cm (100 m) | Limits the prototype's same-floor pursuit when the doorway is unsealed. Navigation and physical obstructions still apply. |

**Changing the player's walking speed:** open `BP_ThirdPersonCharacter`, select **CharacterMovement**, and change **Max Walk Speed**. Stop pushing before comparing speeds. Pushing temporarily limits the current movement speed and restores the previous setting on release. The ghost's speed is separately configured in `BP_StoreGhost` → **CharacterMovement** → **Max Walk Speed**; its previous prototype value is 180 cm/s. Increasing ghost speed can make the light and door timing much less forgiving.

The cart is steered through the existing character movement controls. It has one pusher at a time, collision checks, ground support checks, and an owner-controlled server request. Stand behind the rear handle (the cart's local −X side), about 65–175 cm behind its pivot and within 85 cm sideways, to begin pushing. Aim selection is forgiving around the cart body and handle, but walls still block interaction.

When released, the cart is an upright physics body: walking or running into it can move and turn it, allowing recovery from an awkward doorway position. G temporarily switches to the controlled pushing mode; releasing G restores physics. The cart must settle before it accepts equipment, seals a doorway or charges. Small resting jitter is filtered so the seal does not flicker. A moving cart is not a secure doorway barrier. Ghosts ignore the cart's physical body, while the existing gameplay boundary still confines them when the doorway is properly sealed.

Physics settings are under **Mall → Cart → Physics**:

| Setting | Starting value | How to tune |
| --- | --- | --- |
| **Enable Free Physics** | Enabled | Allows bumps while the cart is released. Disable for a fixed/controlled-only prototype. |
| **Cart Mass Kg** | 100 | Higher mass resists incidental impacts. Test walking and running against each side. |
| **Free Linear Damping** | 3 | Higher values stop rolling sooner. |
| **Free Angular Damping** | 5 | Higher values stop turning sooner. |
| **Max Bump Speed Cm Per Second** | 180 | Limits free-cart speed without changing the player's other physics interactions. |
| **Max Bump Angular Speed Degrees** | 80 | Limits spinning after an off-centre impact. |
| **Settle Duration Seconds** | 0.6 | Time at rest before storage, doorway protection and charging become available. |

The project reserves the **MallCart** collision object channel for cart bodies. Players and the world block it; opted-in store ghosts ignore it on server and clients. Preserve this channel when merging project settings. Do not make carts ignore the general Pawn channel, because that also removes player contact.

The cart does not yet animate gripping hands or rotating caster wheels. Controlled G pushing stops at unsupported drop-offs and steep height changes. A released physics cart can be knocked toward an edge, so keep guardrails and test drops; cart escalator transport is not implemented. Test backwards movement, stairs, ledges, latency, and narrow doors before relying on those situations in a finished level.

## Safety light and charging

| Light-cart setting | Starting value | Effect |
| --- | --- | --- |
| **Runtime Seconds** | 900 | Fifteen minutes of powered use from a full battery. For quick testing, temporarily use 30. |
| **Recharge Seconds** | 300 | Five minutes from empty to full. Partial charging is retained. For a quick test, temporarily use 10. |
| **Battery Fraction** | 1.0 | Initial charge: 1 is full, 0.5 is half, 0 is empty. Runtime authority is on the server. |
| **Safety While Pushing** | Enabled | The light protects while moving. Disable to require parking. |
| **Lamp Height Cm** | 320 above the cart actor pivot | With the pivot 45 cm above the floor, the lamp is about 3.65 m above the floor. Check overhead clearance and the pusher's head. |
| **Cone Half Angle Degrees** | 55 | Half the spotlight's total cone angle. Widening it grows the refuge; the player's whole collision capsule must fit. |
| **Cone Length Cm** | 650 | Spherical spotlight attenuation distance, not just height above the floor. Walls and floors block protection. |
| **Lamp Intensity Lumens** | 18000 | Visual brightness. Zero intensity disables protection. Raising intensity does not enlarge the safety geometry. |
| Charger **Connection Range Cm** | 250 | Maximum clear cable reach. Increase for a deliberately placed powered extension endpoint. |
| Charger **Powered** | Enabled | An unpowered outlet cannot charge. |
| Charger **Extension Lead** | Disabled on wall outlet | Labels an endpoint as an extension supply. This is a placed powered endpoint, not a simulated electrical grid. |

Charging requires a parked cart, a powered nearby charger, and an unobstructed cable route. The safety light switches off while charging. Starting to push disconnects it. An empty battery offers no protection. Do not assume a bright floor patch means the head of a player at the edge is also protected: test the complete character, especially beside shelves, under balconies, or on a different floor.

Human tuning is needed for the cone's coverage, visible light falloff, whether light should work while pushing, battery warning presentation, the cost of shadowed mobile spotlights, cable placement, and recharge availability. Keep gameplay protection and the visible spotlight aligned; do not independently rotate or resize only the rendered light component.

## Existing ghost timing and store lights

Select `StoreGhost_East_F0_02`, then its **MallGhostInteraction** component. Under **Mall → Ghost → Hunt Cycle**, the initial harmless activity is **30–45 seconds**, warning **3 seconds**, hunt **20 seconds**, and cooldown **40–60 seconds**. These are starting values. Adjust **Initial Activity Min/Max Seconds**, **Warning Duration Seconds**, **Hunt Duration Seconds**, and **Cooldown Min/Max Seconds** together. Give new visitors time to set equipment down before the warning. Cooldown is distinct from the older Blueprint's short catch/reset delay.

Select its **MallGhostLights** component to adjust warning/hunt flicker. The current warning minimum is **0.1** (10% of each light's original brightness), chase minimum **0.0** (complete outages), and flicker rate **2.3** brightness steps/s. The explicit light-group tag is `GhostFlicker_East_F0_02`. Group only lights intended to flicker; lights return to their cached original brightness during cooldown. Test accessibility and readability before increasing flicker frequency.

Local store lights now cast shadows. Exposure is currently fixed; stronger global exposure or skylight changes can weaken apparent darkness. Final light placement, light leakage at unfinished walls, shadow cost, and the contrast between the lively mall and abandoned areas remain an art/lighting pass. Keep the already approved light values as a baseline.

## Voice, radio recordings, ghost sounds, and book artwork

- **Microphone:** press **F6** in PIE for the gain/AGC controls. Manual boost applies only when automatic gain is off. Use [VoiceRecognition.md](VoiceRecognition.md) for diagnostics, model details, and supported phrases. Hardware microphone quality and recognition accuracy still need real-player tests.
- **Spirit-box replies and static:** open `BP_MallSpiritBox` → **Class Defaults**. Change the **Responses** entry's **Sound** and **Subtitle**, keeping `where_are_you`, `who_are_you`, and `how_old_are_you` unchanged. Change **Scan Sound** for the static bed. Mono reply WAVs are non-looping; the static should loop cleanly. Import and replacement instructions, including free static sources, are in [SpiritBox.md](SpiritBox.md).
- **Physical ghost sounds:** use the ghost's **MallGhostSounds** component. Invisible ambience and chase recordings come from the ghost itself; spirit-box replies come from the radio. Tune intervals, volume and attenuation by listening from neighboring aisles and floors. Environmental reverb/room acoustics remain a later map pass.
- **Book writing:** use `BP_GhostWritingBook` defaults or the placed book's **Writing Texture**, **Page Material**, **Writing Duration**, and **Test Writing**. The material uses `WritingTexture` and `WritingProgress`. See [BookInteractions.md](BookInteractions.md). Replacing art should not reset runtime evidence. Writing currently persists for the actor's session, not across restarting PIE or loading a saved game.

## Mall scale, placement, and future assets

The accepted scale is **8 m clear height plus 0.5 m structure** between floors: floor surfaces are **8.5 m** apart. Keep the central oval size and the four cardinal bridges at the accepted **12 m** spans. More floors, smooth final curves, polished storefronts, signs and merchants are later work. See [MallOvalTest.md](MallOvalTest.md) and [MallAssetRegister.md](MallAssetRegister.md).

The imported cart is 121.1 × 66 × 110.1 cm with its mesh pivot on the ground. The gameplay cart pivot is 45 cm above the floor, so its model needs a **relative Z of −45 cm**. Shelf top heights are 25, 58 and 92 cm. Changing the mesh or its scale requires moving storage slots, collision, handle reach and the doorway marker together. Avoid scaling the whole actor to change steering or storage capacity.

The mall navigation mesh uses **Dynamic Modifiers Only**. Each cart's **CartBounds** component has **Dynamic Obstacle** enabled and **Area Class Override** set to **NavArea_Null**, with **Use System Default Obstacle Area Class** disabled. Preserve these settings so a moving cart updates the blocked navigation area. Replacement decorative meshes should not independently contribute navigation geometry. The light housing's **Cast Shadow** is disabled because it encloses the emitter; enabling it can block the lamp's own beam.

Asset sources, license files, editable cart source, model dimensions, and hashes are in `SourceAssets/Carts/README.txt` and `provenance.json`. The cart geometry is project-original. The work-lamp head is CC0 from Grab3D. Outlet/extension art starts as original prototype geometry. The previously acquired balcony railing's free license has a revenue condition; keep the existing note to obtain the Pro license if game revenue exceeds $100,000.

The flashlight's new native actor is the authority for ownership and shared storage, while its adapter updates the existing Blueprint hotbar and hand presentation. Other older consumable inventory remains unchanged. Future networked inventory items should use an authoritative ownership contract rather than copying the old local-only pickup behavior. New portable actor types can implement the `MallCartItem` interface and preserve their own state through the same storage contract.

## Suggested human playtest order

1. Push and steer both carts at the default speed, release them, and confirm normal walking returns.
2. Deposit and retrieve the flashlight, spirit box, and a book that already contains writing. Confirm the writing survives and normal tool controls return.
3. Park the equipment cart across the entrance. Check the sealed status; investigate the ghost inside. Move the cart away and verify an active hunt can pursue outside.
4. Move the equipment cart fully inside the store. Confirm dormant status, no hunt, no writing, no radio answer; remove it and verify the initial activity grace returns.
5. Stand under the powered light during a hunt, then cross its edge. Repeat beside a wall and on the upper floor to check that protection does not pass through architecture.
6. With temporary 30-second runtime and 10-second recharge, test empty battery, partial charge, full charge and moving away mid-charge. Restore 900 and 300 before saving the normal defaults.
7. Repeat with two players using a listen server. One pushes; the other watches motion, stored tools, seal state, battery and ghost behavior. Real microphone/proximity-audio testing remains a human check.

## Initial cart verification checkpoint — October 18, 2026

The final native build succeeded and all **13 cart, ghost and voice-network regression tests passed** after a cold start. The new cases cover remote flashlight equip and subsequent hotbar changes, evidence retrieval beneath a spare flashlight, written-book/radio state preservation, and crouched targets inside and outside the safety cone. The test fixtures emit engine warnings about multiple PIE worlds and a transient collision component; there are no test errors. The compact report is `Automation/Carts/Evidence/native_final_report.json`.

Two-player live PIE also passed actual push/release and wall blocking, replicated item transfers, doorway sealing, hallway pursuit when unsealed, inside-store dormancy, moving light protection, and charging/disconnection. See `Automation/Carts/Evidence/live_acceptance.json`. The live charging check used a temporary 10-second recharge; the saved map retains 900-second runtime and 300-second recharge. Final art, hand/caster animations, cart escalator transport, long-session balance and microphone/audio listening remain separate checks.

## Physics and placement correction — October 18, 2026

The full native build and **15 regression tests passed** after adding free-cart physics and F-to-rack placement. Tests exercise actual autonomous player input and server movement collision, upright constraints, broader targeting with wall checks, physical settling, doorway/charging interruption, and attachment/state preservation for all three tools through movement, rotation and late joining. Fixture warnings about temporary floor components and multiple test worlds remain; there are no test errors. See `Automation/Carts/Evidence/physics_native_report.json`.

Live saved-map checks passed the host bumping the doorway cart about 2.7 m out of the way, actual F book/radio storage, T flashlight storage, controlled pushing and 15-degree steering, then a roughly 2.2 m bump with the loaded cart. All three items retained their rack positions and rotations on the host and observing client. PIE ended with the saved map unchanged and no dirty packages. Details and the reproduced original hovering defect are in `Automation/Carts/Evidence/physics_live_acceptance.json` and `placement_follow_baseline.json`.

The automated second PIE client in that run stayed in **Spectating**, with no acknowledged pawn or pawn input component. Its actor replication worked, but its live movement check is not accepted. Remote-player cart contact is covered by the passing native autonomous-client test; saved-map multiplayer input startup needs separate investigation if this occurs during manual testing. No unrelated startup behavior was changed for this cart correction.
