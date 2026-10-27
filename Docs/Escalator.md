# Mall escalator

## Status

The CC0 source model has been repaired, imported, and configured in `/Game/Environment/Escalator/BP_MallEscalator`. A saved up/down pair and shared landings are in `/Game/ThirdPerson/Lvl_ThirdPerson`, under the **Mall Escalator Demo** Outliner folder. The uphill origin is `(1100, 550, 6)` and the downhill origin is `(1100, 800, 6)`; local +X points uphill. The shared upper platform is at Z=366 cm.

PIE checks passed for automatic uphill/downhill rides, both landing transitions, stable exit stopping, walking off both ends, jumping, and stopping/restarting while occupied. The recorded uphill rise was 360 cm. Tests used the existing `BP_ThirdPersonCharacter`; no player or animation Blueprint changes were made. After testing, the complete demo was translated 250 cm sideways to clear the existing evidence cube.

The regular MyProject2Editor Win64 Development build succeeded on October 10, 2026; its log is `Saved/Logs/EscalatorPersistentBuild.log`. After restarting Unreal, the native class, reusable Blueprint, both escalators, and all nine landing/support actors loaded at their final saved positions. A fresh PIE check confirmed automatic uphill and downhill character transport across the flat-to-incline transitions at those positions. Unreal was left open with PIE stopped and the viewport showing the pair, ready for a manual play test.

## Import and place

1. Build the project for Unreal Engine 5.8 so the **Mall Escalator** actor class is available. Create a Blueprint derived from it if reusable project defaults are desired.
2. Import `ExternalAssets/Escalator/SM_MallEscalatorShell.fbx` and `SM_MallEscalatorStep.fbx` as separate static meshes: import scale **1**, **Convert Scene** enabled, **Force Front X Axis** disabled. Meshes use centimeters. Source/license details and exact geometry are recorded beside the FBXs in `LICENSE_AND_SOURCE.txt` and `mesh_metadata.json`.
3. Assign the meshes to **Shell Mesh Asset** and **Step Mesh Asset**. Keep **Shell Mesh Transform** at identity and turn **Show End Plates** off; the prepared shell already has the fixed 45 cm end plates. Preserve the mesh material slots, or assign project materials to them.
4. Place the actor origin on the lower entrance's walking floor. Local **+X** leads uphill; local **+Y** crosses the tread. Keep actor scale at **1, 1, 1**. Place the upper floor at the lower floor height plus **360 cm**, with access beyond the upper entrance. Provide an opening for the escalator rather than letting a floor collision cover its ramp.

The prepared footprint is approximately **883.54 × 136 cm**. Its walking route is 106 cm wide; the incline begins at local `(130, 0, 0)` and ends at `(753.54, 0, 360)`. The overall shell extends below the lower floor and reaches about 475 cm high, so check floor openings and head clearance.

Shell material slots: metal, glass, dark, safety yellow. Step slots: tread dark, tread metal, safety yellow. The FBX files contain no gameplay collision or animation; the actor supplies these.

## Settings

| Property | Default | Meaning |
| --- | ---: | --- |
| Run / Rise | 623.54 / 360 cm | Incline horizontal distance and vertical rise; about 30 degrees. |
| Landing Length | 130 cm | Each flat approach/departure section. |
| Belt Width | 106 cm | Tread and walking-deck width. |
| Bottom Deck Height | 0 cm | Walking-surface height relative to the actor origin. |
| Step Spacing | 40 cm | Requested distance along the route between tread centers. |
| Step Depth | 40 cm | Maximum tread depth; automatically fitted below the actual loop pitch. |
| Step Height | 0 cm | Zero derives riser height from pitch and incline; a positive value overrides it. |
| Stationary End Length | 45 cm | Fixed entry/exit sections; must match the shell's plates. |
| Travel Speed | 50 cm/s | Belt speed along the route. |
| Running | True | Whether the belt is powered. |
| Reverse Direction | False | False goes uphill; true goes downhill. |
| Speed Change Time | 0.5 s | Time to start, stop or change direction; zero changes immediately. |

The shell is an authored mesh and does **not** stretch when Run, Rise, Landing Length or Belt Width changes. Keep those values matched to the shell, or provide another appropriately authored shell. Geometry rebuilds in Construction and BeginPlay; changing geometry during gameplay is unsupported. The actor's root and support collision are static: runtime actor relocation is unsupported. Tread visuals remain movable.

`SetRunning` and `SetReverseDirection` are callable from Blueprint. `GetCurrentBeltSpeed`, `GetTravelLength`, `GetDeckWorldPoint` and `GetBeltVelocityAtWorldPosition` expose diagnostic values. The velocity function describes the belt at a position; it does not prove that a character there is eligible for transport.

At the default dimensions the route is approximately 980 cm long and uses 25 treads, giving **39.20 cm** actual pitch. Tread depth is fitted to **39.05 cm**, leaving a 1.5 mm seam on flat approaches. On the incline the same treads are separated by **33.95 cm horizontally** and **19.60 cm vertically**; the roughly 5.10 cm horizontal overlap nests the upper step over the rear of the lower step. Their top surfaces are at different heights, so that nesting does not create flat coplanar tread overlap. Travel between fixed end sections takes approximately **17.8 seconds** at 50 cm/s.

## Character transport and limits

Three invisible stationary boxes form the lower landing, continuous inclined walking deck and upper landing. Separate side guards prevent ordinary walking off the sides. Shell and tread render meshes have collision disabled. This avoids moving-base snaps when a tread recycles under the end plates.

After normal CharacterMovement, the actor queries nearby Pawn collision and adds a swept belt displacement only to `ACharacter` riders grounded on one of its walking-deck components. Characters can walk against or with the belt. Jumping, falling, flying and standing on another actor do not receive carry. The carry ends at the fixed end section; the player then walks off normally. This works without capsule overlap events, but the character capsule must use the Pawn object channel and ordinary CharacterMovement walking collision.

Physics objects and non-Character pawns are not transported. Moving handrails are not implemented. Multiplayer prediction/replication has not been validated; the current implementation is intended for the local single-player project.

Collision follows a continuous ramp, while the visible treads are flat. Foot IK therefore samples the ramp rather than individual tread tops, and feet can appear above or within a tread during part of its travel. The route's two corners are sharp rather than mechanically curved; tread risers visually nest while entering and leaving the incline. Custom mesh top details and fixed plates should be checked for flicker where they meet; the imported shell's plates should sit just above moving tread details.

The three deck tops meet without a mathematical gap. However, a capsule contacts the incline before its center reaches the lower seam and rounds the upper crest. Carry follows center position, then uses a swept slide and requests a fresh floor check. This needs a runtime check for a brief change in speed/height or a snag at either transition, especially at low frame rates. Inclines above 45 degrees exceed the supplied walkable-slope setting.

## Required PIE check

Ride uphill while standing still through both transitions, then repeat downhill. Confirm the fixed end section releases the rider without a snap or push off the platform. Check walking in both directions, jumping off, stopping/reversing while occupied, side collisions and a low-frame-rate ride. Observe feet and the flat approach tread seams. A character standing on an unrelated floor above or beside the escalator should remain unaffected.
