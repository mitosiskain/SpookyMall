# Mall oval test map

The October 13 construction revision of `/Game/Environment/Mall/Maps/Lvl_MallOval_Test` preserves the central oval and moves the surrounding stores and halls outward. Exactly four upper-floor bridges connect the oval ring to the outer hallways in a plus pattern: front, back, left, and right. Each has a **12 m clear span**. Rooms now have **8 m clear height**, with **0.5 m for structure and services**, giving **8.5 m floor-to-floor spacing**. Before furnishing, all 60 main walking legs and both corrected escalator rides and walk-offs passed. The user has accepted the current scale. The current furnished-store and confined-ghost pass is saved and tested, with PIE stopped; details are below.

The ground-floor central oval remains open with the Hebe fountain at its center. Four connected stores fill the second-floor oval. Side branches and the rear row contain enterable stores on both floors, including doorways between neighboring stores. The public hallways and ring retain open views between floors. The previous `/Game/Environment/Mall/Maps/Lvl_MallAtrium_Test` remains a separate comparison map.

## User scale acceptance

The user accepted the current scale with the feedback, "The scaling is fine." This covers 8 m clear rooms plus 0.5 m for structure and services (8.5 m floor spacing), the 34 x 26 m oval and 41 x 33 m ring, four 12 m clear-span bridges, and the expanded 119 x 85 m surround.

Scale acceptance does not imply final appearance or audio acceptance. Curve smoothing, visual and art polish, fountain audio, broader NPC and merchant gameplay, and future floors remain separate pending work. No additional construction is included in this acceptance record.

## Current furnishing and store ghost

All 32 stores now have sourced shelves, stocked displays, checkout counters, and connected routes. The middle east-side ground store, `East_F0_02`, contains one animated Manny placeholder labeled STORE GHOST. It pursues the player within that store at 180 cm/s, returns home after any of the three exits, and ignores the upper floor. On catch, only the ghost resets and waits about three seconds; player position and health remain unchanged. Player default speed remains 600 cm/s.

Fresh construction/path checks pass all 64 store loops and 66 doorway links for both capsule sizes. Actual player checks pass four sampled loops and all 16 legs, maximum endpoint error 2.78 cm. Ghost runtime checks pass pursuit, changing targets, all three exit transitions, catch/reset/cooldown, and confinement in 467 normal samples. The saved map has 729 actors, including one ghost; all original 146 poses and source meshes are preserved. The final audit confirms the map and both new ghost Blueprints are saved cleanly. See [Mall store chase test](MallStoreChaseTest.md) for finding the store, trying the loops and exits, and current evidence. The earlier checks below describe the scene before these furnishings.

Merchant/sales systems, broader NPC behavior, final chase balancing, visual/art/audio polish, curve smoothing, and later floors remain separate work.

## Current dimensions

| Element | October 13 construction |
| --- | --- |
| Oval store footprint | 34 × 26 m, preserved |
| Outer oval ring footprint | 41 × 33 m, preserved |
| Bridge connections | Exactly four, at front, back, left, and right |
| Bridge clear span | 12 m each |
| Bridge width | 4.5 m |
| Bridge mesh length | 14 m, including 1 m overlap into each landing |
| Surrounding mall extent | 119 × 85 m; X±5950 cm and Y−4750 to 3750 cm |
| Ground walking surface | Z0 cm |
| Second-floor walking surface | Z850 cm |
| Clear room height | 800 cm |
| Structure and services allowance | 50 cm |
| Roof underside | Z1650 cm |
| Future floor repeat interval | 850 cm |

The oval dimensions above are diameters, not radii. Expanding the outer mall supplies the full clear bridge spans while keeping the oval and its ring unchanged. The bridges are separate actors, so their endpoints and sizes can be adjusted independently. The curved edges remain the existing segmented blockout. The current dimensions are accepted; smoother curves remain pending.

## Stores and environment

The October 13 pre-furnishing construction audit recorded **146 total level actors, 135 generated owned actors, and 32 stores**: 14 on the ground floor and 18 on the second floor. These actor counts use different scopes and are not directly comparable to the earlier 145 authored-actor count.

| Area | Ground floor | Second floor |
| --- | --- | --- |
| Central oval | Open fountain space | Four connected stores and surrounding ring hallway |
| Side branches | Three stores on each side | Three stores on each side |
| Rear row | Eight stores | Eight stores |
| Public circulation | Expanded halls around the center and store branches | Oval ring, outer galleries, four cardinal bridges, and real openings between floors |

Direct store-to-store doorways allow passage between neighboring interiors without returning to the public hallway. The connectivity checkpoint retains 66 doorway links. Existing marble, glass and steel railing appearance, retail shell assets, foliage, seating, animated Hebe fountain, and matte plaster roof finish remain the environment basis. The player and flashlight setup remain unchanged. Merchant/sales systems, broader NPC behavior, signage, abandoned wings, and HVAC internals remain deferred. The single test-store ghost is covered above.

The new `BP_MallEscalator_850` and `BP_MallEscalator_850_Down` use a separate `SM_MallEscalatorShell_850` for the 850 cm rise. They are placed in the outer front hall at `(-2000, 3100, 0)` and `(-2000, 3450, 0)` cm, with scale `(1, 1, 1)`. The original 450 cm escalator assets remain unchanged. The 850 cm upper-repeat template is saved at `Saved/Aura/LDA/District/MallOvalTest/upper_repeat_template.json`. Additional floors are not present. The template uses walking heights Z1700, Z2550, and Z3400 cm, with alternating escalator orientation and a moving roof. It builds geometry, guards, stores, shell, and access only; each new floor also needs deterministic imported-art and lighting placement, collision/navigation review, and fresh native PIE acceptance. The existing retained-art relocation covers the two current floors only.

## Structural construction checks October 13 2026

The October 13 construction checkpoint reports a passing construction audit. These checks establish geometry, support, and sampled collision conditions; they are separate from actual player movement in PIE.

| Construction check | Result |
| --- | --- |
| Bridge count and dimensions | Four bridge actors; every clear span is 1200 cm, mesh length 1400 cm, and width 450 cm |
| Bridge landing support | Both ends supported, with 100 cm overlap at each end and no intermediate floor support inside the clear span |
| Doorway capsule sweeps | All 66 recorded links clear |
| Route floor support | All 18 recorded routes supported |
| Open floor voids | All eight probes clear |
| Height samples | All eight samples pass |
| Guard collision samples | All eight samples pass |
| Corrected upper escalator openings | 1206 opening probes, 24 landing-support samples at Z850, 30 clear guard-exit probes, and eight side-guard samples pass |

In that pre-furnishing revision, NPC navigation was not accepted: navigation was rebuilding and the preliminary cross-floor probe did not pass. Current store-local path and ghost results are recorded in the chase guide. The construction checks above remain separate from the actual player checks below.

Current construction evidence is `Saved/Aura/LDA/District/MallOvalTest/layout.json`, `connectivity_waypoints.json`, and `verification_summary.json`. The generator files include `mall_scale_layout.py`, `mall_scale_geometry.py`, `mall_scale_preserve.py`, `mall_scale_storefronts.py`, and `mall_scale_verify.py`. Earlier evidence below belongs to earlier geometry and must not supply coordinates or acceptance results for the current revision.

## Pre furnishing native PIE acceptance October 13 2026

Fresh actual-player checks pass all **60 continuous walking legs** on the current bridge, ring, outer-gallery, and central-shop routes. The small landing correction did not alter those main route surfaces.

| Route | Actual PIE result |
| --- | --- |
| Four cardinal bridges | All 12 legs pass, three per bridge; maximum endpoint error 4.71 cm |
| Upper oval ring | All 32 continuous legs pass; maximum endpoint error 13.31 cm |
| Outer-gallery loop | All four continuous legs pass; maximum endpoint error 1.67 cm |
| Two central-shop routes | All 12 corrected legs pass; maximum endpoint error 3.63 cm |
| Corrected 850 cm escalators | Both no-input rides, landing transitions, and walk-offs pass |

The upward ride boarded at capsule center `(-1815.06, 3100, 129.28)` cm, arrived without movement input at `(-312.76, 3100, 942.15)` cm, and walked off onto the upper gallery at `(-14.20, 3100, 942.15)` cm. The player then walked across the same upper gallery without a new location teleport to board the exposed downward flat at `(-350.02, 3450.05, 942.15)` cm. With no input during transport, the player reached the lower flat at `(-1879.94, 3450.05, 92.40)` cm, then walked onto the ground floor at `(-2179.06, 3450.05, 92.40)` cm.

The downward acceptance uses arrival on the lower flat at ground height plus an unobstructed walk-off. The first automated verdict unnecessarily required the exact stationary X coordinate −1955 cm; contact can transfer to the coplanar ground earlier within the lower flat. That strict verdict remains preserved in the raw evidence and is not represented as passing. `down_corrected_acceptance.json` records the actual functional criteria and passing observations.

Final native game views are lit and preserve visibility between floors: [upper gallery looking down at the fountain](C:/unreal/uprj/MyProject2/Saved/MallFourBridge850Verify/final_upper_view_down_t326.06s_f2570299.png) and [ground floor looking up](C:/unreal/uprj/MyProject2/Saved/MallFourBridge850Verify/final_ground_view_up_t326.99s_f2570335.png). Earlier black captures are excluded. Their cause was retained props and lights moving away during repeated rebuilds; restoring 114 poses from the immutable pre-build baseline corrected the views. Applying that placement twice produced identical poses within bounds.

Actual ride checks also exposed an upper-gallery overlap over the escalator's flat landing. Only the upper slab and guards were rebuilt to align the shaft edge at X−275 cm and derive the guard exits from that boundary. The approximately 5 m² of changed landing floor leaves all 60 main route surfaces unchanged. Corrected checks pass 1206 opening probes, 24 landing supports at Z850, 30 guard-exit probes, and eight side-guard samples. The rebuild left all 144 protected actors' transforms and metadata unchanged; retained poses and baseline hashes were preserved.

The final editor audit passes with **146 actors, four 12 m clear bridges, eight open floor-void probes, eight 800 cm room-height samples, and the fountain at the origin**. All 114 retained poses match their corrected checkpoint exactly, with no differences. No maps or content remain dirty. PIE stopped cleanly. The final film covers game time 0-330.592 seconds.

Current evidence is in `Saved/MallFourBridge850Verify`: `up_corrected_continuous.json`, `down_corrected_acceptance.json`, the preserved raw `down_corrected_continuous.json`, `shop_corrected_001.json`, `shop_corrected_002.json`, route-batch results, `retained_art_repair.json`, and `final_editor_audit.txt`. The final recording is `Saved/AuraVerify/rec_1791886005912422800_42/recording.h264`, with `recording_index.json`: 7827 frames and 260.9 seconds of playback mapped to game time 0–330.592 seconds. It covers the corrected rides and final views; the earlier walking results are separate action evidence. No Computer Use was used.

## Historical layouts and validation October 12 2026

Earlier versions used 4 m clear rooms and 4.5 m floor spacing. Their coordinates, bridge arrangement, movement results, and images are historical and do not validate the October 13 geometry.

Before opening the public hallways, actual PIE passed all 66 doorway links, a 17-leg upper ring, two west-to-east routes through the oval shops, the ground fountain bypass, and both escalator rides and walk-offs. That evidence is preserved in `Saved/MallRedesignVerify/portal_results.json` and `actual_movement_evidence.json`. The maximum doorway endpoint error was 21.03 cm.

The subsequent October 12 open-gallery revision passed 50 continuous segments across seven routes, 37 upper-floor doorway links, eight sampled guards, both rides and landing connections, seven sightline traces, and nine floor-void checks. Maximum route and doorway errors were 15.46 cm and 18.99 cm. The ground-floor links were not rerun in that revision. A numerical preservation audit found no changes across 139 unaffected actors and no dirty packages. Those results and captures are preserved in `Saved/MallOpenHallwaysVerify/actual_movement_evidence.json` and `final_scene_audit.json`.

That session stopped at 970.41 seconds. Its main film, `Saved/AuraVerify/rec_1791841952737914400_2/recording.h264`, ended at the 900-second game-time limit; separate readings and frames support the final descent. None of those earlier test results is reused as acceptance of the expanded map.

## Player inspection

1. Start PIE in `Lvl_MallOval_Test` and walk around the ground-floor fountain.
2. Enter the side and rear stores, and use the interior neighbor doorways.
3. Ride both 850 cm escalators and walk away from their landings.
4. Follow the upper oval ring and cross each of the four bridges: front, back, left, and right.
5. Look down through the open floor gaps and up from the ground hallways.
6. Inspect lighting, material finish, furnished shop details, and fountain audio separately from the accepted scale.

The current scale is accepted. Refining curved edges, visual and art polish, fountain audio, broader NPC and merchant gameplay, and adding the third through fifth floors remain separate pending work.

## Asset intake and licensing

See [Mall asset register](MallAssetRegister.md) for source links, imported assets, acquisition notes, and the earlier map history. The redesign reuses that asset basis without changing its licenses. Preserve the supplied Fab acquisition records and the register's existing licensing note.
