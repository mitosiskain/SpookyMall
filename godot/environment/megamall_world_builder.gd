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
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.03, 0.04, 0.07)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.14, 0.16, 0.22)
	env.ambient_light_energy = 0.55
	env.tonemap_mode = Environment.TONE_MAPPER_ACES
	env.ssil_enabled = true
	env.glow_enabled = true
	env.glow_intensity = 0.45
	env.volumetric_fog_enabled = true
	env.volumetric_fog_density = 0.014
	env.volumetric_fog_albedo = Color(0.75, 0.82, 0.95)
	we.environment = env
	add_child(we)

	var skylight := DirectionalLight3D.new()
	skylight.rotation_degrees = Vector3(-62.0, -35.0, 0.0)
	skylight.light_color = Color(0.62, 0.78, 0.98)
	skylight.light_energy = 0.45
	skylight.shadow_enabled = true
	add_child(skylight)

func _build_mall_architecture() -> void:
	var nav_region := NavigationRegion3D.new()
	var nav_mesh := NavigationMesh.new()
	nav_region.navigation_mesh = nav_mesh
	add_child(nav_region)

	# 1. Central Atrium & Concourse Terrazzo Floor (64m x 44m, top surface at Y = 0.0m)
	_create_static_box(nav_region, Vector3(0, -0.25, 0), Vector3(64.0, 0.5, 44.0), Color(0.24, 0.25, 0.28), 0.25)

	# 2. Floor 2 Mezzanine Balcony Deck (+8.50m top walking surface) WITH Open Escalator Well Cutout!
	#    Escalator Well Opening spans X = [-3.4m .. +3.4m], Z = [-13.0m .. -9.0m] so riders have 100% open headroom!
	#    - West Mezzanine Wing: X = [-22.0 .. -3.4], Z = [-21.0 .. -9.0]
	_create_static_box(nav_region, Vector3(-12.7, 8.25, -15.0), Vector3(18.6, 0.5, 12.0), Color(0.20, 0.22, 0.26), 0.35)
	#    - East Mezzanine Wing: X = [+3.4 .. +22.0], Z = [-21.0 .. -9.0]
	_create_static_box(nav_region, Vector3(12.7, 8.25, -15.0), Vector3(18.6, 0.5, 12.0), Color(0.20, 0.22, 0.26), 0.35)
	#    - Center Rear Mezzanine Landing Promenade (docks flush with escalators' upper landings at Z = -13.00m, Y = 8.50m!):
	#      X = [-3.4 .. +3.4], Z = [-21.0 .. -13.0]
	_create_static_box(nav_region, Vector3(0.0, 8.25, -17.0), Vector3(6.8, 0.5, 8.0), Color(0.22, 0.25, 0.30), 0.30)

	#    - Floor 2 Safety Glass & Brushed-Metal Balustrades around Mezzanine edge & Escalator Well Cutout
	var rail_col := Color(0.45, 0.72, 0.88)
	_create_static_box(self, Vector3(-12.7, 9.02, -9.05), Vector3(18.6, 1.05, 0.12), rail_col, 0.2)
	_create_static_box(self, Vector3(12.7, 9.02, -9.05), Vector3(18.6, 1.05, 0.12), rail_col, 0.2)
	_create_static_box(self, Vector3(-3.45, 9.02, -11.0), Vector3(0.12, 1.05, 4.0), rail_col, 0.2)
	_create_static_box(self, Vector3(3.45, 9.02, -11.0), Vector3(0.12, 1.05, 4.0), rail_col, 0.2)
	# Rear wall on Floor 2
	_create_static_box(self, Vector3(0.0, 10.5, -21.0), Vector3(44.0, 4.0, 0.4), Color(0.16, 0.18, 0.22), 0.6)

	# 3. Geodesic Ark Canopy Roof Ribs (+17.0m elevation with Biotic-Cyan Neon Cove Strips)
	_create_static_box(self, Vector3(0, 17.0, 0), Vector3(64.0, 0.6, 44.0), Color(0.08, 0.10, 0.14), 0.5)
	for i in range(-2, 3):
		var rib := MeshInstance3D.new()
		var rib_box := BoxMesh.new()
		rib_box.size = Vector3(0.45, 0.35, 42.0)
		rib.mesh = rib_box
		rib.position = Vector3(float(i) * 11.0, 16.5, 0.0)
		var cyan_mat := StandardMaterial3D.new()
		cyan_mat.albedo_color = Color(0.2, 0.9, 1.0)
		cyan_mat.emission_enabled = true
		cyan_mat.emission = Color(0.15, 0.85, 1.0)
		cyan_mat.emission_energy_multiplier = 2.2
		rib.material_override = cyan_mat
		add_child(rib)

	# 4. Haunted Storefront Unit #1: East_F0_02 ("Pendelton's Book & Curio Shop" — Onboarding Receptive Store)
	_create_static_box(nav_region, Vector3(21.0, 4.0, -7.0), Vector3(14.0, 8.0, 0.4), Color(0.18, 0.19, 0.23), 0.7)
	_create_static_box(nav_region, Vector3(21.0, 4.0, 7.0), Vector3(14.0, 8.0, 0.4), Color(0.18, 0.19, 0.23), 0.7)
	_create_static_box(nav_region, Vector3(28.0, 4.0, 0.0), Vector3(0.4, 8.0, 14.0), Color(0.15, 0.16, 0.20), 0.7)
	_create_static_box(nav_region, Vector3(14.0, 4.0, -4.4), Vector3(0.4, 8.0, 5.2), Color(0.22, 0.24, 0.29), 0.5)
	_create_static_box(nav_region, Vector3(14.0, 4.0, 4.4), Vector3(0.4, 8.0, 5.2), Color(0.22, 0.24, 0.29), 0.5)
	_create_static_box(nav_region, Vector3(21.0, 6.0, 0.0), Vector3(14.0, 0.35, 14.0), Color(0.14, 0.15, 0.18), 0.6)
	# Flush threshold strip (0.015m high so carts and investigators glide across effortlessly)
	_create_static_box(self, Vector3(14.0, 0.0075, 0.0), Vector3(0.9, 0.015, 3.6), Color(0.95, 0.75, 0.15), 0.3)

	# 5. Dynamic Ceiling Light Fixtures
	var atrium_light_positions := [
		Vector3(-8.0, 7.6, -6.0), Vector3(-8.0, 7.6, 6.0),
		Vector3(0.0, 7.6, -6.0), Vector3(0.0, 7.6, 6.0),
		Vector3(8.0, 7.6, 0.0),
	]
	for pos in atrium_light_positions:
		var fix := MallLightFixture.new()
		fix.store_id = "Atrium_Concourse"
		fix.light_color = Color(1.0, 0.95, 0.86)
		fix.base_energy = 2.8
		fix.position = pos
		add_child(fix)

	var store_light_positions := [
		Vector3(17.5, 5.75, -3.2), Vector3(17.5, 5.75, 3.2),
		Vector3(23.5, 5.75, -3.2), Vector3(23.5, 5.75, 3.2),
	]
	for pos in store_light_positions:
		var fix := MallLightFixture.new()
		fix.store_id = "East_F0_02"
		fix.light_color = Color(0.92, 0.96, 1.0)
		fix.base_energy = 2.4
		fix.position = pos
		add_child(fix)

func _spawn_interactive_props_and_sanctuaries() -> void:
	# 1. Concourse Rest Bench (with solid bench collider + 3rd-Person Camera Transition)
	var bench_pos := Vector3(-4.5, 0.0, 4.5)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25366_ConcourseBench.fbx", bench_pos, Vector3(2.0, 0.48, 0.75), Color(0.48, 0.34, 0.22), true)
	_create_floating_label(bench_pos + Vector3(0, 1.35, 0), "[F] Concourse Rest Bench\n(3rd-Person Character View: +24 Stamina/s, +8 HP/s)", Color(0.65, 0.95, 1.0))

	# 2. South Tangquan Pink Salt Bathhouse (SM_Bath_SteamingOnsenPool.fbx) + Overhauled Small Platform Stairs Collision!
	var spa_pos := Vector3(-11.0, 0.0, 11.0)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Bath_SteamingOnsenPool.fbx", spa_pos, Vector3(5.6, 0.56, 4.7), Color(0.95, 0.55, 0.68), false)
	_build_onsen_pool_and_stairs_collision(spa_pos)
	var spa_light := OmniLight3D.new()
	spa_light.position = spa_pos + Vector3(0, 1.8, 0)
	spa_light.light_color = Color(1.0, 0.58, 0.72)
	spa_light.light_energy = 2.6
	spa_light.omni_range = 8.0
	add_child(spa_light)
	_create_floating_label(spa_pos + Vector3(0, 2.1, 0), "[F] South Pink Salt Spa & Sauna Revival Ritual Shrine\n• Walk Up Small Platform Stairs (North/South) into Pool!\n• Alive [F]: Soak (+60s Pink Salt Ward) & Light Soul-Anchor\n• Ectoplasm Form [F or 9]: Recite Spoken Prayer to Resurrect!", Color(1.0, 0.7, 0.85))

	#    Hinoki Cedar Glass Sauna Pavilion (SM_Bath_GlassSaunaPavilion.fbx) with Entrance Step & Bench Collision
	var sauna_pos := Vector3(-17.2, 0.0, 11.0)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Bath_GlassSaunaPavilion.fbx", sauna_pos, Vector3(4.8, 2.9, 3.8), Color(0.85, 0.55, 0.30), false)
	_build_sauna_pavilion_collision(sauna_pos)

	# 3. North Night-Market Food Court Stall (with solid table collider)
	var food_pos := Vector3(-11.0, 0.0, -5.5)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25373_FoodCourtTableSet.fbx", food_pos, Vector3(1.55, 0.90, 1.55), Color(0.88, 0.32, 0.18), true)
	_create_floating_label(food_pos + Vector3(0, 1.8, 0), "[F] North Night-Market Food Court\n(Eat Chongqing Hot Pot: +40 HP, +45% Light Cart Battery, +25% Speed)", Color(1.0, 0.82, 0.45))

	# 4. Quartermaster Chen's Raised Surplus Platform with Small Platform Stairs + Normalized Low-Poly Character
	var vendor_plat_pos := Vector3(-15.0, 0.0, 0.0)
	_build_platform_with_small_stairs(vendor_plat_pos, Vector3(4.2, 0.48, 5.2), "east", Color(0.26, 0.30, 0.34))
	var vendor_pos := Vector3(-14.2, 0.48, 0.0)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25310_CashDesk120.fbx", vendor_pos, Vector3(1.23, 1.05, 0.75), Color(0.32, 0.58, 0.42), true)
	var chen_node := _spawn_model_or_box("res://SourceAssets/Characters/NormalizedLowPoly/Quartermaster_Chen_LowPoly.glb", Vector3(-15.5, 0.48, 0.0), Vector3(0.5, 1.78, 0.5), Color(0.3, 0.7, 0.5), false)
	chen_node.rotation_degrees = Vector3(0, 90, 0)
	_create_floating_label(vendor_pos + Vector3(0, 2.25, 0), "[F] Quartermaster Chen — Flash & Phantom Surplus\nDispense Relay Fuse Crate ($60) | Camera Film Refill ($20)", Color(0.45, 1.0, 0.65))

	#    Spawn Separate Equipment Tools (Photo Camera, Light Remote Gun, Proton Ray Gun & Trap) as World Pickups!
	var proton_drop := PlaceableEvidenceActors.WorldDroppedItem.new()
	proton_drop.item_data = PlayerController.make_item_data("proton_gun")
	proton_drop.position = Vector3(-11.2, 0.0, -1.6)
	add_child(proton_drop)

	var camera_drop := PlaceableEvidenceActors.WorldDroppedItem.new()
	camera_drop.item_data = PlayerController.make_item_data("photo_camera")
	camera_drop.position = Vector3(-11.2, 0.0, -0.4)
	add_child(camera_drop)

	var remote_drop := PlaceableEvidenceActors.WorldDroppedItem.new()
	remote_drop.item_data = PlayerController.make_item_data("light_remote")
	remote_drop.position = Vector3(-11.2, 0.0, 0.8)
	add_child(remote_drop)

	# 5. Stage 6A: Dual 8.50m MallEscalators (Upward + Downward) connecting Floor 1 (Z = +4.32m, Y = 0.0m) -> Floor 2 (Z = -13.00m, Y = 8.50m)!
	escalator_up = MallEscalator.new()
	escalator_up.escalator_title = "East Concourse Escalator (F1 -> F2)"
	escalator_up.position = Vector3(1.55, 0.0, 4.322432)
	escalator_up.rotation_degrees = Vector3(0.0, 90.0, 0.0) # Local +X points along World -Z!
	escalator_up.reverse_direction = false
	escalator_up.set_operational_mode(MallEscalator.MaintenanceState.OPERATIONAL, true)
	add_child(escalator_up)

	escalator_down = MallEscalator.new()
	escalator_down.escalator_title = "West Concourse Escalator (F2 -> F1)"
	escalator_down.position = Vector3(-1.55, 0.0, 4.322432)
	escalator_down.rotation_degrees = Vector3(0.0, 90.0, 0.0)
	escalator_down.reverse_direction = true
	escalator_down.set_operational_mode(MallEscalator.MaintenanceState.OPERATIONAL, true)
	add_child(escalator_down)

	#    Raised Escalator Breaker Control Dais with Small Platform Stairs at (5.2, 0.0, 4.2)
	_build_platform_with_small_stairs(Vector3(5.2, 0.0, 4.2), Vector3(2.8, 0.48, 2.8), "west", Color(0.25, 0.28, 0.33))
	_create_static_box(self, Vector3(5.8, 1.18, 4.2), Vector3(0.35, 1.4, 0.85), Color(0.20, 0.55, 0.42), 0.4)
	_create_floating_label(Vector3(5.2, 2.35, 4.2), "[F] Escalator Breaker & Direction Control Dais\n(Walk up Small Platform Stairs & press [F] to Toggle/Motorize Escalators)", Color(0.45, 0.95, 1.0))

	# 6. Stage 6B (Floor 2 Beachhead): Two-Handed Heavy Hydraulic Assembly (Hands-Only Pickup!) & Service Freight Elevator
	var heavy_drop := PlaceableEvidenceActors.WorldDroppedItem.new()
	heavy_drop.item_data = PlayerController.make_item_data("heavy_hydraulic")
	heavy_drop.position = Vector3(-10.0, 8.50, -15.0)
	add_child(heavy_drop)

	var freight_pos := Vector3(10.0, 8.50, -15.0)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25359_LiftLobbyModule.fbx", freight_pos, Vector3(2.6, 3.2, 0.35), Color(0.35, 0.42, 0.5), true)
	_create_floating_label(freight_pos + Vector3(0, 2.6, 0), "[F] Stage 6B: Floor 2 Service Freight Elevator Shaft\n(Carry Heavy Hydraulic Assembly in Hands & Press [F] to Bring Carts Up!)", Color(0.45, 1.0, 0.75))

	# Elena Vance's Phased Audit Echo on Floor 2 (1.74m 1:1 scale)
	var _elena_node := _spawn_model_or_box("res://SourceAssets/Characters/NormalizedLowPoly/Elena_Vance_LowPoly.glb", Vector3(0.0, 8.50, -18.0), Vector3(0.5, 1.74, 0.5), Color(0.4, 0.9, 1.0), false)
	_create_floating_label(Vector3(0.0, 10.6, -18.0), "Elena Vance's Audit Trail #1\n\"Ride the Escalator up here, then haul the Hydraulic Assembly in your Hands to unlock the Freight Elevator!\"", Color(0.5, 0.95, 1.0))

	# 7. Haunted Store Interior Props: Fitting Room Hiding Booth, Ghost Writing Ledger & Peaceful Ritual Checkout
	var hiding_pos := Vector3(18.5, 0.0, -5.2)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25284_FittingCubicleClosed.fbx", hiding_pos, Vector3(1.11, 2.25, 1.26), Color(0.36, 0.30, 0.44), false)
	_create_floating_booth_walls(hiding_pos)
	_create_floating_label(hiding_pos + Vector3(0, 2.4, 0), "[F] Fitting Room Hiding Spot\n(Conceals from Lethal Hunts if entered without Ghost witness)", Color(0.75, 0.8, 1.0))

	var book_pos := Vector3(19.5, 0.85, 2.2)
	_create_static_box(self, Vector3(19.5, 0.425, 2.2), Vector3(1.1, 0.85, 0.75), Color(0.32, 0.26, 0.22), 0.5)
	_spawn_model_or_box("res://ExternalAssets/Mall/PolyHaven/decorative_book_set_01/SM_BookHardcover02.fbx", book_pos, Vector3(0.45, 0.12, 0.35), Color(0.65, 0.18, 0.18), false)
	_create_floating_label(book_pos + Vector3(0, 1.05, 0), "[F] Store Ghost Ledger & Peaceful Resolution Stand\n• Inspect Tell & Perform Peaceful Ritual (+$120 if Receptive)\n• Or Speak [9] \"Your shift is over\" on 87.9 FM", Color(1.0, 0.88, 0.55))

	# 8. Spawn Placeable Infrared Motion Sensor & Hidden UV Footprints inside Store East_F0_02
	var motion_sensor := PlaceableEvidenceActors.PlaceableMotionSensor.new()
	motion_sensor.store_id = "East_F0_02"
	motion_sensor.position = Vector3(16.2, 0.0, 0.0)
	add_child(motion_sensor)

	var uv_positions := [
		Vector3(16.8, 0.02, -1.6),
		Vector3(18.4, 0.02, 1.1),
		Vector3(20.2, 0.02, -0.8),
	]
	for i in range(uv_positions.size()):
		var uvp := PlaceableEvidenceActors.UVEvidencePrint.new()
		uvp.store_id = "East_F0_02"
		uvp.print_title = "UV Footprint #%d [Receptive Tell: Neat Clerk Step]" % (i + 1)
		uvp.position = uv_positions[i]
		add_child(uvp)

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
	push_cart.dock_world_position = Vector3(14.0, 0.0, 0.0)
	push_cart.store_interior_center = Vector3(21.0, 0.0, 0.0)
	push_cart.position = Vector3(11.2, 0.0, -1.2)
	add_child(push_cart)
	push_cart.place_item_on_cart(PlayerController.make_item_data("photo_camera"), 6)
	push_cart.place_item_on_cart(PlayerController.make_item_data("light_remote"), 7)
	_create_floating_label(Vector3(14.0, 2.5, 0.0), "STORE [East_F0_02] DOORWAY CART DOCK\nPark Push Cart across threshold = SEALED\nPush inside store = DORMANT", Color(1.0, 0.88, 0.35))

	light_cart = MallCartSystem.new()
	light_cart.cart_type = "LightCart"
	light_cart.position = Vector3(10.5, 0.0, 2.2)
	light_cart.rotation_degrees = Vector3(0, -90, 0)
	add_child(light_cart)

	# 1. Floor 1 Onboarding Store Ghost: Arthur Pendelton (East_F0_02 — Store Territory Bound!)
	store_ghost_east = StoreGhost.new()
	store_ghost_east.store_id = "East_F0_02"
	store_ghost_east.floor_level = 1
	store_ghost_east.can_briefly_pursue_outside_store = false
	store_ghost_east.position = Vector3(21.5, 0.0, 0.0)
	store_ghost_east.linked_push_cart = push_cart
	add_child(store_ghost_east)

	# 2. Floor 1 Secondary Store Ghost: Static Evelyn (West_F1_03 — Store Territory Bound!)
	store_ghost_west = StoreGhost.new()
	store_ghost_west.store_id = "West_F1_03"
	store_ghost_west.floor_level = 1
	store_ghost_west.can_briefly_pursue_outside_store = false
	store_ghost_west.doorway_threshold_x = -999.0
	store_ghost_west.position = Vector3(-19.5, 0.0, -8.5)
	add_child(store_ghost_west)
	var west_tell := "CORRUPTED (Hostile-Only — Peace Will Backfire!)" if store_ghost_west.is_corrupted_hostile else "RECEPTIVE TO PEACE"
	_create_floating_label(Vector3(-19.5, 2.8, -8.5), "West Boutique Haunting: Static Evelyn [West_F1_03] (Store Territory Bound)\nRolled Run Disposition: %s\n[F] Attempt Peaceful Ritual | Proton Ray Gun + Trap" % west_tell, Color(1.0, 0.5, 0.55) if store_ghost_west.is_corrupted_hostile else Color(0.5, 1.0, 0.75))

	# 3. Upper-Floor Store Ghost (Floor 2 East Mezzanine, Y = 8.50m): The Lost Shopper (East_F2_01)
	#    Only upper floors introduce store ghosts comfortable leaving their store briefly (6s) to continue pursuing players!
	store_ghost_floor2 = StoreGhost.new()
	store_ghost_floor2.store_id = "East_F2_01"
	store_ghost_floor2.floor_level = 2
	store_ghost_floor2.can_briefly_pursue_outside_store = true
	store_ghost_floor2.position = Vector3(16.0, 8.50, -15.0)
	add_child(store_ghost_floor2)
	_create_floating_label(
		Vector3(16.0, 11.3, -15.0),
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
		_handle_contextual_interact()

	if Input.is_action_just_pressed("secondary_action") and is_instance_valid(player) and not player.is_floating_essence:
		_try_open_nearby_death_crate_on_floor()

	if Input.is_action_just_pressed("toggle_journal") and journal_panel:
		journal_panel.visible = not journal_panel.visible
		if journal_panel.visible:
			_refresh_journal_text()

	_refresh_hud()

func toggle_coop_menu(force_state: Variant = null) -> void:
	if coop_menu_panel == null:
		return
	var next_vis: bool = (not coop_menu_panel.visible) if force_state == null else bool(force_state)
	coop_menu_panel.visible = next_vis
	if is_instance_valid(player):
		player.voice_menu_open = next_vis or (voice_menu_panel != null and voice_menu_panel.visible)
	if next_vis:
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
	else:
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

func _unhandled_key_input(event: InputEvent) -> void:
	if Engine.is_editor_hint() or voice_menu_panel == null or not voice_menu_panel.visible:
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
	if is_instance_valid(player):
		player.voice_menu_open = next_vis
	if next_vis:
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

func _try_open_nearby_death_crate_on_floor() -> void:
	var ppos := player.global_position
	for c_node in get_tree().get_nodes_in_group("death_inventory_crates"):
		var crate := c_node as PlaceableEvidenceActors.DeathInventoryCrate
		if crate and ppos.distance_to(crate.global_position) < 2.8:
			crate.open_and_spill_to_floor(self)
			return

	# If standing next to the 8-Slot Equipment Push Cart (<= 2.95m) and it has stored items on its shelves, pressing [E] takes the highlighted item off the cart!
	for pc_node in get_tree().get_nodes_in_group("push_carts"):
		var pc := pc_node as MallCartSystem
		if pc and ppos.distance_to(pc.global_position) < 2.95 and pc.get_stored_item_count() > 0:
			var taken := pc.take_highlighted_item_from_cart()
			if not taken.is_empty():
				player.receive_item_from_cart(taken)
				GameState.status_message.emit(
					"Took %s off Equipment Push Cart (%d/8 stored on cart)." % [taken.get("name", "Tool"), pc.get_stored_item_count()],
					Color(1.0, 0.90, 0.42)
				)
				return

	# If pushing or standing next to a Wheeled Light Cart (<= 3.0m), pressing [E] toggles its overhead floodlight ON/OFF!
	for lc_node in get_tree().get_nodes_in_group("light_carts"):
		var lc := lc_node as MallCartSystem
		if lc and ((lc.is_attached_to_player and lc.attached_player == player) or ppos.distance_to(lc.global_position) < 3.0):
			lc.toggle_floodlight("Player Switch [E]")
			return

	# If standing right over a WorldDroppedItem on the floor (<= 2.2m) that can be switched ON/OFF:
	for d_node in get_tree().get_nodes_in_group("world_dropped_items"):
		var drop := d_node as PlaceableEvidenceActors.WorldDroppedItem
		if drop and drop.can_power_toggle() and ppos.distance_to(drop.global_position) < 2.2:
			drop.toggle_ground_power()
			return

func _handle_contextual_interact() -> void:
	var ppos := player.global_position

	# 0. If the player is ALREADY pushing a cart, pressing [F] ALWAYS parks that cart first!
	#    This prevents accidentally grabbing the second cart when parking near it.
	var detached_any := false
	for node in get_tree().get_nodes_in_group("mall_carts"):
		var c := node as MallCartSystem
		if c and c.is_attached_to_player and c.attached_player == player:
			c.detach_from_player(false)
			detached_any = true
	if detached_any:
		return

	# 0A. Pick up a Death Inventory Crate into Hands (Hands-Only — cannot go on the 4-slot hotbar!)
	if not player.is_floating_essence:
		for c_node in get_tree().get_nodes_in_group("death_inventory_crates"):
			var crate := c_node as PlaceableEvidenceActors.DeathInventoryCrate
			if crate and ppos.distance_to(crate.global_position) < 2.8:
				player.pickup_death_crate_in_hands(crate)
				return

	# 0B. Pick up any individual WorldDroppedItem on the floor (slots into 1-4 hotbar, or into Hands if full / Hands-Only!)
	if not player.is_floating_essence:
		var best_drop: PlaceableEvidenceActors.WorldDroppedItem = null
		var best_dist := 2.6
		for d_node in get_tree().get_nodes_in_group("world_dropped_items"):
			var drop := d_node as PlaceableEvidenceActors.WorldDroppedItem
			if drop:
				var d := ppos.distance_to(drop.global_position)
				if d < best_dist:
					best_dist = d
					best_drop = drop
		if best_drop:
			player.pickup_world_item(best_drop)
			return

	# 1. Attach the single closest Cart (Push Cart OR Light Cart — never both!) within 2.5m
	if not player.is_floating_essence:
		var best_cart: MallCartSystem = null
		var best_cart_dist := 2.5
		for node in get_tree().get_nodes_in_group("mall_carts"):
			var c := node as MallCartSystem
			if c:
				var d := ppos.distance_to(c.global_position)
				if d < best_cart_dist:
					best_cart_dist = d
					best_cart = c
		if best_cart:
			best_cart.toggle_attach(player)
			return

	# 2. Concourse Rest Bench (-4.5, 0, 4.5) -> 3rd-Person Rest
	if ppos.distance_to(Vector3(-4.5, 0.0, 4.5)) < 3.0:
		player.enter_contextual_third_person(Vector3(-4.5, 0.0, 4.5), "Sitting on Concourse Rest Bench (+24 Stamina/s)")
		return

	# 3. South Tangquan Pink Salt Bathhouse & Sauna Revival Ritual Shrine (-11, 0, 11)
	if ppos.distance_to(Vector3(-11.0, 0.0, 11.0)) < 5.0 or ppos.distance_to(Vector3(-17.2, 0.0, 11.0)) < 4.5:
		if player.is_floating_essence:
			player.revive_from_sauna_ritual()
		else:
			GameState.sauna_soul_anchor_lit = true
			player.enter_contextual_third_person(Vector3(-11.0, 0.28, 11.0), "Soaking in Pink Salt Spa (+60s Ward & Soul-Anchor Lit!)")
		return

	# 4. North Night-Market Food Court (-11, 0, -5.5) -> Sit down at table in 3rd-Person View for 5.0s minimum & watch bar fill up!
	if ppos.distance_to(Vector3(-11.0, 0.0, -5.5)) < 3.6:
		if player.is_eating_meal:
			GameState.status_message.emit(
				"Still eating Chongqing Hot Pot at the table (%.1fs / %.1fs)..." % [player.eating_timer, player.eating_duration],
				Color(1.0, 0.84, 0.38)
			)
		elif player.contextual_seat_active:
			player.exit_contextual_third_person()
		else:
			player.start_eating_at_table(Vector3(-11.0, 0.0, -6.25), Vector3(-11.0, 0.0, -5.5), 5.0)
		return

	# 5. Quartermaster Chen's Vendor Counter (-14.2, 0.48, 0)
	if ppos.distance_to(Vector3(-14.2, 0.48, 0.0)) < 3.8:
		if not player.is_carrying_item_id("escalator_fuse") and not GameState.powered_circuits.get("EscalatorCircuit_F1_To_F2", false):
			if GameState.spend_cash(60, "Heavy-Duty Escalator Relay Fuse Crate"):
				player.pickup_item_by_id_into_inventory_or_hands("escalator_fuse")
				GameState.status_message.emit("Purchased Escalator Relay Fuse Crate (Carried in Hands)! Take it to the Escalator Breaker Dais (5.2, 0.48, 4.2).", Color(0.45, 1.0, 0.7))
		else:
			if GameState.spend_cash(20, "Evidence Camera Film Pack (+10 Exposures)"):
				player.photo_exposures_left = 10
		return

	# 6. Escalator Breaker Control Dais (5.2, 0.48, 4.2) or Escalator Lower Entrance
	if ppos.distance_to(Vector3(5.2, 0.48, 4.2)) < 3.8 or ppos.distance_to(Vector3(1.55, 0.0, 4.3)) < 3.5:
		if player.consume_carried_item_id("escalator_fuse"):
			GameState.power_escalator_circuit("EscalatorCircuit_F1_To_F2")
			escalator_up.set_operational_mode(MallEscalator.MaintenanceState.OPERATIONAL, true)
			return
		var new_run := not escalator_up.running
		escalator_up.set_operational_mode(
			MallEscalator.MaintenanceState.OPERATIONAL if new_run else MallEscalator.MaintenanceState.STALLED_WALKABLE,
			new_run
		)
		GameState.status_message.emit(
			"Escalator Breaker Toggled! East Escalator is now %s (Walk directly onto steps to ride/climb to Floor 2!)." % ("MOTORIZED (2.15 m/s)" if new_run else "STALLED WALKABLE STAIRS"),
			Color(0.45, 0.95, 1.0)
		)
		return

	# 7. Stage 6B (Floor 2): Service Freight Elevator Shaft (10, 8.5, -15)
	if ppos.distance_to(Vector3(10.0, 8.5, -15.0)) < 4.0:
		if player.consume_carried_item_id("heavy_hydraulic"):
			GameState.repair_freight_elevator(2)
			push_cart.global_position = Vector3(6.5, 8.5, -14.5)
			light_cart.global_position = Vector3(8.0, 8.5, -14.5)
		elif GameState.freight_elevator_unlocked_floors.get(2, false):
			player.global_position = Vector3(2.0, 0.2, 0.0)
			GameState.current_floor = 1
			GameState.status_message.emit("Took Service Freight Elevator back to Floor 1 Central Oval Sanctuary.", Color(0.5, 0.95, 1.0))
		else:
			GameState.status_message.emit("Floor 2 Service Freight Elevator Offline! Pick up the Two-Handed Heavy Hydraulic Assembly on the West Balcony (-10, 8.5, -15) in your Hands first.", Color(1.0, 0.6, 0.35))
		return

	# 8. Fitting Room Hiding Booth (18.5, 0, -5.2)
	if ppos.distance_to(Vector3(18.5, 0.0, -5.2)) < 2.8:
		var witnessed := (store_ghost_east.current_phase == StoreGhost.GhostPhase.VISIBLE_HUNT and store_ghost_east.global_position.distance_to(ppos) < 5.0)
		player.is_hidden_without_witness = not witnessed
		player.enter_contextual_third_person(Vector3(18.5, 0.0, -5.2), "Hidden in Fitting Room Booth (%s)" % ("WITNESSED!" if witnessed else "Undetected"))
		return

	# 9. Store East_F0_02 Ledger & Peaceful Lore Resolution Stand (19.5, 0, 2.2)
	if ppos.distance_to(Vector3(19.5, 0.0, 2.2)) < 3.4:
		GameState.record_evidence("East_F0_02", "Ghost Writing", 40)
		GameState.discover_lore("LORE_LEDGER_EAST_F0_02", true)
		store_ghost_east.try_peaceful_resolution()
		return

	# 10. Store West_F1_03 Peaceful Ritual Attempt (-19.5, 0, -8.5) — May Backfire if Corrupted!
	if ppos.distance_to(Vector3(-19.5, 0.0, -8.5)) < 3.8:
		store_ghost_west.try_peaceful_resolution()
		return

	GameState.status_message.emit("Step closer to a Dropped Item, Death Crate, Cart, Bench, Spa/Sauna, Vendor, Escalator Breaker, or Ledger and press [F].", Color(0.75, 0.82, 0.9))

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

	# [P] Co-op Multiplayer & LAN/Tailscale Connection Modal
	coop_menu_panel = PanelContainer.new()
	coop_menu_panel.set_anchors_preset(Control.PRESET_CENTER)
	coop_menu_panel.offset_left = -310
	coop_menu_panel.offset_right = 310
	coop_menu_panel.offset_top = -185
	coop_menu_panel.offset_bottom = 185
	coop_menu_panel.visible = false
	coop_menu_panel.add_theme_stylebox_override("panel", vm_style)
	root_ctrl.add_child(coop_menu_panel)

	var cm_vbox := VBoxContainer.new()
	cm_vbox.add_theme_constant_override("separation", 10)
	coop_menu_panel.add_child(cm_vbox)

	var cm_title := Label.new()
	cm_title.text = "CO-OP MULTIPLAYER LOBBY [P] — ENET UDP PORT 7777"
	cm_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	cm_title.add_theme_font_size_override("font_size", 18)
	cm_title.modulate = Color(0.45, 0.96, 1.0)
	cm_vbox.add_child(cm_title)

	coop_status_lbl = Label.new()
	coop_status_lbl.text = "Solo / Offline Mode — Press [F8] to Host or [F9] to Join"
	coop_status_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	coop_status_lbl.add_theme_font_size_override("font_size", 13)
	coop_status_lbl.modulate = Color(0.95, 0.85, 0.45)
	cm_vbox.add_child(coop_status_lbl)

	var ip_row := HBoxContainer.new()
	ip_row.alignment = BoxContainer.ALIGNMENT_CENTER
	ip_row.add_theme_constant_override("separation", 10)
	cm_vbox.add_child(ip_row)

	var ip_lbl := Label.new()
	ip_lbl.text = "Host IP (Local / LAN / Tailscale):"
	ip_row.add_child(ip_lbl)

	coop_ip_input = LineEdit.new()
	coop_ip_input.text = "127.0.0.1"
	coop_ip_input.placeholder_text = "127.0.0.1 or LAN/Tailscale IP"
	coop_ip_input.custom_minimum_size = Vector2(240, 34)
	ip_row.add_child(coop_ip_input)

	var host_btn := Button.new()
	host_btn.text = "[F8]  Host Co-op Server on UDP Port 7777 (Max 4 Investigators)"
	host_btn.custom_minimum_size = Vector2(560, 40)
	host_btn.pressed.connect(func() -> void:
		MultiplayerManager.host_coop_session()
		toggle_coop_menu(false)
	)
	cm_vbox.add_child(host_btn)

	var join_btn := Button.new()
	join_btn.text = "[F9]  Join Co-op Server at Host IP above (Port 7777)"
	join_btn.custom_minimum_size = Vector2(560, 40)
	join_btn.pressed.connect(func() -> void:
		MultiplayerManager.join_coop_session(coop_ip_input.text)
		toggle_coop_menu(false)
	)
	cm_vbox.add_child(join_btn)

	var disc_btn := Button.new()
	disc_btn.text = "Disconnect / Return to Solo Mode"
	disc_btn.custom_minimum_size = Vector2(560, 36)
	disc_btn.modulate = Color(1.0, 0.72, 0.55)
	disc_btn.pressed.connect(func() -> void:
		MultiplayerManager.disconnect_coop_session()
		toggle_coop_menu(false)
	)
	cm_vbox.add_child(disc_btn)

	var close_coop_btn := Button.new()
	close_coop_btn.text = "[P / Esc] Close Co-op Menu"
	close_coop_btn.custom_minimum_size = Vector2(560, 34)
	close_coop_btn.pressed.connect(func() -> void: toggle_coop_menu(false))
	cm_vbox.add_child(close_coop_btn)

func _refresh_hud() -> void:
	if hud_label == null or not is_instance_valid(player):
		return
	var seal_text := push_cart.get_seal_state_label() if is_instance_valid(push_cart) else "N/A"
	var east_status := store_ghost_east.get_phase_label() if is_instance_valid(store_ghost_east) else "Dormant"
	var west_tell := store_ghost_west.get_disposition_tell_short() if is_instance_valid(store_ghost_west) else "N/A"
	var ward_str := ("%.0fs ACTIVE" % player.pink_salt_ward_timer) if player.pink_salt_ward_timer > 0.0 else ("Soul-Anchor Lit" if GameState.sauna_soul_anchor_lit else "Inactive")
	var freight_str := "ONLINE (Carts on F2)" if GameState.freight_elevator_unlocked_floors.get(2, false) else "OFFLINE (Need Hydraulic Part)"
	var cur_item := player.get_current_active_item()

	hud_label.text = (
		"[b][color=#66f2ff]MEGAMALL ANOMALY — FLOOR %d (Godot 4.7 Vulkan)[/color][/b]\n" % GameState.current_floor +
		"[b]Team Cash:[/b] [color=#55ff88]$%d[/color] | [b]Deaths:[/b] %d | [b]F2 Freight Elevator:[/b] %s\n" % [
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
		"[color=#aaaaaa][1-4/Wheel] Hotbar | [LMB] Gadget ON/OFF | [G] Throw/Drop Active | [E] Light Cart / Mode | [J] Voice Menu | [F] Interact[/color]"
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
			"[color=#99bbcc][F8] Host Server | [F9] Join 127.0.0.1 | [P] Co-op IP Menu | [V] PTT[/color]"
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
	var ev_east: Array = GameState.store_evidence_ledger.get("East_F0_02", [])
	var lore_lines := ""
	for k in GameState.discovered_lore.keys():
		var item: Dictionary = GameState.discovered_lore[k]
		lore_lines += "• [b]%s:[/b] %s\n\n" % [item["title"], item["text"]]

	journal_text.text = (
		"[b][color=#66f2ff]INVESTIGATOR CASE FILE, DUAL RESOLUTION LEDGER & PROGRESSION STATUS[/color][/b]\n" +
		"==================================================================================\n" +
		"[b]1. East_F0_02 — Pendelton's Book & Curio Shop (Arthur Pendelton — Onboarding Store):[/b]\n" +
		"   • Confirmed Evidence: %s | Disposition Tell: [color=#55ff88]RECEPTIVE TO PEACE[/color]\n" % [", ".join(ev_east) if not ev_east.is_empty() else "Scan with EMF [1], UV/Spirit Box 87.9 FM [3], or Ledger [F]"] +
		"   • Resolution Status: %s\n\n" % GameState.resolved_stores.get("East_F0_02", "Active Haunting (Press [F] at Ledger or speak [9] 'Your shift is over' for +$120 Peaceful Resolution)") +
		"[b]2. West_F1_03 — Sheer Terror Hosiery (Static Evelyn — Rolled Per Run):[/b]\n" +
		"   • Rolled Disposition Tell: %s\n" % ("[color=#ff5555]CORRUPTED (Hostile-Only — Peaceful Ritual Triggers ENRAGED HUNT! Pick up Proton Gun [F] + Canister Trap!)[/color]" if store_ghost_west.is_corrupted_hostile else "[color=#55ff88]RECEPTIVE TO PEACE (Can resolve peacefully with [F] or forcefully with Proton Gun)[/color]") +
		"   • Resolution Status: %s\n\n" % GameState.resolved_stores.get("West_F1_03", "Active Haunting") +
		"[b]3. 4-Slot Hotbar + Hands (Pseudo-5th Slot) & Death Inventory Crate Rules:[/b]\n" +
		"   • Dying packs all your items into a [b]Death Inventory Crate[/b] (unusable until opened; carried in Hands [F]).\n" +
		"   • Opening the crate ([LMB] or [E]) spills all stored items onto the floor near where it was opened to prevent overriding your 4-slot hotbar.\n" +
		"   • Switching to [1-4] while holding an item in Hands drops the hand item if you have no empty slot for it.\n\n" +
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
	var mat := StandardMaterial3D.new()
	mat.albedo_color = color
	mat.roughness = roughness
	mi.material_override = mat
	sb.add_child(mi)
	parent.add_child(sb)
	return sb

func _spawn_model_or_box(res_path: String, pos: Vector3, fallback_size: Vector3, fallback_color: Color, add_box_collision: bool = false) -> Node3D:
	var root := Node3D.new()
	root.position = pos
	add_child(root)

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
			root.add_child(inst)
			return root

	var mi := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = fallback_size
	mi.mesh = box
	mi.position = Vector3(0, fallback_size.y * 0.5, 0)
	var mat := StandardMaterial3D.new()
	mat.albedo_color = fallback_color
	mi.material_override = mat
	root.add_child(mi)
	return root

func _create_floating_label(pos: Vector3, text: String, color: Color) -> Label3D:
	var lbl := Label3D.new()
	lbl.position = pos
	lbl.text = text
	lbl.modulate = color
	lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	lbl.font_size = 26
	lbl.outline_size = 7
	add_child(lbl)
	return lbl
