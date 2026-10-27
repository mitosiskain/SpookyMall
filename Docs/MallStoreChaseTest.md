# Mall store chase test

All 32 stores in `/Game/Environment/Mall/Maps/Lvl_MallOval_Test` have sourced shelves, stocked displays, checkout counters, and connected routes around the furnishings. The middle east-side ground store, `East_F0_02`, now starts with an invisible ghost that cannot chase or catch the player. It can write in an open book on the checkout counter and move or throw the two loose books beside it. Visible hunting remains available through a host test command. The native build, seven automated tests and live book/peer checks pass; see BookInteractions.md for current acceptance. Unreal is open on the saved map with PIE stopped and no dirty packages.

The accepted scale remains unchanged: 8 m clear rooms plus 0.5 m services, 8.5 m floor spacing, the 34 x 26 m oval and 41 x 33 m ring, four 12 m bridges, and the expanded 119 x 85 m surround. Merchant/sales systems, broader NPC behavior, visual/art/audio polish, curve smoothing, and additional floors remain separate later work.

The [spirit-box prototype](SpiritBox.md) is on the evidence stand beside the book. F picks up/places either tool, and R switches a held or focused placed radio on/off. Location, identity and age questions can independently trigger a radio reply and book writing. The radio currently shares one replaceable “I am here” recording across those questions. Current acceptance is in `Automation/SpiritBox/validation_summary.json`; older test counts below describe earlier passes.

## Try the store in PIE

1. Open `Lvl_MallOval_Test` and start PIE. From the ground-floor fountain, enter the middle store on the east/right-hand branch. The ghost and its label are initially hidden. The closed, blank writing book is on the low stand beside the checkout near the main-hall entrance.
2. Aim at the closed book and press **F** to pick it up. **R** opens/closes a readable held inspection view. Aim at a level floor or tabletop and press F to place it open. See [book controls and evidence persistence](BookInteractions.md).
3. For voice testing, place the open book in the store, focus the viewport, wait for recognition, hold V, say **write in the book**, **where are you?**, **who are you?** or **how old are you?**, and release. Writing takes four seconds and stays through closing, pickup and placement. A fresh PIE session starts blank again. Automatic disturbances move/throw loose props only. **Leave me alone** pauses disturbances for three seconds. Microphone hardware acceptance remains a user test.
4. Enter `mall.GhostMode hunt` on the host to enable the visible placeholder and existing store chase. Walk around the shelf loops and leave through any of the three exits. A catch resets only the ghost with a three-second cooldown. Its speed is 180 cm/s and the player's remains 600 cm/s.
5. Enter `mall.GhostMode invisible` during a chase. It immediately stops the route and hides the body/label. **Come here** can request a disturbance in this mode but cannot initiate a hunt.

Catch/reset and cooldown are visible through the ghost's behavior. There is no verified caught-message overlay; the debug print node was not visible in the reviewed captures. The mannequin and label are placeholders, and chase speed/balancing can be refined later.

## Store layout

`East_F0_02` is the middle of the three east-side ground-floor stores. All three openings remain usable.

| Element | Coordinates in cm |
| --- | --- |
| Room bounds | X3950 to 5950; Y-816.667 to 816.667 |
| Floor and clear height | Floor Z0; 800 cm clear |
| Main-hall entrance | X3950, Y0 |
| Neighbor-store openings | X4950, Y-816.667 and Y816.667 |
| Opening widths | 260 cm each |
| Ghost home capsule center | X4950, Y0, Z92.15 |

Furniture and checkout displays are environment props. They provide no inventory, currency, price, or transaction behavior.

## Earlier furnishing and chase validation

Construction/path checks pass across all 32 stores for capsule radii 34 and 35 cm: 64 loops, 256 navigation legs, 700 fixture blockers, 66 doorway links, and 32 shelf detours per capsule size. These are clearance/path-query results, separate from live movement. Actual player PIE passes four sampled continuous loops and all 16 legs, with maximum XY endpoint error 2.78 cm within a 5 cm tolerance. Floor heights are retained and health stays at 100. The automation temporarily used 200 cm/s in PIE, then restored the authored 600 cm/s default; the initial overshoot run using an incorrect speed assumption is rejected evidence.

The placed ghost is `StoreGhost_East_F0_02`, using the saved `BP_StoreGhost` and `BP_StoreGhostController`. Runtime checks pass idle outside, animated pursuit around both shelf sides, route updates for a moving player, catch count increments, ghost-only reset, and a measured 3.006-second home cooldown. Walking out through each of the three exits produces Chasing -> Returning -> Idle. A player on the upper floor is ignored.

All 467 normal-state samples keep the entire ghost capsule inside the store with zero boundary violations. A separate forced displacement outside the room is recovered within 0.3 seconds and intentionally increments the violation counter once. Two path rejections during exit tests stopped safely and recovered; the evidence does not claim zero rejections across all tests. Health remains 100, and catching does not teleport the player.

Before the interaction pass the map had 729 actors, including one placed ghost and 582 fixture-owned actors. There were 5932 visual fixture meshes: 5648 instanced mesh instances plus 284 base static meshes. The original 146 actor poses and source meshes were unchanged, including the fountain's existing 1.7 scale. The initial interaction pass added five actors, giving 734 total. The pickup pass adds one low book stand, giving 735 total, with the accepted mall scale unchanged.

## Sources and evidence

The additional POS terminal and card-terminal stand are saved CC0 props from the already downloaded Retail Store Fixtures and Mall pack:

| Display | Unreal asset |
| --- | --- |
| POS terminal 25324 | `/Game/Environment/Mall/Models/Retail/SM_Retail_25324_POSTerminal` |
| Card terminal stand 25326 | `/Game/Environment/Mall/Models/Retail/SM_Retail_25326_CardTerminalStand` |

Their material slots are assigned, with opaque dark screens. Prepared source files and FBX checks are in `ExternalAssets/Mall/RetailKit/ImportReady/Checkout`; source URLs and hashes remain in the preparation/download manifests. The ghost reuses the project's existing Manny and unarmed animation assets. See [Mall asset register](MallAssetRegister.md) for the unchanged licensing history.

Current evidence is in `Saved/StoreChasePrototype`: `checkout_import_report.json`, `player_walking_acceptance.json`, `ghost_implementation_result.json`, `ghost_placement.json`, `ghost_runtime_acceptance.json`, and `final_scene_audit.json`. The completed furnishing/path audit is `Saved/Aura/LDA/District/MallStoreFixtures/final_resume_clearance_audit.json`. Earlier October 13 results describe the scene before furnishings; implementation/placement snapshots marked runtime pending precede the passing runtime report. Reviewed game captures show stocked shelves, both counter terminals, the animated placeholder, and its STORE GHOST label. No Computer Use was used.

## Proximity voice prototype

The initial hosting design is a player-hosted listen server. EOS is the intended shared service for future Steam/Epic play. Since no EOS product exists yet, the active development backend remains NULL/LAN; Internet matchmaking and cross-store authentication are not activated. Native runtime components attach automatically, without reparenting the existing player Blueprint. The ghost now selects an eligible multiplayer player instead of always selecting player zero, and its existing route, catch and confinement functions remain in control.

Hold **V** while the game viewport has focus to talk; release to close the microphone. Voice is spatialized with full volume within 2 m and a maximum range of 20 m. The local player does not hear a microphone echo. This pass does not simulate sound passing through walls or radios. Use two computers and headphones for the real proximity-audio test; two PIE instances on one computer are useful for networking checks but do not substitute for that test.

Speech recognition runs locally through the bundled Vosk English model using the same captured audio. It submits an utterance when speech pauses or push-to-talk ends. It matches complete configured phrases rather than searching arbitrary sentences for a keyword. The status panel distinguishes local recognition from the server's actual ghost reaction. Audio/transcripts are not written to recordings by this system; proximity-chat audio is sent to nearby players.

| Spoken phrase | Server-authorized reaction |
| --- | --- |
| Where are you? | Attempt writing in a placed, open book plus the usual ghost signal |
| Give me a sign / Show me a sign | Brief visual/sound cue; invisible mode also attempts a prop movement |
| Come here / Come over here | Invisible: attempt a disturbance; visible hunt: prioritize the speaker for 12 seconds |
| Leave me alone / Go away | Stop the current route and pause disturbances for three seconds |
| Write in the book / Write something | Start writing in a nearby unwritten book while invisible |
| Who are you? / What is your name? | Attempt writing in a nearby placed, open book and provide a signal |
| How old are you? / What is your age? | Attempt writing in a nearby placed, open book and provide a signal |

The speaker must be possessed, inside the test store's existing XY/floor bounds, and within 20 m of the ghost. Unknown or low-confidence speech, old speech submissions, and excessive repeat commands do not trigger it. A phrase does not let the ghost leave its store. Reactions are placeholders rather than final ghost dialogue or effects.

Settings are under **Project Settings > Game > Mall Voice Prototype**, with tracked defaults in `Config/DefaultGame.ini`. Edit phrase aliases, confidence, push-to-talk key, distances, status visibility and `DefaultMaxPlayers` there. Four is the initial session default. `mall.Host 8` can request eight slots; this is a configurable capacity, not a claim that eight-player gameplay or performance has been validated. The advertised lobby capacity and server admission cap use the same value. Current EOS lobbies support at most 64 members; that service ceiling is not a gameplay target.

## Local multiplayer controls

These prototype console commands also have Blueprint-callable equivalents on `MallMultiplayerSubsystem`:

- `mall.Host` or `mall.Host 4`: create a session and load the mall test map as a listen server.
- `mall.Find`: search compatible sessions. With the current backend this searches the LAN.
- `mall.Join 0`: join the first search result; indices start at zero.
- `mall.Leave`: leave and return to local play.
- `mall.Login`: explicit Epic account portal login, available after EOS is configured.

For editor network tests, use Play as Listen Server with two players. For a LAN test, run compatible builds on two computers on the same network, host on one, and find/join on the other. Firewall access may need to be allowed for Unreal/the game. Direct `open <host-LAN-IP>:7777` is also available while using the IP driver. Public Internet connectivity through EOS is a separate verification step after product setup.

The host runs the world and ghost AI. If that host leaves, other players lose the session; automatic game-server migration is not implemented. A dedicated-server deployment remains later work. A larger-player-count mod should change the configured host capacity and then test network load, UI, spawn handling, AI and store layouts. Extra content or gameplay mods may also require compatible clients.

## Prepare Epic Online Services

1. In the [Epic Developer Portal](https://dev.epicgames.com/portal), create the game's product and a development sandbox/deployment. Follow Epic's [OSS EOS setup guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-eos-plugin-in-unreal-engine) for product/client registration and the required game-service permissions. The project already enables the OSS EOS plugin, but keeps the backend inactive.
2. Keep the product ID, sandbox ID, deployment ID and client ID for configuration. Keep the client secret and client encryption key local; do not paste them into chat. Development testers need access to the same product/deployment.
3. Use `Config/Examples/EOS.Engine.ini.example` as the development configuration template. It is deliberately not loaded automatically. Merge a filled-in development configuration into the ignored `Config/UserEngine.ini`, preserving its existing local settings, and restart Unreal. This persistent local override is used instead of the generated files under Saved/Config. Edit private values in that file directly: saving the EOS/Android File Server Project Settings panels can copy merged values into tracked defaults, which should be checked before committing. The template uses the current UE 5.8 `SocketSubsystemEOS.NetDriverEOS` class and leaves automatic EOS lobby voice disabled because this prototype sends voice over the game connection.
4. Use `mall.Login` to authenticate the development accounts, then host/find/join with two distinct accounts. Verify relay connectivity on different networks after LAN checks. Do not activate EOS with the placeholder IDs from the example.
5. Steam-native sign-in, Epic-launcher sign-in, identity-provider registration and storefront invites must be configured and tested before claiming Steam/Epic crossplay. The example's development Epic account login is not the finished storefront login flow. Packaged builds read this local hierarchy too. Use a deliberate release configuration rather than packaging development credentials or account settings.

No service account or product was created automatically, and no credentials are included in tracked files. Source and model licenses and download hashes are in `ThirdParty/Vosk` and `ExternalAssets/Voice/provenance.json`.

## Voice validation status

The full `MyProject2Editor Win64 Development` build passes. The latest full Unreal automation passes seven tests with zero failures, warnings or skips: five voice tests plus `Mall.Ghost.StateAndDisturbance` and `Mall.Ghost.BookInteraction`. The later native F/R input binding change also passes Live Coding and actual PIE key checks. All four existing command recordings and three negative recordings pass through the actual native Vosk backend. Writing aliases and their negations pass phrase matching, but no new writing-phrase audio fixture or live microphone recording was tested. The teardown regression repeats concurrent worker startup, sibling shutdown, surviving-worker recognition, cancellation and restart for three rounds. Speech fixtures are in `Automation/Voice/Fixtures`.

The network test uses an isolated server and two real IP-connected clients. Generated audio passes through the actual Opus encoder, owning-controller server RPC, distance filter, listener RPC and decoder. Nearby delivery, beyond-20m exclusion, reverse delivery and speaker identity pass. An allowed phrase from a recently decoded speaker reaches the actual store ghost and replicates reaction telemetry; an outside-store speaker is rejected. Ghost capsules ignore Pawn collisions on every peer, including when voice is disabled. Disabling voice also stops capture, discards pending PCM, clears playback queues and rejects new incoming audio, PTT and phrase submissions.

Aura's live two-player PIE checks pass remote-speaker reactions, replicated telemetry, unknown/low-confidence/repeat rejection, and outside-store/upstairs rejection. Come-here prioritizes the farther speaker through the observed 11.46-second window; later nearest-player fallback passes, but the exact 12-second boundary and early preferred-speaker exit fallback were not separately timed. Leave-me-alone maintains the three-second cooldown and unchanged catch count, with 6.49 cm of initial stopping movement over 76 ms before becoming stationary. The strict 1 cm stationary-from-call criterion was not met.

Across 4,470 accepted position samples from two PIE sessions, the entire ghost capsule stays in its store and its boundary counter remains zero. Fresh active chases through all three exits produce Chasing -> Returning -> Idle and settle within 20 cm of home. A temporary hearing radius 10 cm shorter than the actual 887.36 cm separation rejects the speaker; a radius 10 cm longer accepts them. The test restores 2000 cm and preserves the controller/catch counter. It does not establish a physical 20 m ghost-hearing boundary. Host and joined player health remain 100 and movement speed remains 600 cm/s.

Stopping the original multiplayer PIE exposed a shared native speech DLL teardown crash. The fix keeps the four native DLLs loaded until application exit and serializes backend/model lifetime changes; per-worker models, threads and callbacks still shut down. The cold build and concurrent-worker regression pass. Three fresh two-player PIE sessions subsequently stop without crashing, with both recognizers confirmed ready in sessions 1 and 3; session 2 ended early without readiness acceptance. Updating these Windows-only libraries requires an application restart.

Editor Python forces nested RPCs to execute locally, so direct receiver calls establish receiver/AI behavior and replicated properties, not complete transport or multicast-effect delivery. A native deferred owning-client submission without captured audio receives rejected server feedback with zero packets sent. A deferred native receiver call replicates its signal reaction count to the joined player. Rendered light/text/chime media has not been reviewed. An earlier hearing-range test reconstructed the Blueprint through editor change notifications; its subsequent chase/exit evidence is discarded and superseded by the fresh tests above.

The compact results are in `Automation/Voice/ghost_pie_acceptance.json` and `Automation/Voice/validation_summary.json`, with raw evidence under `Saved/Automation/VoicePIE`. The prepared plan is marked executed with observations. Unreal remains open on the mall map, PIE is stopped, and map/content packages are clean. Existing unrelated hotbar UMG and escalator mobility warnings remain. No Computer Use was used.

Live microphone recognition and audible proximity still require the two-computer/headphone test above. Focus the viewport, wait for **Offline English voice recognition ready**, hold **V**, say a configured phrase, and release. Compare local recognition with the server's reaction feedback while inside the ghost store. Session host/find/join commands and real LAN discovery remain untested; the automated network fixture uses direct IP connections. EOS Internet/cross-store checks await product configuration. The build receipt includes all 31 voice runtime dependencies as NonUFS files; a packaged game has not been staged or tested.

## Ghost interaction prototype — initial acceptance at e5297e9

The paragraphs below retain the initial ghost/prop checks. The subsequent pickup/inspection behavior and current book rules are documented in [Book interactions](BookInteractions.md).

The new source adds an invisible disturbance mode and a visible hunt mode. The intended starting mode is invisible: authored ghost meshes and labels are hidden, hunting/catches are blocked, and the existing physical store guard continues to run. Brief voice signal components remain available. Invisible ghosts can write in designated open books and move/throw designated loose props. Props ignore Pawn collision, have bounded velocity, and recover into their owning store if displaced outside it; no damage callback is added. Ordinary shelving, furniture and scenery are not throw targets.

The independent ghost subsystem works when voice is disabled. Automatic disturbances require a possessed player inside the store and run about every 12 seconds, alternating small prop movements and throws. Book writing now requires an explicit writing request or recognized ghost question, with the book placed and open. The interaction radius is 10 m, and targets' full bounds must fit the same ground-floor store. Hunting pauses disturbances. The existing leave-me-alone phrase also pauses disturbance actions.

The **write in the book** / **write something** aliases and location/name/age questions request writing through the existing authenticated speech path. **Come here** cannot start a hunt while the ghost is invisible; it can instead disturb a nearby prop. In visible hunt mode its existing speaker preference remains available. Prototype authority-only console controls are `mall.GhostMode invisible`, `mall.GhostMode hunt`, and `mall.GhostDisturb write|move|throw`. These are host test controls, not a player-facing hunt mechanic.

The open book uses a normal transparent handwriting texture and a dynamic material instance with `WritingTexture` and `WritingProgress` parameters. The server replicates whether writing started and its start time; clients reveal the same image over four seconds. Completed writing persists for late joiners. A render target is unnecessary for this first pass; it would be useful for future freehand strokes, different stroke paths or live-generated artwork.

The test image is `ExternalAssets/GhostInteractions/T_GhostWriting_TestRGBA.png`, generated by the adjacent PowerShell script, and reads **I AM STILL HERE**. User artwork should be a transparent portrait PNG with opaque ink and transparent empty paper; the page material supplies the paper color. The right page is 19.5 x 28.5 cm, so approximately 1024 x 1496 pixels avoids stretching. Import a replacement texture, assign the book's **Writing Texture**, and leave **Page Material** pointing to `/Game/Generated_Materials/M_GhostWritingPage`. Book pickup/inspection now uses F/R through a dedicated replicated component. Page turning remains separate work. The existing unrelated E interaction component still has the unresolved trace failure documented in `Docs/InteractionAuthoring.md`.

The two existing Poly Haven hardcover/softcover models are the selected free loose props. Their CC0 source and verified import hashes are recorded in `ExternalAssets/GhostInteractions/manifest.json`. The brief search did not find a suitable free open writing book, so its cover and page geometry are project-generated. No paid assets were acquired.

`BP_GhostWritingBook`, `BP_GhostLooseHardcover` and `BP_GhostLooseSoftcover` are saved under `/Game/Environment/Mall/Interactions/Blueprints` and placed on the test-store checkout. The two physics meshes are copies with fitted box collision; the original Poly Haven assets remain unchanged. Existing floor/counter triangle collision did not support these rigid bodies in the initial PIE attempt, so hidden simple-collision supports were added inside those surfaces. The corrected test keeps the untouched hardcover on the counter and lets the softcover land on the floor after a throw.

The final native build succeeds. All six automation tests pass with zero failures, warnings or skips. `Mall.Ghost.StateAndDisturbance` covers independent attachment with voice disabled, authority, mode replication, writing, a real late-joining client, store rejection, pauses, automatic player-presence checks and prop recovery. Saved Blueprint gates separately protect `RuntimeStateTick` and `TryCatchPlayer`; a direct live catch attempt with the old catch conditions forced true is rejected while invisible.

In live listen-server PIE, writing requested through the remote speaker's receiver appears and finishes on both peers; both rendered book views were inspected. This is a timed image reveal rather than a pen following individual strokes. The loose softcover moves and throws, and its action count and motion replicate. Leave-me-alone refuses further disturbances during the pause. Five intentional catches occur only after visible hunting is enabled, with both players' health staying 100. A separate transition from a moving 80 cm/s chase immediately clears the path, target and velocity on becoming invisible; the temporary fixture speed was restored to 180 cm/s.

A corrected 120-second trace contains 1,859 samples. Every sampled ghost capsule fits inside the store with the additional 5 cm margin and the normal boundary counter remains zero. The softcover moves 110.42 cm in total displacement and settles on the floor; its maximum sampled interval speed is 240.28 cm/s. Those position samples do not prove an instantaneous physics speed ceiling or every rotated prop bound. A later, separate forced ghost displacement recovers while invisible and intentionally increments the boundary counter once. Both speech workers report ready before PIE stops without a crash.

Compact acceptance is in `Automation/GhostInteractions/validation_summary.json`; raw tool results and trace analysis are under ignored `Saved/GhostInteractionPrototype`. Discarded pre-fix collision/scale fixtures are not acceptance evidence. Live receiver calls prove receiver behavior and replicated properties, not the complete microphone-to-server path; hardware recognition/proximity audio and final cue polish remain user tests. Asset replacement is supported through Blueprint defaults; live server-side appearance edits would need an explicit visual refresh. No Computer Use was used.
