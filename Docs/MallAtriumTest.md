# Mall atrium test map

Open `/Game/Environment/Mall/Maps/Lvl_MallAtrium_Test` in Unreal. This is a separate environment and movement test map using the existing third-person player.

The ground floor and second-floor gallery use 4 m clear shell height and a 0.5 m structural/services zone, giving 4.5 m floor spacing. The services space is reserved without modeled HVAC. The current roof and floor organization are intended to support later expansion to three to five floors.

This pass includes the supplied marble and glass/steel balcony appearance, open enterable retail shells, plants, seating, and the animated Hebe fountain. Merchants, NPCs, shopping gameplay, signage, and abandoned wings are deferred.

## Saved layout

The map is saved and stable with **325 authored actors**, organized under `Blockout_MallAtriumTest` in the Outliner. The nominal building footprint is **40 × 48 m**, surrounding a **16 × 32 m** atrium opening. Measured bounds including wall trim and props extend from `(-2015, -2420, -128)` to `(2015, 2420, 1038)` cm, giving a 40.3 × 48.4 m overall footprint; the bounds are not the clear room dimensions. The construction/layout checkpoint is backed up, the earlier background Aura run has stopped, and the mall packages are saved.

| Part | Saved layout |
| --- | --- |
| Ground walking surface | Z0 cm |
| Second-floor gallery walking surface | Z450 cm |
| Gallery slab underside | Z400 cm; 400 cm clear height below, then a 50 cm structural/services zone |
| Roof underside | Z850 cm; 400 cm above the second-floor walking surface |
| Atrium opening | X−800 to +800 cm; Y−1600 to +1600 cm |
| Open retail bays | 12 total: six per floor, with three on each east/west side |
| Gallery rail modules | 22 balcony modules, plus separate landing/vertical-access guards |
| Planting | Eight planting actors on the ground floor and eight on the gallery |
| Seating | Four ground-floor seating assemblies and two gallery assemblies |
| Fountain | One animated Hebe fountain in the atrium |
| Future floor walking heights | Z900, Z1350, and Z1800 cm for later third through fifth floors |

The roof/skylight assembly is grouped under `Blockout_MallAtriumTest/RemovableRoof/RoofSkylight` so it can be removed or lifted when extending the building. A reserved services zone does not imply that HVAC internals have been modeled.

The escalators rise 450 cm and point along world +Y at yaw90°. Their saved origins are **`(-100, 690.577, 6)` cm** for the upward ride and **`(100, 690.577, 6)` cm** for the reversed downward ride. The 6 cm lift provides a small floor-contact clearance margin; the existing escalator cutouts are retained. Clean PIE tests verify both rides and walking away from both landings; the character transitions between capsule-centre Z98.15/Z548.15 on the escalator and Z92.15/Z542.15 on the adjacent ground/gallery floors.

Source measurements and actor groups are recorded in `Saved/Aura/LDA/District/MallAtriumTest/layout.json` and `verification.json`; those construction records precede the final escalator-origin adjustment. Clean runtime measurements, nested action checks, frames, and timestamps are collected in `Saved/MallBuild/verification_summary.json`. The older interrupted verification and the initial downhill setup that snapped to the roof are excluded from acceptance evidence.

## Materials and saved assets

All **40 new mall mesh assets** have assigned materials in every slot, with no default WorldGrid assignments. Substrate TwoSidedFoliage uses one shared Pachira leaf parent and one potted-plant leaf instance, supporting the four Pachira variants and the potted plant. Numeric shading value `6` identifies the foliage shading enum, not a material count. The new mall Blueprints/materials and map are saved, with no dirty mall packages in the clean audit. Player-height appearance and overall scale remain for user review.

## Player scale test

1. Start PIE and judge the character against the storefronts and ceiling height.
2. Walk into the open store shells on both floors.
3. Ride both escalators, then walk away from each landing.
4. Walk around the upper gallery and inspect the balcony edges.
5. Check the fountain, plants, tables, and seating from player height.

Record whether the atrium footprint, gallery width, ceiling height, and furniture scale match the intended mall feel. Additional floors should wait until this scale is accepted.

## Validation

The stable saved scene has clean movement evidence for both escalators, all twelve store entrances, and a continuous circuit around the upper gallery. Every nested action passed: 13 uphill actions, 10 corrected downhill actions, 50 store-entry actions, 21 final gallery actions, 16 guard actions, and 5 fountain-capture actions (115 total). The player's measured MaxWalkSpeed is 200 cm/s.

| Clean test | Measured result | Status |
| --- | --- | --- |
| Upward transport | Capsule centre `(-100, 776.375, 98.15)` at t40.610 s → `(-100, 1685.000, 548.15)` at t63.797 s; no movement input during the ride | Passed: 450 cm rise, settled MOVE_Walking |
| Upward walk-off | 0.8 s forward input → `(-100, 1846.485, 542.15)` | Passed: exits onto gallery floor |
| Corrected downward transport | Capsule centre `(100, 1644.213, 548.15)` at t188.795 s → `(100, 735.577, 98.15)` at t211.899 s; setup uses snap_to_ground=false | Passed: 450 cm descent, settled MOVE_Walking |
| Downward walk-off | 0.8 s forward input → `(100, 577.224, 92.15)` | Passed: exits onto ground floor |
| Twelve retail entrances | Each approach uses 1.5 s forward input; all cross storefront X±1200 and end at X≈±1347–1353, with capsule Z92.15 or Z542.15 | Passed: all six ground and six gallery interiors entered |
| Continuous upper-gallery circuit after transition repair | t1.254 `(900, −1900, 542.15)` → t20.558 `(900, 1910.667, 542.15)` → t29.812 `(−908.962, 1910.667, 542.15)` → t49.140 `(−908.962, −1903.072, 542.15)` → t58.405 `(897.645, −1903.072, 542.15)` | Passed: Z542.15 throughout; MOVE_Walking; one initial placement, then yaw-only turns |

The final continuous circuit passes after restoring four missing 200 × 200 cm transition slabs to the existing upper-gallery floor actor at X±900, Y±1700, with top Z450 and underside Z400. Its DynamicMesh changed from 48 to 96 triangles and collision was refreshed; the map is saved and the actor count remains 325. Both earlier gallery attempts fell to the ground and are retained as excluded diagnostics in the verification summary. Balcony collision was also refreshed on the two new derivatives; original source geometry, UVs, and materials are unchanged.

Four sampled guard tests stop the player outside the atrium opening at X≈±847.20, south Y−1647.197, and north Y1647.195, remaining at capsule Z542.15. These test sampled locations rather than every rail join. The gallery and fountain images were reviewed: the supplied water/ripple appearance changes between live frames t114.015/t116.203 s, 2.188 s apart. Dark water/reflections and overall lighting quality remain for user appearance review; fountain audio was not listened to.

PIE stopped successfully at t146.885 s, with `is_pie_active=false`. The NVENC recording stopped with reason `pie_stopping` and is saved at `Saved/AuraVerify/rec_1791668561798270700_1/recording.h264`; its `recording_index.json` maps video time to PIE time. The editor is left on the new map overview. Final scene evidence in `Automation/Mall/final_scene_audit_result.json` confirms the 325 actors, 96-triangle repaired gallery floor, escalator origins Z6, and PlayerStart `(0, −2000, 100)`. The final saved-material audit finds no missing/WorldGrid assignments or dirty mall packages; 86 unrelated dirty packages were preserved. Automated movement/guard checks and agent visual verification are complete. User scale/appearance/audio acceptance remains pending.

The initial automatic navigation reachability probes failed; navmesh coverage and AI routes remain future requirements for NPCs and do not negate the measured player traversal passes.

## Asset intake and licensing

See [Mall asset register](MallAssetRegister.md) for source links, acquisition notes, imported assets, and remaining optional gaps. The supplied balcony is subject to its acquired Fab license. Fab bases Personal/Professional tier eligibility on revenue at acquisition and does not require upgrading an existing Personal acquisition solely because revenue later crosses the threshold. [Fab license summary](https://www.fab.com/eula).
