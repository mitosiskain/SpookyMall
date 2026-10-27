# Ghost interaction art intake

Implemented and checked October 16, 2026. No paid models or new external model downloads were added to the game. Existing CC0 models were reused, and the project-generated test handwriting texture was imported.

The two selected loose props are already imported CC0 Poly Haven books:

- `/Game/Environment/Mall/Models/PolyHaven/SM_BookHardcover02`, 4.188 x 15.387 x 22.758 cm, 1,592 triangles.
- `/Game/Environment/Mall/Models/PolyHaven/SM_BookSoftcover04`, 3.740 x 14.938 x 22.447 cm, 1,312 triangles.

Their material slots and UV channels are retained in `SM_GhostLooseHardcover02` and `SM_GhostLooseSoftcover04` under `/Game/Environment/Mall/Interactions/Meshes`. These derivatives add fitted box collision for simulation and are placed flat on the test-store checkout; the original imported meshes remain unchanged. Original FBX/source and texture provenance remain under `ExternalAssets/Mall/PolyHaven`. Source: https://polyhaven.com/a/decorative_book_set_01 . License: https://polyhaven.com/license . Both the models and community cover designs are CC0.

A brief search did not establish a suitable directly downloadable CC0 open writing book with a predictable blank-page UV. `AMallGhostWritingBook` therefore constructs an open cover, pages and a separate writing overlay from engine primitives. `BP_GhostWritingBook` is placed between the checkout terminals. The overlay uses `M_GhostWritingPage` with `WritingTexture` and `WritingProgress`; its server-timed reveal replicates writing state rather than texture pixels.

`T_GhostWriting_TestRGBA.png` is a project-generated transparent 768 x 1024 image reading **I AM STILL HERE** with tally marks. The adjacent PowerShell script reproduces it. For replacement art, use opaque ink on transparent paper, approximately 1024 x 1496 pixels for the 19.5 x 28.5 cm page. Import the texture and assign **Writing Texture** in the book Blueprint defaults. Keep the page material assigned. A render target is not needed for this image-reveal prototype.

The evidence book now starts closed and blank. F picks it up or places it open; R opens/closes a readable held inspection view. Writing starts only in a placed, open book after an explicit writing phrase or recognized ghost question, and persists through moving/closing/reopening for the session. See `Docs/BookInteractions.md` for current controls and `Automation/BookInteractions/validation_summary.json` for the latest acceptance.

The counter and room also have hidden simple-collision supports inside their existing visible surfaces. The initial triangle-collision physics attempt failed; corrected live PIE verifies stable resting books, a nudge, a throw, floor landing and replication. Initial ghost/prop acceptance remains in `Automation/GhostInteractions/validation_summary.json`.

The official Kenney Furniture Kit ZIP was inspected but not selected. It has closed books and bookcases, cardboard boxes, a radio, and small decor; it has no open book suitable for the writing test. The optional ZIP and its CC0 license are kept only in ignored `Saved/GhostInteractionPrototype/AssetSearch/KenneyFurniture`, rather than committed as unused runtime art. Source: https://kenney.nl/assets/furniture-kit . The publisher page currently advertises 140 files while the included license labels this archive Furniture Kit 2.0; preserve the archive hash rather than infer a version from the website display.

Download, source hashes, selected existing asset details, and status are in `manifest.json`.
