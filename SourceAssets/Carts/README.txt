Cart asset intake - October 18, 2026

Selected downloaded model
-------------------------
Work Lamp by Grab3D, CC0 1.0 Universal. Original source/license/manifest retained.
https://grab3d.com/models/tools/work-lamp/
https://creativecommons.org/publicdomain/zero/1.0/
No attribution required. Optional credit: Work Lamp by Grab3D (CC0).
Creator disclosure: procedural Blender geometry and textures; generator code was
written with help from an AI coding assistant. No AI-generated geometry/textures.

Prepared files
--------------
Prepared/SM_CartWorkLampHead.fbx: isolated floodlight head with its lens, 4292
triangles. Centimeters, Z up, +X lamp front. Origin is the original tilt-axis pivot.
The source's 10-degree downward tilt is baked into the geometry; confirm aim when
placing the Spotlight component. Source dimensions about 15.72 x 30 x 28.98 cm.

Prepared/SM_CartWorkLampFull.fbx: complete original tripod lamp, 10156 triangles,
68.2 x 77.95 x 133.20 cm; origin at ground level. It is an optional source variant.
The head-only model is suited to a mast built onto the gameplay cart.

Prepared/SM_EquipmentUtilityCart.fbx: project-original modeled cart, 5104 triangles,
121.10 cm front-to-back x 66 cm wide x 110.10 cm high. +X forward, Z up, ground
pivot centered under decks. Three shelf surfaces at Z25,58,92 cm; top retaining
lip at Z95.2 cm. Push handle is at the -X rear, height108cm. Open sides and three
usable decks. Static wheels; gameplay movement and collision belong to the actor.
Prepared/EquipmentUtilityCart.blend preserves the editable source.
No third-party geometry or textures are used in this project-original cart.

Utility cart material values are in Prepared/utility_cart_preparation.json:
Cart_BluePaint, Cart_BrushedSteel, Cart_Rubber, Cart_AmberSafety. Each uses scalar
roughness/metallic and constant linear base color; no textures are needed.

Work Lamp materials
-------------------
Full lamp slots: work-lamp_body_pbr, work-lamp_head_pbr, work-lamp_lens_glow_x.
Head-only slots: work-lamp_head_pbr, work-lamp_lens_glow_x.
The PNG maps are under Prepared/Textures. Basecolor textures use sRGB. Roughness,
metallic and normals are linear. Normals were already green-flipped from glTF
OpenGL to DirectX convention; do not flip green a second time on import.
Scalar metallic and roughness maps were split from the GLB's G/B packed texture.
Lens: linear base color [0.9215819,0.8468732,0.7379104], metallic0, roughness0.25.
Original emissive linear RGB [1,0.699319,0.182588] times strength1.474531. The game
material should expose emissive strength so power-off can make the lens dark.
Emission does not implement the actual light or safety volume.

Validation and reproduction
---------------------------
Blender: --background --factory-startup --python prepare_work_lamp.py
Python with Pillow: extract_work_lamp_textures.py
Blender: --background --factory-startup --python create_utility_cart.py
Blender: --background --factory-startup --python verify_prepared_models.py
Prepared/fbx_verification.json validates dimensions, triangle counts and UV
presence after an FBX round-trip. Maximum measured dimension error <0.00001cm.
Prepared/utility_cart_preview.png is the reviewed offline model preview.

Other researched assets (not acquired or imported)
------------------------------------------------
3DAssets.dev Warehouse shelf trolley, CC0, ID23960, 3088triangles, clean three-shelf
utility trolley: https://3dassets.dev/assets/hardware-store-and-diy-warehouse-warehouse-shelf-troll-c805d36f
Its CDN hostname did not resolve and public page/API requests returned403, so it
was not downloaded. Project-original cart geometry is supplied instead.
The project already has 3DAssets.dev CC0 cleaning trolley25375 and kioskcart25374
under ExternalAssets/Mall/RetailKit/GLB, but their cleaning/kiosk props do not fit
the requested equipment cart, so they are not reused in these prepared meshes.
Polyfork's trolley license has restrictions and download-account requirements;
it was not acquired. No wall outlet or extension-lead mesh was acquired in this
brief search; simple original geometry remains appropriate for the prototype.
