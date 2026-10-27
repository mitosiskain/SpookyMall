# Evidence book pickup and inspection

The test book starts closed and blank on a low stand beside the middle east-side ground-floor store's checkout. The stand avoids the checkout's raised collision rim. The shared evidence-tool multiplayer endpoint leaves the existing E interaction and hotbar intact. A [spirit box](SpiritBox.md) is now beside the book; F picks up/places the focused tool, and R inspects a held book or switches radio power. Place the held tool before picking up another.

## Controls

1. Aim the center of the camera at the nearby book and press **F** to pick it up. Move within about 2.8 m; walls and intervening objects block pickup.
2. Press **R** while carrying it to open a readable inspection view. Press R again to close it. The book's open state is also visible to other players. Movement is not paused by inspection.
3. Aim at a clear, level floor or tabletop and press **F** to place the book open. The complete open book must fit on that surface. Failed placement leaves it in your possession and shows a reason.
4. Place it inside the ghost's ground-floor test store, then hold **V**, speak a recognized phrase and release. The ghost must be invisible, in range, and not paused. The book must be placed, open and unwritten.
5. Pick it up again and press R to inspect the evidence. Closing, carrying and placing never erase or restart its writing. Other players and late joiners receive the same evidence.

Only one player can carry a particular book, and each player can carry one evidence book. A held book cannot receive new writing. If its carrier dies, disconnects or loses their pawn, it returns to its previous placed position with its evidence intact. Books do not occupy a hotbar slot in this prototype; the carried visual uses a simple waist pose rather than a new hand animation.

## Phrases that trigger writing

| Phrase | Aliases |
| --- | --- |
| Write in the book | Write something |
| Where are you? | — |
| Who are you? | What is your name? |
| How old are you? | What is your age? |

The current response artwork is the same **I AM STILL HERE** test image for every writing trigger. Recognition accepts close whole-phrase variants, including **right in the book** and **the write of the book**, using known homophones and flexible articles/prepositions. The confidence cutoff defaults to zero; low-confidence words and phrases can still trigger a matching command. Unknown speech, longer unrelated sentences and negations do not become commands. Questions can still produce their normal ghost signal when a suitable book is unavailable; explicit writing is accepted only if a book actually starts writing.

The voice panel appears in the bottom-left corner during Play and should show **Hold V to talk** plus **Offline English voice recognition ready.** Click the game viewport before holding V. The panel's third line reports fresh listening/processing, recognition and server reaction feedback. Failed matches show a short local transcript rather than writing spoken text or audio to logs. If the book reacts to `mall.GhostDisturb write` but not speech, inspect those lines to distinguish microphone capture, recognition and ghost eligibility.

### Microphone controls

Press **F6** during Play to open microphone controls. Hold **V** and speak normally while adjusting the boost slider; automatic gain and the basic rumble/noise filter default to enabled. F6 or the Close button returns to gameplay. These preferences are saved for the local user in `GameUserSettings.ini`, independently of multiplayer settings. Console alternatives are `mall.MicGain 6` (dB), `mall.MicAutoGain 1`, `mall.MicNoiseFilter 1`, and `mall.MicReset`.

The panel uses a logarithmic output meter with a short peak hold and time-based decay. Its settings view also shows source/output dBFS and actual applied gain; clipping is flagged. Automatic gain targets ordinary speech volume and caps total positive gain at 30 dB, including the slider's bias. With automatic gain off, the slider applies a fixed gain. The optional filter removes low-frequency rumble and gently attenuates the lowest noise floor; it does not isolate speech from loud background noise.

Platform capture gates are persistently disabled and platform gain is unity, so quiet samples reach the project processor. Processed PCM is shared by recognition and proximity networking, preserving 16 kHz mono timing and every sample. Changing the Windows default microphone, including switching to a processed virtual input, can require restarting Unreal because the engine caches its capture-device selection.

An October 16 follow-up corrects two setup bugs: UE 5.8 resets viewport anchors when widget size/position are set, so the original panel sat above the screen; the final anchors/alignment are now applied last. The key config also now uses the required bare `PushToTalkKey=V`; the previous struct-style value imported as `(` and never matched V. The cold native build and configured-key/phrase regression pass. Restart/visual acceptance is recorded in `Automation/Voice/status_panel_acceptance.json`.

Automatic disturbances now move or throw loose props only. Waiting in the store, **give me a sign**, and **come here** do not write in a blank book. Completed evidence is immutable during normal play. A fresh PIE session creates a fresh blank book; saving evidence across game launches is not implemented.

## Artwork

The right page is 19.5 x 28.5 cm. Use opaque ink on transparent paper in a portrait PNG (approximately 1024 x 1496 pixels), import it and assign **Writing Texture** in `BP_GhostWritingBook` defaults. Keep **Page Material** assigned to `/Game/Generated_Materials/M_GhostWritingPage`. The world book and local inspection read the same replicated writing state and texture. This is a four-second image reveal, without a render target or freehand strokes.

## Verification

The native build and full seven-test ghost/voice suite pass without failures, warnings or skips. Both strengthened ghost tests also pass a final rerun. Actual client-owned RPCs verify pickup, ownership, open/close, placement and late join. The later F/R binding-only change compiles and patches through Live Coding; live host keyboard inputs and both blank/written inspection views pass. A ghost location question starts writing on both PIE peers; pickup, close/reopen and moving the book to the floor retain the original writing timestamp. The joined-player keyboard inspection attempt is excluded because the harness teleport did not move its local pawn; native real-IP tests cover client ownership. See `Automation/BookInteractions/validation_summary.json`. Actual microphone recognition and two-computer audible proximity remain user tests.

The October 17 microphone follow-up passes a cold native build and all eleven voice tests, including actual Vosk recognition of a very quiet speech fixture after fresh AGC. Aura verifies F6, slider/toggle persistence, reset, close and the visible recognition-ready panel. The final button capture adjustment passes Live Coding and fresh PIE checks. See `Automation/Voice/microphone_agc_acceptance.json` and its screenshot; the real microphone remains the user's next test.
