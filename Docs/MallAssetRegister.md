# Mall asset register

## Cart asset intake — October 18, 2026

The pushable equipment cart uses project-original utility-cart geometry: three shelves, a rear push handle and caster wheels, 121.1 × 66 × 110.1 cm, 5,104 triangles. The editable Blender source, FBX, dimensions, material recipes and round-trip checks are retained under `SourceAssets/Carts/Prepared`. No external cart geometry is included.

The light-cart floodlight head comes from [Work Lamp by Grab3D](https://grab3d.com/models/tools/work-lamp/), released under **CC0 1.0**. The original GLB/FBX, license, source manifest and hashes are retained. The prepared head is 4,292 triangles, with imported 2K PBR maps and converted DirectX normals. The source creator describes procedural Blender geometry/textures with AI assistance for generator code, and no AI-generated geometry/textures. The in-game mast, charger and cable are original prototype geometry.

The [3DAssets warehouse shelf trolley](https://3dassets.dev/assets/hardware-store-and-diy-warehouse-warehouse-shelf-troll-c805d36f) was identified as a CC0 candidate but its download was unavailable; it was not acquired or imported. The previously downloaded cleaning/kiosk carts did not fit this equipment role. No paid asset or account-bound download was used. See `SourceAssets/Carts/README.txt` and `provenance.json` for provenance, and [AdjustmentGuide.md](AdjustmentGuide.md) for speed, battery, protection and placement settings.

Both cart Blueprints, wall/extension chargers, two native flashlights and a low supply tray are placed in `Lvl_MallOval_Test`. Matching existing glazed storefront modules narrow the test-store front entrance to about 1.4 m for the doorway cart. The cart now controls this store's confinement: a parked doorway cart seals it, a cart fully inside suppresses ghost activity, and an unsealed store permits same-floor hall pursuit. This supersedes the earlier permanently confined chase prototype for this opt-in ghost only. Two-player live checks are recorded in `Automation/Carts/Evidence/live_acceptance.json`.

The active map is `/Game/Environment/Mall/Maps/Lvl_MallOval_Test`. Its accepted October 13 structural revision preserves the 34 × 26 m oval and 41 × 33 m ring, expands the surround to 119 × 85 m, and supplies four cardinal bridges with 12 m clear spans. The current room scale is 8 m clear plus 0.5 m services, giving 8.5 m floor spacing. Before furnishing, all 60 main walking legs and both corrected escalator rides and walk-offs passed. Current fixture and ghost acceptance is recorded below. The user has accepted the current scale. The asset intake and licensing records below are retained; earlier geometry and validation sections are historical.

All 32 stores are now furnished with sourced shelves, stocked displays, and checkout props. One confined ground-floor test ghost is placed in `East_F0_02`. Fresh furnishing/path checks, four sampled player loops, and ghost runtime acceptance pass. The map is saved and PIE is stopped. See [Mall store chase test](MallStoreChaseTest.md) for the player test.

The October 17 spirit-box prototype adds one native primitive radio to the existing evidence stand, with a locally synthesized placeholder voice and an original radio-noise loop. No external model or sampled dialogue was added. Imported audio is under `/Game/Audio/Ghost/SpiritBox`; masters, generation details and hashes are in `SourceAssets/SpiritBox/provenance.json`. See [spirit-box controls and recording replacement](SpiritBox.md).

## Historical atrium construction and intake

The earlier atrium deliverable was a **separate, welcoming two-floor atrium map for scale and PIE testing**: a ground floor and second-floor gallery around an open central space, connected by the working escalators, with plants, seating, the animated Hebe fountain, and open enterable retail shells. The accepted dimensions are **4 m clear room height plus a 0.5 m structural and services zone**, giving **4.5 m floor-to-floor spacing**. Use repeatable modules that can support three to five floors later. In this request, **empty means no merchants/NPCs, signage, or gameplay systems**; it does not defer environmental furnishing, foliage, or the supplied water effect.

The references guide the current lively central atrium and the later closed and abandoned wings. Use the downloaded plants, bench, shelves, and book props, user-supplied seating, and Hebe fountain in this environment pass. Additional fountain styles, custom flowing-water channels, extra asset variants, merchant behavior, and signage remain later work.

Sources checked **October 10, 2026**. **Sourced** means the download offering and license information were reviewed. **Downloaded** means source files are staged locally; **Prepared** adds checked import files; **Imported** means Unreal assets have been saved. **Existing** means assets were already present. **Acquisition candidate** means a Fab library claim and license/version check are still needed. Intake is now **166 downloaded retail GLBs, 28 imported retail meshes, nine imported Poly Haven meshes, 66 imported texture maps across the selected models and four surface sets, one imported 450 cm escalator shell, and two imported balcony derivatives**. That is 40 new mall meshes; material assembly, placement, collision, and PIE validation are separate steps.

The **saved and stable environment** is `/Game/Environment/Mall/Maps/Lvl_MallAtrium_Test`: a nominal 40 × 48 m building around a 16 × 32 m opening, with 12 open retail bays, 22 gallery balcony modules, plants, seating, the Hebe fountain, and 325 authored actors. Walking floors are Z0/Z450 cm, the gallery underside is Z400, and the roof underside is Z850. The roof is removable for later expansion. Both escalator origins are saved at Z6 cm after a 6 cm floor-contact correction. All 40 new meshes have assigned materials in every slot without WorldGrid defaults. Substrate TwoSidedFoliage uses one shared Pachira leaf parent and one potted-plant leaf instance; numeric shading value `6` is the enum, not a material count. Mall Blueprints/materials and map are saved, with no dirty mall packages in the clean audit. Clean PIE confirms uphill/downhill transport, both walk-offs, all twelve store interiors, and a continuous upper-gallery circuit after restoring four missing transition slabs. **Four sampled guard boundaries pass; reviewed live frames show changing fountain water/ripples, and PIE stopped cleanly. User scale/appearance/audio acceptance remains pending, including dark water/reflections and lighting quality.** The previous background Aura run has stopped. See [Mall atrium test map](MallAtriumTest.md) for measured layout and validation evidence.

## User supplied assets and licensing

The user reports acquiring [Modular Balcony Structure with Glass Railing](https://www.fab.com/listings/b82906d1-1b98-4266-83da-c89a389bb62a) using its free Personal tier. Use its **Stone marble texture** for the mall flooring and its glass/steel railing appearance for the galleries. The recovered source also supports separate straight and corner gallery modules. Restaurant table/chair, Bistrot table/chair, waiting chairs, and additional chairs were supplied by the user for the current furnishing pass. The [Hebe Fountain](https://www.fab.com/listings/cd1dc9b7-be6e-46c8-8e7d-7367b1c4ac92) is placed in the atrium and includes water, ripples, and sound. Reviewed live frames show water/ripple changes; appearance and audio still need user acceptance.

**User requested license note:** budget for the balcony's Professional tier if the game exceeds $100,000. **Fab clarification:** Personal and Professional are pricing tiers with the same use rights. Eligibility depends on the buyer's relevant gross commercial revenue during the preceding 12 months at acquisition, rather than this game's future revenue alone; the agreement governs which associated entities and funding count. Fab states that moving above the threshold after acquisition does not require upgrading an already acquired Personal asset. Preserve the acquisition receipt and selected tier, and check eligibility for later acquisitions. Sources: [Fab licenses and pricing](https://dev.epicgames.com/documentation/fab/licenses-and-pricing-in-fab), [Fab license summary and agreement](https://www.fab.com/eula). No Professional license was purchased, and the user-supplied Fab assets are not recorded as CC0.

## Current intake

| Source | Local staging location | Intake status |
| --- | --- | --- |
| Retail Store Fixtures and Mall | `ExternalAssets/Mall/RetailKit` | All 166 CC0 GLBs downloaded, 14.8 MB. Individual URLs and SHA256 values are in `downloads.json`; 28 checked models are imported into `/Game/Environment/Mall/Models/Retail`. |
| Poly Haven foliage and furnishings | `ExternalAssets/Mall/PolyHaven` | 71 source/map files, 243 MB; nine meshes for four Pachira variants, a potted plant, bench, shelves, and two books are imported into `/Game/Environment/Mall/Models/PolyHaven` for the current environment. Selected model maps contribute to the 66 imported textures. Manifests record provenance and dimensions. |
| Additional surfaces | `ExternalAssets/Mall/Surfaces` | Four CC0 sets: plaster, terrazzo, worn shutters, acoustic ceiling tiles. All 18 usable maps are 2K, with DirectX normals selected. Recipes and hashes are recorded locally; texture maps are imported. |
| Escalator for 4.5 m floors | `ExternalAssets/Mall/Escalator450` | Imported CC0 derivative. Rise 450 cm, run 779.4229 cm, original flat landing and rail dimensions preserved. |
| Balcony variants | `ExternalAssets/Mall/UserImports/Prepared` | `SM_MallBalconyStraight_400` and `SM_MallBalconyModule_400` imported into `/Game/Environment/Mall/Models/Architecture`. Each has a 400 × 200 cm deck, walking surface at local Z0, slab bottom at Z−50, and a 132.367 cm guard. The straight part has a long +Y guard with open ends; the corner part retains a +X return. Original material slots and both UV channels are retained. |
| User original import sources | `ExternalAssets/Mall/UserImports` | Copies of cached balcony, restaurant, and Bistrot GLBs retained for recovery and provenance. Original user acquisition terms apply. |

Saved intake evidence is in `Saved/MallBuild/import_report.json`; the 66 texture assets are under `/Game/Environment/Mall/Textures`. Source hashes, dimensions, material recipes, and normal-map conventions are recorded in the staging manifests. Material assignment and saving are verified for all 40 new meshes. Clean runtime results are summarized in `Saved/MallBuild/verification_summary.json`: both rides, both walk-offs, all twelve store entrances, and the repaired continuous upper-gallery circuit passed. Four sampled guard boundaries pass; reviewed live frames show changing fountain water/ripples, and PIE stopped cleanly. User scale/appearance/audio acceptance remains pending, including dark water/reflections and lighting quality.

## Style from the references

For this atrium, use pale columns and gallery fascias, glass/metal balcony rails, broad storefront openings, planted islands, seating, and the supplied Hebe fountain. The supplied marble is the selected floor finish for the test; cream square tiles and colored borders remain optional reference-inspired alternatives. Skylights and warm shop lighting should make the central section inviting. Custom shallow water channels can follow later. The abandoned references establish the later deterioration of this same architecture: closed shutters, damaged ceiling panels, grime, and debris. Preserve the same floor treatment and structural modules when creating those variants.

## Assets already in the project

| Asset | Location | Use and status |
| --- | --- | --- |
| Working mall escalators | `/Game/Environment/Escalator/BP_MallEscalator` | Existing and PIE tested. Up/down pair, meshes, materials, and landing platform are available. See [Escalator notes](Escalator.md). |
| Uniblocks architecture | `/Game/Uniblocks` | Existing blocks, cladding, pillars, railings, windows, doors, garden pieces, and lights. Useful first choices for the atrium shell; select and inspect their visual fit before sourcing replacements. Presence does not establish the original acquisition license. |
| Manny and Quinn | `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple` and `SKM_Quinn_Simple` | Existing animation resources and future temporary merchant bodies. No NPC placement is required in the current environment test. Included Unreal template content, not CC0. [Epic template documentation](https://dev.epicgames.com/documentation/unreal-engine/third-person-template-in-unreal-engine). |
| Existing MetaHuman | `/Game/MetaHumans/NewMetaHumanCharacter/BP_NewMetaHumanCharacter` | Future dressed shopkeeper candidate. Merchant actors are deferred; preserve the existing player character and flashlight animation setup. |
| Interaction foundation | `/Game/Game/Blueprints/Interaction` | Existing candidates for future vendor interaction. [Interaction authoring notes](InteractionAuthoring.md) record an earlier unresolved runtime trace failure; investigate when merchant gameplay resumes, not as a blocker for this environment scale test. |

## Model catalogue and intake status

All entries in this table are free under CC0. Poly Haven provides its [license here](https://polyhaven.com/license); the 3DAssets pages state CC0 1.0 Universal. The status column separates imported models used in the current environment from optional extra sources. Formats are source download offerings, not a guarantee of automatic Unreal conversion.

| Model or pack | Status | Mall use and preparation |
| --- | --- | --- |
| [Retail Store Fixtures and Mall](https://3dassets.dev/packs/retail-store-fixtures-and-mall) | 166 GLBs downloaded; 28 curated meshes imported | Shopfronts, open entrances, shutters, cash desks, racks, lighting, benches, and planters. Same family as our escalator. AI-authored geometry; collision, navigation, and gameplay are separate. [Pack manifest](https://3dassets.dev/api/v1/packs/retail-store-fixtures-and-mall). |
| [Large Atrium Planter](https://3dassets.dev/assets/retail-store-fixtures-and-mall-atrium-planter-large-ee11d12d) | Imported in the retail selection | Planter for current foliage islands; use the selected stone palette. |
| [Cash Desk Module 1200](https://3dassets.dev/assets/retail-store-fixtures-and-mall-cash-desk-module-1200-daec0656) | Imported in the retail selection | Retail counter prop. Source GLB retains rigid drawer clips; imported mesh is static and contains no sales logic. |
| [Modular Street Seating](https://polyhaven.com/a/modular_street_seating) | One combined bench imported | Source FBX/glTF/BLEND/USD; current seating around plants and the fountain. |
| [Potted Plant 01](https://polyhaven.com/a/potted_plant_01) | One mesh imported | Source FBX/glTF/BLEND/USD. Current indoor greenery; check masked leaves, scale, and repeated-instance cost. |
| [Pachira Aquatica 01](https://polyhaven.com/a/pachira_aquatica_01) | Four variants imported | Source FBX/glTF/BLEND/USD. Current tall tropical planting; check scale and leaf materials. |
| [Outdoor Table Chair Set 01](https://polyhaven.com/a/outdoor_table_chair_set_01) | Sourced only; optional | Backup café furniture. User-supplied restaurant and Bistrot sets take priority; no extra download is needed. |
| [Steel Frame Shelves 01](https://polyhaven.com/a/steel_frame_shelves_01) | One mesh imported | Source FBX/glTF/BLEND/USD. Current retail-shell dressing; stock gameplay and navigation behavior are separate. |
| [Decorative Book Set 01](https://polyhaven.com/a/decorative_book_set_01) | Two selected books imported | Source FBX/BLEND/USD. Current shelf props; selected covers retain the required second UV channel. |
| [Retro Arcade and Mall 1989](https://3dassets.dev/packs/retro-arcade-and-mall-1989) | Sourced only; optional | Additional GLB fountain, kiosk, arcade, and skylight candidates. No acquisition is needed for the current scale test. |

## Surface sources and selected sets

The supplied balcony's **Stone marble** is the chosen mall floor finish. The optional CC0 alternatives below do not replace that selection. Four surface sets have been downloaded and their 18 maps imported: White Plaster 02, Terrazzo Tiles, Painted Metal Shutter, and TextureCan Tiles0013. Those selected maps are 2K; intake verified their dimensions and selected DirectX normal maps. Material construction and in-scene checks remain distinct from texture import.

Poly Haven provides its PBR sets under [CC0](https://polyhaven.com/license). TextureCan offers map packs and SBSAR sources under its [CC0 terms](https://www.texturecan.com/terms/). Other listed sets remain optional sources; they have not been downloaded in this pass.

| Material | Proposed use | Intake / phase |
| --- | --- | --- |
| [Interior Tiles](https://polyhaven.com/a/interior_tiles) | Optional cream/beige square floor alternative. | Sourced only; later alternative |
| [Terrazzo Tiles](https://polyhaven.com/a/terrazzo_tiles) | Speckled accents and borders. | Four maps imported; optional trim |
| [Marble 01](https://polyhaven.com/a/marble_01) | Optional stone finish for later planter caps or fountain rims. | Sourced only; supplied marble takes priority |
| [White Plaster 02](https://polyhaven.com/a/white_plaster_02) | Columns, gallery fascias, walls, and soffits; tint warm white. | Four maps imported; current shell |
| [Office Ceiling Tiles, Tiles0013](https://www.texturecan.com/details/131/) | Acoustic ceiling appearance. Panel/grid geometry is separate. | Six maps imported; current shell candidate |
| [Polished Aluminium Metal Sheet, Metal0058](https://www.texturecan.com/details/475/) | Optional alternative metal finish. Supplied railing steel takes priority. | Sourced only; later alternative |
| [Painted Metal Shutter](https://polyhaven.com/a/painted_metal_shutter) | Worn closed-shop shutter surfaces. | Four maps imported; later wings |
| [Painted Plaster Wall](https://polyhaven.com/a/painted_plaster_wall) | Mild discoloration near neglected corridors. | Sourced only; later wings |
| [Peeling Painted Wall](https://polyhaven.com/a/peeling_painted_wall) | Heavily abandoned walls. | Sourced only; later wings |
| [Dirty Tiles](https://polyhaven.com/a/dirty_tiles) | Neglected corridors; a different pattern from Interior Tiles. | Sourced only; optional later |

Reference-specific colored bands, accent squares, fountain mosaics, and matched wear masks are later art work. Keep the selected central floor treatment consistent when making neglected variants.

## Additional packs and character options

These are **optional future alternatives**, not missing dependencies for the current environment scale test. Fab listings below were free when checked, except the two City Shopping Mall options. Their library acquisition, exact applicable terms, and Unreal 5.8 compatibility still need confirmation; none has been claimed or installed in this pass. The user-supplied balcony, furniture, and Hebe fountain are tracked separately above.

| Candidate | Cost and license status | Useful content and limits |
| --- | --- | --- |
| [Downtown West Modular Pack](https://www.fab.com/listings/0faf8b5d-7a5f-4fee-a297-7a8efaba8896) | Free; acquisition candidate. Exact license field unavailable in the public page. | Outdoor shopping-plaza components, plants, lights, signs, decals, and a fountain Blueprint with trickling waterfalls. Adapt selected pieces indoors; inspect the water effect in our renderer. |
| [Modular Building Set](https://www.fab.com/listings/474a0598-ed86-40b6-baa1-c801d96ef4ab?lang=en) | Free; acquisition candidate. Exact license field unavailable. | Storefront/building pieces. Advertised interiors are first-floor only; interior props are absent. Secondary option if existing Uniblocks components do not fit. |
| [Coffee Shop Environment](https://www.fab.com/listings/a0c7819e-a61d-4a19-8d3b-f0f5e584e6e0?lang=en) | Free; acquisition candidate. Exact license field unavailable. | Realistic café scene and 42 meshes; potential first furnished, enterable shop. |
| [City Sample Crowds](https://www.fab.com/listings/903037e9-e1ac-4f41-96e8-1683c6fa7ad4) | Free; explicitly Unreal Engine only. Acquisition candidate. | Rigged MetaHuman-derived bodies, heads, and clothing. Better dressed NPCs after temporary merchants; includes no selling behavior. |
| [Ultimate Modular Men](https://quaternius.com/packs/ultimatemodularcharacters.html) | Free CC0; sourced. | FBX, OBJ, glTF, BLEND. Lightweight clothed characters and animations; stylized compared with the realistic reference direction. Backup placeholders. |
| [City Shopping Mall Modular Building](https://www.fab.com/listings/dc99cca3-8e71-474b-ae09-0408ad117ef3) | Paid optional alternative; price and acquisition terms must be checked before purchase. | Closest complete architecture candidate with configurable floors. Store rooms are empty. |
| [City Shopping Mall Modular Environment V1.5](https://www.fab.com/listings/b1628005-1f64-4833-a076-475ae954daec) | Paid optional alternative; price and acquisition terms must be checked before purchase. | Same building plus store props; some shops remain poster facades. [Creator's distinction](https://forums.unrealengine.com/t/aleksandr-zhdanov-city-shopping-mall-modular-environment-v-1-5/2494815). Choose one mall pack if needed. |

## Flowing water

The supplied **Hebe Fountain** is placed in the current atrium with animated water, ripples, and sound. Reviewed live frames at t114.015/t116.203 s show water/ripple changes. Dark water/reflections and lighting quality remain for user appearance review; fountain audio was not listened to. Downtown West remains an optional source for additional fountain styles and has not been acquired.

Additional reference-shaped shallow channels could use a custom basin with [Single Layer Water shading](https://dev.epicgames.com/documentation/en-us/unreal-engine/single-layer-water-shading-model-in-unreal-engine), [animated UVs](https://dev.epicgames.com/documentation/en-us/unreal-engine/animating-uv-coordinates-in-unreal-engine), and Niagara jets/splashes. These are proposed Unreal techniques, not finished imported effects. Custom channels, spillway geometry, foam masks, and their audio integration remain later optional work; full fluid simulation is not required.

## Current construction needs and later gaps

Built, placed, and saved statuses below describe the stable saved map after the background Aura run stopped. The escalator correction, rides, store entrances, and repaired upper-gallery circuit are verified. Four sampled guard boundaries pass; reviewed live frames show changing fountain water/ripples, and PIE stopped cleanly. Final scale/appearance/audio acceptance is for the user.

| Need | Available basis | Remaining work / phase |
| --- | --- | --- |
| Separate two-floor environment test map | Saved `/Game/Environment/Mall/Maps/Lvl_MallAtrium_Test` | Stable saved 40 × 48 m atrium with 325 authored actors; clean rides, store entries, and continuous upper-gallery circuit pass. Four guard samples pass, live fountain water/ripple changes were reviewed, and PIE stopped cleanly. User scale/appearance/audio acceptance remains pending. The original 360 cm demo remains separate. |
| Repeatable floor structure | Saved floor/gallery shell, 22 balcony modules, retail shell pieces, and grouped roof | Built at 450 cm intervals with removable roof and planned third-to-fifth-floor heights. Four missing 200 × 200 cm transition slabs restored to the existing upper-gallery floor actor at X±900/Y±1700, top450/bottom400 cm; DynamicMesh 48→96 triangles. Continuous gallery circuit now passes without further location teleports. Actor count remains 325. |
| Floor interval and ceiling allowance | Imported 450 cm escalator shell and up/down pair | Walking surfaces Z0/Z450, gallery underside Z400, roof underside Z850. Escalator roots are saved at Z6; clean 450 cm uphill/downhill transport and both walk-offs pass. HVAC internals remain deferred. |
| Materials and validation | Supplied marble/glass/steel; 66 imported maps and four surface recipes | Final saved audit: no missing/WorldGrid assignments across all 40 new meshes; shared Pachira leaf parent and potted-plant instance use Substrate TwoSidedFoliage. Dirty mall packages are empty; 86 unrelated dirty packages preserved. Automated movement checks pass; user appearance/audio acceptance remains pending. |
| Open enterable retail shells | 12 saved open store bays, six on each floor | All twelve fronts crossed into interiors in clean PIE, with 50 nested actions passing at MaxWalkSpeed 200 cm/s. Detailed specialty-store art, broader stock, selling behavior, and future NPC navmesh/routes remain later work. |
| Merchant gameplay | Existing character bodies and interaction foundation | Deferred by the user. Later: separate NPCs, idle/greeting animation, interaction, inventory, prices, currency, purchasing, and persistence. |
| Plants, seating, and Hebe fountain | Saved scene has 16 planting actors, six seating assemblies, and one Hebe fountain | Placed and reviewed from captured game views; fountain water/ripple changes observed. User furniture/plant scale, dark water/reflections, lighting quality, and audio acceptance remain pending. Extra water channels, spillways, styles, and variants remain optional later work. |
| Signage and broader merchandise | Current generic fixtures and book props; blank sign candidates | Signage is explicitly deferred, including shop names, directories, hanging signs, and price displays. The current static props require no inventory or purchase behavior. Broader merchandise can follow later. |
| Exact reference furniture | User-supplied restaurant, Bistrot, and chair assets | Current: use the supplied seating and verify scale. Further sourcing is optional only if these sets do not fit; no extra furniture download blocks the test. |
| Clean and abandoned variants | Selected clean PBR finishes plus downloaded shutter material | Later: matching grime/wear masks, broken ceilings, debris, boarded shops, leaks, and shutter variants. Keep the central test architecture clean. |

## Saved construction and validation

The map and construction/layout checkpoint are saved and backed up. The earlier Aura background run is stopped; the Z6 escalator adjustment and four restored gallery transition slabs are saved. Final scene audit confirms 325 actors, the repaired 96-triangle gallery floor, and the intended PlayerStart. Final materials audit has no missing/WorldGrid assignments and no dirty mall packages; 86 unrelated dirty packages were preserved. Both rides, both exits, all twelve store entrances, the continuous upper-gallery circuit, and four sampled guard boundaries pass. Reviewed live frames show changing fountain water/ripples. PIE and its recording stopped cleanly; the editor is left on the new map overview. Evidence is in `Saved/MallBuild/verification_summary.json`, `Automation/Mall/final_scene_audit_result.json`, `final_saved_materials_audit_result.json`, and `finish_pie_final_result.json`. User scale/appearance/audio acceptance remains pending.

1. Saved: separate two-floor atrium environment, 450 cm floor interval, four-metre clear rooms, 50 cm structural/services allowance, and a removable roof for later floors.
2. Saved: gallery modules, shell/storefront architecture, 12 open retail bays, floor and edge guards, and escalator openings. Measurements and grouping are recorded in the map's layout files.
3. Saved and checked: marble/rail/plaster/ceiling/furnishing materials, with no unassigned or WorldGrid slots across the 40 new meshes. The shared Pachira leaf parent and potted-plant leaf instance use Substrate TwoSidedFoliage.
4. Saved and verified: the 450 cm up/down escalator pair at yaw90°, with origins `(-100, 690.577, 6)` and `(100, 690.577, 6)` cm. No-input transport raises/lowers the character by 450 cm, and both landing exits pass. Earlier interrupted/roof-snapped tests are excluded.
5. Placed and saved: plants, seating, the animated Hebe fountain, and retail-shell furnishing. Merchants/NPCs, signage, and gameplay systems remain deferred.
6. Verified: all twelve store entrances and the final continuous upper-gallery circuit; the circuit remains at capsule Z542.15 through all four sides after restoring four missing transition slabs. Both failed earlier gallery attempts remain excluded diagnostic evidence. Four sampled guard boundaries pass; reviewed live frames show changing fountain water/ripples. PIE stopped cleanly. Pending: user scale/appearance/audio acceptance, particularly dark water/reflections and lighting quality; fountain audio was not listened to. NPC navmesh/routes, extra water styles, expanded shop art, merchants, signage, and abandoned wings remain later work.

New intake should retain the source URL, license, and hashes, then check source dimensions, pivots, texture maps, and Unreal 5.8 loading before saving the imported assets. Mark **Imported** after that saved intake; verify assigned materials, useful scale, and appropriate collision in the actual scene before treating an asset as ready for play. Optional source candidates do not add work to the accepted environment pass by themselves.

## Historical oval redesign October 12 2026

The current scale-test map is now `/Game/Environment/Mall/Maps/Lvl_MallOval_Test`, saved with **145 authored actors and 32 stores**: 14 ground-floor stores and 18 upper-floor stores. The ground oval is open with the Hebe fountain centered; both side branches and the rear row contain stores. Four connected shops fill the upper oval, with direct interior doorways, a surrounding ring hallway, and horizontal routes through the center. The layout retains **4 m clear rooms, a 0.5 m services zone, and 4.5 m floor spacing**, with a template for later upper floors.

Actual PIE checks pass all **66 doorway traversals** with a maximum endpoint error of **21.03 cm**, the continuous 17-leg upper ring, two west-to-east routes through the oval stores, the ground fountain bypass, and both escalator rides and walk-offs. PIE is stopped and the redesigned map is open. The earlier atrium map and its history above remain preserved. Materials and lighting need future visual polish; merchants, signage, and NPC navigation acceptance remain deferred. No Computer Use was used. See [Mall oval test map](MallOvalTest.md) for the layout, measured movement, recording, and evidence paths. Existing acquisition and license notes above still apply.

The roof actor's stretched metallic ceiling bands were cleaned up with an existing matte plaster material override. A matching PIE view confirms the long bands are removed, and only the redesigned map was saved. Geometry, original shared material assets, and movement collision remain unchanged. Texture scale and lighting still need visual polish. The comparison is `Saved/MallRedesignVerify/ceiling_matte_after_t15.58s_f2038734.png`.

## Historical hallway visibility correction October 12 2026

The current oval map now has real openings between floors, with upper galleries and guarded bridges that preserve views both upward and downward. The earlier oval validation above is historical for the layout before this correction. The scene retains **145 actors, 32 stores, 4 m clear rooms, a 0.5 m services zone, and 4.5 m floor spacing**. Store interiors, the matte roof override, the future upper-floor template, and the comparison map are retained. A final numerical preservation audit found no changes across 139 unaffected actors and no dirty maps or content.

Fresh PIE passes **50 continuous segments across seven routes**, **37 upper-floor doorway links**, **eight sampled guards**, and both escalator rides with continuous gallery/landing connections. Maximum route and upper-doorway endpoint errors are **15.46 cm** and **18.99 cm**. The 29 previous ground-floor doorway checks remain historical; all 66 links were not newly rerun. Seven sightline traces, nine void checks, and native views from both floors confirm the visibility openings. PIE stopped cleanly at 970.41 seconds. The recovered main film ended at its 900-second game-time limit and does not cover the final descent; separate position readings and explicit frames support that descent. See [Mall oval test map](MallOvalTest.md) and `Saved/MallOpenHallwaysVerify/actual_movement_evidence.json` for current evidence. Lighting, material finish, and stepped curve refinement remain pending. Merchant, signage, NPC, and license scope are unchanged; no Computer Use was used.


## Four bridges and increased room height October 13 2026

The current oval-map construction uses exactly four bridges in a front, back, left, and right plus pattern. Each is 12 m clear by 4.5 m wide, with a 14 m mesh including 1 m landing overlap at each end. The oval shops remain 34 × 26 m and the surrounding ring 41 × 33 m; the surrounding mall expands to 119 × 85 m. Floors are Z0/Z850 cm, rooms have 800 cm clear height, and the services allowance remains 50 cm. The roof underside is Z1650 cm. This supersedes the earlier 4 m rooms and 4.5 m floor interval for the active oval map; those dimensions remain historical for the preserved comparison map.

A separate 850 cm escalator shell and up/down Blueprints were imported, compiled, and saved. The original 450 cm assets remain unchanged. The checkpoint records 146 total level actors, 135 generated owned actors, 32 stores, and four bridge actors. Construction checks pass all 66 doorway capsule sweeps, 18 supported routes, eight void probes, eight height samples, eight guard samples, and the four bridge dimensional/landing checks. Actual player checks pass all four cardinal bridges (three legs each, maximum error 4.71 cm) and the complete 32-leg upper ring (maximum error 13.31 cm). The outer-gallery loop passes all four legs (maximum error 1.67 cm), and both corrected central-shop routes pass all 12 legs (maximum error 3.63 cm), establishing 60 passing walking legs. Both corrected 850 cm carrier rides, landing transitions, and walk-offs pass; the map is saved and clean with PIE stopped. Earlier October 12 movement evidence does not validate this expanded geometry. Preliminary NPC navigation is not accepted. The current sizes are accepted; smoother curves remain pending. Existing intake, licenses, player setup, fountain, and merchant/signage scope remain unchanged.


The retained-art repair restored 114 poses from the immutable pre-build `live_scene_baseline.json`, correcting the earlier black game views. Applying the relocation twice produces identical in-bounds poses. The corrected map is saved and clean with 146 actors, and fresh native captures show open views between floors. Earlier black captures are excluded. Future floor templates include geometry and access; imported-art and lighting placement must be added and checked separately for each new floor. See `Saved/MallFourBridge850Verify/retained_art_repair.json` and the current [Mall oval test map](MallOvalTest.md) for evidence.


The initial 850 cm rides are diagnostic because the upper gallery floor overlapped the upper flat landing. The saved targeted correction rebuilt only the upper slab and its guard exits, aligning the shaft edge at X−275 cm (X−2000 cm for the future even-floor counterpart). The approximately 5 m² of changed landing floor does not alter the geometry used by the 60 main walking legs. Corrected construction checks pass 1206 opening probes, 24 landing-support samples, 30 clear guard-exit probes, and eight side-guard samples. All 144 protected actors have zero numeric change; metadata, retained poses, and immutable baseline hashes are unchanged. The map has 146 actors and reloaded cleanly after save. Fresh full corrected rides and landing transitions now pass. The downward verdict uses lower-flat arrival at ground height plus unobstructed walk-off; an earlier exact-stop-coordinate requirement remains a false raw diagnostic, not a passing result.


Final native captures show the lit ground fountain from above and the upper gallery from below. The final editor audit passes with 146 actors, no differences across 114 retained poses, exactly four bridges, eight open voids, eight 800 cm clear-height checks, and no dirty maps or content. PIE stopped cleanly. The final film covers game time 0-330.592 seconds. The final film at `Saved/AuraVerify/rec_1791886005912422800_42/recording.h264` maps game time 0–330.592 seconds and covers the corrected rides and final views; earlier walking tests remain separate action evidence. Current results are in `Saved/MallFourBridge850Verify`, including `down_corrected_acceptance.json` and `final_editor_audit.txt`. Smoother curves, lighting/art polish, and future NPC navigation remain deferred; no Computer Use was used.


## User scale acceptance

The user accepted the current scale with the feedback, "The scaling is fine." This covers 8 m clear rooms plus 0.5 m for structure and services (8.5 m floor spacing), the 34 x 26 m oval and 41 x 33 m ring, four 12 m clear-span bridges, and the expanded 119 x 85 m surround.

Scale acceptance does not imply final appearance or audio acceptance. Curve smoothing, visual and art polish, fountain audio, NPC and merchant gameplay, and future floors remain separate pending work. No additional construction is included in this acceptance record.


## Store furnishing and chase prototype October 14 2026

All 32 stores have fixture layouts with shelf islands and connected loops. The middle east-side ground store, `East_F0_02`, is the fully furnished chase test room. Its hall entrance and both neighbor-store openings are retained at 260 cm wide. The accepted scale, four bridges, oval/ring footprint, original 146 actor poses, and source meshes are unchanged, including the fountain's existing 1.7 scale.

Two additional static checkout props are imported and saved from the existing CC0 Retail Store Fixtures and Mall pack. They add no payment, inventory, or sales logic.

| Prop | Source and license | Imported Unreal asset | Size in cm |
| --- | --- | --- | --- |
| POS terminal 25324 | [3DAssets POS terminal](https://3dassets.dev/assets/retail-store-fixtures-and-mall-pos-terminal-8d0454bd), CC0 1.0 Universal | `/Game/Environment/Mall/Models/Retail/SM_Retail_25324_POSTerminal` | Approximately 32 x 25 x 39.72 |
| Card terminal stand 25326 | [3DAssets card terminal stand](https://3dassets.dev/assets/retail-store-fixtures-and-mall-card-terminal-stand-a5c2c2d7), CC0 1.0 Universal | `/Game/Environment/Mall/Models/Retail/SM_Retail_25326_CardTerminalStand` | Approximately 15 x 15.61 x 24.95 |

The [retail pack](https://3dassets.dev/packs/retail-store-fixtures-and-mall) and [CC0 dedication](https://creativecommons.org/publicdomain/zero/1.0/) are recorded in `ExternalAssets/Mall/RetailKit/ImportReady/Checkout/LICENSE_AND_SOURCE.txt`. Original GLBs, source URLs, and hashes remain in the preparation/download manifests. FBX round trips pass bounds, pivot, orientation, material-section, and UV-count checks. The procedural props have no texture maps or UV channels. `Saved/StoreChasePrototype/checkout_import_report.json` confirms saved native imports and assigned material slots, with opaque dark screens. The ghost reuses existing Epic mannequin/animation assets already catalogued above; no new character asset was acquired. Existing Fab and other license notes remain unchanged.

The saved map contains 729 actors: the 146 original actors, 582 fixture-owned actors, and one placed ghost. Fixture-owned counts include 354 physical/organizational fixture actors and 228 navigation-only actors. Visual fixture count is 5932: 5648 instanced mesh instances plus 284 base static meshes; it excludes the new skeletal placeholder. Final navigation settings are radius 35 cm, height 180 cm, and 20 cm obstacle padding. All 228 modifiers and their manifests are reconciled.

Construction/path checks pass both 34 and 35 cm-radius capsules across all 32 stores: 64 loops, 256 navigation legs, 700 fixture blockers, 66 door links, and 32 shelf detours per size. These are clearance/path queries. Actual player PIE passes four sampled continuous loops and all 16 legs, maximum XY error 2.78 cm within 5 cm tolerance, retained floor heights, and health 100. Verification temporarily used 200 cm/s in PIE and restored the authored 600 cm/s default. The initial overshoot run based on an assumed 200 cm/s default is rejected evidence. Native views show stocked shelves, POS/card props, rear-store interiors, and upper-oval routes.

`StoreGhost_East_F0_02` uses the new saved `BP_StoreGhost` and `BP_StoreGhostController`, with animated Manny and a visible STORE GHOST label. Speed is 180 cm/s. Runtime acceptance passes pursuit around shelves and a moving target, all three exit transitions to Returning then Idle, upper-floor exclusion, and a ghost-only catch reset with a measured 3.006-second cooldown. Player position and health remain unchanged. All 467 normal samples retain the complete capsule inside the room with zero boundary violations. A separate forced-boundary test restores the ghost within 0.3 seconds and intentionally raises one violation; two exit-test path rejections stop safely and recover. Debug catch text was not visible in the reviewed composited captures, so a caught-message overlay is not accepted.

Evidence is `Saved/StoreChasePrototype/player_walking_acceptance.json`, `ghost_runtime_acceptance.json`, `ghost_placement.json`, `ghost_implementation_result.json`, and the furnishing audit at `Saved/Aura/LDA/District/MallStoreFixtures/final_resume_clearance_audit.json`. Earlier pending implementation/placement snapshots precede the passing runtime report. `Saved/StoreChasePrototype/final_scene_audit.json` confirms 729 actors, one ghost at its home, all 146 original poses and 11 source mesh hashes preserved, matching navigation metadata, and no dirty target packages. The map and both ghost Blueprints are saved cleanly, PIE is stopped, and player speed is restored. Broader NPC/merchant gameplay, final chase balancing, visual/art/audio polish, curve smoothing, and additional floors remain separate work. No Computer Use was used.

## Physical ghost audio October 17 2026

Three short mono 48 kHz PCM16 Sound Waves are sourced from qubodup's CC0 recordings on OpenGameArt. The source pages explicitly identify CC0; commercial use and edits are permitted without required attribution. Voluntary credit is retained.

| Sound | Source | Imported Unreal asset |
| --- | --- | --- |
| Invisible breath, 3.416 s | [Ghost breath](https://opengameart.org/content/ghost-breath), qubodup, CC0 | `/Game/Audio/Ghost/Physical/SW_Ghost_InvisibleBreath` |
| Invisible moan, 3.024 s | [Ghost Monster Voice Moaning & Growling](https://opengameart.org/content/ghost-monster-voice-moaning-growling), qubodup, CC0; GhostMoan01 | `/Game/Audio/Ghost/Physical/SW_Ghost_InvisibleMoan` |
| Chase growl, 1.734 s | Same pack; GhostMoan05 | `/Game/Audio/Ghost/Physical/SW_Ghost_HuntGrowl` |

`SourceAssets/GhostSounds` preserves original downloads, license notes, source/download URLs, file hashes, quality metrics and a reproducible preparation script. Prepared clips have fades, no digital clipping and no newly added reverb/pitch processing. An unused primbal CC0 horror-breathing candidate is retained and explicitly marked unselected; it is not imported. Final perceived tone and loudness remain a user listening check. The ghost's own sounds are independent of the spirit box and follow the actual ghost position; environmental acoustics remain deferred. See [Spirit box and physical ghost audio](SpiritBox.md) for playback rules and replacement instructions.

## Ghost-hunting equipment intake and Blueprints — October 26, 2026

Five new ghost-hunting equipment items (`BP_MallEMFReader`, `BP_MallUVLight`, `BP_MallThermometer`, `BP_MallMotionSensor`, `BP_MallPhotoCamera`) plus the UV evidence print (`BP_MallUVEvidence`) are implemented on the shared `AMallHandheldItem` C++ base class (`Source/MyProject2/Equipment/MallGhostEquipment.h`) and placed near the `East_F0_02` test store in `/Game/Environment/Mall/Maps/Lvl_MallOval_Test`.

| Equipment / Asset | Source & License | Imported Unreal Mesh & Blueprint | Dimensions (cm) & Triangles |
| --- | --- | --- | --- |
| **EMF Reader** | [Poly Haven `retro_multimeter`](https://polyhaven.com/a/retro_multimeter), CC0 1.0 | `/Game/Equipment/EMFReader/Meshes/SM_EMFReader`<br>`/Game/Equipment/EMFReader/BP_MallEMFReader` | `16.0 × 10.0 × 6.5` cm, 8,948 tris |
| **UV Blacklight** | [Poly Haven `signal_flashlight`](https://polyhaven.com/a/signal_flashlight), CC0 1.0 | `/Game/Equipment/UVLight/Meshes/SM_UVLight`<br>`/Game/Equipment/UVLight/BP_MallUVLight` | `20.0 × 6.5 × 6.5` cm, 9,730 tris |
| **Photo Camera** | [Poly Haven `Camera_01`](https://polyhaven.com/a/Camera_01), CC0 1.0 | `/Game/Equipment/PhotoCamera/Meshes/SM_PhotoCamera`<br>`/Game/Equipment/PhotoCamera/BP_MallPhotoCamera` | `14.5 × 13.5 × 10.5` cm, 26,987 tris |
| **Spirit Box (CC0 Mesh)** | [Poly Haven `portable_cassette_player`](https://polyhaven.com/a/portable_cassette_player), CC0 1.0 | `/Game/Equipment/SpiritBox/Meshes/SM_SpiritBox_CC0` | `22.0 × 14.0 × 9.5` cm, 13,307 tris |
| **Thermal Scanner** | Project-original Blender geometry (`prepare_equipment_models.py`) | `/Game/Equipment/Thermometer/Meshes/SM_Thermometer`<br>`/Game/Equipment/Thermometer/BP_MallThermometer` | `16.5 × 6.2 × 13.5` cm, 692 tris |
| **Motion Sensor** | Project-original Blender geometry (`prepare_equipment_models.py`) | `/Game/Equipment/MotionSensor/Meshes/SM_MotionSensor`<br>`/Game/Equipment/MotionSensor/BP_MallMotionSensor` | `12.5 × 19.5 × 14.5` cm, 476 tris |
| **UV Footprint Print** | Project-original Blender geometry (`prepare_equipment_models.py`) | `/Game/Equipment/UVLight/Meshes/SM_UVFootprint`<br>`/Game/Equipment/UVLight/BP_MallUVEvidence` | `24.0 × 14.5 × 0.4` cm, 288 tris |

Original downloads, CC0 provenance, SHA256 hashes, and the headless Blender preparation script are retained in `SourceAssets/Equipment/PolyHavenCC0/provenance.json` and `SourceAssets/Equipment/Prepared/metadata.json`. For the complete list of custom/bespoke 3D models to contract out (with dimensions, triangle budgets, material slots, and sockets), see [GhostHuntingEquipmentAndContractModels.md](GhostHuntingEquipmentAndContractModels.md).

