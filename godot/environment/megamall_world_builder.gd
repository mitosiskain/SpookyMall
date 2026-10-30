@tool
extends Node3D
## Megamall Anomaly — Main Level Builder & Interactive Hub Coordinator.
## Implements:
## - 4-Slot Hotbar ([1]-[4]) + "Hands" Pseudo-5th Slot + Hands-Only Items (Death Inventory Crate, Heavy Hydraulic Assembly, Relay Fuse Crate)
## - Death Inventory Crate (unusable until opened; carried in Hands [F]; opening [LMB/E] spills all items onto the floor near where opened)
## - Overhauled Dual 8.50m MallEscalators (SM_MallEscalatorShell_850.fbx + 49 animated SM_MallEscalatorStep.fbx treads + bevelled deck + Floor 2 well opening)
## - Overhauled Small Platform Stairs & Prop Collisions (Onsen Pool stairs, Sauna Pavilion steps, Raised Vendor/Atrium Platform Stairs, solid furniture colliders).

var player: PlayerController = null
var players_by_peer: Dictionary = {} # int peer_id -> PlayerController
var push_cart: MallCartSystem = null
var light_cart: MallCartSystem = null
var escalator_up: MallEscalator = null
var escalator_down: MallEscalator = null
var freight_elevator: MallFreightElevator = null
var store_ghost_east: StoreGhost = null
var store_ghost_west: StoreGhost = null
var store_ghost_floor2: StoreGhost = null
var hud_label: RichTextLabel = null
var coop_hud_label: RichTextLabel = null
var coop_menu_panel: PanelContainer = null
var coop_ip_input: LineEdit = null
var coop_status_lbl: Label = null
var journal_panel: PanelContainer = null
var journal_text: RichTextLabel = null
var voice_menu_panel: PanelContainer = null
var voice_menu_status_lbl: Label = null
var trader_menu_panel: PanelContainer = null
var trader_status_lbl: Label = null
var trader_buttons: Array[Button] = []
var status_banner: Label = null
var hotbar_gui_container: HBoxContainer = null
var hotbar_slot_panels: Array[PanelContainer] = []
var hotbar_slot_styles: Array[StyleBoxFlat] = []
var hotbar_slot_icons: Array[Label] = []
var hotbar_slot_names: Array[Label] = []
var hotbar_slot_subs: Array[Label] = []
var hand_slot_panel: PanelContainer = null
var hand_slot_style: StyleBoxFlat = null
var hand_slot_icon: Label = null
var hand_slot_name: Label = null
var hand_slot_sub: Label = null
var eating_bar_panel: PanelContainer = null
var eating_progress_bar: ProgressBar = null
var eating_bar_label: Label = null
var campaign_expansion: CampaignFloorsAndLore = null
var curved_atrium: MallCurvedAtriumRing = null
var stacked_h_wings: MallStackedHWings = null
var cart_charger: ExpandedMallGadgets.MallCartChargerStation = null
var zoltar_booth: ExpandedMallGadgets.ZoltarFortuneBooth = null
var _status_timer: float = 0.0

const VOICE_MENU_OPTIONS: Array[Dictionary] = [
	{
		"phrase": "Where are you?",
		"button_text": "[1]  \"Where are you?\"  —  Spirit Box Location EVP (87.9 / 91.5 FM)",
	},
	{
		"phrase": "Who are you?",
		"button_text": "[2]  \"Who are you?\"  —  Spirit Box Identity & Disposition Tell",
	},
	{
		"phrase": "How old are you?",
		"button_text": "[3]  \"How old are you?\"  —  Spirit Box Age EVP Response",
	},
	{
		"phrase": "Write in the book",
		"button_text": "[4]  \"Write in the book\"  —  Trigger Ghost Writing in Store Ledger",
	},
	{
		"phrase": "Give me a sign",
		"button_text": "[5]  \"Give me a sign\"  —  Provoke EMF Level 5 & Light Disturbance",
	},
	{
		"phrase": "Your shift is over",
		"button_text": "[6]  \"Your shift is over\"  —  Peaceful Resolution / Sauna Revival Prayer",
	},
	{
		"phrase": "Leave me alone",
		"button_text": "[7]  \"Leave me alone\"  —  Pause Ghost Disturbances for 3.0s",
	},
]

func _ready() -> void:
	_setup_world_environment()
	_build_mall_architecture()
	_spawn_interactive_props_and_sanctuaries()
	campaign_expansion = CampaignFloorsAndLore.new()
	add_child(campaign_expansion)
	_spawn_carts_ghosts_and_gremlin()
	if Engine.is_editor_hint():
		return
	_spawn_player()
	_build_hud_and_journal()

	MultiplayerManager.peer_joined.connect(_on_coop_peer_joined)
	MultiplayerManager.peer_left.connect(_on_coop_peer_left)
	MultiplayerManager.peer_list_updated.connect(_on_coop_peer_list_updated)
	MultiplayerManager.local_role_changed.connect(_on_coop_local_role_changed)
	MultiplayerManager.network_status_changed.connect(_on_coop_network_status_changed)

	GameState.status_message.connect(_on_status_message)
	_on_status_message(
		"Megamall Anomaly Ready! [1-4/Wheel] Tools | [F8] Host Co-op | [F9] Join Co-op (127.0.0.1) | [P] Co-op IP Menu | [J] Voice Menu.",
		Color(0.45, 0.95, 1.0)
	)

func _setup_world_environment() -> void:
	var we := WorldEnvironment.new()
	var env := Environment.new()

	var sky_mat := ProceduralSkyMaterial.new()
	sky_mat.sky_top_color = Color(0.14, 0.20, 0.30)
	sky_mat.sky_horizon_color = Color(0.42, 0.50, 0.60)
	sky_mat.ground_bottom_color = Color(0.12, 0.13, 0.15)
	sky_mat.ground_horizon_color = Color(0.32, 0.36, 0.42)
	sky_mat.sun_angle_max = 28.0
	var sky := Sky.new()
	sky.sky_material = sky_mat
	env.sky = sky
	env.background_mode = Environment.BG_SKY

	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.ambient_light_sky_contribution = 0.65
	env.ambient_light_color = Color(0.76, 0.80, 0.86)
	env.ambient_light_energy = 0.82
	env.reflected_light_source = Environment.REFLECTION_SOURCE_SKY

	# AgX Filmic Tonemapping + Natural Desaturation & Contrast
	env.tonemap_mode = Environment.TONE_MAPPER_AGX
	env.tonemap_exposure = 1.18
	env.tonemap_white = 6.5
	env.adjustment_enabled = true
	env.adjustment_saturation = 0.86
	env.adjustment_contrast = 1.05
	env.adjustment_brightness = 1.02

	# Godot 4.7 Forward+ Real-Time Global Illumination (SDFGI) + SSAO + SSIL + SSR
	env.sdfgi_enabled = true
	env.sdfgi_use_occlusion = true
	env.sdfgi_read_sky_light = true
	env.sdfgi_energy = 0.88
	env.ssao_enabled = true
	env.ssao_radius = 1.15
	env.ssao_intensity = 1.65
	env.ssil_enabled = true
	env.ssil_radius = 3.5
	env.ssil_intensity = 0.85
	env.ssr_enabled = true
	env.ssr_max_steps = 48

	# Restrained Bloom & Subtle Architectural Haze (never washing out PBR normal maps)
	env.glow_enabled = true
	env.glow_intensity = 0.24
	env.glow_bloom = 0.04
	env.volumetric_fog_enabled = true
	env.volumetric_fog_density = 0.0038
	env.volumetric_fog_albedo = Color(0.92, 0.94, 0.97)
	we.environment = env
	add_child(we)

	# Natural Warm Architectural Skylight Sunlight shining through the Vaulted Geodesic Dome
	var skylight := DirectionalLight3D.new()
	skylight.rotation_degrees = Vector3(-58.0, -32.0, 0.0)
	skylight.light_color = Color(1.0, 0.96, 0.90)
	skylight.light_energy = 1.15
	skylight.shadow_enabled = true
	skylight.shadow_blur = 1.25
	add_child(skylight)

	# Real-time PBR Specular Reflection Probes for Marble, Terrazzo, Brushed Steel & Glass
	for probe_y in [5.5, 14.5]:
		var rp := ReflectionProbe.new()
		rp.position = Vector3(0.0, probe_y, 0.0)
		rp.size = Vector3(66.0, 28.0, 52.0)
		rp.intensity = 0.52
		rp.ambient_mode = ReflectionProbe.AMBIENT_ENVIRONMENT
		add_child(rp)

func _build_mall_architecture() -> void:
	var nav_region := NavigationRegion3D.new()
	var nav_mesh := NavigationMesh.new()
	nav_region.navigation_mesh = nav_mesh
	add_child(nav_region)

	# 0. Multi-Floor Central Oval Atrium Ring, Hebe Fountain, 4-Quadrant Hot Springs,
	#    4 Cardinal Clear-Span Bridges, 4 Connected Floor 2 Oval Stores, Floor 3 Sky Arboretum,
	#    4 Suspended Floating Forest Planter Pods, and Vaulted Geodesic Skylight Dome!
	curved_atrium = MallCurvedAtriumRing.new()
	add_child(curved_atrium)

	# 0B. Stacked-"H" Dual Parallel Hallway Wings (North & South Wings, 20m-26m Deep Stores, Sightline Bends, and 60m x 26m End-Cap Anchor Stores!)
	stacked_h_wings = MallStackedHWings.new()
	add_child(stacked_h_wings)

	# 1. Unified Ground Floor Polished Marble Slab (160m x 84m: X = -114.0..+46.0, Z = -42.0..+42.0, top surface at Y = 0.0m,
	#    butt-jointing cleanly at Z = +-42.0m with MallStackedHWings with ZERO coplanar Z-fighting!)
	var ground_sb := _create_static_box(nav_region, Vector3(-34.0, -0.25, 0.0), Vector3(160.0, 0.50, 84.0), Color(0.88, 0.87, 0.85), 0.18)
	for ch in ground_sb.get_children():
		if ch is MeshInstance3D:
			(ch as MeshInstance3D).material_override = MallPbrMaterialLibrary.get_marble_floor_material()

	# 2. Central Hub East Perimeter Wall at X = +46.0m (West side opens directly into the 80m x 76m Himalayan Pink Salt & Tangquan Sauna Liminal Biome!)
	var perim_col := Color(0.86, 0.84, 0.80)
	_create_static_box(self, Vector3(46.0, 26.0, 0.0), Vector3(0.4, 52.0, 84.0), perim_col, 0.55)
	# Upper-floor West perimeter wall above Floor 1 Sauna Biome (Y = 8.5m .. 52.0m at X = -46.0m)
	_create_static_box(self, Vector3(-46.0, 30.25, 0.0), Vector3(0.4, 43.5, 84.0), perim_col, 0.55)

	# 3. Structural Mall Atrium Columns around the Double-Radius Elliptical Rim (AtriumRadius = 28.5m x 30.0m) supporting Floors 2..7+
	var col_color := Color(0.76, 0.78, 0.82)
	var atrium_col_positions := [
		Vector3(-29.2, 0.0, -14.0), Vector3(29.2, 0.0, -14.0),
		Vector3(-29.2, 0.0, 14.0),  Vector3(29.2, 0.0, 14.0),
		Vector3(-16.0, 0.0, -27.5), Vector3(16.0, 0.0, -27.5),
		Vector3(-16.0, 0.0, 27.5),  Vector3(16.0, 0.0, 27.5),
	]
	for cpos in atrium_col_positions:
		_create_static_box(self, cpos + Vector3(0.0, 26.0, 0.0), Vector3(0.78, 52.0, 0.78), col_color, 0.28)
		_create_static_box(self, cpos + Vector3(0.0, 0.35, 0.0), Vector3(1.05, 0.70, 1.05), Color(0.58, 0.60, 0.64), 0.25)

	# NOTE:
	# - All Upper Floors (Floors 2..7+, Y = 8.5m..51.0m+) around the Double-Radius Oval (AtriumRadius = 28.5m x 30.0m -> Outer (±46m, ±42m))
	#   are generated as 100% continuous watertight curved-to-rectangular slabs with safety balustrades and Void-Spanning Escalators by MallCurvedAtriumRing!
	# - All 32.0m-wide x 26.0m-deep Storefronts (including Bay 0 Stores East_F0_02, West_F1_03, West_F1_02, East_F2_01, West_F2_01, Center_F3_01)
	#   are generated by MallStackedHWings!

	# 4. Store Fascia Signs for the 32.0m-Wide x 26.0m-Deep Signature Haunted Storefronts in Bay 0 (Z = -74..-42m North Wing)
	_create_static_box(self, Vector3(20.0, 0.0075, -58.0), Vector3(0.9, 0.015, 5.6), Color(0.82, 0.68, 0.32), 0.3)
	_build_store_sign_header(Vector3(19.65, 4.45, -58.0), Vector3(0, -90, 0), "PENDELTON'S BOOKS & CURIOS [East_F0_02 — 32m Wide x 26m Deep]", Color(0.88, 0.94, 0.98))

	_create_static_box(self, Vector3(-20.0, 0.0075, -58.0), Vector3(0.9, 0.015, 5.6), Color(0.84, 0.64, 0.72), 0.3)
	_build_store_sign_header(Vector3(-19.65, 4.45, -58.0), Vector3(0, 90, 0), "SHEER TERROR HOSIERY & APPAREL [West_F1_03 — 32m Wide x 26m Deep]", Color(0.96, 0.84, 0.90))

	_create_static_box(self, Vector3(20.0, 8.508, -58.0), Vector3(0.9, 0.015, 5.6), Color(0.68, 0.82, 0.90), 0.3)
	_build_store_sign_header(Vector3(19.65, 12.65, -58.0), Vector3(0, -90, 0), "ARCADIA EVENING WEAR & BRIDALS [East_F2_01 — 32m Wide x 26m Deep]", Color(0.86, 0.94, 0.98))

	# 5. Dynamic Ceiling Light Fixtures across Atrium, Concourse, and Haunted Stores
	var atrium_light_positions := [
		Vector3(-18.0, 7.8, -18.0), Vector3(-18.0, 7.8, 18.0),
		Vector3(18.0, 7.8, -18.0),  Vector3(18.0, 7.8, 18.0),
		Vector3(0.0, 7.8, -34.0),   Vector3(0.0, 7.8, 34.0),
		Vector3(-34.0, 15.8, 0.0),  Vector3(34.0, 15.8, 0.0),
	]
	for pos in atrium_light_positions:
		var fix := MallLightFixture.new()
		fix.store_id = "Atrium_Concourse"
		fix.light_color = Color(1.0, 0.95, 0.88)
		fix.base_energy = 1.45
		fix.position = pos
		add_child(fix)

	var store_east_lights := [
		Vector3(24.5, 5.75, -66.0), Vector3(24.5, 5.75, -50.0),
		Vector3(33.0, 5.75, -66.0), Vector3(33.0, 5.75, -50.0),
		Vector3(41.5, 5.75, -66.0), Vector3(41.5, 5.75, -50.0),
	]
	for pos in store_east_lights:
		var fix := MallLightFixture.new()
		fix.store_id = "East_F0_02"
		fix.light_color = Color(0.98, 0.95, 0.90)
		fix.base_energy = 1.35
		fix.position = pos
		add_child(fix)

	var store_west_lights := [
		Vector3(-24.5, 5.75, -66.0), Vector3(-24.5, 5.75, -50.0),
		Vector3(-33.0, 5.75, -66.0), Vector3(-33.0, 5.75, -50.0),
		Vector3(-41.5, 5.75, -66.0), Vector3(-41.5, 5.75, -50.0),
	]
	for pos in store_west_lights:
		var fix := MallLightFixture.new()
		fix.store_id = "West_F1_03"
		fix.light_color = Color(0.98, 0.94, 0.92)
		fix.base_energy = 1.35
		fix.position = pos
		add_child(fix)

	var store_f2_lights := [
		Vector3(24.5, 14.2, -66.0), Vector3(24.5, 14.2, -50.0),
		Vector3(33.0, 14.2, -66.0), Vector3(33.0, 14.2, -50.0),
		Vector3(41.5, 14.2, -66.0), Vector3(41.5, 14.2, -50.0),
	]
	for pos in store_f2_lights:
		var fix := MallLightFixture.new()
		fix.store_id = "East_F2_01"
		fix.light_color = Color(0.96, 0.95, 0.92)
		fix.base_energy = 1.35
		fix.position = pos
		add_child(fix)

func _build_store_sign_header(pos: Vector3, rot_deg: Vector3, title_text: String, glow_col: Color) -> void:
	var sign_root := _spawn_model_or_box(
		"res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25346_FasciaSignBoard.fbx",
		pos,
		Vector3(4.2, 0.65, 0.18),
		Color(0.24, 0.26, 0.30),
		false
	)
	sign_root.rotation_degrees = rot_deg
	var lbl := Label3D.new()
	lbl.position = pos + Vector3(0.0, 0.32, 0.0)
	lbl.text = title_text
	lbl.modulate = glow_col.lerp(Color(0.96, 0.94, 0.88), 0.35)
	lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	lbl.font_size = 22
	lbl.outline_size = 5
	lbl.visibility_range_end = 26.0
	lbl.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
	add_child(lbl)

func _spawn_interactive_props_and_sanctuaries() -> void:
	# 1. Concourse Rest Bench on the Double-Radius Central Oval Island (with solid bench collider + 3rd-Person Camera Transition)
	var bench_pos := Vector3(-6.5, 0.0, 6.5)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25366_ConcourseBench.fbx", bench_pos, Vector3(2.0, 0.48, 0.75), Color(0.54, 0.40, 0.28), true)
	_create_floating_label(bench_pos + Vector3(0, 1.35, 0), "[F] Concourse Rest Bench\n(3rd-Person Character View: +24 Stamina/s, +8 HP/s)", Color(0.84, 0.94, 0.98))

	# 2. Whole-West-Side Floor 1 Himalayan Pink Salt & Tangquan Sauna Liminal Biome (X = -32m .. -112m, Z = -38m .. +38m)
	#    Built by MallStackedHWings with 3 Steaming Geothermal Onsen Pools, 3-Tier Walkable Cedar Sauna Amphitheater,
	#    6 Red-Hot Sauna Stoves, 4 Glass Sauna Pavilions, and 16 Active GPUParticles3D Volumetric Steam Emitters!
	#    Add walkable Small Platform Stairs & Rim collision for all 3 Onsen Pools and 4 Glass Sauna Pavilions:
	for pool_p in [Vector3(-48.0, 0.0, 0.0), Vector3(-50.0, 0.0, -20.0), Vector3(-50.0, 0.0, 20.0)]:
		_build_onsen_pool_and_stairs_collision(pool_p)
	for pav_p in [Vector3(-50.0, 0.0, -32.0), Vector3(-50.0, 0.0, 32.0), Vector3(-64.0, 0.0, -18.0), Vector3(-64.0, 0.0, 18.0)]:
		_build_sauna_pavilion_collision(pav_p)

	# 3. North Night-Market Food Court Plaza at (-18.0, 0.0, -32.0) (with Hot Pot Dining Table, Bistro Tables, Service Counter & Waste Station)
	var food_pos := Vector3(-18.0, 0.0, -32.0)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25373_FoodCourtTableSet.fbx", food_pos, Vector3(1.55, 0.90, 1.55), Color(0.68, 0.42, 0.28), true)
	_create_floating_label(food_pos + Vector3(0, 1.8, 0), "[F] North Night-Market Food Court\n(Sit at Table for 5.0s & Eat Chongqing Hot Pot: +40 HP, +45% Light Cart Battery, +25% Speed)", Color(0.96, 0.88, 0.68))
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_User_RestaurantTableSet.fbx", Vector3(-14.5, 0.0, -32.2), Vector3(1.5, 0.85, 1.5), Color(0.62, 0.44, 0.28), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_User_BistrotTableSet.fbx", Vector3(-14.5, 0.0, -35.2), Vector3(1.2, 0.80, 1.2), Color(0.58, 0.42, 0.28), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25310_CashDesk120.fbx", Vector3(-18.0, 0.0, -36.8), Vector3(1.25, 1.05, 0.75), Color(0.62, 0.38, 0.28), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25376_WasteStation.fbx", Vector3(-15.8, 0.0, -36.8), Vector3(0.65, 1.0, 0.55), Color(0.48, 0.52, 0.56), true)
	var chef_node := _spawn_model_or_box("res://SourceAssets/Characters/NormalizedLowPoly/Quartermaster_Chen_LowPoly.glb", Vector3(-18.0, 0.0, -38.0), Vector3(0.5, 1.76, 0.5), Color(0.88, 0.48, 0.32), false)
	chef_node.add_to_group("mall_npcs")
	chef_node.set_meta("npc_name", "Night-Market Hot Pot Chef")
	chef_node.set_meta("body_temp_c", 37.2)
	chef_node.set_meta("is_living_human", true)
	chef_node.set_meta("barcode_sku", "EMP-1997-CHEF: NIGHT-MARKET CULINARY STAFF [ACTIVE SHIFT]")
	chef_node.set_meta("parabolic_sound", "Hot Pot Chef: Simmering chili broth & cleaver prep (41 dB)")

	# 4. Quartermaster Chen's Raised Surplus Platform on the East Concourse Plaza (23.6, 0.0, 0.0) with West-facing stairs toward the Oval Island!
	var vendor_plat_pos := Vector3(23.6, 0.0, 0.0)
	_build_platform_with_small_stairs(vendor_plat_pos, Vector3(4.2, 0.48, 5.2), "west", Color(0.72, 0.70, 0.66))
	var vendor_pos := Vector3(22.8, 0.48, 0.0)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25310_CashDesk120.fbx", vendor_pos, Vector3(1.23, 1.05, 0.75), Color(0.45, 0.58, 0.48), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/PolyHaven/ImportReady/SM_RetailShelves01.fbx", Vector3(25.0, 0.48, -1.6), Vector3(1.1, 2.1, 0.55), Color(0.55, 0.58, 0.62), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25400_StockRollCage.fbx", Vector3(25.0, 0.48, 1.6), Vector3(0.85, 1.7, 0.75), Color(0.60, 0.64, 0.68), true)
	var chen_node := _spawn_model_or_box("res://SourceAssets/Characters/MikuDoomer_Chen.glb", Vector3(24.1, 0.48, 0.0), Vector3(0.5, 1.62, 0.5), Color(0.42, 0.68, 0.52), false)
	chen_node.rotation_degrees = Vector3(0, -90, 0)
	chen_node.add_to_group("mall_npcs")
	chen_node.set_meta("npc_name", "Quartermaster Chen (Surplus Trader)")
	chen_node.set_meta("body_temp_c", 36.8)
	chen_node.set_meta("is_living_human", true)
	chen_node.set_meta("barcode_sku", "EMP-1994-CHEN: QUARTERMASTER CHEN [CLOCKED IN: 10,240 CONSECUTIVE HRS]")
	chen_node.set_meta("parabolic_sound", "Quartermaster Chen: Shortwave radio static & counting brass tokens (35 dB)")
	var chen_anim_ik := HumanoidIKAndAnimController.new()
	chen_anim_ik.default_animation = "Custom_Chen_Idle"
	chen_anim_ik.enable_look_at_ik = true
	chen_node.add_child(chen_anim_ik)
	_create_floating_label(
		vendor_pos + Vector3(0, 2.25, 0),
		"[E] Quartermaster Chen — Trader & Upgrade Shop (MikuDoomer + Idle & Real-Time Look-At IK)\n• Sell Filled Ectoplasm Canisters (+$85 Gremlin / +$180-$260 Store Ghost)\n• Buy Empty Canister ($35) | Relay Fuse ($60) | Film ($20)\n• Ray Gun & Canister Upgrades (Higher-Tier Upgrades Unlock on Floor 2+!)",
		Color(0.78, 0.96, 0.84)
	)

	# 4B. Side-by-Side Animation & IK Comparison Showcase (Universal Animation Library vs. Custom Procedural Animations)
	#     Placed immediately North of Quartermaster Chen's platform (X = 23.2m, Z = -4.2m .. -8.4m) facing West toward the player!
	_create_static_box(self, Vector3(23.2, 0.08, -6.3), Vector3(1.6, 0.16, 5.8), Color(0.28, 0.32, 0.38), 0.35)
	var comparison_specs: Array[Dictionary] = [
		{"anim": "Custom_Chen_Idle", "z": -4.2, "title": "1. CUSTOM IDLE (Custom_Chen_Idle)\n+ Real-Time Spine/Neck/Head Look-At IK", "col": Color(0.48, 1.0, 0.78)},
		{"anim": "UAL_Idle_Loop", "z": -5.6, "title": "2. UNIVERSAL LIB IDLE (UAL_Idle_Loop)\n+ Real-Time Spine/Neck/Head Look-At IK", "col": Color(0.55, 0.90, 1.0)},
		{"anim": "Custom_Walk_Loop", "z": -7.0, "title": "3. CUSTOM WALK (Custom_Walk_Loop)\nProcedural Keyframe Walk Cycle", "col": Color(1.0, 0.88, 0.48)},
		{"anim": "UAL_Walk_Loop", "z": -8.4, "title": "4. UNIVERSAL LIB WALK (UAL_Walk_Loop)\nRetargeted Quaternius UAL1 Walk", "col": Color(1.0, 0.72, 0.88)},
	]
	for spec in comparison_specs:
		var m_pos := Vector3(23.2, 0.16, float(spec["z"]))
		var m_node := _spawn_model_or_box("res://SourceAssets/Characters/MikuDoomer_Chen.glb", m_pos, Vector3(0.5, 1.62, 0.5), Color(0.5, 0.6, 0.7), false)
		m_node.rotation_degrees = Vector3(0, -90, 0)
		m_node.add_to_group("mall_npcs")
		m_node.add_to_group("anim_comparison_models")
		m_node.set_meta("npc_name", "Animation Showcase (%s)" % str(spec["anim"]))
		m_node.set_meta("body_temp_c", 36.6)
		m_node.set_meta("is_living_human", true)
		var ctrl := HumanoidIKAndAnimController.new()
		ctrl.default_animation = str(spec["anim"])
		ctrl.enable_look_at_ik = true
		m_node.add_child(ctrl)
		var m_lbl := _create_floating_label(
			m_pos + Vector3(0.0, 1.92, 0.0),
			"%s\n[Look + Press E to Cycle All 8 Animations]" % str(spec["title"]),
			spec["col"] as Color
		)
		m_node.set_meta("anim_controller", ctrl)
		m_node.set_meta("anim_label", m_lbl)
		m_node.set_meta("base_title", str(spec["title"]))

	#    Quartermaster Chen's Single-Line Surplus Equipment Display Table (at X = 18.2m, Z = -13.2m..+13.2m on the East Concourse Plaza)
	#    Laid out in ONE straight line (single row, 1.48m apart) so no item ever sits behind another item!
	_create_static_box(self, Vector3(18.2, 0.38, 0.0), Vector3(0.95, 0.76, 26.8), Color(0.54, 0.42, 0.30), 0.42)

	var single_line_table_item_ids: Array[String] = [
		"ectoplasm_canister",
		"proton_gun",
		"photo_camera",
		"light_remote",
		"video_camera",
		"camera_stand",
		"audio_recorder",
		"sls_camera",
		"laser_grid_dome",
		"barcode_scanner",
		"parabolic_mic",
		"ecto_viewmaster",
		"pink_salt_pouch",
		"eas_pedestal_ward",
		"talkboy_decoy",
		"helium_balloon",
		"mannequin_disguise",
		"price_slasher_baton",
	]
	var item_spacing_z := 1.48
	var start_z := -0.5 * float(single_line_table_item_ids.size() - 1) * item_spacing_z # -12.58m .. +12.58m
	for idx in range(single_line_table_item_ids.size()):
		var item_id_str: String = single_line_table_item_ids[idx]
		var sp_drop := PlaceableEvidenceActors.WorldDroppedItem.new()
		sp_drop.item_data = PlayerController.make_item_data(item_id_str)
		sp_drop.position = Vector3(18.05, 0.76, start_z + float(idx) * item_spacing_z)
		add_child(sp_drop)

	#    Wall-Mounted Cart Battery Charger Station (AMallCartCharger) near the Light Cart dock + Ask Zoltar Fortune Booth!
	cart_charger = ExpandedMallGadgets.MallCartChargerStation.new()
	cart_charger.position = Vector3(12.2, 0.0, 4.2)
	add_child(cart_charger)

	zoltar_booth = ExpandedMallGadgets.ZoltarFortuneBooth.new()
	zoltar_booth.position = Vector3(-10.5, 0.0, 8.5)
	zoltar_booth.add_to_group("mall_npcs")
	zoltar_booth.set_meta("npc_name", "Zoltar Coin-Op Automaton")
	zoltar_booth.set_meta("body_temp_c", 31.8)
	zoltar_booth.set_meta("is_living_human", false)
	zoltar_booth.set_meta("barcode_sku", "SKU #1988-ZOL: ANIMATRONIC FORTUNE TELLER [UNPLUGGED — WHY IS IT WARM?!]")
	zoltar_booth.set_meta("parabolic_sound", "Zoltar Automaton: Clockwork gears clicking & mechanical breathing (31 dB)")
	add_child(zoltar_booth)

	var orb_f2 := VideoAndAudioSurveillance.GhostOrbActor.new()
	orb_f2.store_id = "East_F2_01"
	orb_f2.position = Vector3(25.5, 9.8, -58.0)
	add_child(orb_f2)

	# 5. Stage 6A: Void-Spanning MallEscalators (Floors 1..7+ at Z = +22.20m inside the South Open Elliptical Atrium Void!)
	#    Built directly by MallCurvedAtriumRing so zero floor cutouts or upper-floor holes exist anywhere!
	if is_instance_valid(curved_atrium):
		escalator_up = curved_atrium.escalators_up.get(1)
		escalator_down = curved_atrium.escalators_down.get(1)

	#    Raised Escalator Breaker Control Dais with Small Platform Stairs at (-22.5, 0.0, 22.2) right beside the South Escalator Lower Entrance (-19.82, 0.0, 22.2)!
	_build_platform_with_small_stairs(Vector3(-22.5, 0.0, 22.2), Vector3(2.4, 0.48, 2.2), "east", Color(0.68, 0.70, 0.74))
	_create_static_box(self, Vector3(-23.1, 1.18, 22.2), Vector3(0.35, 1.4, 0.85), Color(0.32, 0.56, 0.46), 0.4)
	_create_floating_label(Vector3(-22.5, 2.35, 22.2), "[F] Escalator Breaker & Direction Control Dais\n(Walk up Small Platform Stairs & press [F] to Toggle/Motorize Escalators)", Color(0.82, 0.94, 0.98))

	# 6. Physical Rideable Service Freight Elevator (X = 10.0, Y = 0.0..51.0m+, Z = -25.8) inside the North Open Elliptical Atrium Void!
	#    Docks flush against the North Outer Atrium Rim at Z = -28.55m across all 7 built-in floors + Procedural Infinite Floors 8+!
	freight_elevator = MallFreightElevator.new()
	freight_elevator.position = Vector3(10.0, 0.0, -25.8)
	add_child(freight_elevator)
	if is_instance_valid(curved_atrium):
		freight_elevator.procedural_floor_requested.connect(func(new_floor: int) -> void:
			curved_atrium.spawn_procedural_upper_floor(new_floor)
		)

	var heavy_drop := PlaceableEvidenceActors.WorldDroppedItem.new()
	heavy_drop.item_data = PlayerController.make_item_data("heavy_hydraulic")
	heavy_drop.position = Vector3(-10.0, 8.50, -34.0)
	add_child(heavy_drop)

	# Elena Vance's Phased Audit Echo on Floor 2 North Promenade (1.74m 1:1 scale, right in front of the North Cardinal Bridge at Z = -30.0m!)
	var _elena_node := _spawn_model_or_box("res://SourceAssets/Characters/NormalizedLowPoly/Elena_Vance_LowPoly.glb", Vector3(0.0, 8.50, -34.0), Vector3(0.5, 1.74, 0.5), Color(0.6, 0.88, 0.96), false)
	_elena_node.add_to_group("mall_npcs")
	_elena_node.set_meta("npc_name", "Elena Vance (Phased Audit Echo)")
	_elena_node.set_meta("body_temp_c", 13.8)
	_elena_node.set_meta("is_living_human", false)
	_elena_node.set_meta("barcode_sku", "ARC-0001-VANCE: CHIEF ARCHITECT ELENA VANCE [TIMELINE DESYNC]")
	_elena_node.set_meta("parabolic_sound", "Elena Vance Echo: Whispering 1998 structural load calculations (29 dB)")
	_create_floating_label(Vector3(0.0, 10.6, -34.0), "Elena Vance's Audit Trail #1\n\"Cross the 4 Cardinal Bridges onto the Double-Radius Central Oval Island, or wheel your Carts onto the Service Freight Elevator [10, 0, -25.8] across all 7+ Floors!\"", Color(0.84, 0.95, 0.98))

	# 7. Haunted Store Interior Props & Mall Store Furnishings inside 32.0m-Wide x 26.0m-Deep Bay 0 Store East_F0_02 (X = 20..46, Z = -74..-42)
	var hiding_pos := Vector3(24.5, 0.0, -64.0)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25284_FittingCubicleClosed.fbx", hiding_pos, Vector3(1.11, 2.25, 1.26), Color(0.58, 0.54, 0.64), false)
	_create_floating_booth_walls(hiding_pos)
	_create_floating_label(hiding_pos + Vector3(0, 2.4, 0), "[F] Fitting Room Hiding Spot\n(Conceals from Lethal Hunts if entered without Ghost witness)", Color(0.84, 0.88, 0.96))

	var book_pos := Vector3(25.5, 0.85, -54.5)
	_create_static_box(self, Vector3(25.5, 0.425, -54.5), Vector3(1.1, 0.85, 0.75), Color(0.52, 0.42, 0.32), 0.5)
	_spawn_model_or_box("res://ExternalAssets/Mall/PolyHaven/decorative_book_set_01/SM_BookHardcover02.fbx", book_pos, Vector3(0.45, 0.12, 0.35), Color(0.65, 0.24, 0.22), false)
	_create_floating_label(book_pos + Vector3(0, 1.05, 0), "[F] Store Ghost Ledger & Peaceful Resolution Stand\n• Inspect Tell & Perform Peaceful Ritual (+$120 if Receptive)\n• Or Speak [9] \"Your shift is over\" on 87.9 FM", Color(0.96, 0.90, 0.70))

	_furnish_stores_and_atrium_concourse()

	# 8. Spawn Placeable Infrared Motion Sensor & Hidden UV Footprints inside Store East_F0_02 (X = 20..46, Z = -74..-42)
	var motion_sensor := PlaceableEvidenceActors.PlaceableMotionSensor.new()
	motion_sensor.store_id = "East_F0_02"
	motion_sensor.position = Vector3(22.2, 0.0, -58.0)
	add_child(motion_sensor)

	var uv_positions := [
		Vector3(22.8, 0.02, -59.6),
		Vector3(24.4, 0.02, -56.9),
		Vector3(26.2, 0.02, -58.8),
	]
	for i in range(uv_positions.size()):
		var uvp := PlaceableEvidenceActors.UVEvidencePrint.new()
		uvp.store_id = "East_F0_02"
		uvp.print_title = "UV Footprint #%d [Receptive Tell: Neat Clerk Step]" % (i + 1)
		uvp.position = uv_positions[i]
		add_child(uvp)

func _furnish_stores_and_atrium_concourse() -> void:
	# A. Furnish Store #1: East_F0_02 ("Pendelton's Book & Curio Shop" — X = 20.0..46.0, Z = -74.0..-42.0 — 32m Wide x 26m Deep!)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25334_SecurityPedestalPair.fbx", Vector3(20.6, 0.0, -58.0), Vector3(0.25, 1.45, 1.8), Color(0.55, 0.62, 0.70), false)
	_spawn_model_or_box("res://ExternalAssets/Mall/PolyHaven/ImportReady/SM_RetailShelves01.fbx", Vector3(30.5, 0.0, -68.8), Vector3(1.1, 2.1, 0.55), Color(0.32, 0.35, 0.40), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/PolyHaven/ImportReady/SM_RetailShelves01.fbx", Vector3(41.5, 0.0, -68.8), Vector3(1.1, 2.1, 0.55), Color(0.32, 0.35, 0.40), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25242_MerchandiseGondola.fbx", Vector3(30.5, 0.0, -62.2), Vector3(1.25, 1.55, 0.85), Color(0.42, 0.38, 0.34), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25242_MerchandiseGondola.fbx", Vector3(30.5, 0.0, -53.4), Vector3(1.25, 1.55, 0.85), Color(0.42, 0.38, 0.34), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25242_MerchandiseGondola.fbx", Vector3(40.5, 0.0, -62.2), Vector3(1.25, 1.55, 0.85), Color(0.42, 0.38, 0.34), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25242_MerchandiseGondola.fbx", Vector3(40.5, 0.0, -53.4), Vector3(1.25, 1.55, 0.85), Color(0.42, 0.38, 0.34), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25225_TieredRoundTable.fbx", Vector3(23.2, 0.0, -51.4), Vector3(1.1, 0.95, 1.1), Color(0.52, 0.40, 0.28), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25310_CashDesk120.fbx", Vector3(32.2, 0.0, -49.6), Vector3(1.25, 1.05, 0.75), Color(0.36, 0.30, 0.25), true)

	# B. Furnish Store #2: West_F1_03 ("Sheer Terror Hosiery & Apparel" — X = -46.0..-20.0, Z = -74.0..-42.0 — 32m Wide x 26m Deep!)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25334_SecurityPedestalPair.fbx", Vector3(-20.6, 0.0, -58.0), Vector3(0.25, 1.45, 1.8), Color(0.65, 0.55, 0.68), false)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25257_FemaleMannequin.fbx", Vector3(-22.5, 0.0, -64.8), Vector3(0.55, 1.78, 0.45), Color(0.85, 0.82, 0.88), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25265_FemaleWalkingMannequin.fbx", Vector3(-22.5, 0.0, -51.2), Vector3(0.55, 1.78, 0.55), Color(0.85, 0.82, 0.88), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25259_MaleMannequin.fbx", Vector3(-40.8, 0.0, -64.2), Vector3(0.55, 1.82, 0.45), Color(0.80, 0.80, 0.85), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25273_LegForm.fbx", Vector3(-24.8, 0.0, -65.6), Vector3(0.45, 1.15, 0.45), Color(0.88, 0.78, 0.84), false)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25208_StraightHangingRail.fbx", Vector3(-30.5, 0.0, -64.5), Vector3(1.35, 1.45, 0.60), Color(0.60, 0.64, 0.70), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25210_RoundHangingRail.fbx", Vector3(-30.5, 0.0, -51.5), Vector3(1.10, 1.38, 1.10), Color(0.60, 0.64, 0.70), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25208_StraightHangingRail.fbx", Vector3(-40.5, 0.0, -64.5), Vector3(1.35, 1.45, 0.60), Color(0.60, 0.64, 0.70), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25183_PerimeterWallBay240.fbx", Vector3(-45.0, 0.0, -58.0), Vector3(0.65, 2.40, 2.40), Color(0.42, 0.38, 0.46), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25310_CashDesk120.fbx", Vector3(-25.5, 0.0, -60.0), Vector3(1.25, 1.05, 0.75), Color(0.48, 0.34, 0.45), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25284_FittingCubicleClosed.fbx", Vector3(-42.5, 0.0, -48.0), Vector3(1.11, 2.25, 1.26), Color(0.38, 0.32, 0.44), true)

	# C. Furnish Store #3 (Floor 2 East Mezzanine, Y = 8.50m): East_F2_01 ("Arcadia Evening Wear" — X = 20.0..46.0, Z = -74.0..-42.0 — 32m Wide x 26m Deep!)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25267_HeadlessMannequin.fbx", Vector3(22.6, 8.50, -64.5), Vector3(0.55, 1.62, 0.45), Color(0.82, 0.86, 0.92), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25271_TorsoFormStand.fbx", Vector3(22.6, 8.50, -51.5), Vector3(0.48, 1.50, 0.45), Color(0.82, 0.86, 0.92), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25208_StraightHangingRail.fbx", Vector3(30.5, 8.50, -64.5), Vector3(1.35, 1.45, 0.60), Color(0.65, 0.70, 0.78), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25210_RoundHangingRail.fbx", Vector3(30.5, 8.50, -51.5), Vector3(1.10, 1.38, 1.10), Color(0.65, 0.70, 0.78), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25208_StraightHangingRail.fbx", Vector3(40.5, 8.50, -64.5), Vector3(1.35, 1.45, 0.60), Color(0.65, 0.70, 0.78), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25198_WallBayMirror.fbx", Vector3(45.0, 8.50, -58.0), Vector3(0.45, 2.40, 1.20), Color(0.55, 0.72, 0.85), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25310_CashDesk120.fbx", Vector3(25.5, 8.50, -62.0), Vector3(1.25, 1.05, 0.75), Color(0.35, 0.42, 0.52), true)

	# D. Central Double-Radius Oval Island Planters, Tropical Foliage, Directory Totems & Wayfinding Signage
	var planter_positions := [
		Vector3(-9.5, 0.0, 12.5),
		Vector3(9.5, 0.0, 12.5),
		Vector3(-9.5, 0.0, -12.5),
		Vector3(9.5, 0.0, -12.5),
	]
	for ppos in planter_positions:
		_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25369_LargeAtriumPlanter.fbx", ppos, Vector3(1.8, 0.65, 1.8), Color(0.58, 0.56, 0.52), true)
		_spawn_model_or_box("res://ExternalAssets/Mall/PolyHaven/ImportReady/SM_PachiraD.fbx", ppos + Vector3(0.0, 0.45, 0.0), Vector3(0.8, 1.9, 0.8), Color(0.24, 0.58, 0.30), false)

	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25372_DirectoryTotem.fbx", Vector3(-8.5, 0.0, 1.8), Vector3(0.85, 2.25, 0.35), Color(0.32, 0.48, 0.58), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25374_ConcourseKioskCart.fbx", Vector3(9.5, 0.0, 15.5), Vector3(1.8, 2.1, 1.1), Color(0.55, 0.40, 0.28), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25378_ConcoursePlanterBench.fbx", Vector3(-9.5, 0.0, 15.8), Vector3(2.4, 0.65, 1.1), Color(0.48, 0.38, 0.28), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/PolyHaven/ImportReady/SM_PottedPlant01.fbx", Vector3(-9.5, 0.45, 15.8), Vector3(0.6, 1.35, 0.6), Color(0.25, 0.62, 0.32), false)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25371_HangingWayfindingSign.fbx", Vector3(0.0, 6.2, -16.8), Vector3(2.8, 0.65, 0.25), Color(0.24, 0.28, 0.34), false)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25371_HangingWayfindingSign.fbx", Vector3(14.5, 6.2, -10.5), Vector3(2.8, 0.65, 0.25), Color(0.24, 0.28, 0.34), false)

	# Floor 2 Gallery Seating & Planter Troughs on the Outer Concourse Gallery
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25370_AtriumPlanterTrough.fbx", Vector3(-6.0, 8.50, -35.5), Vector3(1.8, 0.65, 0.75), Color(0.56, 0.54, 0.50), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/PolyHaven/ImportReady/SM_PachiraA.fbx", Vector3(-6.0, 8.95, -35.5), Vector3(0.7, 1.3, 0.7), Color(0.25, 0.60, 0.32), false)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_User_BistrotTableSet.fbx", Vector3(-34.5, 8.50, 12.0), Vector3(1.2, 0.80, 1.2), Color(0.55, 0.38, 0.24), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_User_BistrotTableSet.fbx", Vector3(34.5, 8.50, 12.0), Vector3(1.2, 0.80, 1.2), Color(0.55, 0.38, 0.24), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25368_CurvedConcourseBench.fbx", Vector3(4.5, 8.50, -35.5), Vector3(2.2, 0.50, 0.85), Color(0.52, 0.40, 0.28), true)

## Builds exact walkable Small Platform Stairs + Pool Rim + Inner Basin Floor collision for SM_Bath_SteamingOnsenPool.fbx
func _build_onsen_pool_and_stairs_collision(spa_pos: Vector3) -> void:
	var sb := StaticBody3D.new()
	sb.name = "OnsenPoolStairsCollision"
	sb.position = spa_pos
	add_child(sb)

	# 1. Inner soaking basin floor (X = -2.35..+2.35, Z = -1.45..+1.45, top surface Y = 0.28m)
	_add_box_collider(sb, Vector3(0.0, 0.14, 0.0), Vector3(4.7, 0.28, 2.9))
	# 2. Left & Right solid stone pool walls (Y = 0..0.56m, leaving X = -0.95..+0.95 open on North & South for the Small Platform Stairs!)
	_add_box_collider(sb, Vector3(-1.88, 0.28, 0.0), Vector3(1.84, 0.56, 3.8))
	_add_box_collider(sb, Vector3(1.88, 0.28, 0.0), Vector3(1.84, 0.56, 3.8))

	# 3. North & South Small Platform Stairs (X = -0.95..+0.95, Z = +-1.45 to +-2.45):
	#    Solid BoxShape3D step treads + tilted BoxShape3D stair ramps so Jolt Physics never allows falling through!
	for z_sign in [-1.0, 1.0]:
		# 3 discrete steps leading up to the 0.42m/0.56m pool rim crest
		_add_box_collider(sb, Vector3(0.0, 0.07, z_sign * 2.28), Vector3(1.9, 0.14, 0.34))
		_add_box_collider(sb, Vector3(0.0, 0.15, z_sign * 2.02), Vector3(1.9, 0.30, 0.34))
		_add_box_collider(sb, Vector3(0.0, 0.21, z_sign * 1.72), Vector3(1.9, 0.42, 0.44))
		# Smooth tilted BoxShape3D stair ramp flush with the steps
		var ramp_col := CollisionShape3D.new()
		var ramp_box := BoxShape3D.new()
		ramp_box.size = Vector3(1.9, 0.16, 1.15)
		ramp_col.shape = ramp_box
		ramp_col.position = Vector3(0.0, 0.18, z_sign * 2.08)
		ramp_col.rotation = Vector3(z_sign * deg_to_rad(23.0), 0.0, 0.0)
		sb.add_child(ramp_col)

## Builds wooden entrance step, perimeter walls, and interior bench collision for SM_Bath_GlassSaunaPavilion.fbx
func _build_sauna_pavilion_collision(sauna_pos: Vector3) -> void:
	var sb := StaticBody3D.new()
	sb.name = "SaunaPavilionCollision"
	sb.position = sauna_pos
	add_child(sb)
	# Floor platform (Y = 0..0.12m) + front entrance step ramp
	_add_box_collider(sb, Vector3(0.0, 0.06, 0.0), Vector3(4.8, 0.12, 3.8))
	_add_box_collider(sb, Vector3(0.0, 0.03, -2.05), Vector3(1.8, 0.06, 0.35))
	# Back & Side walls
	_add_box_collider(sb, Vector3(0.0, 1.45, 1.82), Vector3(4.8, 2.9, 0.16))
	_add_box_collider(sb, Vector3(-2.32, 1.45, 0.0), Vector3(0.16, 2.9, 3.8))
	_add_box_collider(sb, Vector3(2.32, 1.45, 0.0), Vector3(0.16, 2.9, 3.8))

## Builds a raised platform with visible 4-step Small Platform Stairs + solid BoxShape3D step & ramp collision!
func _build_platform_with_small_stairs(center_pos: Vector3, plat_size: Vector3, stair_side: String, color: Color) -> void:
	# 1. Main raised platform block
	_create_static_box(self, center_pos + Vector3(0.0, plat_size.y * 0.5, 0.0), plat_size, color, 0.35)

	# 2. Visible 4-step small platform stairs + solid BoxShape3D step colliders + tilted BoxShape3D stair ramp
	var step_count := 4
	var stair_run := 1.28
	var stair_width := 2.2
	var step_depth := stair_run / float(step_count)
	var step_h := plat_size.y / float(step_count)
	var dir_x := 1.0 if stair_side == "east" else -1.0
	var edge_x := center_pos.x + dir_x * (plat_size.x * 0.5)

	var sb := StaticBody3D.new()
	sb.position = Vector3(edge_x, 0.0, center_pos.z)
	add_child(sb)

	for i in range(step_count):
		var cur_h := plat_size.y - float(i) * step_h
		var local_cx := dir_x * (float(i) + 0.5) * step_depth
		var mi := MeshInstance3D.new()
		var bm := BoxMesh.new()
		bm.size = Vector3(step_depth, cur_h, stair_width)
		mi.mesh = bm
		mi.position = Vector3(edge_x + local_cx, cur_h * 0.5, center_pos.z)
		var mat := StandardMaterial3D.new()
		mat.albedo_color = color.lightened(0.08 + float(i) * 0.03)
		mat.roughness = 0.35
		mi.material_override = mat
		add_child(mi)

		# Solid BoxShape3D collider for each step tread
		_add_box_collider(sb, Vector3(local_cx, cur_h * 0.5, 0.0), Vector3(step_depth, cur_h, stair_width))

	# Smooth tilted BoxShape3D stair-ramp collider flushed along the step noses
	var slope_len := sqrt(stair_run * stair_run + plat_size.y * plat_size.y)
	var pitch_rad := atan2(plat_size.y, stair_run)
	var ramp_col := CollisionShape3D.new()
	var ramp_box := BoxShape3D.new()
	ramp_box.size = Vector3(slope_len, 0.14, stair_width)
	ramp_col.shape = ramp_box
	ramp_col.position = Vector3(dir_x * stair_run * 0.5, plat_size.y * 0.5 - 0.05, 0.0)
	ramp_col.rotation = Vector3(0.0, 0.0, -dir_x * pitch_rad)
	sb.add_child(ramp_col)

func _create_floating_booth_walls(pos: Vector3) -> void:
	var sb := StaticBody3D.new()
	sb.position = pos
	add_child(sb)
	_add_box_collider(sb, Vector3(-0.55, 1.1, 0.0), Vector3(0.08, 2.2, 1.2))
	_add_box_collider(sb, Vector3(0.55, 1.1, 0.0), Vector3(0.08, 2.2, 1.2))
	_add_box_collider(sb, Vector3(0.0, 1.1, -0.60), Vector3(1.1, 2.2, 0.08))

func _add_box_collider(sb: StaticBody3D, local_pos: Vector3, size: Vector3) -> void:
	var col := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = size
	col.shape = shape
	col.position = local_pos
	sb.add_child(col)

func _spawn_carts_ghosts_and_gremlin() -> void:
	push_cart = MallCartSystem.new()
	push_cart.cart_type = "PushCart"
	push_cart.linked_store_id = "East_F0_02"
	push_cart.dock_world_position = Vector3(20.0, 0.0, -58.0)
	push_cart.store_interior_center = Vector3(30.0, 0.0, -58.0)
	push_cart.position = Vector3(11.2, 0.0, -1.2)
	add_child(push_cart)
	push_cart.place_item_on_cart(PlayerController.make_item_data("proton_gun"), 4)
	push_cart.place_item_on_cart(PlayerController.make_item_data("ectoplasm_canister"), 5)
	push_cart.place_item_on_cart(PlayerController.make_item_data("photo_camera"), 6)
	push_cart.place_item_on_cart(PlayerController.make_item_data("light_remote"), 7)
	_create_floating_label(Vector3(20.0, 2.5, -58.0), "STORE [East_F0_02] DOORWAY CART DOCK\nPark Push Cart across threshold = SEALED\nPush inside store = DORMANT", Color(1.0, 0.88, 0.35))

	light_cart = MallCartSystem.new()
	light_cart.cart_type = "LightCart"
	light_cart.position = Vector3(10.5, 0.0, 2.2)
	light_cart.rotation_degrees = Vector3(0, -90, 0)
	add_child(light_cart)

	# 1. Floor 1 Onboarding Store Ghost: Arthur Pendelton (East_F0_02 — 32m Wide x 26m Deep Bay 0 East!)
	store_ghost_east = StoreGhost.new()
	store_ghost_east.store_id = "East_F0_02"
	store_ghost_east.floor_level = 1
	store_ghost_east.can_briefly_pursue_outside_store = false
	store_ghost_east.position = Vector3(30.0, 0.0, -58.0)
	store_ghost_east.linked_push_cart = push_cart
	add_child(store_ghost_east)

	# 2. Floor 1 Secondary Store Ghost: Static Evelyn (West_F1_03 — 32m Wide x 26m Deep Bay 0 West!)
	store_ghost_west = StoreGhost.new()
	store_ghost_west.store_id = "West_F1_03"
	store_ghost_west.floor_level = 1
	store_ghost_west.can_briefly_pursue_outside_store = false
	store_ghost_west.doorway_threshold_x = -999.0
	store_ghost_west.position = Vector3(-30.0, 0.0, -58.0)
	add_child(store_ghost_west)
	var west_tell := "CORRUPTED (Hostile-Only — Peace Will Backfire!)" if store_ghost_west.is_corrupted_hostile else "RECEPTIVE TO PEACE"
	_create_floating_label(Vector3(-25.5, 2.8, -58.0), "West Boutique Haunting: Static Evelyn [West_F1_03] (Store Territory Bound)\nRolled Run Disposition: %s\n[F] Attempt Peaceful Ritual | Proton Ray Gun + Trap" % west_tell, Color(1.0, 0.5, 0.55) if store_ghost_west.is_corrupted_hostile else Color(0.5, 1.0, 0.75))

	# 3. Upper-Floor Store Ghost (Floor 2 East Mezzanine, Y = 8.50m): The Lost Shopper (East_F2_01 — 32m Wide x 26m Deep!)
	store_ghost_floor2 = StoreGhost.new()
	store_ghost_floor2.store_id = "East_F2_01"
	store_ghost_floor2.floor_level = 2
	store_ghost_floor2.can_briefly_pursue_outside_store = true
	store_ghost_floor2.position = Vector3(30.0, 8.50, -58.0)
	add_child(store_ghost_floor2)
	_create_floating_label(
		Vector3(25.5, 11.3, -58.0),
		"Floor 2 Boutique Haunting: The Lost Shopper [East_F2_01]\n(Upper-Floor Ghost: Leaves store briefly [6s] to continue pursuing players!\nBlock its exit using the edge of the Wheeled Light Cart's Circle of Safety!)",
		Color(0.65, 0.92, 1.0)
	)

	# 4. 95cm Hallway Gremlin Concourse Nuisance Pest (Hallway_Gremlin_LowPoly.glb)
	var gremlin := PlaceableEvidenceActors.HallwayGremlin.new()
	gremlin.position = Vector3(7.5, 0.0, -4.5)
	add_child(gremlin)

func _get_spawn_pos_for_peer(pid: int) -> Vector3:
	if pid == 1:
		return Vector3(2.0, 0.1, 0.0)
	var offsets := [
		Vector3(2.0, 0.1, 1.45),
		Vector3(0.75, 0.1, -1.25),
		Vector3(0.75, 0.1, 1.25),
	]
	return offsets[posmod(pid, offsets.size())]

func _spawn_player() -> void:
	var local_id := MultiplayerManager.get_local_peer_id() if MultiplayerManager != null else 1
	player = _spawn_or_get_peer_player(local_id)

func _spawn_or_get_peer_player(pid: int) -> PlayerController:
	if players_by_peer.has(pid) and is_instance_valid(players_by_peer[pid]):
		var existing := players_by_peer[pid] as PlayerController
		existing.configure_for_peer(pid)
		return existing

	var p := PlayerController.new()
	p.position = _get_spawn_pos_for_peer(pid)
	p.rotation_degrees = Vector3(0, -90, 0)
	p.configure_for_peer(pid)
	p.player_died_with_inventory.connect(_on_player_died_with_inventory)
	p.meal_finished.connect(func() -> void:
		if is_instance_valid(light_cart):
			light_cart.boost_battery(45.0)
	)
	add_child(p)
	p.configure_for_peer(pid)
	players_by_peer[pid] = p
	return p

func _on_coop_local_role_changed(_is_host: bool, my_peer_id: int) -> void:
	# If the local player was originally spawned as peer 1 in solo mode and just joined a server as my_peer_id != 1,
	# reassign our local PlayerController to my_peer_id and spawn Peer #1 as the remote Host avatar!
	if is_instance_valid(player) and player.peer_id != my_peer_id:
		var old_id := player.peer_id
		if players_by_peer.get(old_id) == player:
			players_by_peer.erase(old_id)
		player.configure_for_peer(my_peer_id)
		player.global_position = _get_spawn_pos_for_peer(my_peer_id)
		players_by_peer[my_peer_id] = player
	_sync_peer_player_avatars()

func _on_coop_peer_joined(pid: int) -> void:
	_spawn_or_get_peer_player(pid)
	_sync_peer_player_avatars()

func _on_coop_peer_left(pid: int) -> void:
	if players_by_peer.has(pid):
		var p := players_by_peer[pid] as PlayerController
		players_by_peer.erase(pid)
		if is_instance_valid(p) and p != player:
			p.queue_free()
	_sync_peer_player_avatars()

func _on_coop_peer_list_updated(_peers: Array[int]) -> void:
	_sync_peer_player_avatars()

func _on_coop_network_status_changed(status_text: String) -> void:
	if coop_status_lbl:
		coop_status_lbl.text = status_text

func _sync_peer_player_avatars() -> void:
	var local_id := MultiplayerManager.get_local_peer_id() if MultiplayerManager != null else 1
	for pid in MultiplayerManager.connected_peers:
		var p := _spawn_or_get_peer_player(pid)
		if pid == local_id:
			player = p
	var to_remove: Array[int] = []
	for pid in players_by_peer.keys():
		if int(pid) not in MultiplayerManager.connected_peers and int(pid) != local_id:
			to_remove.append(int(pid))
	for pid in to_remove:
		var old_p := players_by_peer[pid] as Node
		players_by_peer.erase(pid)
		if is_instance_valid(old_p) and old_p != player:
			old_p.queue_free()

## When the player dies, pack their inventory inside a DeathInventoryCrate at their death site!
func _on_player_died_with_inventory(death_pos: Vector3, killer_name: String, stored_items: Array) -> void:
	_spawn_death_crate_local(death_pos, killer_name, stored_items)
	if MultiplayerManager != null and MultiplayerManager.is_coop_active and multiplayer.has_multiplayer_peer():
		_rpc_spawn_death_crate.rpc(death_pos, killer_name, stored_items)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_spawn_death_crate(death_pos: Vector3, killer_name: String, stored_items: Array) -> void:
	_spawn_death_crate_local(death_pos, killer_name, stored_items)

func _spawn_death_crate_local(death_pos: Vector3, killer_name: String, stored_items: Array) -> void:
	var crate := PlaceableEvidenceActors.DeathInventoryCrate.new()
	for it in stored_items:
		crate.stored_items.append((it as Dictionary).duplicate(true))
	crate.killer_name = killer_name
	crate.position = death_pos
	add_child(crate)

func _process(delta: float) -> void:
	if Engine.is_editor_hint():
		return
	if _status_timer > 0.0:
		_status_timer = maxf(0.0, _status_timer - delta)
		if _status_timer <= 0.0 and status_banner:
			status_banner.text = ""

	if Input.is_action_just_pressed("coop_host_quick"):
		MultiplayerManager.host_coop_session()
	elif Input.is_action_just_pressed("coop_join_quick"):
		var ip := coop_ip_input.text.strip_edges() if coop_ip_input != null and not coop_ip_input.text.strip_edges().is_empty() else "127.0.0.1"
		MultiplayerManager.join_coop_session(ip)
	elif Input.is_action_just_pressed("toggle_coop_menu"):
		toggle_coop_menu()

	if Input.is_action_just_pressed("toggle_voice_menu"):
		toggle_voice_menu()

	if Input.is_action_just_pressed("interact") and is_instance_valid(player):
		_handle_pickup_interact()

	if Input.is_action_just_pressed("secondary_action") and is_instance_valid(player):
		_handle_world_object_interact()

	if Input.is_action_just_pressed("toggle_journal") and journal_panel:
		journal_panel.visible = not journal_panel.visible
		if journal_panel.visible:
			_refresh_journal_text()

	_refresh_hud()

func _any_modal_open() -> bool:
	return (
		(voice_menu_panel != null and voice_menu_panel.visible)
		or (coop_menu_panel != null and coop_menu_panel.visible)
		or (trader_menu_panel != null and trader_menu_panel.visible)
	)

func toggle_coop_menu(force_state: Variant = null) -> void:
	if coop_menu_panel == null:
		return
	var next_vis: bool = (not coop_menu_panel.visible) if force_state == null else bool(force_state)
	coop_menu_panel.visible = next_vis
	if next_vis and trader_menu_panel:
		trader_menu_panel.visible = false
	if is_instance_valid(player):
		player.voice_menu_open = _any_modal_open()
	if _any_modal_open():
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
	else:
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

func toggle_trader_menu(force_state: Variant = null) -> void:
	if trader_menu_panel == null:
		return
	var next_vis: bool = (not trader_menu_panel.visible) if force_state == null else bool(force_state)
	trader_menu_panel.visible = next_vis
	if next_vis:
		if voice_menu_panel:
			voice_menu_panel.visible = false
		if coop_menu_panel:
			coop_menu_panel.visible = false
		_refresh_trader_menu_ui()
	if is_instance_valid(player):
		player.voice_menu_open = _any_modal_open()
	if _any_modal_open():
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
	else:
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

func _unhandled_key_input(event: InputEvent) -> void:
	if Engine.is_editor_hint():
		return
	if trader_menu_panel != null and trader_menu_panel.visible:
		if event is InputEventKey and event.pressed and not event.echo:
			var kc_t: int = event.physical_keycode if event.physical_keycode != 0 else event.keycode
			if kc_t >= KEY_1 and kc_t <= KEY_9:
				execute_trader_shop_option(kc_t - KEY_1)
				get_viewport().set_input_as_handled()
			elif kc_t == KEY_0:
				execute_trader_shop_option(9)
				get_viewport().set_input_as_handled()
			elif kc_t == KEY_ESCAPE:
				toggle_trader_menu(false)
				get_viewport().set_input_as_handled()
		return
	if voice_menu_panel == null or not voice_menu_panel.visible:
		return
	if event is InputEventKey and event.pressed and not event.echo:
		var kc: int = event.physical_keycode if event.physical_keycode != 0 else event.keycode
		if kc >= KEY_1 and kc <= KEY_7:
			select_voice_menu_option(kc - KEY_1)
			get_viewport().set_input_as_handled()
		elif kc == KEY_ESCAPE:
			toggle_voice_menu(false)
			get_viewport().set_input_as_handled()

## Toggles the On-Screen Voice Command Menu ([J]) for non-verbal ghost communication.
func toggle_voice_menu(force_state: Variant = null) -> void:
	if voice_menu_panel == null:
		return
	var next_vis: bool = (not voice_menu_panel.visible) if force_state == null else bool(force_state)
	voice_menu_panel.visible = next_vis
	if next_vis and trader_menu_panel:
		trader_menu_panel.visible = false
	if is_instance_valid(player):
		player.voice_menu_open = _any_modal_open()
	if _any_modal_open():
		_refresh_voice_menu_status()
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
	else:
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

## Speaks one of the 7 options from the On-Screen Voice Command Menu ([J]).
func select_voice_menu_option(idx: int) -> void:
	if idx < 0 or idx >= VOICE_MENU_OPTIONS.size() or not is_instance_valid(player):
		return
	var phrase: String = VOICE_MENU_OPTIONS[idx]["phrase"]
	toggle_voice_menu(false)
	player.speak_voice_prompt(phrase)

func _refresh_voice_menu_status() -> void:
	if voice_menu_status_lbl == null or not is_instance_valid(player):
		return
	var freq := player.spirit_box_channels[player.spirit_box_channel_idx]
	var cur_item := player.get_current_active_item()
	var holding_sb: bool = (str(cur_item.get("id", "")) == "spirit_box")
	voice_menu_status_lbl.text = (
		"Spirit Box Channel: %.1f FM (%s) | Click any phrase below or press [1]–[7] to speak aloud:" % [
			freq,
			"ACTIVE IN HAND" if holding_sb else "Equip Spirit Box [3] for EVP Radio Replies"
		]
	)

## Computes how closely the player's crosshair ray (`look_origin`, `look_fwd`) aims at `target_pos`.
## Returns `-1.0` if the item is out of range OR the player is not looking at it!
## Otherwise returns a lower-is-better alignment score (`perp_dist + dist * 0.08`).
func _get_look_target_score(
	look_origin: Vector3,
	look_fwd: Vector3,
	target_pos: Vector3,
	max_range: float = 2.85,
	hit_radius: float = 0.28,
	min_dot: float = 0.88
) -> float:
	if not is_instance_valid(player):
		return -1.0
	if absf(target_pos.y - player.global_position.y) > 2.45:
		return -1.0
	var to_target := target_pos - look_origin
	var dist := to_target.length()
	if dist < 0.12 or dist > max_range:
		return -1.0
	var ray_proj := to_target.dot(look_fwd)
	if ray_proj <= 0.12:
		return -1.0
	var dot_val := ray_proj / dist
	var perp_dist := (to_target - look_fwd * ray_proj).length()
	# Must be both within the angular cone OR tight perpendicular radius around the crosshair ray!
	if dot_val < min_dot or perp_dist > hit_radius:
		return -1.0
	return perp_dist + dist * 0.08

## [F] ("interact"): Strictly reserved for PICKING UP items that the player is IN RANGE OF AND LOOKING DIRECTLY AT!
## Never uses proximity-only fallbacks — evaluates all looked-at pickup targets and selects the one closest to the crosshair ray.
func _handle_pickup_interact() -> void:
	if player.is_floating_essence:
		GameState.status_message.emit(
			"Cannot pick up items while in Ectoplasm Form! Float to the West Himalayan Sauna Biome and press [E] or [J] to resurrect.",
			Color(0.35, 1.0, 0.95)
		)
		return

	var ppos := player.global_position
	var look_cam := player.main_camera if player.main_camera else null
	var look_origin := look_cam.global_position if look_cam else (ppos + Vector3(0.0, 1.4, 0.0))
	var look_fwd := (-look_cam.global_transform.basis.z.normalized()) if look_cam else (-player.global_transform.basis.z.normalized())

	var best_kind := ""
	var best_node: Object = null
	var best_score := 999.0

	# 1. Mounted Cart CCTV Monitor (must be looked at directly!)
	for mon_node in get_tree().get_nodes_in_group("cart_computer_monitors"):
		var mon := mon_node as VideoAndAudioSurveillance.CartComputerMonitor
		if mon and mon.is_mounted_on_cart:
			var sc := _get_look_target_score(look_origin, look_fwd, mon.global_position, 2.85, 0.25, 0.88)
			if sc >= 0.0 and sc < best_score:
				best_score = sc
				best_kind = "cart_monitor"
				best_node = mon

	# 2. Highlighted (looked-at) item on an 8-Slot Equipment Push Cart shelf or exterior hook
	for pc_node in get_tree().get_nodes_in_group("push_carts"):
		var pc := pc_node as MallCartSystem
		if pc and ppos.distance_to(pc.global_position) <= 2.95:
			if pc.highlighted_take_slot >= 0 and pc.highlighted_take_slot < pc.cart_slot_items.size() and not pc.cart_slot_items[pc.highlighted_take_slot].is_empty():
				var slot_world := pc.to_global(MallCartSystem.PUSH_CART_SLOT_OFFSETS[pc.highlighted_take_slot] + Vector3(0.0, 0.08, 0.0))
				var sc := _get_look_target_score(look_origin, look_fwd, slot_world, 2.85, 0.24, 0.88)
				if sc >= 0.0 and sc < best_score:
					best_score = sc
					best_kind = "cart_slot"
					best_node = pc
			elif pc.highlighted_hook_take_slot >= 0 and pc.highlighted_hook_take_slot < pc.exterior_hook_items.size() and not pc.exterior_hook_items[pc.highlighted_hook_take_slot].is_empty():
				var hook_world := pc.to_global(MallCartSystem.EXTERIOR_HOOK_OFFSETS[pc.highlighted_hook_take_slot])
				var sc := _get_look_target_score(look_origin, look_fwd, hook_world, 2.85, 0.25, 0.88)
				if sc >= 0.0 and sc < best_score:
					best_score = sc
					best_kind = "cart_slot"
					best_node = pc

	# 3. WorldDroppedItems on the Equipment Table or floor (must be looked at directly!)
	for d_node in get_tree().get_nodes_in_group("world_dropped_items"):
		var drop := d_node as PlaceableEvidenceActors.WorldDroppedItem
		if drop:
			var sc := _get_look_target_score(look_origin, look_fwd, drop.global_position + Vector3(0.0, 0.12, 0.0), 2.85, 0.28, 0.88)
			if sc >= 0.0 and sc < best_score:
				best_score = sc
				best_kind = "world_drop"
				best_node = drop

	# 4. Death Inventory Crate (must be looked at!)
	for c_node in get_tree().get_nodes_in_group("death_inventory_crates"):
		var crate := c_node as PlaceableEvidenceActors.DeathInventoryCrate
		if crate:
			var sc := _get_look_target_score(look_origin, look_fwd, crate.global_position + Vector3(0.0, 0.20, 0.0), 2.85, 0.36, 0.86)
			if sc >= 0.0 and sc < best_score:
				best_score = sc
				best_kind = "death_crate"
				best_node = crate

	# 5. Ectoplasm Canister Trap on the floor (must be looked at!)
	for can_node in get_tree().get_nodes_in_group("ectoplasm_canisters"):
		var can_trap := can_node as PlaceableEvidenceActors.EctoplasmCanisterTrap
		if can_trap:
			var sc := _get_look_target_score(look_origin, look_fwd, can_trap.global_position + Vector3(0.0, 0.14, 0.0), 2.85, 0.30, 0.88)
			if sc >= 0.0 and sc < best_score:
				best_score = sc
				best_kind = "canister"
				best_node = can_trap

	# 6. Placeable Infrared Motion Sensor on the floor (must be looked at!)
	for ms_node in get_tree().get_nodes_in_group("placeable_motion_sensors"):
		var ms := ms_node as PlaceableEvidenceActors.PlaceableMotionSensor
		if ms:
			var sc := _get_look_target_score(look_origin, look_fwd, ms.global_position + Vector3(0.0, 0.18, 0.0), 2.85, 0.30, 0.88)
			if sc >= 0.0 and sc < best_score:
				best_score = sc
				best_kind = "motion_sensor"
				best_node = ms

	# 7. PlacedVideoCameraActor (standalone camera or 1.45m tripod rig — check both base and head height!)
	for cam_node in get_tree().get_nodes_in_group("placed_video_cameras"):
		var pcam := cam_node as VideoAndAudioSurveillance.PlacedVideoCameraActor
		if pcam:
			var head_h := 1.35 if pcam.is_on_tripod_stand else 0.22
			var sc_head := _get_look_target_score(look_origin, look_fwd, pcam.global_position + Vector3(0.0, head_h, 0.0), 2.85, 0.32, 0.87)
			var sc_base := _get_look_target_score(look_origin, look_fwd, pcam.global_position + Vector3(0.0, 0.25, 0.0), 2.85, 0.32, 0.87)
			var sc := sc_head if (sc_head >= 0.0 and (sc_base < 0.0 or sc_head < sc_base)) else sc_base
			if sc >= 0.0 and sc < best_score:
				best_score = sc
				best_kind = "placed_camera"
				best_node = pcam

	# 8. ParanormalAudioRecorderActor, LaserGridDomeActor, EASPedestalWardActor (must be looked at!)
	for rec_node in get_tree().get_nodes_in_group("audio_recorders"):
		var rec := rec_node as VideoAndAudioSurveillance.ParanormalAudioRecorderActor
		if rec:
			var sc := _get_look_target_score(look_origin, look_fwd, rec.global_position + Vector3(0.0, 0.12, 0.0), 2.85, 0.28, 0.88)
			if sc >= 0.0 and sc < best_score:
				best_score = sc
				best_kind = "audio_recorder"
				best_node = rec
	for dome_node in get_tree().get_nodes_in_group("laser_grid_domes"):
		var dome := dome_node as ExpandedMallGadgets.LaserGridDomeActor
		if dome:
			var sc := _get_look_target_score(look_origin, look_fwd, dome.global_position + Vector3(0.0, 0.14, 0.0), 2.85, 0.28, 0.88)
			if sc >= 0.0 and sc < best_score:
				best_score = sc
				best_kind = "laser_dome"
				best_node = dome
	for ward_node in get_tree().get_nodes_in_group("eas_pedestal_wards"):
		var ward := ward_node as ExpandedMallGadgets.EASPedestalWardActor
		if ward and not ward.is_burned_out:
			var sc := _get_look_target_score(look_origin, look_fwd, ward.global_position + Vector3(0.0, 0.55, 0.0), 2.85, 0.34, 0.87)
			if sc >= 0.0 and sc < best_score:
				best_score = sc
				best_kind = "eas_ward"
				best_node = ward

	# Execute ONLY the single best looked-at pickup candidate!
	match best_kind:
		"cart_monitor":
			var mon := best_node as VideoAndAudioSurveillance.CartComputerMonitor
			var mon_item := mon.unmount_to_hand_item()
			if not mon_item.is_empty():
				if mon.parent_cart != null and mon.parent_cart.has_method("_refresh_push_cart_header_label"):
					mon.parent_cart.call("_refresh_push_cart_header_label")
				player.receive_item_from_cart(mon_item)
				player.play_flashlight_click(true)
				GameState.status_message.emit(
					"Picked up Portable CCTV Monitor from Cart into HANDS [F]! ([LMB] Power ON/OFF | [RMB] Cycle Camera Locations | Look at Cart Mount + [G] to Mount Back)",
					Color(0.45, 0.96, 1.0)
				)
				return
		"cart_slot":
			var pc := best_node as MallCartSystem
			var taken := pc.take_highlighted_item_from_cart()
			if not taken.is_empty():
				player.receive_item_from_cart(taken)
				GameState.status_message.emit(
					"Picked up %s [F] off Equipment Push Cart (%d/8 stored on cart)." % [taken.get("name", "Tool"), pc.get_stored_item_count()],
					Color(1.0, 0.90, 0.42)
				)
				return
		"world_drop":
			var drop := best_node as PlaceableEvidenceActors.WorldDroppedItem
			player.pickup_world_item(drop)
			return
		"death_crate":
			var crate := best_node as PlaceableEvidenceActors.DeathInventoryCrate
			player.pickup_death_crate_in_hands(crate)
			return
		"canister":
			var can_trap := best_node as PlaceableEvidenceActors.EctoplasmCanisterTrap
			if can_trap.is_captured:
				var filled_data := PlayerController.make_filled_canister_item_data(
					can_trap.captured_entity_name,
					can_trap.captured_bounty_value,
					can_trap.captured_store_id
				)
				can_trap.remove_from_group("ectoplasm_canisters")
				can_trap.queue_free()
				player.receive_item_from_cart(filled_data)
				GameState.status_message.emit(
					"Picked up FILLED Ectoplasm Canister (%s — Worth $%d)! Take it to Trader Chen and press [E] to sell it!" % [
						filled_data.get("captured_entity", "Ghost"), int(filled_data.get("bounty_value", 180))
					],
					Color(1.0, 0.88, 0.35)
				)
			else:
				var can_data := PlayerController.make_item_data("ectoplasm_canister")
				can_data["deployed_seconds_left"] = can_trap.deployed_seconds_left
				can_trap.remove_from_group("ectoplasm_canisters")
				can_trap.queue_free()
				player.receive_item_from_cart(can_data)
				var max_t := GameState.get_canister_max_deployed_time()
				GameState.status_message.emit(
					"Picked up Ectoplasm Canister (Recharging battery: %.1fs / %.0fs) — press [LMB] or [G] to set on floor, then [E] to kick & open!" % [
						float(can_data["deployed_seconds_left"]), max_t
					],
					Color(0.35, 1.0, 0.68)
				)
			return
		"motion_sensor":
			var ms := best_node as PlaceableEvidenceActors.PlaceableMotionSensor
			var ms_data := PlayerController.make_item_data("motion_sensor")
			ms.remove_from_group("placeable_motion_sensors")
			ms.queue_free()
			player.receive_item_from_cart(ms_data)
			GameState.status_message.emit("Picked up Infrared Motion Sensor [F]!", Color(0.45, 1.0, 0.75))
			return
		"placed_camera":
			var pcam := best_node as VideoAndAudioSurveillance.PlacedVideoCameraActor
			var cam_data: Dictionary = pcam.to_pickup_item_data()
			pcam.remove_from_group("placed_video_cameras")
			pcam.queue_free()
			player.receive_item_from_cart(cam_data)
			GameState.status_message.emit("Picked up %s [F]!" % cam_data.get("name", "Video Camera"), Color(0.45, 1.0, 0.75))
			return
		"audio_recorder":
			var rec := best_node as VideoAndAudioSurveillance.ParanormalAudioRecorderActor
			var rec_data: Dictionary = rec.to_pickup_item_data()
			rec.remove_from_group("audio_recorders")
			rec.queue_free()
			player.receive_item_from_cart(rec_data)
			GameState.status_message.emit("Picked up %s [F]!" % rec_data.get("name", "Audio Recorder"), Color(0.45, 1.0, 0.75))
			return
		"laser_dome":
			var dome := best_node as ExpandedMallGadgets.LaserGridDomeActor
			var d_data := PlayerController.make_item_data("laser_grid_dome")
			dome.remove_from_group("laser_grid_domes")
			dome.queue_free()
			player.receive_item_from_cart(d_data)
			GameState.status_message.emit("Picked up D.O.T.S. Laser Grid Projector Dome [F]!", Color(0.45, 1.0, 0.75))
			return
		"eas_ward":
			var ward := best_node as ExpandedMallGadgets.EASPedestalWardActor
			var w_data := PlayerController.make_item_data("eas_pedestal_ward")
			ward.remove_from_group("eas_pedestal_wards")
			ward.queue_free()
			player.receive_item_from_cart(w_data)
			GameState.status_message.emit("Picked up Hacked EAS Anti-Theft Pedestal Ward [F]!", Color(0.45, 0.95, 1.0))
			return

	GameState.status_message.emit(
		"Look directly at an item within reach and press [F] to pick it up! (Use [E] to Push/Release Carts & interact with world fixtures.)",
		Color(0.75, 0.84, 0.94)
	)

## [E] ("secondary_action"): Strictly reserved for INTERACTING WITH WORLD OBJECTS THAT CANNOT BE PICKED UP
## (Push/Release Carts, Elevator UP/DOWN Physical Buttons, Rotate Placed Camera +35°, Kick Open Deployed Canisters, Benches, Sauna, Food Court, Trader Chen, Hiding Spots, etc.)!
## Note: The Cart CCTV Monitor NEVER intercepts [E] — use [F] to pick up the monitor into Hands, [G] to mount it on the cart, [LMB] for Power, and [RMB] to cycle cameras!
func _handle_world_object_interact() -> void:
	var ppos := player.global_position
	var look_cam := player.main_camera if player.main_camera else null
	var look_origin := look_cam.global_position if look_cam else (ppos + Vector3(0.0, 1.4, 0.0))
	var look_fwd := (-look_cam.global_transform.basis.z.normalized()) if look_cam else (-player.global_transform.basis.z.normalized())

	# 1. HIGHEST PRIORITY: If the player is ALREADY pushing a cart, pressing [E] IMMEDIATELY releases/parks that cart!
	#    (Unless looking straight up at the Light Cart's overhead floodlight lamp head to toggle the lamp!)
	var detached_any := false
	for node in get_tree().get_nodes_in_group("mall_carts"):
		var c := node as MallCartSystem
		if c and c.is_attached_to_player and c.attached_player == player:
			if c.cart_type == "LightCart":
				var to_lamp := ((c.global_position + Vector3(0.0, 1.55, 0.0)) - look_origin).normalized()
				if look_fwd.dot(to_lamp) > 0.72:
					c.toggle_floodlight("Player Switch [E]")
					return
			c.detach_from_player(false)
			detached_any = true
	if detached_any:
		return

	# 2. If standing at either of the 2 Spaced-Out Physical Elevator Buttons (Left ▲ UP Button or Right ▼ DOWN Button) or a Call Pedestal, press it with [E]!
	if is_instance_valid(freight_elevator):
		var cab_up_pos := freight_elevator.to_global(Vector3(-freight_elevator.cab_width * 0.5 + 0.45, freight_elevator.current_cab_y, 0.0))
		var cab_down_pos := freight_elevator.to_global(Vector3(freight_elevator.cab_width * 0.5 - 0.45, freight_elevator.current_cab_y, 0.0))
		if ppos.distance_to(cab_up_pos) <= 2.4 or ppos.distance_to(cab_down_pos) <= 2.4 or freight_elevator.is_player_near_any_elevator_control(ppos):
			if player.consume_carried_item_id("heavy_hydraulic"):
				freight_elevator.install_hydraulic_overdrive()
			freight_elevator.activate_elevator_for_player(player)
			return

	# 4. If standing near an EctoplasmCanisterTrap set on the floor (<= 2.6m), pressing [E] KICKS IT & OPENS IT to start sucking the ghost in!
	if not player.is_floating_essence:
		for can_node in get_tree().get_nodes_in_group("ectoplasm_canisters"):
			var can_trap := can_node as PlaceableEvidenceActors.EctoplasmCanisterTrap
			if can_trap and ppos.distance_to(can_trap.global_position) < 2.6:
				var kick_dir := -player.global_transform.basis.z.normalized()
				can_trap.kick_and_open(kick_dir)
				return

	# 5. If standing near a PlacedVideoCameraActor on the floor (<= 2.4m), pressing [E] attaches a carried Tripod Stand or pans its motorized head (+35°)!
	if not player.is_floating_essence:
		for cam_node in get_tree().get_nodes_in_group("placed_video_cameras"):
			var pcam := cam_node as VideoAndAudioSurveillance.PlacedVideoCameraActor
			if pcam and ppos.distance_to(pcam.global_position) < 2.4:
				if not pcam.is_on_tripod_stand and player.consume_carried_item_id("camera_stand"):
					pcam.attach_tripod_stand()
					return
				pcam.pan_camera(35.0)
				return

	# 6. If standing near a ParanormalAudioRecorderActor on the floor (<= 2.4m), pressing [E] triggers EVP Playback (+$75 Bounty & Tell)!
	if not player.is_floating_essence:
		for rec_node in get_tree().get_nodes_in_group("audio_recorders"):
			var rec := rec_node as VideoAndAudioSurveillance.ParanormalAudioRecorderActor
			if rec and ppos.distance_to(rec.global_position) < 2.4:
				rec.trigger_playback(true)
				return

	# 7. If standing near a UVWallStencilActor (<= 2.4m), pressing [E] cycles its glowing wayfinding symbol!
	if not player.is_floating_essence:
		for st_node in get_tree().get_nodes_in_group("uv_wall_stencils"):
			var st := st_node as ExpandedMallGadgets.UVWallStencilActor
			if st and ppos.distance_to(st.global_position) < 2.4:
				st.cycle_symbol()
				return

	# 8. Attach/Push the Cart (Push Cart OR Light Cart) within 2.85m that the player is looking at (or closest) — or toggle Light Cart floodlight if aiming up at its lamp!
	if not player.is_floating_essence:
		var best_cart: MallCartSystem = null
		var best_cart_score := 999.0
		for node in get_tree().get_nodes_in_group("mall_carts"):
			var c := node as MallCartSystem
			if c:
				var d := ppos.distance_to(c.global_position)
				if d < 2.85:
					var to_c := (c.global_position + Vector3(0.0, 0.9, 0.0)) - look_origin
					var align := look_fwd.dot(to_c.normalized())
					var score := d - maxf(0.0, align) * 1.25
					if score < best_cart_score:
						best_cart_score = score
						best_cart = c
		if best_cart:
			if best_cart.cart_type == "LightCart":
				var to_lamp := ((best_cart.global_position + Vector3(0.0, 1.55, 0.0)) - look_origin).normalized()
				if look_fwd.dot(to_lamp) > 0.72:
					best_cart.toggle_floodlight("Player Switch [E]")
					return
			best_cart.toggle_attach(player)
			return

	# 9. Concourse Rest Bench (-6.5, 0, 6.5) -> 3rd-Person Rest
	if ppos.distance_to(Vector3(-6.5, 0.0, 6.5)) < 3.2 or ppos.distance_to(Vector3(-4.5, 0.0, 4.5)) < 3.0:
		player.enter_contextual_third_person(Vector3(-6.5, 0.0, 6.5), "Sitting on Concourse Rest Bench (+24 Stamina/s)")
		return

	# 10. Whole-West-Side Floor 1 Himalayan Pink Salt & Tangquan Sauna Liminal Biome (X = -32m .. -112m, Z = -38m .. +38m)
	if (
		ppos.distance_to(Vector3(-48.0, 0.0, 0.0)) < 7.5
		or ppos.distance_to(Vector3(-50.0, 0.0, -20.0)) < 6.5
		or ppos.distance_to(Vector3(-50.0, 0.0, 20.0)) < 6.5
		or (ppos.x <= -68.0 and ppos.x >= -112.0 and absf(ppos.z) <= 36.0 and absf(ppos.y) < 3.5)
		or ppos.distance_to(Vector3(-11.0, 0.0, 11.0)) < 4.5
	):
		if player.is_floating_essence:
			player.revive_from_sauna_ritual()
		else:
			GameState.sauna_soul_anchor_lit = true
			var soak_target := Vector3(-48.0, 0.28, 0.0) if ppos.x <= -30.0 else Vector3(-11.0, 0.28, 11.0)
			player.enter_contextual_third_person(soak_target, "Soaking in Himalayan Pink Salt & Tangquan Sauna Biome (+60s Ward & Soul-Anchor Lit!)")
		return

	# 11. North Night-Market Food Court (-18.0, 0.0, -32.0) -> Sit down at table in 3rd-Person View for 5.0s minimum & watch bar fill up!
	if ppos.distance_to(Vector3(-18.0, 0.0, -32.0)) < 4.2 or ppos.distance_to(Vector3(-11.0, 0.0, -5.5)) < 3.6:
		if player.is_eating_meal:
			GameState.status_message.emit(
				"Still eating Chongqing Hot Pot at the table (%.1fs / %.1fs)..." % [player.eating_timer, player.eating_duration],
				Color(1.0, 0.84, 0.38)
			)
		elif player.contextual_seat_active:
			player.exit_contextual_third_person()
		else:
			var eat_seat := Vector3(-18.0, 0.0, -32.75) if ppos.z < -20.0 else Vector3(-11.0, 0.0, -6.25)
			var eat_look := Vector3(-18.0, 0.0, -32.0) if ppos.z < -20.0 else Vector3(-11.0, 0.0, -5.5)
			player.start_eating_at_table(eat_seat, eat_look, 5.0)
		return

	# 11B. Animation Comparison Showcase Models (Look at model within 3.8m and press [E] to cycle animations!)
	for comp_node in get_tree().get_nodes_in_group("anim_comparison_models"):
		var cn := comp_node as Node3D
		if cn and ppos.distance_to(cn.global_position) <= 3.8:
			var to_cn := (cn.global_position + Vector3(0.0, 1.0, 0.0)) - look_origin
			if look_fwd.dot(to_cn.normalized()) >= 0.82:
				var ctrl: HumanoidIKAndAnimController = cn.get_meta("anim_controller", null) as HumanoidIKAndAnimController
				if ctrl:
					var next_a := ctrl.cycle_next_animation()
					cn.set_meta("npc_name", "Animation Showcase (%s)" % next_a)
					var lbl: Label3D = cn.get_meta("anim_label", null) as Label3D
					if lbl:
						lbl.text = "ACTIVE ANIMATION: %s\n[Look + Press E to Cycle All 8 Animations]" % next_a
					GameState.status_message.emit(
						"Animation Showcase switched to: %s (Look-At IK Active!)" % next_a,
						Color(0.55, 0.96, 1.0)
					)
					return

	# 12. Quartermaster Chen's Trader & Upgrade Counter (22.8, 0.48, 0.0)
	if ppos.distance_to(Vector3(22.8, 0.48, 0.0)) < 4.2 or ppos.distance_to(Vector3(-14.2, 0.48, 0.0)) < 3.8:
		player.sell_all_carried_filled_canisters()
		player._recharge_carried_canisters(100.0)
		toggle_trader_menu()
		return

	# 13. Escalator Breaker Control Dais (-22.5, 0.48, 22.2) or Void-Spanning Escalator Lower Entrance (-19.82, 0, 22.2)
	if (
		ppos.distance_to(Vector3(-22.5, 0.48, 22.2)) < 4.2
		or ppos.distance_to(Vector3(-19.82, 0.0, 22.2)) < 4.2
		or ppos.distance_to(Vector3(-7.2, 0.48, 15.2)) < 3.8
		or ppos.distance_to(Vector3(-8.66, 0.0, 17.5)) < 3.8
		or ppos.distance_to(Vector3(5.2, 0.48, 4.2)) < 3.8
	):
		if player.consume_carried_item_id("escalator_fuse"):
			GameState.power_escalator_circuit("EscalatorCircuit_F1_To_F2")
			if is_instance_valid(escalator_up):
				escalator_up.set_operational_mode(MallEscalator.MaintenanceState.OPERATIONAL, true)
			return
		if is_instance_valid(escalator_up):
			var new_run := not escalator_up.running
			escalator_up.set_operational_mode(
				MallEscalator.MaintenanceState.OPERATIONAL if new_run else MallEscalator.MaintenanceState.STALLED_WALKABLE,
				new_run
			)
			GameState.status_message.emit(
				"Escalator Breaker Toggled [E]! South Escalator is now %s (Walk directly onto steps to ride/climb to Floor 2!)." % ("MOTORIZED (2.15 m/s)" if new_run else "STALLED WALKABLE STAIRS"),
				Color(0.45, 0.95, 1.0)
			)
		return

	# 14. Fitting Room Hiding Booth (24.5, 0.0, -64.0)
	if ppos.distance_to(Vector3(24.5, 0.0, -64.0)) < 3.0 or ppos.distance_to(Vector3(18.5, 0.0, -5.2)) < 2.8:
		var witnessed := (store_ghost_east.current_phase == StoreGhost.GhostPhase.VISIBLE_HUNT and store_ghost_east.global_position.distance_to(ppos) < 5.0)
		player.is_hidden_without_witness = not witnessed
		var hide_target := Vector3(24.5, 0.0, -64.0) if ppos.z < -30.0 else Vector3(18.5, 0.0, -5.2)
		player.enter_contextual_third_person(hide_target, "Hidden in Fitting Room Booth (%s)" % ("WITNESSED!" if witnessed else "Undetected"))
		return

	# 15. Store East_F0_02 Ledger & Peaceful Lore Resolution Stand (25.5, 0.0, -54.5)
	if ppos.distance_to(Vector3(25.5, 0.0, -54.5)) < 4.2 or ppos.distance_to(Vector3(19.5, 0.0, 2.2)) < 3.4:
		GameState.record_evidence("East_F0_02", "Ghost Writing", 40)
		GameState.discover_lore("LORE_LEDGER_EAST_F0_02", true)
		store_ghost_east.try_peaceful_resolution()
		return

	# 16. Store West_F1_03 Peaceful Ritual Attempt (-25.5, 0.0, -58.0) — May Backfire if Corrupted!
	if ppos.distance_to(Vector3(-25.5, 0.0, -58.0)) < 4.5 or ppos.distance_to(Vector3(-30.0, 0.0, -58.0)) < 4.5 or ppos.distance_to(Vector3(-19.5, 0.0, -8.5)) < 3.8:
		store_ghost_west.try_peaceful_resolution()
		return

	# 17. Wall-Mounted Cart Battery Charger Station (12.2, 0, 4.2)
	if is_instance_valid(cart_charger) and ppos.distance_to(cart_charger.global_position) < 3.2:
		cart_charger.interact_charger()
		return

	# 18. "Ask Zoltar" Coin-Op Fortune Booth (-10.5, 0, 8.5)
	if is_instance_valid(zoltar_booth) and ppos.distance_to(zoltar_booth.global_position) < 3.2:
		zoltar_booth.interact_zoltar(player)
		return

	# 19. Campaign Expansion Interactions (All 5 Hiding Spot Types, Stores #2/#4/#6 Ledgers, Lore Ledgers & UV Ciphers, F2->F3 Escalator, and Floor 3 Elena Vance Ark Extraction Console!)
	if is_instance_valid(campaign_expansion) and campaign_expansion.try_handle_campaign_interact(player):
		return
	if is_instance_valid(campaign_expansion) and campaign_expansion.try_handle_campaign_secondary_action(player):
		return

	GameState.status_message.emit(
		"Step closer to a Cart, Cart CCTV Monitor, Elevator Button, Deployed Canister, Bench, Sauna, Trader Chen, or Breaker and press [E]! (Use [F] to pick up items.)",
		Color(0.75, 0.82, 0.9)
	)

func _build_hud_and_journal() -> void:
	var canvas := CanvasLayer.new()
	add_child(canvas)

	var root_ctrl := Control.new()
	root_ctrl.set_anchors_preset(Control.PRESET_FULL_RECT)
	root_ctrl.mouse_filter = Control.MOUSE_FILTER_IGNORE
	canvas.add_child(root_ctrl)

	var crosshair := Label.new()
	crosshair.text = "+"
	crosshair.set_anchors_preset(Control.PRESET_CENTER)
	crosshair.add_theme_font_size_override("font_size", 22)
	crosshair.modulate = Color(0.85, 0.95, 1.0, 0.8)
	root_ctrl.add_child(crosshair)

	var hud_panel := PanelContainer.new()
	hud_panel.position = Vector2(18, 18)
	hud_panel.custom_minimum_size = Vector2(740, 275)
	root_ctrl.add_child(hud_panel)

	hud_label = RichTextLabel.new()
	hud_label.bbcode_enabled = true
	hud_label.fit_content = true
	hud_label.scroll_active = false
	hud_label.custom_minimum_size = Vector2(720, 260)
	hud_panel.add_child(hud_label)

	# Top-Right Co-op Multiplayer & Teammate Radar Panel
	var coop_hud_panel := PanelContainer.new()
	coop_hud_panel.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	coop_hud_panel.offset_left = -435
	coop_hud_panel.offset_right = -18
	coop_hud_panel.offset_top = 18
	coop_hud_panel.offset_bottom = 178
	root_ctrl.add_child(coop_hud_panel)

	coop_hud_label = RichTextLabel.new()
	coop_hud_label.bbcode_enabled = true
	coop_hud_label.fit_content = true
	coop_hud_label.scroll_active = false
	coop_hud_label.custom_minimum_size = Vector2(400, 145)
	coop_hud_panel.add_child(coop_hud_label)

	status_banner = Label.new()
	status_banner.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
	status_banner.offset_left = -560
	status_banner.offset_right = 560
	status_banner.offset_top = -152
	status_banner.offset_bottom = -114
	status_banner.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	status_banner.add_theme_font_size_override("font_size", 18)
	root_ctrl.add_child(status_banner)

	# Bottom-of-Screen 4-Square Hotbar GUI ([1]-[4] + Mouse Wheel) + Hands (Pseudo-5th Slot) Badge
	hotbar_gui_container = HBoxContainer.new()
	hotbar_gui_container.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
	hotbar_gui_container.offset_left = -268
	hotbar_gui_container.offset_right = 268
	hotbar_gui_container.offset_top = -108
	hotbar_gui_container.offset_bottom = -14
	hotbar_gui_container.alignment = BoxContainer.ALIGNMENT_CENTER
	hotbar_gui_container.add_theme_constant_override("separation", 10)
	root_ctrl.add_child(hotbar_gui_container)

	hotbar_slot_panels.clear()
	hotbar_slot_styles.clear()
	hotbar_slot_icons.clear()
	hotbar_slot_names.clear()
	hotbar_slot_subs.clear()

	for i in range(4):
		var slot_panel := PanelContainer.new()
		slot_panel.custom_minimum_size = Vector2(92, 92)
		var style := StyleBoxFlat.new()
		style.bg_color = Color(0.08, 0.11, 0.16, 0.88)
		style.border_width_left = 3
		style.border_width_top = 3
		style.border_width_right = 3
		style.border_width_bottom = 3
		style.border_color = Color(0.26, 0.34, 0.44, 0.90)
		style.corner_radius_top_left = 6
		style.corner_radius_top_right = 6
		style.corner_radius_bottom_left = 6
		style.corner_radius_bottom_right = 6
		style.content_margin_left = 6
		style.content_margin_right = 6
		style.content_margin_top = 4
		style.content_margin_bottom = 4
		slot_panel.add_theme_stylebox_override("panel", style)

		var vbox := VBoxContainer.new()
		vbox.alignment = BoxContainer.ALIGNMENT_BEGIN
		vbox.add_theme_constant_override("separation", 2)
		slot_panel.add_child(vbox)

		var key_lbl := Label.new()
		key_lbl.text = "[%d]" % (i + 1)
		key_lbl.add_theme_font_size_override("font_size", 11)
		key_lbl.modulate = Color(0.68, 0.82, 0.95)
		vbox.add_child(key_lbl)

		var icon_lbl := Label.new()
		icon_lbl.text = "[ ]"
		icon_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		icon_lbl.add_theme_font_size_override("font_size", 17)
		vbox.add_child(icon_lbl)

		var name_lbl := Label.new()
		name_lbl.text = "EMPTY"
		name_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		name_lbl.add_theme_font_size_override("font_size", 11)
		vbox.add_child(name_lbl)

		var sub_lbl := Label.new()
		sub_lbl.text = "---"
		sub_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		sub_lbl.add_theme_font_size_override("font_size", 10)
		sub_lbl.modulate = Color(0.70, 0.80, 0.88)
		vbox.add_child(sub_lbl)

		hotbar_gui_container.add_child(slot_panel)
		hotbar_slot_panels.append(slot_panel)
		hotbar_slot_styles.append(style)
		hotbar_slot_icons.append(icon_lbl)
		hotbar_slot_names.append(name_lbl)
		hotbar_slot_subs.append(sub_lbl)

	# Pseudo-5th "HANDS" Indicator Square next to the 4 hotbar squares
	hand_slot_panel = PanelContainer.new()
	hand_slot_panel.custom_minimum_size = Vector2(114, 92)
	hand_slot_style = StyleBoxFlat.new()
	hand_slot_style.bg_color = Color(0.07, 0.09, 0.12, 0.72)
	hand_slot_style.border_width_left = 2
	hand_slot_style.border_width_top = 2
	hand_slot_style.border_width_right = 2
	hand_slot_style.border_width_bottom = 2
	hand_slot_style.border_color = Color(0.28, 0.30, 0.35, 0.75)
	hand_slot_style.corner_radius_top_left = 6
	hand_slot_style.corner_radius_top_right = 6
	hand_slot_style.corner_radius_bottom_left = 6
	hand_slot_style.corner_radius_bottom_right = 6
	hand_slot_style.content_margin_left = 6
	hand_slot_style.content_margin_right = 6
	hand_slot_style.content_margin_top = 4
	hand_slot_style.content_margin_bottom = 4
	hand_slot_panel.add_theme_stylebox_override("panel", hand_slot_style)

	var h_vbox := VBoxContainer.new()
	h_vbox.add_theme_constant_override("separation", 2)
	hand_slot_panel.add_child(h_vbox)

	var h_title := Label.new()
	h_title.text = "[HANDS (5th)]"
	h_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	h_title.add_theme_font_size_override("font_size", 10)
	h_title.modulate = Color(1.0, 0.80, 0.42)
	h_vbox.add_child(h_title)

	hand_slot_icon = Label.new()
	hand_slot_icon.text = "[ - ]"
	hand_slot_icon.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	hand_slot_icon.add_theme_font_size_override("font_size", 16)
	h_vbox.add_child(hand_slot_icon)

	hand_slot_name = Label.new()
	hand_slot_name.text = "FREE"
	hand_slot_name.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	hand_slot_name.add_theme_font_size_override("font_size", 11)
	h_vbox.add_child(hand_slot_name)

	hand_slot_sub = Label.new()
	hand_slot_sub.text = "1-4 / Wheel"
	hand_slot_sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	hand_slot_sub.add_theme_font_size_override("font_size", 9)
	h_vbox.add_child(hand_slot_sub)

	hotbar_gui_container.add_child(hand_slot_panel)

	journal_panel = PanelContainer.new()
	journal_panel.set_anchors_preset(Control.PRESET_CENTER)
	journal_panel.offset_left = -450
	journal_panel.offset_right = 450
	journal_panel.offset_top = -290
	journal_panel.offset_bottom = 290
	journal_panel.visible = false
	root_ctrl.add_child(journal_panel)

	journal_text = RichTextLabel.new()
	journal_text.bbcode_enabled = true
	journal_text.custom_minimum_size = Vector2(880, 560)
	journal_panel.add_child(journal_text)

	# [J] On-Screen Non-Verbal Voice Command Menu (Select What to Say to the Ghost)
	voice_menu_panel = PanelContainer.new()
	voice_menu_panel.set_anchors_preset(Control.PRESET_CENTER)
	voice_menu_panel.offset_left = -335
	voice_menu_panel.offset_right = 335
	voice_menu_panel.offset_top = -235
	voice_menu_panel.offset_bottom = 235
	voice_menu_panel.visible = false

	var vm_style := StyleBoxFlat.new()
	vm_style.bg_color = Color(0.06, 0.09, 0.14, 0.96)
	vm_style.border_width_left = 3
	vm_style.border_width_top = 3
	vm_style.border_width_right = 3
	vm_style.border_width_bottom = 3
	vm_style.border_color = Color(0.35, 0.92, 1.0, 1.0)
	vm_style.corner_radius_top_left = 8
	vm_style.corner_radius_top_right = 8
	vm_style.corner_radius_bottom_left = 8
	vm_style.corner_radius_bottom_right = 8
	vm_style.content_margin_left = 18
	vm_style.content_margin_right = 18
	vm_style.content_margin_top = 14
	vm_style.content_margin_bottom = 14
	voice_menu_panel.add_theme_stylebox_override("panel", vm_style)
	root_ctrl.add_child(voice_menu_panel)

	var vm_vbox := VBoxContainer.new()
	vm_vbox.add_theme_constant_override("separation", 8)
	voice_menu_panel.add_child(vm_vbox)

	var vm_title := Label.new()
	vm_title.text = "VOICE COMMAND SYSTEM [J] — SELECT WHAT TO SAY"
	vm_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	vm_title.add_theme_font_size_override("font_size", 18)
	vm_title.modulate = Color(0.45, 0.96, 1.0)
	vm_vbox.add_child(vm_title)

	voice_menu_status_lbl = Label.new()
	voice_menu_status_lbl.text = "Click any option below or press keys [1]–[7] to speak to nearby ghosts:"
	voice_menu_status_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	voice_menu_status_lbl.add_theme_font_size_override("font_size", 13)
	voice_menu_status_lbl.modulate = Color(0.95, 0.85, 0.45)
	vm_vbox.add_child(voice_menu_status_lbl)

	for i in range(VOICE_MENU_OPTIONS.size()):
		var opt: Dictionary = VOICE_MENU_OPTIONS[i]
		var btn := Button.new()
		btn.text = str(opt["button_text"])
		btn.custom_minimum_size = Vector2(620, 40)
		btn.alignment = HORIZONTAL_ALIGNMENT_LEFT
		btn.add_theme_font_size_override("font_size", 14)
		var idx := i
		btn.pressed.connect(func() -> void: select_voice_menu_option(idx))
		vm_vbox.add_child(btn)

	var close_btn := Button.new()
	close_btn.text = "[J / Esc] Cancel & Close Voice Menu"
	close_btn.custom_minimum_size = Vector2(620, 34)
	close_btn.modulate = Color(0.85, 0.72, 0.72)
	close_btn.pressed.connect(func() -> void: toggle_voice_menu(false))
	vm_vbox.add_child(close_btn)

	# Seated Food Court Eating Progress Bar UI (Shown while sitting at the table eating for 5.0s minimum)
	eating_bar_panel = PanelContainer.new()
	eating_bar_panel.set_anchors_preset(Control.PRESET_CENTER_BOTTOM)
	eating_bar_panel.offset_left = -255
	eating_bar_panel.offset_right = 255
	eating_bar_panel.offset_top = -238
	eating_bar_panel.offset_bottom = -164
	eating_bar_panel.visible = false

	var eat_style := StyleBoxFlat.new()
	eat_style.bg_color = Color(0.12, 0.08, 0.04, 0.94)
	eat_style.border_width_left = 3
	eat_style.border_width_top = 3
	eat_style.border_width_right = 3
	eat_style.border_width_bottom = 3
	eat_style.border_color = Color(1.0, 0.76, 0.25, 1.0)
	eat_style.corner_radius_top_left = 8
	eat_style.corner_radius_top_right = 8
	eat_style.corner_radius_bottom_left = 8
	eat_style.corner_radius_bottom_right = 8
	eat_style.content_margin_left = 14
	eat_style.content_margin_right = 14
	eat_style.content_margin_top = 8
	eat_style.content_margin_bottom = 8
	eating_bar_panel.add_theme_stylebox_override("panel", eat_style)
	root_ctrl.add_child(eating_bar_panel)

	var eat_vbox := VBoxContainer.new()
	eat_vbox.add_theme_constant_override("separation", 6)
	eating_bar_panel.add_child(eat_vbox)

	eating_bar_label = Label.new()
	eating_bar_label.text = "SITTING AT FOOD COURT TABLE — EATING CHONGQING HOT POT (0%)"
	eating_bar_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	eating_bar_label.add_theme_font_size_override("font_size", 14)
	eating_bar_label.modulate = Color(1.0, 0.88, 0.42)
	eat_vbox.add_child(eating_bar_label)

	eating_progress_bar = ProgressBar.new()
	eating_progress_bar.min_value = 0.0
	eating_progress_bar.max_value = 100.0
	eating_progress_bar.value = 0.0
	eating_progress_bar.show_percentage = true
	eating_progress_bar.custom_minimum_size = Vector2(470, 26)
	var fill_style := StyleBoxFlat.new()
	fill_style.bg_color = Color(1.0, 0.68, 0.18, 0.96)
	fill_style.corner_radius_top_left = 4
	fill_style.corner_radius_top_right = 4
	fill_style.corner_radius_bottom_left = 4
	fill_style.corner_radius_bottom_right = 4
	eating_progress_bar.add_theme_stylebox_override("fill", fill_style)
	eat_vbox.add_child(eating_progress_bar)

	# [P] Co-op Multiplayer & Invite Code Connection Modal
	coop_menu_panel = PanelContainer.new()
	coop_menu_panel.set_anchors_preset(Control.PRESET_CENTER)
	coop_menu_panel.offset_left = -335
	coop_menu_panel.offset_right = 335
	coop_menu_panel.offset_top = -235
	coop_menu_panel.offset_bottom = 235
	coop_menu_panel.visible = false
	coop_menu_panel.add_theme_stylebox_override("panel", vm_style)
	root_ctrl.add_child(coop_menu_panel)

	var cm_vbox := VBoxContainer.new()
	cm_vbox.add_theme_constant_override("separation", 8)
	coop_menu_panel.add_child(cm_vbox)

	var cm_title := Label.new()
	cm_title.text = "CO-OP MULTIPLAYER LOBBY [P] — INVITE CODES & UPnP"
	cm_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	cm_title.add_theme_font_size_override("font_size", 18)
	cm_title.modulate = Color(0.45, 0.96, 1.0)
	cm_vbox.add_child(cm_title)

	coop_status_lbl = Label.new()
	coop_status_lbl.text = "Your Shareable Invite Code: %s" % MultiplayerManager.active_invite_code
	coop_status_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	coop_status_lbl.add_theme_font_size_override("font_size", 13)
	coop_status_lbl.modulate = Color(0.95, 0.85, 0.45)
	cm_vbox.add_child(coop_status_lbl)

	var code_row := HBoxContainer.new()
	code_row.alignment = BoxContainer.ALIGNMENT_CENTER
	code_row.add_theme_constant_override("separation", 10)
	cm_vbox.add_child(code_row)

	var copy_code_btn := Button.new()
	copy_code_btn.text = "Copy My Host Invite Code (%s) to Clipboard" % MultiplayerManager.active_invite_code
	copy_code_btn.custom_minimum_size = Vector2(580, 36)
	copy_code_btn.modulate = Color(0.55, 1.0, 0.78)
	copy_code_btn.pressed.connect(func() -> void:
		DisplayServer.clipboard_set(MultiplayerManager.active_invite_code)
		GameState.status_message.emit("Copied Invite Code [%s] to clipboard! Send it to your testers to paste in [P]." % MultiplayerManager.active_invite_code, Color(0.4, 1.0, 0.7))
	)
	code_row.add_child(copy_code_btn)
	MultiplayerManager.invite_code_updated.connect(func(new_code: String, detail: String) -> void:
		copy_code_btn.text = "Copy My Host Invite Code (%s) to Clipboard" % new_code
		if coop_status_lbl:
			coop_status_lbl.text = "%s | Code: %s" % [detail, new_code]
	)

	var ip_row := HBoxContainer.new()
	ip_row.alignment = BoxContainer.ALIGNMENT_CENTER
	ip_row.add_theme_constant_override("separation", 10)
	cm_vbox.add_child(ip_row)

	var ip_lbl := Label.new()
	ip_lbl.text = "Enter Invite Code or Host IP:"
	ip_row.add_child(ip_lbl)

	coop_ip_input = LineEdit.new()
	coop_ip_input.text = "127.0.0.1"
	coop_ip_input.placeholder_text = "MALL-XXXX-XXXX, host:port, or IP"
	coop_ip_input.custom_minimum_size = Vector2(310, 34)
	ip_row.add_child(coop_ip_input)

	var host_btn := Button.new()
	host_btn.text = "[F8]  Host Co-op Server (Local / LAN / Tailscale — UDP 7777)"
	host_btn.custom_minimum_size = Vector2(580, 38)
	host_btn.pressed.connect(func() -> void:
		MultiplayerManager.host_coop_session(MultiplayerManager.DEFAULT_PORT, false)
		toggle_coop_menu(false)
	)
	cm_vbox.add_child(host_btn)

	var upnp_host_btn := Button.new()
	upnp_host_btn.text = "Host + Auto-Open Router Port via UPnP (Zero-Install Internet Co-op)"
	upnp_host_btn.custom_minimum_size = Vector2(580, 38)
	upnp_host_btn.modulate = Color(0.55, 0.95, 1.0)
	upnp_host_btn.pressed.connect(func() -> void:
		MultiplayerManager.host_coop_session(MultiplayerManager.DEFAULT_PORT, true)
		toggle_coop_menu(false)
	)
	cm_vbox.add_child(upnp_host_btn)

	var join_btn := Button.new()
	join_btn.text = "[F9]  Join Co-op Server using Invite Code / Address above"
	join_btn.custom_minimum_size = Vector2(580, 38)
	join_btn.pressed.connect(func() -> void:
		MultiplayerManager.join_coop_session(coop_ip_input.text)
		toggle_coop_menu(false)
	)
	cm_vbox.add_child(join_btn)

	var disc_btn := Button.new()
	disc_btn.text = "Disconnect / Return to Solo Mode"
	disc_btn.custom_minimum_size = Vector2(580, 34)
	disc_btn.modulate = Color(1.0, 0.72, 0.55)
	disc_btn.pressed.connect(func() -> void:
		MultiplayerManager.disconnect_coop_session()
		toggle_coop_menu(false)
	)
	cm_vbox.add_child(disc_btn)

	var close_coop_btn := Button.new()
	close_coop_btn.text = "[P / Esc] Close Co-op Menu"
	close_coop_btn.custom_minimum_size = Vector2(580, 32)
	close_coop_btn.pressed.connect(func() -> void: toggle_coop_menu(false))
	cm_vbox.add_child(close_coop_btn)

	# Quartermaster Chen — Trader & Upgrade Shop Modal ([F] at Surplus Counter)
	trader_menu_panel = PanelContainer.new()
	trader_menu_panel.set_anchors_preset(Control.PRESET_CENTER)
	trader_menu_panel.offset_left = -390
	trader_menu_panel.offset_right = 390
	trader_menu_panel.offset_top = -295
	trader_menu_panel.offset_bottom = 295
	trader_menu_panel.visible = false
	trader_menu_panel.add_theme_stylebox_override("panel", vm_style)
	root_ctrl.add_child(trader_menu_panel)

	var tr_vbox := VBoxContainer.new()
	tr_vbox.add_theme_constant_override("separation", 6)
	trader_menu_panel.add_child(tr_vbox)

	var tr_title := Label.new()
	tr_title.text = "QUARTERMASTER CHEN — TRADER, EXTRA PUSH CARTS & EQUIPMENT UPGRADES"
	tr_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	tr_title.add_theme_font_size_override("font_size", 18)
	tr_title.modulate = Color(0.45, 1.0, 0.72)
	tr_vbox.add_child(tr_title)

	trader_status_lbl = Label.new()
	trader_status_lbl.text = "Sell Filled Ectoplasm Canisters & Purchase Extra Push Carts, Surveillance Gear, or Upgrades:"
	trader_status_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	trader_status_lbl.add_theme_font_size_override("font_size", 13)
	trader_status_lbl.modulate = Color(1.0, 0.88, 0.42)
	tr_vbox.add_child(trader_status_lbl)

	trader_buttons.clear()
	for i in range(10):
		var t_btn := Button.new()
		t_btn.custom_minimum_size = Vector2(730, 34)
		t_btn.alignment = HORIZONTAL_ALIGNMENT_LEFT
		t_btn.add_theme_font_size_override("font_size", 13)
		var opt_idx := i
		t_btn.pressed.connect(func() -> void: execute_trader_shop_option(opt_idx))
		tr_vbox.add_child(t_btn)
		trader_buttons.append(t_btn)

	var close_trader_btn := Button.new()
	close_trader_btn.text = "[F / Esc] Close Trader Shop"
	close_trader_btn.custom_minimum_size = Vector2(730, 32)
	close_trader_btn.modulate = Color(0.85, 0.75, 0.75)
	close_trader_btn.pressed.connect(func() -> void: toggle_trader_menu(false))
	tr_vbox.add_child(close_trader_btn)

	if is_instance_valid(campaign_expansion):
		campaign_expansion.build_briefing_and_endgame_ui(root_ctrl)

func _refresh_trader_menu_ui() -> void:
	if trader_menu_panel == null or trader_buttons.size() < 10 or not is_instance_valid(player):
		return
	var filled_cnt := player.get_carried_filled_canisters_count()
	var filled_val := player.get_carried_filled_canisters_total_value()
	var push_cart_cnt := get_tree().get_nodes_in_group("push_carts").size()
	if trader_status_lbl:
		trader_status_lbl.text = "Team Cash: $%d | Highest Floor: Floor %d | Push Carts Owned: %d | Mobile Light Cart: 1 (Strictly 1-Per-Team!)" % [
			GameState.team_cash, GameState.highest_floor_reached, push_cart_cnt
		]

	# Option 0: [1] Sell Filled Ectoplasm Canisters & Recharge Empty Canisters
	if filled_cnt > 0:
		trader_buttons[0].text = "[1]  SELL %d FILLED ECTOPLASM CANISTER(S)  —  Earn +$%d! (Once Full Must Be Sold)" % [filled_cnt, filled_val]
		trader_buttons[0].modulate = Color(1.0, 0.92, 0.35)
	else:
		trader_buttons[0].text = "[1]  Sell Filled Ectoplasm Canisters (None Carried) & Recharge Carried Canisters (FREE)"
		trader_buttons[0].modulate = Color(0.75, 0.90, 0.82)

	# Option 1: [2] Buy New Empty Ectoplasm Canister ($35)
	trader_buttons[1].text = "[2]  Buy Empty Ectoplasm Canister ($35)  —  %.0fs Deployed Active Time Limit" % GameState.get_canister_max_deployed_time()
	trader_buttons[1].modulate = Color(0.45, 1.0, 0.72)

	# Option 2: [3] Buy Heavy-Duty Escalator Relay Fuse Crate ($60)
	var fuse_done: bool = bool(GameState.powered_circuits.get("EscalatorCircuit_F1_To_F2", false)) and bool(GameState.powered_circuits.get("EscalatorCircuit_F2_To_F3", false))
	trader_buttons[2].text = "[3]  Buy Escalator Relay Fuse Crate ($60)  —  %s" % ("All Escalators Powered!" if fuse_done else "Powers Escalator Breaker Dais (F1->F2 / F2->F3)")
	trader_buttons[2].modulate = Color(0.65, 0.75, 0.80) if fuse_done else Color(0.55, 0.95, 1.0)

	# Option 3: [4] Buy Evidence Camera Film Pack (+10 Exposures) ($20)
	trader_buttons[3].text = "[4]  Buy Camera Film Refill ($20)  —  Refill Photo Camera to 10/10 Exposures (Currently %d/10)" % player.photo_exposures_left
	trader_buttons[3].modulate = Color(0.75, 0.92, 1.0)

	# Option 4: [5] Upgrade Proton Ray Gun Power Level (Floor 1: Lvl 2 $80 | Floor 2+ Unlock: Lvl 3 $140 | Floor 3+ Unlock: Lvl 4 $220)
	if GameState.ray_gun_power_level >= 4:
		trader_buttons[4].text = "[5]  Proton Ray Gun Power Level: MAX LEVEL 4 (185 DPS / 6.5x Wrangle Power)"
		trader_buttons[4].modulate = Color(0.55, 0.85, 0.65)
	else:
		var next_rlvl := GameState.ray_gun_power_level + 1
		var r_cost := GameState.get_ray_gun_upgrade_cost()
		var r_req_floor := GameState.get_ray_gun_next_upgrade_required_floor()
		var r_locked := (GameState.highest_floor_reached < r_req_floor)
		var dps_preview := 65 if next_rlvl == 2 else (120 if next_rlvl == 3 else 185)
		if r_locked:
			trader_buttons[4].text = "[5]  [SAVED FOR FLOOR %d+] Upgrade Proton Ray Gun to Level %d (%d DPS) ($%d) — Reach Floor %d to Unlock!" % [
				r_req_floor, next_rlvl, dps_preview, r_cost, r_req_floor
			]
			trader_buttons[4].modulate = Color(1.0, 0.58, 0.38)
		else:
			trader_buttons[4].text = "[5]  Upgrade Proton Ray Gun to Level %d ($%d)  —  Boosts Damage to %d DPS & Stronger Ghost Pull!" % [
				next_rlvl, r_cost, dps_preview
			]
			trader_buttons[4].modulate = Color(0.45, 0.96, 1.0)

	# Option 5: [6] Upgrade Ectoplasm Canister Tech (Floor 1: Lvl 2 $65 [28s] | Floor 2+ Unlock: Lvl 3 $110 [40s + 4.0m Wide Circle])
	if GameState.canister_upgrade_level >= 3:
		trader_buttons[5].text = "[6]  Ectoplasm Canister Tech: MAX LEVEL 3 (40s Deployed Limit & 4.0m Wide Circle)"
		trader_buttons[5].modulate = Color(0.55, 0.85, 0.65)
	else:
		var next_clvl := GameState.canister_upgrade_level + 1
		var c_cost := GameState.get_canister_upgrade_cost()
		var c_req_floor := GameState.get_canister_next_upgrade_required_floor()
		var c_locked := (GameState.highest_floor_reached < c_req_floor)
		var time_preview := 28 if next_clvl == 2 else 40
		if c_locked:
			trader_buttons[5].text = "[6]  [SAVED FOR FLOOR %d+] Upgrade Canister Tech to Level %d (%ds Limit + 4.0m Circle) ($%d) — Reach Floor %d!" % [
				c_req_floor, next_clvl, time_preview, c_cost, c_req_floor
			]
			trader_buttons[5].modulate = Color(1.0, 0.58, 0.38)
		else:
			trader_buttons[5].text = "[6]  Upgrade Ectoplasm Canister Capacitor to Level %d ($%d)  —  %ds Deployed Active Time & Faster Recharge!" % [
				next_clvl, c_cost, time_preview
			]
			trader_buttons[5].modulate = Color(0.45, 1.0, 0.78)

	# Option 6: [7] Buy Extra 8-Slot Equipment Push Cart ($95) (Mobile Light Cart is strictly 1-per-team!)
	trader_buttons[6].text = "[7]  Buy Extra 8-Slot Equipment Push Cart ($95)  —  Includes Pusher-Side Monitor & 2 Hooks (Currently %d Owned; Light Cart is Strictly 1)" % push_cart_cnt
	trader_buttons[6].modulate = Color(0.42, 0.98, 0.85)

	# Option 7: [8] Buy Video Surveillance Rig Kit ($90: IR Video Camera + Tripod Stand)
	trader_buttons[7].text = "[8]  Buy Video Surveillance Bundle ($90)  —  IR Video Camera + Tripod Stand (Snap Together or Hang on Cart Hook!)"
	trader_buttons[7].modulate = Color(0.55, 0.95, 1.0)

	# Option 8: [9] Buy Paranormal Audio Recorder + D.O.T.S. Laser Grid Dome ($110)
	trader_buttons[8].text = "[9]  Buy Audio Recorder + D.O.T.S. Laser Grid Dome ($110)  —  Record/Reverse EVP (+$75) & Project 3D Laser Silhouette"
	trader_buttons[8].modulate = Color(0.55, 1.0, 0.72)

	# Option 9: [0] Buy Cart Mod & Ward Kit ($85: 2-Cart Tow-Hook + EAS Pedestal Ward + Pink Salt Pouch + Helium Balloon)
	trader_buttons[9].text = "[0]  Buy Cart Mod & Survival Kit ($85)  —  Deliver 2-Cart Tow-Hook, EAS Ward, Pink Salt & Helium Balloon to Airlock Bay"
	trader_buttons[9].modulate = Color(1.0, 0.84, 0.45)

func execute_trader_shop_option(idx: int) -> void:
	if not is_instance_valid(player):
		return
	match idx:
		0: # [1] Sell Filled Canisters & Recharge Carried Empty Canisters
			var sold := player.sell_all_carried_filled_canisters()
			player._recharge_carried_canisters(100.0)
			if sold == 0:
				GameState.status_message.emit(
					"Recharged carried Ectoplasm Canisters to 100%! (Capture a ghost in an open canister first to sell a Filled Canister here!)",
					Color(0.45, 1.0, 0.78)
				)
		1: # [2] Buy New Empty Ectoplasm Canister ($35)
			if GameState.spend_cash(35, "New Empty Ectoplasm Collection Canister"):
				var new_can := PlayerController.make_item_data("ectoplasm_canister")
				new_can["deployed_seconds_left"] = GameState.get_canister_max_deployed_time()
				player.receive_item_from_cart(new_can)
				GameState.status_message.emit(
					"Purchased a new Empty Ectoplasm Canister ($35, %.0fs charge)! Set it on the floor [LMB/G] and kick it open [E] to suck a ghost in!" % GameState.get_canister_max_deployed_time(),
					Color(0.35, 1.0, 0.72)
				)
		2: # [3] Buy Escalator Relay Fuse Crate ($60)
			if not player.is_carrying_item_id("escalator_fuse"):
				if GameState.spend_cash(60, "Heavy-Duty Escalator Relay Fuse Crate"):
					player.pickup_item_by_id_into_inventory_or_hands("escalator_fuse")
					GameState.status_message.emit("Purchased Escalator Relay Fuse Crate (Carried in Hands)! Take it to an Escalator Breaker Dais (F1->F2 or F2->F3).", Color(0.45, 1.0, 0.7))
			else:
				GameState.status_message.emit("You are already carrying an Escalator Relay Fuse Crate in your Hands!", Color(0.85, 0.9, 0.95))
		3: # [4] Buy Camera Film Pack ($20)
			if GameState.spend_cash(20, "Evidence Camera Film Pack (+10 Exposures)"):
				player.photo_exposures_left = 10
				player._refresh_equipped_visuals()
		4: # [5] Upgrade Proton Ray Gun Power Level (Floor-Gated for Lvl 3 & Lvl 4!)
			if GameState.upgrade_ray_gun(false):
				player._refresh_equipped_visuals()
		5: # [6] Upgrade Ectoplasm Canister Tech (Floor-Gated for Lvl 3!)
			if GameState.upgrade_canister_tech(false):
				player._recharge_carried_canisters(100.0)
				player._refresh_equipped_visuals()
		6: # [7] Buy Extra 8-Slot Equipment Push Cart ($95) — Mobile Light Cart remains strictly 1!
			if GameState.spend_cash(95, "Extra 8-Slot Equipment Push Cart (w/ Pusher-Side Monitor)"):
				var existing_cnt := get_tree().get_nodes_in_group("push_carts").size()
				var extra_pc := MallCartSystem.new()
				extra_pc.cart_type = "PushCart"
				extra_pc.linked_store_id = "West_F1_03" if existing_cnt == 1 else "East_F2_01"
				extra_pc.position = Vector3(-9.5, 0.0, 1.5 + float(existing_cnt) * 1.8)
				add_child(extra_pc)
				GameState.status_message.emit(
					"Purchased Extra 8-Slot Equipment Push Cart #%d ($95, parked at [%.1f, 0, %.1f] with Pusher-Side Monitor)! Note: The 18,000 lm Mobile Light Cart is strictly 1-per-team." % [
						existing_cnt + 1, extra_pc.position.x, extra_pc.position.z
					],
					Color(0.35, 1.0, 0.82)
				)
		7: # [8] Buy Video Surveillance Bundle ($90: IR Video Camera + Camera Tripod Stand)
			if GameState.spend_cash(90, "Video Surveillance Bundle (IR Video Camera + Tripod Stand)"):
				player.receive_item_from_cart(PlayerController.make_item_data("video_camera"))
				if is_instance_valid(campaign_expansion):
					campaign_expansion.spawn_item_in_delivery_airlock(PlayerController.make_item_data("camera_stand"))
				GameState.status_message.emit("Purchased Video Surveillance Bundle ($90)! IR Video Camera added to inventory & Tripod Stand delivered to Airlock Bay (-5.5, 0, -18.5)!", Color(0.45, 0.96, 1.0))
		8: # [9] Buy Paranormal Audio Recorder + D.O.T.S. Laser Grid Dome ($110)
			if GameState.spend_cash(110, "Paranormal Audio Recorder + D.O.T.S. Laser Grid Dome"):
				player.receive_item_from_cart(PlayerController.make_item_data("audio_recorder"))
				player.receive_item_from_cart(PlayerController.make_item_data("laser_grid_dome"))
				GameState.status_message.emit("Purchased Paranormal Audio Recorder + D.O.T.S. Laser Grid Projector Dome ($110)!", Color(0.45, 1.0, 0.78))
		9: # [0] Buy Cart Mod & Survival Kit ($85)
			if GameState.spend_cash(85, "Cart Mod & Survival Kit (Tow-Hook, EAS Ward, Pink Salt, Helium Balloon)"):
				player.receive_item_from_cart(PlayerController.make_item_data("cart_tow_hook"))
				if is_instance_valid(campaign_expansion):
					campaign_expansion.spawn_item_in_delivery_airlock(PlayerController.make_item_data("eas_pedestal_ward"))
					campaign_expansion.spawn_item_in_delivery_airlock(PlayerController.make_item_data("pink_salt_pouch"))
					campaign_expansion.spawn_item_in_delivery_airlock(PlayerController.make_item_data("helium_balloon"))
	_refresh_trader_menu_ui()

func _refresh_hud() -> void:
	if hud_label == null or not is_instance_valid(player):
		return
	var seal_text := push_cart.get_seal_state_label() if is_instance_valid(push_cart) else "N/A"
	var east_status := store_ghost_east.get_phase_label() if is_instance_valid(store_ghost_east) else "Dormant"
	var west_tell := store_ghost_west.get_disposition_tell_short() if is_instance_valid(store_ghost_west) else "N/A"
	var ward_str := ("%.0fs ACTIVE" % player.pink_salt_ward_timer) if player.pink_salt_ward_timer > 0.0 else ("Soul-Anchor Lit" if GameState.sauna_soul_anchor_lit else "Inactive")
	var freight_str := "N/A"
	if is_instance_valid(freight_elevator):
		if freight_elevator.is_moving:
			freight_str = "MOVING (Y=%.1fm)" % freight_elevator.current_cab_y
		else:
			freight_str = "DOCKED FLOOR %d [E: UP/DOWN Buttons]" % freight_elevator.current_floor_num
	var cur_item := player.get_current_active_item()

	hud_label.text = (
		"[b][color=#66f2ff]MEGAMALL ANOMALY — FLOOR %d (Godot 4.7 Vulkan)[/color][/b]\n" % GameState.current_floor +
		"[b]Team Cash:[/b] [color=#55ff88]$%d[/color] | [b]Deaths:[/b] %d | [b]Service Elevator:[/b] %s\n" % [
			GameState.team_cash, GameState.total_death_tally, freight_str
		] +
		"[b]Vitality:[/b] HP %.0f/100 | Stamina %.0f/100 | [color=#ff99cc]Sauna Ward/Anchor:[/color] %s | [b]View:[/b] %s\n" % [
			player.health, player.stamina, ward_str, player.contextual_reason
		] +
		"[b]4-Slot Hotbar + Hands:[/b] %s\n" % player.get_hotbar_bar_summary() +
		"[b]Active Hand/Tool [%s]:[/b] [color=#ffe680]%s[/color]\n" % [
			cur_item.get("name", "Empty Hands"), player.get_active_tool_mode_text()
		] +
		"[b]Doorway Seal [East_F0_02]:[/b] %s | [b]Wheeled Light Cart [E]:[/b] %s (%.0f%%)\n" % [
			seal_text,
			("ON" if light_cart.is_floodlight_on else "OFF") if is_instance_valid(light_cart) else "N/A",
			light_cart.battery_percent if is_instance_valid(light_cart) else 0.0
		] +
		"[b]East_F0_02 (Arthur):[/b] %s | [b]West_F1_03 (Evelyn):[/b] %s\n" % [east_status, west_tell] +
		"[color=#aaaaaa][LMB] Use/Power | [RMB] Alt/Next Cam/NV | [F] Pick Up Item/Monitor | [G] Drop/Place/Mount | [E] Push/Release Cart & World Interact[/color]"
	)

	if coop_hud_label:
		var net_mode := "[color=#ffdd66]SOLO / OFFLINE[/color]"
		if MultiplayerManager.is_coop_active:
			if MultiplayerManager.is_host():
				net_mode = "[color=#55ff99]CO-OP HOST (UDP :%d)[/color]" % MultiplayerManager.current_port
			else:
				net_mode = "[color=#66f2ff]CO-OP CLIENT (Peer #%d)[/color]" % MultiplayerManager.get_local_peer_id()
		var peer_lines: Array[String] = []
		var my_id := MultiplayerManager.get_local_peer_id()
		for pid in MultiplayerManager.connected_peers:
			if pid == my_id:
				peer_lines.append("• [b][color=#66f2ff]You (Investigator #%d%s):[/color][/b] HP %.0f%% | %s" % [
					pid,
					" HOST" if pid == 1 else "",
					player.health,
					"ECTOPLASM" if player.is_floating_essence else str(cur_item.get("short_label", "READY"))
				])
			elif players_by_peer.has(pid) and is_instance_valid(players_by_peer[pid]):
				var rp := players_by_peer[pid] as PlayerController
				var dist := player.global_position.distance_to(rp.global_position)
				var st := "[color=#ff6666]ECTOPLASM[/color]" if rp.is_floating_essence else ("HP %.0f%% | %s" % [rp.health, rp._net_held_item_label])
				peer_lines.append("• [b][color=#ffe680]Investigator #%d%s (%.1fm):[/color][/b] %s" % [
					pid,
					" HOST" if pid == 1 else "",
					dist,
					st
				])
		coop_hud_label.text = (
			"[b][color=#66f2ff]CO-OP MULTIPLAYER RADAR (%d/%d)[/color][/b] — %s\n" % [
				MultiplayerManager.connected_peers.size(), MultiplayerManager.MAX_PLAYERS, net_mode
			] +
			"\n".join(peer_lines) + "\n" +
			"[color=#99bbcc][F8] Host Server | [F9] Join 127.0.0.1 | [P] Co-op IP Menu | [B] Walkie PTT[/color]"
		)
	if eating_bar_panel and eating_progress_bar and eating_bar_label:
		eating_bar_panel.visible = player.is_eating_meal
		if player.is_eating_meal:
			var pct := player.get_eating_progress_ratio() * 100.0
			eating_progress_bar.value = pct
			eating_bar_label.text = "SITTING AT TABLE — EATING CHONGQING HOT POT (%.1fs / %.1fs) | HP: %.0f%% | Stamina: %.0f%%" % [
				player.eating_timer, player.eating_duration, player.health, player.stamina
			]

	_refresh_hotbar_gui()

func _refresh_hotbar_gui() -> void:
	if hotbar_slot_panels.size() < 4 or not is_instance_valid(player):
		return
	var hands_busy := not player.hand_slot_item.is_empty()

	for i in range(4):
		var item: Dictionary = player.hotbar_slots[i]
		var is_active := (not hands_busy and player.active_slot == i and not player.is_floating_essence)
		var style := hotbar_slot_styles[i]

		if item.is_empty():
			hotbar_slot_icons[i].text = "[   ]"
			hotbar_slot_icons[i].modulate = Color(0.45, 0.52, 0.60)
			hotbar_slot_names[i].text = "EMPTY"
			hotbar_slot_names[i].modulate = Color(0.55, 0.62, 0.70)
			hotbar_slot_subs[i].text = "---"
			hotbar_slot_subs[i].modulate = Color(0.55, 0.62, 0.70)
		else:
			var accent: Color = item.get("accent_color", Color(0.45, 0.90, 1.0))
			var is_on: bool = bool(item.get("powered_on", true))
			var can_toggle: bool = bool(item.get("can_power_toggle", false))
			hotbar_slot_icons[i].text = str(item.get("icon_symbol", "[*]"))
			hotbar_slot_icons[i].modulate = accent if (not can_toggle or is_on) else accent.darkened(0.48)
			hotbar_slot_names[i].text = str(item.get("short_label", item.get("name", "TOOL")))
			hotbar_slot_names[i].modulate = Color(1.0, 1.0, 1.0) if is_active else Color(0.84, 0.90, 0.96)
			hotbar_slot_subs[i].text = player.get_slot_subtext(item)
			if can_toggle:
				hotbar_slot_subs[i].modulate = Color(0.38, 1.0, 0.65) if is_on else Color(0.55, 0.62, 0.70)
			else:
				hotbar_slot_subs[i].modulate = Color(0.75, 0.85, 0.95)

		if is_active:
			style.bg_color = Color(0.14, 0.24, 0.34, 0.95)
			style.border_color = Color(0.38, 0.96, 1.0, 1.0)
			style.border_width_left = 3
			style.border_width_top = 3
			style.border_width_right = 3
			style.border_width_bottom = 3
		else:
			style.bg_color = Color(0.07, 0.10, 0.14, 0.84)
			style.border_color = Color(0.24, 0.32, 0.42, 0.85)
			style.border_width_left = 2
			style.border_width_top = 2
			style.border_width_right = 2
			style.border_width_bottom = 2

	if hand_slot_panel and hand_slot_style:
		if hands_busy:
			var h_item := player.hand_slot_item
			hand_slot_style.bg_color = Color(0.28, 0.18, 0.06, 0.96)
			hand_slot_style.border_color = Color(1.0, 0.76, 0.24, 1.0)
			hand_slot_style.border_width_left = 3
			hand_slot_style.border_width_top = 3
			hand_slot_style.border_width_right = 3
			hand_slot_style.border_width_bottom = 3
			hand_slot_icon.text = str(h_item.get("icon_symbol", "[CRATE]"))
			hand_slot_icon.modulate = Color(1.0, 0.84, 0.35)
			hand_slot_name.text = str(h_item.get("short_label", h_item.get("name", "HAND ITEM")).left(11))
			hand_slot_name.modulate = Color(1.0, 0.94, 0.78)
			hand_slot_sub.text = player.get_slot_subtext(h_item)
			hand_slot_sub.modulate = Color(1.0, 0.80, 0.40)
		else:
			hand_slot_style.bg_color = Color(0.06, 0.08, 0.11, 0.68)
			hand_slot_style.border_color = Color(0.22, 0.26, 0.32, 0.65)
			hand_slot_style.border_width_left = 2
			hand_slot_style.border_width_top = 2
			hand_slot_style.border_width_right = 2
			hand_slot_style.border_width_bottom = 2
			hand_slot_icon.text = "[ - ]"
			hand_slot_icon.modulate = Color(0.45, 0.50, 0.58)
			hand_slot_name.text = "FREE"
			hand_slot_name.modulate = Color(0.55, 0.60, 0.68)
			hand_slot_sub.text = "1-4 / Wheel"
			hand_slot_sub.modulate = Color(0.50, 0.56, 0.64)

func _refresh_journal_text() -> void:
	if journal_text == null:
		return
	var store_order := ["East_F0_02", "West_F1_02", "West_F1_03", "West_F2_01", "East_F2_01", "Center_F3_01"]
	var store_lines := ""
	var idx := 1
	for sid in store_order:
		var prof: Dictionary = GameState.ghost_profiles.get(sid, {})
		var ev_list: Array = GameState.store_evidence_ledger.get(sid, [])
		var ev_str := ", ".join(ev_list) if not ev_list.is_empty() else "Scan with EMF, Therm, Spirit Box, UV, Video Cam, Audio Recorder, SLS/Laser, Barcode, or Parabolic"
		var is_corr: bool = bool(prof.get("is_corrupted_hostile", false))
		var disp_str := "[color=#ff5555]CORRUPTED (Hostile-Only — Use Proton Gun + Canister!)[/color]" if is_corr else "[color=#55ff88]RECEPTIVE TO PEACE (+$120 Ritual or Forceful Capture)[/color]"
		var res_str := str(GameState.resolved_stores.get(sid, "Active Haunting"))
		store_lines += "[b]%d. %s — %s (%s | Floor %d):[/b]\n   • Evidence: %s\n   • Disposition: %s | Status: [color=#ffe680]%s[/color]\n\n" % [
			idx, sid, str(prof.get("name", sid)), str(prof.get("title", "Store Ghost")), int(prof.get("floor_level", 1)),
			ev_str, disp_str, res_str
		]
		idx += 1

	var lore_lines := ""
	for k in GameState.discovered_lore.keys():
		var item: Dictionary = GameState.discovered_lore[k]
		lore_lines += "• [b]%s:[/b] %s\n" % [item["title"], item["text"]]

	journal_text.text = (
		"[b][color=#66f2ff]INVESTIGATOR CASE FILE, 6-STORE DUAL RESOLUTION LEDGER & 3-FLOOR PROGRESSION[/color][/b]\n" +
		"==================================================================================\n" +
		store_lines +
		"[b][color=#ffe680]DISCOVERED HIDDEN LORE & ELENA VANCE AUDIT TRAIL (%d/%d):[/color][/b]\n%s" % [
			GameState.discovered_lore.size(), GameState.lore_catalog.size(), lore_lines
		]
	)

func _on_status_message(msg: String, color: Color) -> void:
	if status_banner:
		status_banner.text = msg
		status_banner.modulate = color
		_status_timer = 7.0

func _create_static_box(parent: Node3D, pos: Vector3, size: Vector3, color: Color, roughness: float) -> StaticBody3D:
	var sb := StaticBody3D.new()
	sb.position = pos
	var col := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = size
	col.shape = shape
	sb.add_child(col)

	var mi := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = size
	mi.mesh = box
	mi.material_override = MallPbrMaterialLibrary.get_smart_box_material(size, color, roughness)
	sb.add_child(mi)
	parent.add_child(sb)
	return sb

func _spawn_model_or_box(res_path: String, pos: Vector3, fallback_size: Vector3, fallback_color: Color, add_box_collision: bool = false) -> Node3D:
	var root := Node3D.new()
	root.position = pos
	add_child(root)

	if "Mannequin" in res_path or "TorsoForm" in res_path or "LegForm" in res_path:
		root.add_to_group("mall_mannequins")
		var seed_val := absi(int(pos.x * 137.0 + pos.z * 251.0 + pos.y * 53.0))
		var m_title := "Headless Display Mannequin" if ("Headless" in res_path or "TorsoForm" in res_path) else ("Walking Retail Mannequin" if "Walking" in res_path else "Retail Display Mannequin")
		root.set_meta("mannequin_name", m_title)
		root.set_meta("uncanny_seed", seed_val)
		root.set_meta("body_temp_c", 29.6 + float(seed_val % 38) * 0.1)
		root.set_meta("barcode_sku", "SKU #0000-M: %s [WARNING: POSE DRIFT +INVENTORY ANOMALY]" % m_title.to_upper())
		root.set_meta("parabolic_sound", "%s: Faint fiberglass joint creak... & muffled breathing?! (26 dB)" % m_title)

	if add_box_collision:
		var sb := StaticBody3D.new()
		var col := CollisionShape3D.new()
		var shape := BoxShape3D.new()
		shape.size = fallback_size
		col.shape = shape
		col.position = Vector3(0.0, fallback_size.y * 0.5, 0.0)
		sb.add_child(col)
		root.add_child(sb)

	if ResourceLoader.exists(res_path):
		var packed := load(res_path) as PackedScene
		if packed:
			var inst := packed.instantiate()
			MallPbrMaterialLibrary.beautify_model_materials(inst, res_path, fallback_color)
			root.add_child(inst)
			return root

	var mi := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = fallback_size
	mi.mesh = box
	mi.position = Vector3(0, fallback_size.y * 0.5, 0)
	mi.material_override = MallPbrMaterialLibrary.get_smart_box_material(fallback_size, fallback_color, 0.45)
	root.add_child(mi)
	return root

func _create_floating_label(pos: Vector3, text: String, color: Color) -> Label3D:
	var lbl := Label3D.new()
	lbl.position = pos
	lbl.text = text
	lbl.modulate = color.lerp(Color(0.96, 0.94, 0.88), 0.35)
	lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	lbl.font_size = 20
	lbl.outline_size = 5
	lbl.visibility_range_end = 13.5
	lbl.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
	add_child(lbl)
	return lbl
