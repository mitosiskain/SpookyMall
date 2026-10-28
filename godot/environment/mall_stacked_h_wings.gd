@tool
class_name MallStackedHWings
extends Node3D
## Megamall Anomaly — Smoothly Curving ~800m-Long Stacked-"H" Parallel Shopping Hallways,
## Wide 32m Storefronts, 60m x 26m End-Cap Anchor Department Stores, and the
## Whole-West-Side Floor 1 Himalayan Pink Salt & Tangquan Sauna Liminal Biome!
##
## Architectural Highlights:
## 1. Whole-Side Floor 1 Himalayan Pink Salt & Tangquan Sauna Liminal Biome (X = -32m .. -112m, Z = -38m .. +38m):
##    - A dedicated 80m x 76m multi-chamber liminal bathhouse & sauna wing occupying the entire West side of Floor 1.
##    - Backlit glowing Himalayan Pink Salt crystal brick walls, slatted Hinoki cedar ceilings, 3-tier walkable cedar
##      sauna amphitheater benches, 6 red-hot sauna stoves, 3 steaming geothermal jade/salt onsen pools, 4 glass sauna
##      pavilions, tiled Poolrooms shower labyrinth, Soul-Anchor Revival Shrine, and active volumetric rising steam
##      (GPUParticles3D with procedural soft radial mist puffs)!
## 2. ~800m-Long Smoothly Curving Parallel Hallway Wings (North & South, 10x Longer!):
##    - 24 Stacked-H Bays of 32.0m-wide stores (768.0m) + 26.0m-deep x 60.0m-wide End-Cap Anchor Store = 794.0m per wing
##      (1,672m = 1.67 km total end-to-end mall length!).
##    - Two Parallel 9.0m-Wide Shopping Concourses:
##      * West Parallel Hallway: local X = [-20.0m .. -11.0m] (center -15.5m)
##      * East Parallel Hallway: local X = [+11.0m .. +20.0m] (center +15.5m)
##    - Central Column of 32.0m-Wide x 22.0m-Deep Pass-Through Stores (local X = [-11.0m .. +11.0m]):
##      * Wide 5.8m entrances on BOTH parallel hallways (creating 24 stacked capital "H" crossbars per wing!)
##      * 3.4m-wide internal neighbor doorways connecting adjacent center stores.
##    - Outer Columns of 32.0m-Wide x 26.0m-Deep Retail Stores (local |X| = [20.0m .. 46.0m]) with 9.9m glazed display windows.
##    - Smooth Gradual Curve Seen Quickly (Bays 1..7, s = 32m..256m, 5.0 deg per bay = 35.0 deg gentle curve, R = 366.7m),
##      followed by a Long 512m Post-Curve Travel (Bays 8..23, s = 256m..768m) into the 60m x 26m End-Cap Anchor Store!

const HUB_HALF_Z: float = 42.0
## PRE-PUBLISH REMINDER: Restore `NUM_BAYS = PRODUCTION_NUM_BAYS` (8 bays = 256m) before publishing!
## Currently set to `PLAYTEST_NUM_BAYS` (2 bays = 64m, 1/4 length) for rapid playtesting of the Multi-Level End-Cap Anchor Stores.
const PRODUCTION_NUM_BAYS: int = 8
const PLAYTEST_NUM_BAYS: int = 2
const NUM_BAYS: int = PLAYTEST_NUM_BAYS
const BAY_LENGTH: float = 32.0
const ANCHOR_DEPTH: float = 32.0
const ANCHOR_WIDTH: float = 60.0

const WEST_OUTER_WALL_X: float = -46.0
const WEST_HALL_OUTER_X: float = -20.0
const WEST_HALL_CENTER_X: float = -15.5
const WEST_HALL_INNER_X: float = -11.0
const EAST_HALL_INNER_X: float = 11.0
const EAST_HALL_CENTER_X: float = 15.5
const EAST_HALL_OUTER_X: float = 20.0
const EAST_OUTER_WALL_X: float = 46.0

# ------------------------------------------------------------------------------
# StoreSecurityShutter: Motorized Corrugated Steel Security Shutter Gate
# Mounted at Haunted Store Entrances — Slams Shut & Locks During Ghost Hunts!
# ------------------------------------------------------------------------------
class StoreSecurityShutter extends StaticBody3D:
	var store_id: String = ""
	var door_width: float = 5.8
	var door_height: float = 4.4
	var is_locked_closed: bool = false
	var shutter_mesh: MeshInstance3D = null
	var shutter_col: CollisionShape3D = null
	var shutter_label: Label3D = null

	func _ready() -> void:
		add_to_group("store_security_shutters")

	func setup_shutter(p_store_id: String, p_width: float, p_height: float, mat_shutter: Material) -> void:
		store_id = p_store_id
		door_width = p_width
		door_height = p_height

		shutter_col = CollisionShape3D.new()
		var box_shape := BoxShape3D.new()
		box_shape.size = Vector3(door_width, door_height, 0.28)
		shutter_col.shape = box_shape
		shutter_col.position = Vector3(0.0, door_height * 0.5, 0.0)
		shutter_col.disabled = true
		add_child(shutter_col)

		shutter_mesh = MeshInstance3D.new()
		var bm := BoxMesh.new()
		bm.size = Vector3(door_width, door_height, 0.16)
		shutter_mesh.mesh = bm
		shutter_mesh.material_override = mat_shutter
		add_child(shutter_mesh)

		shutter_label = Label3D.new()
		shutter_label.position = Vector3(0.0, 2.1, 0.0)
		shutter_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		shutter_label.font_size = 20
		shutter_label.outline_size = 6
		shutter_label.visibility_range_end = 26.0
		shutter_label.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
		add_child(shutter_label)

		_apply_shutter_visual_state()

	func set_shutter_locked_for_store(target_store_id: String, locked: bool) -> void:
		if target_store_id != store_id:
			return
		is_locked_closed = locked
		_apply_shutter_visual_state()

	func force_unlock_shutter() -> void:
		is_locked_closed = false
		_apply_shutter_visual_state()

	func _apply_shutter_visual_state() -> void:
		if shutter_col:
			shutter_col.set_deferred("disabled", not is_locked_closed)
		if shutter_mesh:
			if is_locked_closed:
				shutter_mesh.position = Vector3(0.0, door_height * 0.5, 0.0)
				shutter_mesh.scale = Vector3(1.0, 1.0, 1.0)
			else:
				# Rolled up into the top header valence (4.15m..4.40m) when unlocked
				shutter_mesh.position = Vector3(0.0, door_height - 0.16, 0.0)
				shutter_mesh.scale = Vector3(1.0, 0.07, 1.0)
		if shutter_label:
			shutter_label.visible = is_locked_closed
			if is_locked_closed:
				shutter_label.text = "🔒 SECURITY SHUTTER LOCKED [%s]\nGHOST HUNT IN PROGRESS — EXIT SEALED!" % store_id
				shutter_label.modulate = Color(1.0, 0.30, 0.28)

@export var built_wing_floors: int = 7 ## Builds the Central Hub & Wing architecture across all 7 built-in floors!

var _mat_floor: Material = null
var _mat_terrazzo: Material = null
var _mat_plaster: Material = null
var _mat_plaster_warm: Material = null
var _mat_ceiling: Material = null
var _mat_glass: Material = null
var _mat_steel: Material = null
var _mat_shutter: Material = null
var _mat_pink_salt: StandardMaterial3D = null
var _mat_cedar: Material = null
var _mat_pool_tile: StandardMaterial3D = null
var _mat_water: Material = null
var _steam_puff_tex: Texture2D = null

var _north_stations_c: Array[Vector3] = []
var _north_stations_r: Array[Vector3] = []
var _north_stations_t: Array[Vector3] = []

var _south_stations_c: Array[Vector3] = []
var _south_stations_r: Array[Vector3] = []
var _south_stations_t: Array[Vector3] = []

func _ready() -> void:
	name = "MallStackedHWings"
	add_to_group("mall_stacked_h_wings")
	rebuild_wings()

func _compute_bay_yaw_rad(bay_idx: int, is_north: bool) -> float:
	# Playtest 1/4 length (2 Bays = 64m + 32m Multi-Level End-Cap Anchor Store = 96m per wing)
	# vs Production length (8 Bays = 256m + 32m Multi-Level End-Cap Anchor Store = 288m per wing):
	# - Bay 0 (s = 0..32m): 0 deg so Bay 0 stores stay aligned and players see the curve starting at s = 32m!
	# - Bay 1..3: Smooth 9.5 deg per bay gentle curve into the Multi-Level End-Cap Anchor Store!
	var deg := 0.0
	if bay_idx <= 0:
		deg = 0.0
	elif bay_idx <= 3:
		deg = float(bay_idx) * 9.5
	else:
		deg = 28.5 + 1.0 * sin(float(bay_idx - 3) * 0.65)
	var sign_dir := 1.0 if is_north else -1.0
	return deg_to_rad(deg * sign_dir)

func _precompute_wing_spine(is_north: bool) -> void:
	var c_arr: Array[Vector3] = []
	var r_arr: Array[Vector3] = []
	var t_arr: Array[Vector3] = []

	var z_sign := -1.0 if is_north else 1.0
	var cur_c := Vector3(0.0, 0.0, z_sign * HUB_HALF_Z)
	c_arr.append(cur_c)

	var bay_dirs: Array[Vector3] = []
	var bay_rights: Array[Vector3] = []
	for k in range(NUM_BAYS):
		var yaw := _compute_bay_yaw_rad(k, is_north)
		# At yaw = 0: North forward is (0, 0, -1), South forward is (0, 0, +1).
		# Right vector R at yaw = 0 is ALWAYS (+1, 0, 0) so local_x < 0 is West and local_x > 0 is East!
		var fwd := Vector3(sin(yaw), 0.0, z_sign * cos(yaw)).normalized()
		var right := Vector3(cos(yaw), 0.0, -z_sign * sin(yaw)).normalized()
		bay_dirs.append(fwd)
		bay_rights.append(right)
		cur_c = cur_c + fwd * BAY_LENGTH
		c_arr.append(cur_c)

	for k in range(NUM_BAYS + 1):
		if k == 0:
			r_arr.append(bay_rights[0])
			t_arr.append(bay_dirs[0])
		elif k == NUM_BAYS:
			r_arr.append(bay_rights[NUM_BAYS - 1])
			t_arr.append(bay_dirs[NUM_BAYS - 1])
		else:
			var r_prev := bay_rights[k - 1]
			var r_next := bay_rights[k]
			var miter := (r_prev + r_next).normalized()
			var scale_m := 1.0 / maxf(0.65, miter.dot(r_next))
			r_arr.append(miter * scale_m)
			t_arr.append((bay_dirs[k - 1] + bay_dirs[k]).normalized())

	if is_north:
		_north_stations_c = c_arr
		_north_stations_r = r_arr
		_north_stations_t = t_arr
	else:
		_south_stations_c = c_arr
		_south_stations_r = r_arr
		_south_stations_t = t_arr

## Evaluates the exact 3D world position at fractional bay `bay_float` (0.0 .. 24.8) and lateral offset `local_x` (-46..+46).
func get_wing_world_point(is_north: bool, bay_float: float, local_x: float, floor_y: float = 0.0) -> Vector3:
	if _north_stations_c.is_empty():
		_precompute_wing_spine(true)
		_precompute_wing_spine(false)
	var c_arr := _north_stations_c if is_north else _south_stations_c
	var r_arr := _north_stations_r if is_north else _south_stations_r
	var t_arr := _north_stations_t if is_north else _south_stations_t

	if bay_float <= float(NUM_BAYS):
		var k := clampi(int(floor(bay_float)), 0, NUM_BAYS - 1)
		var frac := clampf(bay_float - float(k), 0.0, 1.0)
		var p0 := c_arr[k] + r_arr[k] * local_x
		var p1 := c_arr[k + 1] + r_arr[k + 1] * local_x
		var p := p0.lerp(p1, frac)
		p.y = floor_y
		return p
	else:
		# Inside the End-Cap Anchor Department Store beyond station NUM_BAYS
		var extra_m := (bay_float - float(NUM_BAYS)) * BAY_LENGTH
		var p_end := c_arr[NUM_BAYS] + r_arr[NUM_BAYS] * local_x + t_arr[NUM_BAYS] * extra_m
		p_end.y = floor_y
		return p_end

func rebuild_wings() -> void:
	for c in get_children():
		c.queue_free()

	_mat_floor = MallPbrMaterialLibrary.get_marble_floor_material()
	_mat_terrazzo = MallPbrMaterialLibrary.get_terrazzo_material(Color(0.87, 0.84, 0.79), 0.42)
	_mat_plaster = MallPbrMaterialLibrary.get_plaster_material(Color(0.92, 0.90, 0.86), 0.38)
	_mat_plaster_warm = MallPbrMaterialLibrary.get_plaster_material(Color(0.90, 0.87, 0.81), 0.42)
	_mat_ceiling = MallPbrMaterialLibrary.get_ceiling_tile_material()
	_mat_glass = MallPbrMaterialLibrary.get_architectural_glass_material(Color(0.76, 0.88, 0.94, 0.28))
	_mat_steel = MallPbrMaterialLibrary.get_brushed_steel_material()
	_mat_shutter = MallPbrMaterialLibrary.get_painted_shutter_material(Color(0.52, 0.56, 0.62), 0.55)
	_mat_cedar = StandardMaterial3D.new()
	_mat_cedar.albedo_color = Color(0.68, 0.46, 0.28)
	_mat_cedar.roughness = 0.52
	_mat_water = MallPbrMaterialLibrary.get_animated_water_material()

	_mat_pink_salt = StandardMaterial3D.new()
	_mat_pink_salt.albedo_color = Color(0.96, 0.62, 0.52)
	_mat_pink_salt.roughness = 0.46
	_mat_pink_salt.emission_enabled = true
	_mat_pink_salt.emission = Color(0.94, 0.44, 0.30)
	_mat_pink_salt.emission_energy_multiplier = 0.42

	_mat_pool_tile = MallPbrMaterialLibrary.get_plaster_material(Color(0.78, 0.88, 0.86), 0.50)

	_steam_puff_tex = _create_procedural_steam_puff_texture()

	_precompute_wing_spine(true)
	_precompute_wing_spine(false)

	_build_central_hub_ground_and_perimeter()
	_build_himalayan_sauna_liminal_biome()
	_build_curved_wing(true)  # North ~282m Curved Stacked-H Wing (8 Bays + End-Cap Anchor Store)
	_build_curved_wing(false) # South ~282m Curved Stacked-H Wing (8 Bays + End-Cap Anchor Store)

# ------------------------------------------------------------------------------
# 1. Central Hub Multi-Floor East/West Hub Perimeter Enclosure
#    (Ground Floor 160m x 84m Slab is built once in MegamallWorldBuilder with zero Z-fighting!)
# ------------------------------------------------------------------------------
func _build_central_hub_ground_and_perimeter() -> void:
	var hub_root := Node3D.new()
	hub_root.name = "ExtendedMallFoundation"
	add_child(hub_root)

	# Upper Floor Perimeter Walls for Floors 2..built_wing_floors along X = -46.0m and X = +46.0m (Z = -42.0..+42.0m)
	# (Floor 1 West X = -46.0m is open into the Himalayan Sauna Biome, while Floor 1 East X = +46.0m is enclosed!)
	var total_h := float(built_wing_floors) * 8.50
	_create_box(hub_root, Vector3(46.0, total_h * 0.5, 0.0), Vector3(0.50, total_h, HUB_HALF_Z * 2.0), _mat_plaster)
	_create_box(hub_root, Vector3(-46.0, 8.50 + (total_h - 8.50) * 0.5, 0.0), Vector3(0.50, total_h - 8.50, HUB_HALF_Z * 2.0), _mat_plaster)

	# North & South Upper Hub End-Wall Closures outside the Parallel Hallways (for Floors 3..built_wing_floors)
	for fl_num in range(3, built_wing_floors + 1):
		var fy := float(fl_num - 1) * 8.50
		for z_sign in [-1.0, 1.0]:
			_create_box(hub_root, Vector3(0.0, fy + 4.0, z_sign * HUB_HALF_Z), Vector3(92.0, 8.0, 0.45), _mat_plaster_warm)

# ------------------------------------------------------------------------------
# 2. Whole-Side Floor 1 Himalayan Pink Salt & Tangquan Sauna Liminal Biome
#    Occupies the ENTIRE West Side of the Ground Floor Hub:
#    X = -32.0m .. -112.0m (80.0m deep!), Z = -38.0m .. +38.0m (76.0m wide!)
# ------------------------------------------------------------------------------
func _build_himalayan_sauna_liminal_biome() -> void:
	var sauna_root := Node3D.new()
	sauna_root.name = "HimalayanSaunaLiminalBiome"
	add_child(sauna_root)

	var biome_h := 8.0

	# 1. Biome Floor (Seafoam/Ivory Liminal Poolrooms Tile lifted +6mm above Y = 0.0m so zero coplanar Z-fighting!) & Cedar Slat Ceiling
	_create_box(sauna_root, Vector3(-72.0, 0.014, 0.0), Vector3(79.6, 0.016, 75.6), _mat_pool_tile, false)
	_create_box(sauna_root, Vector3(-72.0, biome_h + 0.15, 0.0), Vector3(80.0, 0.30, 76.0), _mat_cedar)

	# 2. Outer Perimeter Walls of the 80m x 76m Sauna Biome (Glowing Himalayan Pink Salt Brick Walls!)
	#    - North Wall (Z = -38.0m, X = -112.0..-32.0m)
	#    - South Wall (Z = +38.0m, X = -112.0..-32.0m)
	#    - Far West Cathedral Wall (X = -112.0m, Z = -38.0..+38.0m)
	_create_box(sauna_root, Vector3(-72.0, biome_h * 0.5, -38.0), Vector3(80.0, biome_h, 0.60), _mat_pink_salt)
	_create_box(sauna_root, Vector3(-72.0, biome_h * 0.5, 38.0), Vector3(80.0, biome_h, 0.60), _mat_pink_salt)
	_create_box(sauna_root, Vector3(-112.0, biome_h * 0.5, 0.0), Vector3(0.60, biome_h, 76.0), _mat_pink_salt)

	# 3. Monumental Entrance Facade along the West Concourse of the Central Oval (X = -32.0m, Z = -38.0..+38.0m)
	#    Leaves THREE grand 10.0m-wide open portals at Z = -20.0m, Z = 0.0m, and Z = +20.0m so the entire West side of Floor 1 flows into the Sauna Biome!
	_create_box(sauna_root, Vector3(-32.0, biome_h * 0.5, -31.5), Vector3(0.55, biome_h, 13.0), _mat_pink_salt)
	_create_box(sauna_root, Vector3(-32.0, biome_h * 0.5, -10.0), Vector3(0.55, biome_h, 10.0), _mat_pink_salt)
	_create_box(sauna_root, Vector3(-32.0, biome_h * 0.5, 10.0), Vector3(0.55, biome_h, 10.0), _mat_pink_salt)
	_create_box(sauna_root, Vector3(-32.0, biome_h * 0.5, 31.5), Vector3(0.55, biome_h, 13.0), _mat_pink_salt)
	# Cedar transom headers across the 3 grand portals
	for pz in [-20.0, 0.0, 20.0]:
		_create_box(sauna_root, Vector3(-32.0, biome_h - 1.1, pz), Vector3(0.65, 2.2, 10.0), _mat_cedar)

	_build_sign_header(
		sauna_root,
		Vector3(-31.4, 5.1, 0.0),
		Vector3(0.0, 90.0, 0.0),
		"HIMALAYAN PINK SALT & TANGQUAN SAUNA BIOME [80m x 76m Liminal Sanctuary]"
	)

	# 4. Internal Liminal Archway Partition at X = -70.0m separating:
	#    - Zone A (East Half, X = -32m .. -70m): Liminal Tile Poolrooms & Steaming Geothermal Onsen Basins
	#    - Zone B (West Half, X = -70m .. -112m): Grand Himalayan Pink Salt Dry Sauna Cathedral & Cedar Amphitheater
	#    With 3 wide 8.0m open archways at Z = -22.0m, Z = 0.0m, and Z = +22.0m!
	_create_box(sauna_root, Vector3(-70.0, biome_h * 0.5, -32.0), Vector3(0.60, biome_h, 12.0), _mat_pink_salt)
	_create_box(sauna_root, Vector3(-70.0, biome_h * 0.5, -11.0), Vector3(0.60, biome_h, 14.0), _mat_pink_salt)
	_create_box(sauna_root, Vector3(-70.0, biome_h * 0.5, 11.0), Vector3(0.60, biome_h, 14.0), _mat_pink_salt)
	_create_box(sauna_root, Vector3(-70.0, biome_h * 0.5, 32.0), Vector3(0.60, biome_h, 12.0), _mat_pink_salt)

	# 5. Liminal Poolrooms Ceramic Tile Pillars in Zone A (X = -42m, -56m)
	for px in [-42.0, -56.0]:
		for pz in [-28.0, -10.0, 10.0, 28.0]:
			_create_box(sauna_root, Vector3(px, biome_h * 0.5, pz), Vector3(1.1, biome_h, 1.1), _mat_pool_tile)
			# Glowing Pink Salt sconce band around each pillar
			_create_box(sauna_root, Vector3(px, 2.2, pz), Vector3(1.18, 0.45, 1.18), _mat_pink_salt)

	# 6. Three Steaming Geothermal Onsen Pools in Zone A (with Walkable Stairs + Volumetric Steam Plumes!)
	var onsen_positions := [
		Vector3(-48.0, 0.0, 0.0),   # Primary Central Geothermal Jade/Salt Pool
		Vector3(-50.0, 0.0, -20.0), # North Liminal Onsen Pool
		Vector3(-50.0, 0.0, 20.0),  # South Liminal Onsen Pool
	]
	var onsen_model := "res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Bath_SteamingOnsenPool.fbx"
	for idx in range(onsen_positions.size()):
		var opos: Vector3 = onsen_positions[idx]
		_spawn_prop(sauna_root, onsen_model, opos, Vector3.ZERO, false)
		_build_walkable_onsen_pool_collision(sauna_root, opos)
		# Shimmering geothermal water plane inside the basin
		_create_box(sauna_root, opos + Vector3(0.0, 0.36, 0.0), Vector3(4.6, 0.04, 2.8), _mat_water, false)
		# Volumetric rising steam plume over the entire pool!
		_spawn_volumetric_steam_emitter(sauna_root, opos + Vector3(0.0, 0.45, 0.0), Vector3(2.4, 0.2, 1.6), 28, 3.8, Color(0.96, 0.90, 0.88, 0.36))

	# 7. Grand Himalayan Pink Salt Dry Sauna Cathedral in Zone B (X = -70m .. -112m):
	#    - 3-Tier Walkable Hinoki Cedar Sauna Amphitheater Benches along North (Z = -34m), South (Z = +34m), West (X = -108m),
	#      plus a Central Walkable 3-Tier Cedar Sauna Amphitheater Island (X = -81.5m .. -86.0m, Z = -7.0m .. +7.0m)!
	for tier_i in range(3):
		var step_h := float(tier_i + 1) * 0.28
		var depth_offset := float(2 - tier_i) * 1.15
		# West Cathedral Tiered Cedar Bench (X = -109.5m .. -106.0m, Z = -28.0m .. +28.0m)
		_create_box(sauna_root, Vector3(-110.2 + depth_offset, step_h * 0.5, 0.0), Vector3(1.15, step_h, 54.0), _mat_cedar)
		# North Cathedral Tiered Cedar Bench
		_create_box(sauna_root, Vector3(-90.0, step_h * 0.5, -36.2 + depth_offset), Vector3(32.0, step_h, 1.15), _mat_cedar)
		# South Cathedral Tiered Cedar Bench
		_create_box(sauna_root, Vector3(-90.0, step_h * 0.5, 36.2 - depth_offset), Vector3(32.0, step_h, 1.15), _mat_cedar)
		# Central Walkable 3-Tier Cedar Sauna Amphitheater Island (X = -82.0m .. -85.0m, Z = -6.5m .. +6.5m)
		_create_box(sauna_root, Vector3(-85.0 + depth_offset, step_h * 0.5, 0.0), Vector3(1.15, step_h, 13.0), _mat_cedar, false)

	# Smooth walkable ramp + top platform collider over the Central 3-Tier Cedar Sauna Amphitheater Island so players glide right up all 3 tiers
	var amph_sb := StaticBody3D.new()
	amph_sb.position = Vector3.ZERO
	sauna_root.add_child(amph_sb)
	_add_collider(amph_sb, Vector3(-85.0, 0.42, 0.0), Vector3(1.25, 0.84, 13.0))
	var amph_col := CollisionShape3D.new()
	var amph_box := BoxShape3D.new()
	amph_box.size = Vector3(4.6, 0.16, 13.0)
	amph_col.shape = amph_box
	amph_col.position = Vector3(-82.8, 0.36, 0.0)
	amph_col.rotation = Vector3(0.0, 0.0, deg_to_rad(-12.0))
	amph_sb.add_child(amph_col)

	# 3 Archway Steam Curtains at X = -70.0m (Z = -22m, 0m, +22m)
	for az in [-22.0, 0.0, 22.0]:
		_spawn_volumetric_steam_emitter(sauna_root, Vector3(-70.0, 0.35, az), Vector3(0.8, 0.2, 2.8), 20, 3.2, Color(0.98, 0.90, 0.86, 0.30))

	# 8. Six Red-Hot Sauna Stoves (`SM_Bath_SaunaStoveAndBuckets.fbx`) with Glowing Coals & Dense Rising Steam Columns!
	var stove_model := "res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Bath_SaunaStoveAndBuckets.fbx"
	var stove_positions := [
		Vector3(-78.0, 0.0, -14.0),
		Vector3(-78.0, 0.0, 14.0),
		Vector3(-92.0, 0.0, -18.0),
		Vector3(-92.0, 0.0, 0.0),
		Vector3(-92.0, 0.0, 18.0),
		Vector3(-102.0, 0.0, 0.0),
	]
	for spos in stove_positions:
		# Protective cedar stove guardrail curb + stove mesh + glowing coal light + rising steam!
		_create_box(sauna_root, spos + Vector3(0.0, 0.45, 0.0), Vector3(1.3, 0.90, 1.3), _mat_cedar)
		_spawn_prop(sauna_root, stove_model, spos + Vector3(0.0, 0.10, 0.0), Vector3.ZERO, false)
		_create_box(sauna_root, spos + Vector3(0.0, 0.94, 0.0), Vector3(0.85, 0.08, 0.85), _mat_pink_salt, false)
		_spawn_volumetric_steam_emitter(sauna_root, spos + Vector3(0.0, 1.02, 0.0), Vector3(0.65, 0.15, 0.65), 24, 4.5, Color(1.0, 0.92, 0.86, 0.42))

		var coal_light := OmniLight3D.new()
		coal_light.position = spos + Vector3(0.0, 1.35, 0.0)
		coal_light.light_color = Color(1.0, 0.58, 0.34)
		coal_light.light_energy = 1.45
		coal_light.omni_range = 14.0
		coal_light.distance_fade_enabled = true
		coal_light.distance_fade_begin = 45.0
		coal_light.distance_fade_length = 15.0
		sauna_root.add_child(coal_light)

	# 9. Four Hinoki Cedar Glass Sauna Pavilions (`SM_Bath_GlassSaunaPavilion.fbx`) & Open Rinse Showers (`SM_Bath_OpenRinseShowerRow.fbx`)
	var pav_model := "res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Bath_GlassSaunaPavilion.fbx"
	var shower_model := "res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Bath_OpenRinseShowerRow.fbx"
	for pz in [-28.0, 28.0]:
		_spawn_prop(sauna_root, pav_model, Vector3(-62.0, 0.0, pz), Vector3(4.8, 2.9, 3.8), true)
		_spawn_volumetric_steam_emitter(sauna_root, Vector3(-62.0, 0.5, pz), Vector3(1.6, 0.2, 1.4), 18, 2.4, Color(0.98, 0.92, 0.86, 0.32))
		_spawn_prop(sauna_root, shower_model, Vector3(-38.0, 0.0, pz), Vector3(3.2, 2.2, 1.1), true)
		_spawn_volumetric_steam_emitter(sauna_root, Vector3(-38.0, 1.8, pz), Vector3(1.4, 0.2, 0.6), 16, 2.0, Color(0.92, 0.96, 1.0, 0.30))

	# 10. Warm Himalayan Amber Ceiling Lighting Across the Biome
	for lx in [-40.0, -58.0, -80.0, -100.0]:
		for lz in [-24.0, 0.0, 24.0]:
			var s_light := OmniLight3D.new()
			s_light.position = Vector3(lx, 6.8, lz)
			s_light.light_color = Color(1.0, 0.76, 0.58)
			s_light.light_energy = 1.35
			s_light.omni_range = 20.0
			s_light.distance_fade_enabled = true
			s_light.distance_fade_begin = 55.0
			s_light.distance_fade_length = 18.0
			sauna_root.add_child(s_light)

	var shrine_lbl := Label3D.new()
	shrine_lbl.position = Vector3(-48.0, 2.6, 0.0)
	shrine_lbl.text = (
		"[F] Himalayan Pink Salt & Tangquan Sauna Sanctuary Biome\n" +
		"• Walk Up Small Platform Stairs into Steaming Onsen Pools or Sit on Cedar Sauna Benches!\n" +
		"• Alive [F]: Soak (+60s Pink Salt Ward) & Light Soul-Anchor | Ectoplasm [F/9]: Resurrect!"
	)
	shrine_lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	shrine_lbl.font_size = 21
	shrine_lbl.outline_size = 6
	shrine_lbl.modulate = Color(1.0, 0.86, 0.82)
	shrine_lbl.visibility_range_end = 28.0
	shrine_lbl.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
	sauna_root.add_child(shrine_lbl)

func _build_walkable_onsen_pool_collision(parent: Node3D, spa_pos: Vector3) -> void:
	var sb := StaticBody3D.new()
	sb.position = spa_pos
	parent.add_child(sb)

	_add_collider(sb, Vector3(0.0, 0.14, 0.0), Vector3(4.7, 0.28, 2.9))
	_add_collider(sb, Vector3(-1.88, 0.28, 0.0), Vector3(1.84, 0.56, 3.8))
	_add_collider(sb, Vector3(1.88, 0.28, 0.0), Vector3(1.84, 0.56, 3.8))

	for z_sign in [-1.0, 1.0]:
		_add_collider(sb, Vector3(0.0, 0.07, z_sign * 2.28), Vector3(1.9, 0.14, 0.34))
		_add_collider(sb, Vector3(0.0, 0.15, z_sign * 2.02), Vector3(1.9, 0.30, 0.34))
		_add_collider(sb, Vector3(0.0, 0.21, z_sign * 1.72), Vector3(1.9, 0.42, 0.44))
		var ramp_col := CollisionShape3D.new()
		var ramp_box := BoxShape3D.new()
		ramp_box.size = Vector3(1.9, 0.16, 1.15)
		ramp_col.shape = ramp_box
		ramp_col.position = Vector3(0.0, 0.18, z_sign * 2.08)
		ramp_col.rotation = Vector3(z_sign * deg_to_rad(23.0), 0.0, 0.0)
		sb.add_child(ramp_col)

func _create_procedural_steam_puff_texture() -> Texture2D:
	var img := Image.create(64, 64, false, Image.FORMAT_RGBA8)
	for y in range(64):
		for x in range(64):
			var dx := (float(x) - 31.5) / 31.5
			var dy := (float(y) - 31.5) / 31.5
			var r2 := dx * dx + dy * dy
			var alpha := clampf(pow(maxf(0.0, 1.0 - r2), 2.2) * 0.55, 0.0, 1.0)
			img.set_pixel(x, y, Color(1.0, 0.97, 0.95, alpha))
	return ImageTexture.create_from_image(img)

func _spawn_volumetric_steam_emitter(
	parent: Node3D,
	pos: Vector3,
	box_extents: Vector3,
	amount_count: int,
	rise_height: float,
	tint: Color
) -> GPUParticles3D:
	var particles := GPUParticles3D.new()
	particles.name = "SaunaSteamEmitter"
	particles.position = pos
	particles.emitting = true
	particles.amount = amount_count
	particles.lifetime = 4.2
	particles.preprocess = 2.0
	particles.visibility_aabb = AABB(Vector3(-8.0, -1.0, -8.0), Vector3(16.0, 10.0, 16.0))
	particles.visibility_range_end = 65.0
	particles.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
	particles.add_to_group("sauna_steam_emitters")

	var pmat := ParticleProcessMaterial.new()
	pmat.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_BOX
	pmat.emission_box_extents = box_extents
	pmat.direction = Vector3(0.0, 1.0, 0.0)
	pmat.spread = 22.0
	pmat.initial_velocity_min = rise_height * 0.22
	pmat.initial_velocity_max = rise_height * 0.36
	pmat.gravity = Vector3(0.0, 0.12, 0.0)
	pmat.scale_min = 1.2
	pmat.scale_max = 2.4
	pmat.color = tint
	particles.process_material = pmat

	var quad := QuadMesh.new()
	quad.size = Vector2(2.2, 2.2)
	var smat := StandardMaterial3D.new()
	smat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	smat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	smat.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	smat.billboard_keep_scale = true
	smat.vertex_color_use_as_albedo = true
	smat.albedo_color = tint
	smat.albedo_texture = _steam_puff_tex
	smat.proximity_fade_enabled = true
	smat.proximity_fade_distance = 0.65
	quad.material = smat
	particles.draw_pass_1 = quad

	parent.add_child(particles)
	return particles

# ------------------------------------------------------------------------------
# 3. ~800m-Long Smoothly Curving Parallel Hallway & Stacked-"H" Wing Builder
#    Builds 24 Wide (32.0m) Store Bays + 60m x 26m End-Cap Anchor Store in
#    100% Watertight Batched SurfaceTool Meshes!
# ------------------------------------------------------------------------------
func _build_curved_wing(is_north: bool) -> void:
	var wing_root := Node3D.new()
	wing_root.name = "NorthStackedHWing" if is_north else "SouthStackedHWing"
	add_child(wing_root)

	var c_arr := _north_stations_c if is_north else _south_stations_c
	var r_arr := _north_stations_r if is_north else _south_stations_r
	var t_arr := _north_stations_t if is_north else _south_stations_t

	# We build the Stacked-H Wings on Floors 1 and 2 so Floor 2 follows the exact layout of Floor 1
	# and the 2-Level End-Cap Anchor Store connects Floor 1 (Y = 0.0m) and Floor 2 (Y = 8.50m)!
	for floor_idx in range(2):
		var floor_num := floor_idx + 1
		var fy := float(floor_idx) * 8.50
		var wall_h := 8.00

		var fl_root := Node3D.new()
		fl_root.name = "%s_Floor%d" % ["NorthWing" if is_north else "SouthWing", floor_num]
		wing_root.add_child(fl_root)

		var wing_sb := StaticBody3D.new()
		wing_sb.name = "WingBatchedArchitecture_F%d" % floor_num
		fl_root.add_child(wing_sb)

		var st_floor := SurfaceTool.new()
		st_floor.begin(Mesh.PRIMITIVE_TRIANGLES)
		var st_terrazzo := SurfaceTool.new()
		st_terrazzo.begin(Mesh.PRIMITIVE_TRIANGLES)
		var st_plaster := SurfaceTool.new()
		st_plaster.begin(Mesh.PRIMITIVE_TRIANGLES)
		var st_glass := SurfaceTool.new()
		st_glass.begin(Mesh.PRIMITIVE_TRIANGLES)
		var st_ceiling := SurfaceTool.new()
		st_ceiling.begin(Mesh.PRIMITIVE_TRIANGLES)

		for k in range(NUM_BAYS):
			var c0 := c_arr[k] + Vector3(0.0, fy, 0.0)
			var c1 := c_arr[k + 1] + Vector3(0.0, fy, 0.0)
			var r0 := r_arr[k]
			var r1 := r_arr[k + 1]

			# 1. Full-Width Walkable Floor Slab (local X = -46..+46m) & Acoustic Ceiling Soffit (y = wall_h)
			var f_w0 := c0 + r0 * WEST_OUTER_WALL_X
			var f_e0 := c0 + r0 * EAST_OUTER_WALL_X
			var f_e1 := c1 + r1 * EAST_OUTER_WALL_X
			var f_w1 := c1 + r1 * WEST_OUTER_WALL_X
			_add_quad(st_floor, f_w0, f_e0, f_e1, f_w1, Vector3.UP)
			_add_quad(
				st_ceiling,
				f_w0 + Vector3(0, wall_h, 0),
				f_w1 + Vector3(0, wall_h, 0),
				f_e1 + Vector3(0, wall_h, 0),
				f_e0 + Vector3(0, wall_h, 0),
				Vector3.DOWN
			)

			# 2. West & East Parallel Hallway Terrazzo Runner Carpets + Stacked-"H" Crossbar Runner through Center Store!
			_add_quad(
				st_terrazzo,
				c0 + r0 * -18.2 + Vector3(0, 0.012, 0),
				c0 + r0 * -12.8 + Vector3(0, 0.012, 0),
				c1 + r1 * -12.8 + Vector3(0, 0.012, 0),
				c1 + r1 * -18.2 + Vector3(0, 0.012, 0),
				Vector3.UP
			)
			_add_quad(
				st_terrazzo,
				c0 + r0 * 12.8 + Vector3(0, 0.012, 0),
				c0 + r0 * 18.2 + Vector3(0, 0.012, 0),
				c1 + r1 * 18.2 + Vector3(0, 0.012, 0),
				c1 + r1 * 12.8 + Vector3(0, 0.012, 0),
				Vector3.UP
			)
			# Horizontal Crossbar of the Capital "H" (from West Hallway -12.8m straight through the 22m-deep Center Store to East Hallway +12.8m!)
			var h_c0 := c0.lerp(c1, 0.42) + Vector3(0, 0.012, 0)
			var h_c1 := c0.lerp(c1, 0.58) + Vector3(0, 0.012, 0)
			var h_r0 := r0.lerp(r1, 0.42)
			var h_r1 := r0.lerp(r1, 0.58)
			_add_quad(
				st_terrazzo,
				h_c0 + h_r0 * -12.8,
				h_c0 + h_r0 * 12.8,
				h_c1 + h_r1 * 12.8,
				h_c1 + h_r1 * -12.8,
				Vector3.UP
			)

			# 3. Solid Outer Perimeter Walls at local X = -46.0m and +46.0m
			_add_thick_wall_segment(st_plaster, c0 + r0 * WEST_OUTER_WALL_X, c1 + r1 * WEST_OUTER_WALL_X, wall_h, 0.45)
			_add_thick_wall_segment(st_plaster, c0 + r0 * EAST_OUTER_WALL_X, c1 + r1 * EAST_OUTER_WALL_X, wall_h, 0.45)

			# 4. Four 32.0m-Wide Storefront Facade Walls (each with a 5.8m-wide Open Entrance Portal + Dual 9.9m Glazed Display Windows!):
			#    - West Outer Storefront Wall at local X = -20.0m
			#    - Center Pass-Through Store West Wall at local X = -11.0m
			#    - Center Pass-Through Store East Wall at local X = +11.0m
			#    - East Outer Storefront Wall at local X = +20.0m
			for wf_x in [WEST_HALL_OUTER_X, WEST_HALL_INNER_X, EAST_HALL_INNER_X, EAST_HALL_OUTER_X]:
				var w_p0: Vector3 = c0 + r0 * float(wf_x)
				var w_p1: Vector3 = c1 + r1 * float(wf_x)
				_add_storefront_facade_32m(st_plaster, st_glass, w_p0, w_p1, wall_h)

			# 5. Demising Partition Walls with 3.4m-Wide Neighbor Doorways at Joint k
			#    - West Outer Store Partition: local X = -46.0m .. -20.0m (door at -33.0m)
			#    - Center Pass-Through Store Partition: local X = -11.0m .. +11.0m (door at 0.0m)
			#    - East Outer Store Partition: local X = +20.0m .. +46.0m (door at +33.0m)
			_add_partition_wall_with_door(st_plaster, c0 + r0 * WEST_OUTER_WALL_X, c0 + r0 * WEST_HALL_OUTER_X, wall_h, 3.4, 3.8)
			_add_partition_wall_with_door(st_plaster, c0 + r0 * WEST_HALL_INNER_X, c0 + r0 * EAST_HALL_INNER_X, wall_h, 3.4, 3.8)
			_add_partition_wall_with_door(st_plaster, c0 + r0 * EAST_HALL_OUTER_X, c0 + r0 * EAST_OUTER_WALL_X, wall_h, 3.4, 3.8)

		# 6. Terminal Joint k = NUM_BAYS:
		#    Seal outer store shoulders (-46..-30m and +30..+46m) and build the Massive 60.0m Wide x 32.0m Deep
		#    2-Level Flagship End-Cap Anchor Department Store (Rotunda, Wrap-Around Upper Gallery,
		#    Mezzanine Overlook, & Solid Box-Collision Grand Staircase connecting Floor 1 -> Floor 2!)
		var c_end := c_arr[NUM_BAYS] + Vector3(0.0, fy, 0.0)
		var r_end := r_arr[NUM_BAYS]
		var t_end := t_arr[NUM_BAYS]
		var anchor_half_w := ANCHOR_WIDTH * 0.5 # 30.0m
		var anchor_wall_h := 8.50

		_add_thick_wall_segment(st_plaster, c_end + r_end * WEST_OUTER_WALL_X, c_end + r_end * -anchor_half_w, wall_h, 0.45)
		_add_thick_wall_segment(st_plaster, c_end + r_end * anchor_half_w, c_end + r_end * EAST_OUTER_WALL_X, wall_h, 0.45)

		_add_endcap_anchor_to_batches(
			st_floor, st_terrazzo, st_plaster, st_glass, st_ceiling,
			c_end, r_end, t_end, anchor_half_w, ANCHOR_DEPTH, anchor_wall_h, floor_num
		)

		# Build solid convex BoxShape3D floor slabs + Grand Rotunda Staircase & Mezzanine on Floor 1!
		_build_endcap_solid_box_colliders_and_staircase(
			fl_root, c_end, r_end.normalized(), t_end.normalized(), anchor_half_w, ANCHOR_DEPTH, floor_num
		)

		# Commit Batched SurfaceTool Meshes + Exact Trimesh Collision Shapes!
		var mi_floor := MeshInstance3D.new()
		mi_floor.mesh = st_floor.commit()
		mi_floor.material_override = _mat_floor
		wing_sb.add_child(mi_floor)
		var col_floor := CollisionShape3D.new()
		col_floor.shape = mi_floor.mesh.create_trimesh_shape()
		wing_sb.add_child(col_floor)

		var mi_terrazzo := MeshInstance3D.new()
		mi_terrazzo.mesh = st_terrazzo.commit()
		mi_terrazzo.material_override = _mat_terrazzo
		fl_root.add_child(mi_terrazzo)

		var mi_plaster := MeshInstance3D.new()
		mi_plaster.mesh = st_plaster.commit()
		mi_plaster.material_override = _mat_plaster
		wing_sb.add_child(mi_plaster)
		var col_plaster := CollisionShape3D.new()
		col_plaster.shape = mi_plaster.mesh.create_trimesh_shape()
		wing_sb.add_child(col_plaster)

		var mi_glass := MeshInstance3D.new()
		mi_glass.mesh = st_glass.commit()
		mi_glass.material_override = _mat_glass
		wing_sb.add_child(mi_glass)
		var col_glass := CollisionShape3D.new()
		col_glass.shape = mi_glass.mesh.create_trimesh_shape()
		wing_sb.add_child(col_glass)

		var mi_ceiling := MeshInstance3D.new()
		mi_ceiling.mesh = st_ceiling.commit()
		mi_ceiling.material_override = _mat_ceiling
		fl_root.add_child(mi_ceiling)

		# 7. Populate Distance-Culled Concourse Lighting, Storefront Signs & Furnishings Along the Curved Wing!
		_populate_wing_lights_signs_and_props(fl_root, is_north, floor_num, fy, wall_h)

func _add_endcap_anchor_to_batches(
	st_floor: SurfaceTool,
	st_terrazzo: SurfaceTool,
	st_plaster: SurfaceTool,
	st_glass: SurfaceTool,
	st_ceiling: SurfaceTool,
	c_front: Vector3,
	r_dir: Vector3,
	t_dir: Vector3,
	half_w: float,
	depth_m: float,
	wall_h: float,
	floor_num: int = 1
) -> void:
	var r_unit := r_dir.normalized()
	var t_unit := t_dir.normalized()
	var c_back := c_front + t_unit * depth_m
	var fl_w0 := c_front - r_unit * half_w
	var fl_e0 := c_front + r_unit * half_w
	var fl_e1 := c_back + r_unit * half_w
	var fl_w1 := c_back - r_unit * half_w

	# Central 2-Story Rotunda Light-Well Cutout Bounds (24.0m Wide x 14.0m Deep: X = -12.0..+12.0m, t = 8.5..22.5m)
	# Reference: Galeries Lafayette Haussmann / Nordstrom Flagships / Macy's Central Rotunda Atrium!
	var void_x := 12.0
	var void_t0 := 8.5
	var void_t1 := 22.5
	var soffit_h := wall_h - 0.38 if floor_num == 1 else wall_h

	# 1. Floor Medallions, Void Fascia & Safety Balustrades
	#    (Walkable Floor Slabs & Visual Floor Meshes for the End-Cap Store are built in
	#    _build_endcap_solid_box_colliders_and_staircase with 100% pure BoxShape3D colliders!)
	if floor_num == 1:
		# Central Ground-Floor Rotunda Medallion Inlay (Cosmetics & Fragrance Hall)
		var med_c0 := c_front + t_unit * 10.5 + Vector3(0, 0.012, 0)
		var med_c1 := c_front + t_unit * 17.5 + Vector3(0, 0.012, 0)
		_add_quad(
			st_terrazzo,
			med_c0 - r_unit * 5.8,
			med_c0 + r_unit * 5.8,
			med_c1 + r_unit * 5.8,
			med_c1 - r_unit * 5.8,
			Vector3.UP
		)
	else:
		var v_w0 := c_front + t_unit * void_t0 - r_unit * void_x
		var v_w1 := c_front + t_unit * void_t1 - r_unit * void_x
		var v_e0 := c_front + t_unit * void_t0 + r_unit * void_x
		var v_e1 := c_front + t_unit * void_t1 + r_unit * void_x

		# Plaster Slab Edge Fascia around the 24m x 14m Rotunda Void (-0.38m .. 0.0m),
		# leaving the [+6.4..+10.2m] East Flight B stair arrival completely clear!
		_add_thick_wall_band(st_plaster, v_w0, c_front + t_unit * void_t0 + r_unit * 6.4, -0.38, 0.0, 0.24)
		_add_thick_wall_band(st_plaster, c_front + t_unit * void_t0 + r_unit * 10.2, v_e0, -0.38, 0.0, 0.24)
		_add_thick_wall_band(st_plaster, v_w1, v_e1, -0.38, 0.0, 0.24)
		_add_thick_wall_band(st_plaster, v_w0, v_w1, -0.38, 0.0, 0.24)
		_add_thick_wall_band(st_plaster, v_e0, v_e1, -0.38, 0.0, 0.24)

		# Tempered Glass & Plaster Safety Balustrades (1.15m high) around the Rotunda Void
		# (On Floor 2, the Front Rim has a single 3.8m opening at [+6.4..+10.2m] where East Flight B arrives from Floor 1!)
		_add_rotunda_balustrade_span(st_plaster, st_glass, v_w0, v_w1) # West Void Rim
		_add_rotunda_balustrade_span(st_plaster, st_glass, v_e0, v_e1) # East Void Rim
		_add_rotunda_balustrade_span(st_plaster, st_glass, v_w1, v_e1) # Rear Void Rim
		_add_rotunda_balustrade_span(st_plaster, st_glass, c_front + t_unit * void_t0 - r_unit * void_x, c_front + t_unit * void_t0 + r_unit * 6.4)
		_add_rotunda_balustrade_span(st_plaster, st_glass, c_front + t_unit * void_t0 + r_unit * 10.2, c_front + t_unit * void_t0 + r_unit * void_x)

	# 2. Ceiling Soffits (Open over the 24m x 14m Rotunda Light-Well on Floor 1; Crowned by a Vaulted Skylight Cupola on Floor 2!)
	var ceil_y := Vector3(0.0, soffit_h, 0.0)
	var c_vt0_w := c_front + t_unit * void_t0 - r_unit * half_w + ceil_y
	var c_vt0_e := c_front + t_unit * void_t0 + r_unit * half_w + ceil_y
	var c_vt1_w := c_front + t_unit * void_t1 - r_unit * half_w + ceil_y
	var c_vt1_e := c_front + t_unit * void_t1 + r_unit * half_w + ceil_y
	var cv_w0 := c_front + t_unit * void_t0 - r_unit * void_x + ceil_y
	var cv_w1 := c_front + t_unit * void_t1 - r_unit * void_x + ceil_y
	var cv_e0 := c_front + t_unit * void_t0 + r_unit * void_x + ceil_y
	var cv_e1 := c_front + t_unit * void_t1 + r_unit * void_x + ceil_y

	_add_quad(st_ceiling, fl_w0 + ceil_y, c_vt0_w, c_vt0_e, fl_e0 + ceil_y, Vector3.DOWN)
	_add_quad(st_ceiling, c_vt1_w, fl_w1 + ceil_y, fl_e1 + ceil_y, c_vt1_e, Vector3.DOWN)
	_add_quad(st_ceiling, c_vt0_w, c_vt1_w, cv_w1, cv_w0, Vector3.DOWN)
	_add_quad(st_ceiling, cv_e0, cv_e1, c_vt1_e, c_vt0_e, Vector3.DOWN)

	if floor_num == 2:
		# Vaulted Galeries-Lafayette Skylight Cupola crowning the 2-Story Central Rotunda (y = wall_h .. wall_h + 2.2m)
		var cup_top := wall_h + 2.20
		var v0_b := c_front + t_unit * void_t0 - r_unit * void_x
		var v1_b := c_front + t_unit * void_t0 + r_unit * void_x
		var v2_b := c_front + t_unit * void_t1 + r_unit * void_x
		var v3_b := c_front + t_unit * void_t1 - r_unit * void_x
		_add_thick_wall_band(st_glass, v0_b, v1_b, wall_h, cup_top, 0.22)
		_add_thick_wall_band(st_glass, v1_b, v2_b, wall_h, cup_top, 0.22)
		_add_thick_wall_band(st_glass, v2_b, v3_b, wall_h, cup_top, 0.22)
		_add_thick_wall_band(st_glass, v3_b, v0_b, wall_h, cup_top, 0.22)
		_add_quad(
			st_glass,
			v0_b + Vector3(0, cup_top, 0),
			v3_b + Vector3(0, cup_top, 0),
			v2_b + Vector3(0, cup_top, 0),
			v1_b + Vector3(0, cup_top, 0),
			Vector3.DOWN
		)

	# 3. Non-Overlapping Terrazzo Cross-Promenade & Wing Runners on Every Level
	var prom_c0 := c_front + t_unit * 2.5 + Vector3(0, 0.012, 0)
	var prom_c1 := c_front + t_unit * 7.5 + Vector3(0, 0.012, 0)
	_add_quad(st_terrazzo, prom_c0 - r_unit * 26.0, prom_c0 + r_unit * 26.0, prom_c1 + r_unit * 26.0, prom_c1 - r_unit * 26.0, Vector3.UP)
	for hx in [WEST_HALL_CENTER_X, EAST_HALL_CENTER_X]:
		_add_quad(
			st_terrazzo,
			c_front + r_unit * (hx - 2.2) + Vector3(0, 0.012, 0),
			c_front + r_unit * (hx + 2.2) + Vector3(0, 0.012, 0),
			prom_c0 + r_unit * (hx + 2.2),
			prom_c0 + r_unit * (hx - 2.2),
			Vector3.UP
		)
	for side_x in [-21.0, 21.0]:
		_add_quad(
			st_terrazzo,
			prom_c1 + r_unit * (side_x - 2.0),
			prom_c1 + r_unit * (side_x + 2.0),
			c_back - t_unit * 3.0 + r_unit * (side_x + 2.0) + Vector3(0, 0.012, 0),
			c_back - t_unit * 3.0 + r_unit * (side_x - 2.0) + Vector3(0, 0.012, 0),
			Vector3.UP
		)

	# 4. Anchor Back Wall & Side Walls (Full 8.50m Floor-to-Floor Height for Seamless Multi-Story Enclosure)
	_add_thick_wall_segment(st_plaster, fl_w1, fl_e1, wall_h, 0.50)
	_add_thick_wall_segment(st_plaster, fl_w0, fl_w1, wall_h, 0.50)
	_add_thick_wall_segment(st_plaster, fl_e0, fl_e1, wall_h, 0.50)

	# 5. Anchor Front Wall at c_front with TWO 5.8m-Wide Portals Aligned with West (-15.5m) & East (+15.5m) Parallel Hallways!
	var door_hw := 2.90
	var x0 := -half_w
	var x1 := WEST_HALL_CENTER_X - door_hw
	var x2 := WEST_HALL_CENTER_X + door_hw
	var x3 := EAST_HALL_CENTER_X - door_hw
	var x4 := EAST_HALL_CENTER_X + door_hw
	var x5 := half_w

	_add_storefront_window_span(st_plaster, st_glass, c_front + r_unit * x0, c_front + r_unit * x1, wall_h)
	_add_portal_transom_span(st_plaster, c_front + r_unit * x1, c_front + r_unit * x2, 4.4, wall_h, 0.45)
	_add_storefront_window_span(st_plaster, st_glass, c_front + r_unit * x2, c_front + r_unit * x3, wall_h)
	_add_portal_transom_span(st_plaster, c_front + r_unit * x3, c_front + r_unit * x4, 4.4, wall_h, 0.45)
	_add_storefront_window_span(st_plaster, st_glass, c_front + r_unit * x4, c_front + r_unit * x5, wall_h)

func _add_rotunda_balustrade_span(st_plaster: SurfaceTool, st_glass: SurfaceTool, p0: Vector3, p1: Vector3) -> void:
	if p0.distance_to(p1) < 0.15:
		return
	_add_thick_wall_band(st_plaster, p0, p1, 0.0, 0.18, 0.22)
	_add_thick_wall_band(st_glass, p0, p1, 0.18, 1.05, 0.12)
	_add_thick_wall_band(st_plaster, p0, p1, 1.05, 1.16, 0.20)

## Builds 100% Solid Convex BoxShape3D Colliders for the End-Cap Store Floor Slabs,
## Intermediate Mezzanine Overlook Balcony (Y = 4.25m), and Twin-Flight Grand Staircase
## connecting Floor 1 (Y = 0.00m) -> Mezzanine (Y = 4.25m) -> Floor 2 (Y = 8.50m) with ZERO Trimesh Bounce!
func _build_endcap_solid_box_colliders_and_staircase(
	fl_root: Node3D,
	c_front: Vector3,
	r_unit: Vector3,
	t_unit: Vector3,
	half_w: float,
	depth_m: float,
	floor_num: int
) -> void:
	# Construct a right-handed orthonormal basis (r_unit, UP, bz_horiz) for the End-Cap Store
	var bx_horiz := r_unit.normalized()
	var bz_horiz := bx_horiz.cross(Vector3.UP).normalized()
	var store_basis := Basis(bx_horiz, Vector3.UP, bz_horiz)

	var sb := StaticBody3D.new()
	sb.name = "EndCapSolidBoxPhysics_F%d" % floor_num
	fl_root.add_child(sb)

	var slab_thick := 0.40
	var void_x := 12.0
	var void_t0 := 8.5
	var void_t1 := 22.5
	var c_back := c_front + t_unit * depth_m
	var fl_w0 := c_front - r_unit * half_w
	var fl_e0 := c_front + r_unit * half_w
	var fl_e1 := c_back + r_unit * half_w
	var fl_w1 := c_back - r_unit * half_w

	var st_endcap_floor := SurfaceTool.new()
	st_endcap_floor.begin(Mesh.PRIMITIVE_TRIANGLES)

	if floor_num == 1:
		_add_quad(st_endcap_floor, fl_w0, fl_e0, fl_e1, fl_w1, Vector3.UP)
		# 1. Solid BoxShape3D Ground Floor Slab for the entire 60m x 32m End-Cap Store (top surface at Y = c_front.y)
		_add_oriented_box_collider(
			sb,
			c_front + t_unit * (depth_m * 0.5) - Vector3(0.0, slab_thick * 0.5, 0.0),
			store_basis,
			Vector3(half_w * 2.0, slab_thick, depth_m)
		)

		# 2. Solid BoxShape3D Intermediate Mezzanine Overlook Balcony (top surface at y = +4.25m, X = -10.8..+10.8m, t = 18.5..22.5m)
		var y_mez := 4.25
		var mez_t0 := 18.5
		var mez_t1 := 22.5
		var mez_x := 10.8
		var mez_d := mez_t1 - mez_t0
		var mez_center_top := c_front + t_unit * ((mez_t0 + mez_t1) * 0.5) + Vector3(0.0, y_mez, 0.0)
		_add_oriented_box_collider(
			sb,
			mez_center_top - Vector3(0.0, slab_thick * 0.5, 0.0),
			store_basis,
			Vector3(mez_x * 2.0, slab_thick, mez_d)
		)

		# Solid BoxShape3D Safety Balustrades around the Mezzanine (leaving 3.8m openings at [-10.2..-6.4] and [+6.4..+10.2])
		var rail_h := 1.20
		# Rear Mezzanine Guardrail (t = 22.4m, X = -10.8..+10.8m)
		_add_oriented_box_collider(
			sb,
			c_front + t_unit * (mez_t1 - 0.12) + Vector3(0.0, y_mez + rail_h * 0.5, 0.0),
			store_basis,
			Vector3(mez_x * 2.0, rail_h, 0.24)
		)
		# West & East Mezzanine Side Guardrails (X = -10.68m and +10.68m, t = 18.5..22.5m)
		for side_s in [-1.0, 1.0]:
			_add_oriented_box_collider(
				sb,
				mez_center_top + r_unit * (side_s * (mez_x - 0.12)) + Vector3(0.0, rail_h * 0.5, 0.0),
				store_basis,
				Vector3(0.24, rail_h, mez_d)
			)
		# Center Front Mezzanine Overlook Guardrail (t = 18.62m, X = -6.4..+6.4m)
		_add_oriented_box_collider(
			sb,
			c_front + t_unit * (mez_t0 + 0.12) + Vector3(0.0, y_mez + rail_h * 0.5, 0.0),
			store_basis,
			Vector3(12.8, rail_h, 0.24)
		)

		# 3. Solid BoxShape3D Ramp Colliders & Side Guardrails for West Flight A (0.0 -> 4.25m) and East Flight B (4.25 -> 8.50m)
		_add_stair_flight_box_colliders(
			sb,
			c_front + t_unit * 8.5,
			c_front + t_unit * mez_t0 + Vector3(0.0, y_mez, 0.0),
			r_unit,
			-10.2,
			-6.4
		)
		_add_stair_flight_box_colliders(
			sb,
			c_front + t_unit * mez_t0 + Vector3(0.0, y_mez, 0.0),
			c_front + t_unit * 8.5 + Vector3(0.0, 8.50, 0.0),
			r_unit,
			6.4,
			10.2
		)

		# 4. Visual-Only (NO Trimesh Collision!) Meshes for the Mezzanine & Grand Rotunda Staircase Treads, Risers & Balustrades
		_build_endcap_staircase_and_mezzanine_visual_meshes(fl_root, c_front, r_unit, t_unit, mez_t0, mez_t1, mez_x, y_mez)
	else:
		var p_vt0_w := c_front + t_unit * void_t0 - r_unit * half_w
		var p_vt0_e := c_front + t_unit * void_t0 + r_unit * half_w
		var p_vt1_w := c_front + t_unit * void_t1 - r_unit * half_w
		var p_vt1_e := c_front + t_unit * void_t1 + r_unit * half_w
		var v_w0 := c_front + t_unit * void_t0 - r_unit * void_x
		var v_w1 := c_front + t_unit * void_t1 - r_unit * void_x
		var v_e0 := c_front + t_unit * void_t0 + r_unit * void_x
		var v_e1 := c_front + t_unit * void_t1 + r_unit * void_x
		_add_quad(st_endcap_floor, fl_w0, fl_e0, p_vt0_e, p_vt0_w, Vector3.UP)
		_add_quad(st_endcap_floor, p_vt1_w, p_vt1_e, fl_e1, fl_w1, Vector3.UP)
		_add_quad(st_endcap_floor, p_vt0_w, v_w0, v_w1, p_vt1_w, Vector3.UP)
		_add_quad(st_endcap_floor, v_e0, p_vt0_e, p_vt1_e, v_e1, Vector3.UP)

		# Floor 2 (Y = 8.50m): 4 Solid BoxShape3D Wrap-Around Gallery Slabs around the 24m x 14m Rotunda Light-Well!
		# A. Front Concourse Promenade Slab (t = 0.0 .. 8.5m, X = -30..+30m)
		_add_oriented_box_collider(
			sb,
			c_front + t_unit * (void_t0 * 0.5) - Vector3(0.0, slab_thick * 0.5, 0.0),
			store_basis,
			Vector3(half_w * 2.0, slab_thick, void_t0)
		)
		# B. Rear Grand Gallery Slab (t = 22.5 .. 32.0m, X = -30..+30m)
		var rear_d := depth_m - void_t1
		_add_oriented_box_collider(
			sb,
			c_front + t_unit * (void_t1 + rear_d * 0.5) - Vector3(0.0, slab_thick * 0.5, 0.0),
			store_basis,
			Vector3(half_w * 2.0, slab_thick, rear_d)
		)
		# C. West & East Wing Gallery Slabs (t = 8.5 .. 22.5m, |X| = 12.0 .. 30.0m)
		var side_w := half_w - void_x
		var void_d := void_t1 - void_t0
		for side_s in [-1.0, 1.0]:
			_add_oriented_box_collider(
				sb,
				c_front + t_unit * ((void_t0 + void_t1) * 0.5) + r_unit * (side_s * (void_x + side_w * 0.5)) - Vector3(0.0, slab_thick * 0.5, 0.0),
				store_basis,
				Vector3(side_w, slab_thick, void_d)
			)

	var mi_endcap_floor := MeshInstance3D.new()
	mi_endcap_floor.name = "EndCapFloorVisual_F%d" % floor_num
	mi_endcap_floor.mesh = st_endcap_floor.commit()
	mi_endcap_floor.material_override = _mat_floor
	fl_root.add_child(mi_endcap_floor)

func _add_oriented_box_collider(sb: StaticBody3D, center_pos: Vector3, box_basis: Basis, box_size: Vector3) -> void:
	var col := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = box_size
	col.shape = shape
	col.transform = Transform3D(box_basis.orthonormalized(), center_pos)
	sb.add_child(col)

func _add_stair_flight_box_colliders(
	sb: StaticBody3D,
	p_start: Vector3,
	p_end: Vector3,
	r_unit: Vector3,
	x_left: float,
	x_right: float
) -> void:
	var stair_w := absf(x_right - x_left)
	var x_mid := (x_left + x_right) * 0.5
	var ramp_vec := p_end - p_start
	var ramp_len := ramp_vec.length()
	var bz := ramp_vec / maxf(ramp_len, 0.001)
	var bx := (r_unit - bz * r_unit.dot(bz)).normalized()
	var by := bz.cross(bx).normalized()
	if by.y < 0.0:
		by = -by
		bx = -bx
	var ramp_basis := Basis(bx, by, bz).orthonormalized()

	var ramp_thick := 0.40
	var mid_top := (p_start + p_end) * 0.5 + r_unit * x_mid
	_add_oriented_box_collider(
		sb,
		mid_top - by * (ramp_thick * 0.5),
		ramp_basis,
		Vector3(stair_w, ramp_thick, ramp_len)
	)

	# Solid BoxShape3D Side Guardrails along Both Edges of the Stair Flight (1.20m tall, 0.22m thick)
	var rail_h := 1.20
	var rail_thick := 0.22
	for side_x in [x_left, x_right]:
		var sx: float = float(side_x)
		var rail_mid := (p_start + p_end) * 0.5 + r_unit * sx + by * (rail_h * 0.48)
		_add_oriented_box_collider(
			sb,
			rail_mid,
			ramp_basis,
			Vector3(rail_thick, rail_h, ramp_len)
		)

func _build_endcap_staircase_and_mezzanine_visual_meshes(
	fl_root: Node3D,
	c_front: Vector3,
	r_unit: Vector3,
	t_unit: Vector3,
	mez_t0: float,
	mez_t1: float,
	mez_x: float,
	y_mez: float
) -> void:
	var vis_root := Node3D.new()
	vis_root.name = "EndCapStaircaseVisuals"
	fl_root.add_child(vis_root)

	var st_floor := SurfaceTool.new()
	st_floor.begin(Mesh.PRIMITIVE_TRIANGLES)
	var st_terrazzo := SurfaceTool.new()
	st_terrazzo.begin(Mesh.PRIMITIVE_TRIANGLES)
	var st_plaster := SurfaceTool.new()
	st_plaster.begin(Mesh.PRIMITIVE_TRIANGLES)
	var st_glass := SurfaceTool.new()
	st_glass.begin(Mesh.PRIMITIVE_TRIANGLES)

	# 1. Visual Mezzanine Overlook Balcony Deck (y = 4.25m, X = -10.8..+10.8m, t = 18.5..22.5m)
	var m_w0 := c_front + t_unit * mez_t0 - r_unit * mez_x + Vector3(0, y_mez, 0)
	var m_e0 := c_front + t_unit * mez_t0 + r_unit * mez_x + Vector3(0, y_mez, 0)
	var m_e1 := c_front + t_unit * mez_t1 + r_unit * mez_x + Vector3(0, y_mez, 0)
	var m_w1 := c_front + t_unit * mez_t1 - r_unit * mez_x + Vector3(0, y_mez, 0)
	_add_quad(st_floor, m_w0, m_e0, m_e1, m_w1, Vector3.UP)
	_add_quad(
		st_plaster,
		m_w0 - Vector3(0, 0.36, 0),
		m_w1 - Vector3(0, 0.36, 0),
		m_e1 - Vector3(0, 0.36, 0),
		m_e0 - Vector3(0, 0.36, 0),
		Vector3.DOWN
	)
	_add_quad(
		st_terrazzo,
		m_w0 + t_unit * 0.5 + r_unit * 0.6 + Vector3(0, 0.012, 0),
		m_e0 + t_unit * 0.5 - r_unit * 0.6 + Vector3(0, 0.012, 0),
		m_e1 - t_unit * 0.5 - r_unit * 0.6 + Vector3(0, 0.012, 0),
		m_w1 - t_unit * 0.5 + r_unit * 0.6 + Vector3(0, 0.012, 0),
		Vector3.UP
	)
	_add_thick_wall_band(st_plaster, m_w0, m_e0, -0.36, 0.0, 0.28)
	_add_thick_wall_band(st_plaster, m_w1, m_e1, -0.36, 0.0, 0.28)
	_add_thick_wall_band(st_plaster, m_w0, m_w1, -0.36, 0.0, 0.28)
	_add_thick_wall_band(st_plaster, m_e0, m_e1, -0.36, 0.0, 0.28)

	_add_rotunda_balustrade_span(st_plaster, st_glass, m_w1, m_e1)
	_add_rotunda_balustrade_span(st_plaster, st_glass, m_w0, m_w1)
	_add_rotunda_balustrade_span(st_plaster, st_glass, m_e0, m_e1)
	_add_rotunda_balustrade_span(
		st_plaster, st_glass,
		c_front + t_unit * mez_t0 - r_unit * 6.4 + Vector3(0, y_mez, 0),
		c_front + t_unit * mez_t0 + r_unit * 6.4 + Vector3(0, y_mez, 0)
	)

	# 2. Visual West Flight A (0.0m -> 4.25m) & East Flight B (4.25m -> 8.50m)
	_add_stair_flight_visual_geometry(
		st_terrazzo, st_plaster, st_glass,
		c_front + t_unit * 8.5,
		c_front + t_unit * mez_t0 + Vector3(0, y_mez, 0),
		r_unit,
		-10.2,
		-6.4,
		20
	)
	_add_stair_flight_visual_geometry(
		st_terrazzo, st_plaster, st_glass,
		c_front + t_unit * mez_t0 + Vector3(0, y_mez, 0),
		c_front + t_unit * 8.5 + Vector3(0, 8.50, 0),
		r_unit,
		6.4,
		10.2,
		20
	)

	var mi_floor := MeshInstance3D.new()
	mi_floor.mesh = st_floor.commit()
	mi_floor.material_override = _mat_floor
	vis_root.add_child(mi_floor)

	var mi_terrazzo := MeshInstance3D.new()
	mi_terrazzo.mesh = st_terrazzo.commit()
	mi_terrazzo.material_override = _mat_terrazzo
	vis_root.add_child(mi_terrazzo)

	var mi_plaster := MeshInstance3D.new()
	mi_plaster.mesh = st_plaster.commit()
	mi_plaster.material_override = _mat_plaster
	vis_root.add_child(mi_plaster)

	var mi_glass := MeshInstance3D.new()
	mi_glass.mesh = st_glass.commit()
	mi_glass.material_override = _mat_glass
	vis_root.add_child(mi_glass)

func _add_stair_flight_visual_geometry(
	st_terrazzo: SurfaceTool,
	st_plaster: SurfaceTool,
	st_glass: SurfaceTool,
	p_start: Vector3,
	p_end: Vector3,
	r_unit: Vector3,
	x_left: float,
	x_right: float,
	num_steps: int
) -> void:
	var s_l0 := p_start + r_unit * x_left
	var s_r0 := p_start + r_unit * x_right
	var s_l1 := p_end + r_unit * x_left
	var s_r1 := p_end + r_unit * x_right

	# Sloped underside plaster carriage soffit & side stringers
	var soffit_drop := Vector3(0.0, -0.36, 0.0)
	_add_quad(st_plaster, s_l0 + soffit_drop, s_l1 + soffit_drop, s_r1 + soffit_drop, s_r0 + soffit_drop, Vector3.DOWN)
	_add_quad(st_plaster, s_l0 + soffit_drop, s_l0, s_l1, s_l1 + soffit_drop, -r_unit)
	_add_quad(st_plaster, s_r0, s_r0 + soffit_drop, s_r1 + soffit_drop, s_r1, r_unit)

	# 20 Visual Stepped Terrazzo Treads & Risers along the flight (recessed below the BoxShape3D walk plane so feet never clip!)
	for i in range(num_steps):
		var f0 := float(i) / float(num_steps)
		var f1 := float(i + 1) / float(num_steps)
		var tread_y := lerpf(p_start.y, p_end.y, f0)
		var next_y := lerpf(p_start.y, p_end.y, f1)
		var base_0 := p_start.lerp(p_end, f0)
		var base_1 := p_start.lerp(p_end, f1)
		base_0.y = tread_y
		base_1.y = tread_y
		var tl0 := base_0 + r_unit * (x_left + 0.14)
		var tr0 := base_0 + r_unit * (x_right - 0.14)
		var tl1 := base_1 + r_unit * (x_left + 0.14)
		var tr1 := base_1 + r_unit * (x_right - 0.14)
		_add_quad(st_terrazzo, tl0, tr0, tr1, tl1, Vector3.UP)
		var rl1 := Vector3(tl1.x, next_y, tl1.z)
		var rr1 := Vector3(tr1.x, next_y, tr1.z)
		_add_quad(st_plaster, tl1, tr1, rr1, rl1, (p_start - p_end).normalized())

	# Sloped 1.15m-Tall Glass & Plaster Side Balustrades on Both Sides of the Staircase Flight
	var rail_h := 1.15
	for side_x in [x_left, x_right]:
		var sx: float = float(side_x)
		var b0: Vector3 = p_start + r_unit * sx
		var b1: Vector3 = p_end + r_unit * sx
		var rw := r_unit * 0.07
		_add_quad(st_plaster, b0 - rw, b1 - rw, b1 - rw + Vector3(0, 0.18, 0), b0 - rw + Vector3(0, 0.18, 0), -r_unit)
		_add_quad(st_plaster, b1 + rw, b0 + rw, b0 + rw + Vector3(0, 0.18, 0), b1 + rw + Vector3(0, 0.18, 0), r_unit)
		_add_quad(st_glass, b0 - rw * 0.5 + Vector3(0, 0.18, 0), b1 - rw * 0.5 + Vector3(0, 0.18, 0), b1 - rw * 0.5 + Vector3(0, 1.05, 0), b0 - rw * 0.5 + Vector3(0, 1.05, 0), -r_unit)
		_add_quad(st_glass, b1 + rw * 0.5 + Vector3(0, 0.18, 0), b0 + rw * 0.5 + Vector3(0, 0.18, 0), b0 + rw * 0.5 + Vector3(0, 1.05, 0), b1 + rw * 0.5 + Vector3(0, 1.05, 0), r_unit)
		_add_quad(st_plaster, b0 - rw + Vector3(0, 1.05, 0), b1 - rw + Vector3(0, 1.05, 0), b1 - rw + Vector3(0, rail_h, 0), b0 - rw + Vector3(0, rail_h, 0), -r_unit)
		_add_quad(st_plaster, b1 + rw + Vector3(0, 1.05, 0), b0 + rw + Vector3(0, 1.05, 0), b0 + rw + Vector3(0, rail_h, 0), b1 + rw + Vector3(0, rail_h, 0), r_unit)
		_add_quad(st_plaster, b0 - rw + Vector3(0, rail_h, 0), b0 + rw + Vector3(0, rail_h, 0), b1 + rw + Vector3(0, rail_h, 0), b1 - rw + Vector3(0, rail_h, 0), Vector3.UP)

## Returns the canonical store_id for a store slot (mapping the 6 signature Bay 0 stores to their exact IDs).
func _get_store_id_for_slot(is_north: bool, floor_num: int, bay_k: int, col_name: String) -> String:
	if bay_k == 0:
		if is_north and floor_num == 1 and col_name == "East":
			return "East_F0_02"
		if is_north and floor_num == 1 and col_name == "West":
			return "West_F1_03"
		if not is_north and floor_num == 1 and col_name == "West":
			return "West_F1_02"
		if is_north and floor_num == 2 and col_name == "West":
			return "West_F2_01"
		if is_north and floor_num == 2 and col_name == "East":
			return "East_F2_01"
		if is_north and floor_num == 3 and col_name == "East":
			return "Center_F3_01"
	var wing_prefix := "North" if is_north else "South"
	return "%s_%s_B%d_F%d" % [wing_prefix, col_name, bay_k + 1, floor_num]

## Designates 10 Haunted Stores per Floor (5 in North Wing + 5 in South Wing out of 48 stores per floor).
## Haunted stores have the lights OFF around their storefront as a subtle visual hint and feature
## motorized Security Shutters that slam shut & lock when a Ghost Hunt is triggered inside!
func _is_haunted_store_slot(is_north: bool, _floor_num: int, bay_k: int, col_name: String) -> bool:
	if is_north:
		return (
			(bay_k == 0 and col_name == "West")
			or (bay_k == 0 and col_name == "East")
			or (bay_k == 2 and col_name == "Center")
			or (bay_k == 4 and col_name == "West")
			or (bay_k == 6 and col_name == "East")
		)
	else:
		return (
			(bay_k == 0 and col_name == "West")
			or (bay_k == 1 and col_name == "East")
			or (bay_k == 3 and col_name == "Center")
			or (bay_k == 5 and col_name == "West")
			or (bay_k == 7 and col_name == "East")
		)

func _spawn_store_security_shutter(
	parent: Node3D,
	store_id: String,
	pos: Vector3,
	yaw_rad: float,
	door_w: float,
	door_h: float
) -> StoreSecurityShutter:
	var shutter := StoreSecurityShutter.new()
	shutter.name = "Shutter_%s" % store_id
	shutter.position = pos
	shutter.rotation = Vector3(0.0, yaw_rad, 0.0)
	parent.add_child(shutter)
	shutter.setup_shutter(store_id, door_w, door_h, _mat_shutter)
	return shutter

func _populate_wing_lights_signs_and_props(
	fl_root: Node3D,
	is_north: bool,
	floor_num: int,
	fy: float,
	wall_h: float
) -> void:
	var c_arr := _north_stations_c if is_north else _south_stations_c
	var r_arr := _north_stations_r if is_north else _south_stations_r
	var t_arr := _north_stations_t if is_north else _south_stations_t

	var center_titles := [
		"SILK ROAD PASSAGE & TEA ARCADE",
		"CRESCENT CROSSING BOOKS & RECORDS",
		"SOLARIS OPTICS & WATCH GALLERY",
		"VELVET PAVILION & FRAGRANCE PASSAGE",
		"ARCADIA GAMES & HOBBY CROSSWAY",
		"IMPERIAL PORCELAIN & CURIO ARCADE",
	]
	var outer_titles := [
		"CELESTIAL APPAREL & FOOTWEAR",
		"MERIDIAN ELECTRONICS & AUDIO",
		"CROWN JEWELERS & HOROLOGY",
		"BOTANICA HERBALS & APOTHECARY",
		"BRICKWORKZ TOY & MODEL EMPORIUM",
		"STARLIGHT HOME & SLEEP GALLERY",
	]

	var shelf_model := "res://ExternalAssets/Mall/PolyHaven/ImportReady/SM_RetailShelves01.fbx"
	var gondola_model := "res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25242_MerchandiseGondola.fbx"
	var counter_model := "res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25339_GlazedDisplayCounter.fbx"
	var desk_model := "res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25310_CashDesk120.fbx"

	for k in range(NUM_BAYS):
		var c0 := c_arr[k] + Vector3(0.0, fy, 0.0)
		var c1 := c_arr[k + 1] + Vector3(0.0, fy, 0.0)
		var r0 := r_arr[k]
		var r1 := r_arr[k + 1]
		var c_mid := (c0 + c1) * 0.5
		var r_mid := (r0 + r1).normalized()
		var t_mid := (t_arr[k] + t_arr[k + 1]).normalized()
		var bay_yaw_deg := rad_to_deg(atan2(t_mid.x, -t_mid.z if is_north else t_mid.z))
		var wall_yaw_rad := atan2(r_mid.x, r_mid.z)
		var part_yaw_rad := atan2(-t_mid.z, t_mid.x)

		var west_id := _get_store_id_for_slot(is_north, floor_num, k, "West")
		var center_id := _get_store_id_for_slot(is_north, floor_num, k, "Center")
		var east_id := _get_store_id_for_slot(is_north, floor_num, k, "East")

		var west_haunted := _is_haunted_store_slot(is_north, floor_num, k, "West")
		var center_haunted := _is_haunted_store_slot(is_north, floor_num, k, "Center")
		var east_haunted := _is_haunted_store_slot(is_north, floor_num, k, "East")

		# 1. Concourse & Store Lighting:
		#    Non-haunted stores have warm concourse & storefront lighting ON;
		#    The 10 Haunted Stores per floor have the lights OFF around their storefront as a subtle visual hint!
		if not west_haunted and not center_haunted:
			var w_light := OmniLight3D.new()
			w_light.position = c_mid + r_mid * WEST_HALL_CENTER_X + Vector3(0.0, wall_h - 0.45, 0.0)
			w_light.light_color = Color(0.98, 0.94, 0.86)
			w_light.light_energy = 1.25
			w_light.omni_range = 20.0
			w_light.distance_fade_enabled = true
			w_light.distance_fade_begin = 55.0
			w_light.distance_fade_length = 20.0
			fl_root.add_child(w_light)

		if not center_haunted:
			var c_light := OmniLight3D.new()
			c_light.position = c_mid + Vector3(0.0, wall_h - 0.45, 0.0)
			c_light.light_color = Color(0.98, 0.94, 0.86)
			c_light.light_energy = 1.15
			c_light.omni_range = 18.0
			c_light.distance_fade_enabled = true
			c_light.distance_fade_begin = 50.0
			c_light.distance_fade_length = 18.0
			fl_root.add_child(c_light)

		if not east_haunted and not center_haunted:
			var e_light := OmniLight3D.new()
			e_light.position = c_mid + r_mid * EAST_HALL_CENTER_X + Vector3(0.0, wall_h - 0.45, 0.0)
			e_light.light_color = Color(0.98, 0.94, 0.86)
			e_light.light_energy = 1.25
			e_light.omni_range = 20.0
			e_light.distance_fade_enabled = true
			e_light.distance_fade_begin = 55.0
			e_light.distance_fade_length = 20.0
			fl_root.add_child(e_light)

		# 2. Motorized Security Shutters on Haunted Stores (slam shut & lock during Ghost Hunts!):
		if west_haunted:
			_spawn_store_security_shutter(fl_root, west_id, c_mid + r_mid * WEST_HALL_OUTER_X, wall_yaw_rad, 5.8, 4.4)
			_spawn_store_security_shutter(fl_root, west_id, c0 + r0 * -33.0, part_yaw_rad, 3.4, 3.8)
			if k + 1 < NUM_BAYS:
				_spawn_store_security_shutter(fl_root, west_id, c1 + r1 * -33.0, part_yaw_rad, 3.4, 3.8)
		if center_haunted:
			_spawn_store_security_shutter(fl_root, center_id, c_mid + r_mid * WEST_HALL_INNER_X, wall_yaw_rad, 5.8, 4.4)
			_spawn_store_security_shutter(fl_root, center_id, c_mid + r_mid * EAST_HALL_INNER_X, wall_yaw_rad, 5.8, 4.4)
			_spawn_store_security_shutter(fl_root, center_id, c0, part_yaw_rad, 3.4, 3.8)
			if k + 1 < NUM_BAYS:
				_spawn_store_security_shutter(fl_root, center_id, c1, part_yaw_rad, 3.4, 3.8)
		if east_haunted:
			_spawn_store_security_shutter(fl_root, east_id, c_mid + r_mid * EAST_HALL_OUTER_X, wall_yaw_rad, 5.8, 4.4)
			_spawn_store_security_shutter(fl_root, east_id, c0 + r0 * 33.0, part_yaw_rad, 3.4, 3.8)
			if k + 1 < NUM_BAYS:
				_spawn_store_security_shutter(fl_root, east_id, c1 + r1 * 33.0, part_yaw_rad, 3.4, 3.8)

		# 3. Storefront Sign Headers & Representative Interior Furnishings across all 8 Bays:
		var c_tag := " [HAUNTED — LIGHTS OFF]" if center_haunted else ""
		var w_tag := " [HAUNTED — LIGHTS OFF]" if west_haunted else ""
		var e_tag := " [HAUNTED — LIGHTS OFF]" if east_haunted else ""

		var c_title: String = "%s [%s]%s" % [center_titles[k % center_titles.size()], center_id, c_tag]
		var w_title: String = "%s [%s]%s" % [outer_titles[k % outer_titles.size()], west_id, w_tag]
		var e_title: String = "%s [%s]%s" % [outer_titles[(k + 3) % outer_titles.size()], east_id, e_tag]

		var c_col := Color(0.54, 0.66, 0.78) if center_haunted else Color(0.98, 0.94, 0.84)
		var w_col := Color(0.54, 0.66, 0.78) if west_haunted else Color(0.98, 0.94, 0.84)
		var e_col := Color(0.54, 0.66, 0.78) if east_haunted else Color(0.98, 0.94, 0.84)

		_build_sign_header(fl_root, c_mid + r_mid * (WEST_HALL_INNER_X - 0.35) + Vector3(0, 4.55, 0), Vector3(0, -90.0 - bay_yaw_deg, 0), c_title, c_col)
		_build_sign_header(fl_root, c_mid + r_mid * (EAST_HALL_INNER_X + 0.35) + Vector3(0, 4.55, 0), Vector3(0, 90.0 - bay_yaw_deg, 0), c_title, c_col)
		if k > 0 or not is_north:
			_build_sign_header(fl_root, c_mid + r_mid * (WEST_HALL_OUTER_X + 0.35) + Vector3(0, 4.55, 0), Vector3(0, 90.0 - bay_yaw_deg, 0), w_title, w_col)
			_build_sign_header(fl_root, c_mid + r_mid * (EAST_HALL_OUTER_X - 0.35) + Vector3(0, 4.55, 0), Vector3(0, -90.0 - bay_yaw_deg, 0), e_title, e_col)

		if floor_num == 1 and (k <= 4 or k == NUM_BAYS - 1):
			# Furnish Center Pass-Through Store (leaving the central H-crossbar and N-S neighbor doors 100% clear!)
			_spawn_prop(fl_root, gondola_model, c_mid + r_mid * -5.5 + t_mid * 6.5, Vector3(1.4, 1.5, 0.9))
			_spawn_prop(fl_root, counter_model, c_mid + r_mid * 5.5 - t_mid * 6.5, Vector3(1.4, 1.1, 0.8))
			# Furnish West & East 26m-Deep Outer Stores (leaving x = +-33m neighbor doorway paths clear!)
			_spawn_prop(fl_root, shelf_model, c_mid + r_mid * -27.5 + t_mid * 6.0, Vector3(1.4, 2.0, 0.7))
			_spawn_prop(fl_root, desk_model, c_mid + r_mid * -38.5 - t_mid * 6.0, Vector3(1.4, 1.1, 0.8))
			_spawn_prop(fl_root, shelf_model, c_mid + r_mid * 27.5 + t_mid * 6.0, Vector3(1.4, 2.0, 0.7))
			_spawn_prop(fl_root, desk_model, c_mid + r_mid * 38.5 - t_mid * 6.0, Vector3(1.4, 1.1, 0.8))

	# --------------------------------------------------------------------------
	# Multi-Level Flagship End-Cap Anchor Department Store (60m x 32m, 3 Connected Levels!)
	# Reference: Galeries Lafayette Haussmann / Nordstrom Flagships / Macy's Central Rotunda
	# --------------------------------------------------------------------------
	var c_end := c_arr[NUM_BAYS] + Vector3(0.0, fy, 0.0)
	var r_end := r_arr[NUM_BAYS].normalized()
	var t_end := t_arr[NUM_BAYS].normalized()
	var store_brand := "AURA GRAND EMPORIUM" if is_north else "MONARCH GRAND GALLERIA"
	var dept_subtitle := "L1: COSMETICS, FRAGRANCE & WATCH ROTUNDA"
	if floor_num == 2:
		dept_subtitle = "L2: DESIGNER APPAREL, EVENING WEAR & CONCOURSE GALLERY"
	elif floor_num == 3:
		dept_subtitle = "L3: HOME LIVING, HI-FI AUDIO & SKY CUPOLA ROTUNDA"

	var anchor_title := "%s — %s [60m x 32m MULTI-LEVEL ANCHOR | F%d]" % [store_brand, dept_subtitle, floor_num]
	for hx in [WEST_HALL_CENTER_X, EAST_HALL_CENTER_X]:
		_build_sign_header(fl_root, c_end + r_end * hx - t_end * 0.4 + Vector3(0, 4.75, 0), Vector3.ZERO, anchor_title, Color(1.0, 0.94, 0.78))

	# Grand Staircase & Mezzanine Wayfinding Signs inside the Rotunda
	if floor_num == 1:
		_build_sign_header(
			fl_root,
			c_end + t_end * 8.2 - r_end * 8.3 + Vector3(0, 2.75, 0),
			Vector3.ZERO,
			"▲ GRAND ROTUNDA STAIRCASE -> MEZZANINE (+4.25m) -> FLOOR 2 CONCOURSE (+8.50m)",
			Color(0.68, 0.96, 0.88)
		)
		_build_sign_header(
			fl_root,
			c_end + t_end * 20.4 + Vector3(0, 6.45, 0),
			Vector3.ZERO,
			"%s — MEZZANINE OVERLOOK BALCONY [F1 -> F2]" % store_brand,
			Color(0.98, 0.90, 0.74)
		)
	elif floor_num == 2:
		_build_sign_header(
			fl_root,
			c_end + t_end * 8.2 + r_end * 8.3 + Vector3(0, 2.75, 0),
			Vector3.ZERO,
			"▼ GRAND ROTUNDA STAIRCASE DOWN TO FLOOR 1 | ◄► EXIT PORTALS TO FLOOR 2 CONCOURSE",
			Color(0.76, 0.92, 1.0)
		)

	# Warm Department Store Gallery & Rotunda Lighting
	var c_anchor_mid := c_end + t_end * 15.5
	for ax in [-21.0, 0.0, 21.0]:
		var alight := OmniLight3D.new()
		alight.position = c_anchor_mid + r_end * ax + Vector3(0.0, wall_h - 0.55, 0.0)
		alight.light_color = Color(1.0, 0.95, 0.86)
		alight.light_energy = 1.45
		alight.omni_range = 26.0
		alight.distance_fade_enabled = true
		alight.distance_fade_begin = 70.0
		alight.distance_fade_length = 22.0
		fl_root.add_child(alight)

	# 3-Story Suspended Rotunda Chandelier Ring at the Center of the Light-Well (X = 0, t = 14.2m)
	var chand_mi := MeshInstance3D.new()
	var chand_torus := TorusMesh.new()
	chand_torus.inner_radius = 2.6 - float(floor_num - 1) * 0.45
	chand_torus.outer_radius = chand_torus.inner_radius + 0.24
	chand_mi.mesh = chand_torus
	chand_mi.position = c_end + t_end * 14.2 + Vector3(0.0, 5.8, 0.0)
	chand_mi.material_override = MallPbrMaterialLibrary.get_cove_accent_material(Color(1.0, 0.91, 0.74), 1.35)
	fl_root.add_child(chand_mi)

	# Multi-Level Department Furnishings:
	# - Level 1: Central Cosmetics/Fragrance Rotunda Island + West/East Checkout Desks & Gondolas
	# - Level 2: West & East Wrap-Around Fashion Galleries (Mannequins, Hanging Rails & Glazed Counters)
	# - Level 3: West & East Wrap-Around Home & Audio Galleries (Steel Shelving & Display Counters)
	var mannequin_model := "res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25257_FemaleMannequin.fbx"
	var rail_model := "res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25208_StraightHangingRail.fbx"
	if floor_num == 1:
		# Central Rotunda Cosmetics & Watch Island Counters (inside X = -4..+4m, t = 12..16m)
		for cx in [-3.2, 3.2]:
			_spawn_prop(fl_root, counter_model, c_end + t_end * 12.8 + r_end * cx, Vector3(1.6, 1.15, 0.85))
			_spawn_prop(fl_root, counter_model, c_end + t_end * 15.8 + r_end * cx, Vector3(1.6, 1.15, 0.85))
		for ax in [-23.0, -16.5, 16.5, 23.0]:
			_spawn_prop(fl_root, gondola_model, c_end + t_end * 14.5 + r_end * ax, Vector3(1.6, 1.5, 0.9))
			_spawn_prop(fl_root, desk_model, c_end + t_end * 26.5 + r_end * ax, Vector3(1.4, 1.1, 0.8))
	elif floor_num == 2:
		for ax in [-23.5, -17.5, 17.5, 23.5]:
			_spawn_prop(fl_root, rail_model, c_end + t_end * 12.5 + r_end * ax, Vector3(1.4, 1.45, 0.65))
			_spawn_prop(fl_root, mannequin_model, c_end + t_end * 17.5 + r_end * ax, Vector3(0.6, 1.78, 0.6))
			_spawn_prop(fl_root, counter_model, c_end + t_end * 26.5 + r_end * ax, Vector3(1.6, 1.15, 0.85))
	elif floor_num == 3:
		for ax in [-23.5, -17.5, 17.5, 23.5]:
			_spawn_prop(fl_root, shelf_model, c_end + t_end * 13.5 + r_end * ax, Vector3(1.4, 2.0, 0.7))
			_spawn_prop(fl_root, counter_model, c_end + t_end * 26.5 + r_end * ax, Vector3(1.6, 1.15, 0.85))

# ------------------------------------------------------------------------------
# 4. Batched SurfaceTool Wall, Window & Portal Helpers
# ------------------------------------------------------------------------------
func _add_storefront_facade_32m(
	st_plaster: SurfaceTool,
	st_glass: SurfaceTool,
	p0: Vector3,
	p1: Vector3,
	wall_h: float
) -> void:
	# 32.0m-Wide Retail Storefront with:
	# - [0.00..0.10] (3.2m): Left Structural Column Pier
	# - [0.10..0.41] (9.9m): Left Glazed Display Window
	# - [0.41..0.59] (5.8m): Wide Open Storefront Entrance Portal (4.4m tall!)
	# - [0.59..0.90] (9.9m): Right Glazed Display Window
	# - [0.90..1.00] (3.2m): Right Structural Column Pier
	var v_a := p0
	var v_b := p0.lerp(p1, 0.10)
	var v_c := p0.lerp(p1, 0.41)
	var v_d := p0.lerp(p1, 0.59)
	var v_e := p0.lerp(p1, 0.90)
	var v_f := p1

	_add_thick_wall_segment(st_plaster, v_a, v_b, wall_h, 0.42)
	_add_storefront_window_span(st_plaster, st_glass, v_b, v_c, wall_h)
	_add_portal_transom_span(st_plaster, v_c, v_d, 4.4, wall_h, 0.42)
	_add_storefront_window_span(st_plaster, st_glass, v_d, v_e, wall_h)
	_add_thick_wall_segment(st_plaster, v_e, v_f, wall_h, 0.42)

func _add_storefront_window_span(
	st_plaster: SurfaceTool,
	st_glass: SurfaceTool,
	p0: Vector3,
	p1: Vector3,
	wall_h: float
) -> void:
	var knee_h := 0.82
	var win_top := 4.40
	# Knee-wall curb (0 .. 0.82m)
	_add_thick_wall_band(st_plaster, p0, p1, 0.0, knee_h, 0.36)
	# Architectural display window glass (0.82 .. 4.40m)
	_add_thick_wall_band(st_glass, p0, p1, knee_h, win_top, 0.14)
	# Upper fascia transom header (4.40 .. wall_h)
	_add_thick_wall_band(st_plaster, p0, p1, win_top, wall_h, 0.38)

func _add_portal_transom_span(
	st_plaster: SurfaceTool,
	p0: Vector3,
	p1: Vector3,
	door_h: float,
	wall_h: float,
	thickness: float
) -> void:
	_add_thick_wall_band(st_plaster, p0, p1, door_h, wall_h, thickness)

func _add_partition_wall_with_door(
	st_plaster: SurfaceTool,
	p0: Vector3,
	p1: Vector3,
	wall_h: float,
	door_width: float,
	door_h: float
) -> void:
	var span_len := p0.distance_to(p1)
	if span_len <= door_width + 0.5:
		return
	var half_frac := (door_width * 0.5) / span_len
	var d0 := p0.lerp(p1, 0.5 - half_frac)
	var d1 := p0.lerp(p1, 0.5 + half_frac)

	_add_thick_wall_segment(st_plaster, p0, d0, wall_h, 0.38)
	_add_portal_transom_span(st_plaster, d0, d1, door_h, wall_h, 0.38)
	_add_thick_wall_segment(st_plaster, d1, p1, wall_h, 0.38)

func _add_thick_wall_segment(st: SurfaceTool, p0: Vector3, p1: Vector3, wall_h: float, thickness: float) -> void:
	_add_thick_wall_band(st, p0, p1, 0.0, wall_h, thickness)

func _add_thick_wall_band(
	st: SurfaceTool,
	p0: Vector3,
	p1: Vector3,
	y_bot: float,
	y_top: float,
	thickness: float
) -> void:
	var dir := p1 - p0
	dir.y = 0.0
	if dir.length_squared() < 0.001:
		return
	dir = dir.normalized()
	var n := Vector3(-dir.z, 0.0, dir.x) * (thickness * 0.5)
	var n_unit := n.normalized()

	var a0 := p0 + n + Vector3(0.0, y_bot, 0.0)
	var a1 := p1 + n + Vector3(0.0, y_bot, 0.0)
	var a2 := p1 + n + Vector3(0.0, y_top, 0.0)
	var a3 := p0 + n + Vector3(0.0, y_top, 0.0)

	var b0 := p0 - n + Vector3(0.0, y_bot, 0.0)
	var b1 := p1 - n + Vector3(0.0, y_bot, 0.0)
	var b2 := p1 - n + Vector3(0.0, y_top, 0.0)
	var b3 := p0 - n + Vector3(0.0, y_top, 0.0)

	_add_quad(st, a0, a1, a2, a3, n_unit)
	_add_quad(st, b1, b0, b3, b2, -n_unit)
	_add_quad(st, b3, a3, a2, b2, Vector3.UP)
	_add_quad(st, a0, b0, b1, a1, Vector3.DOWN)
	_add_quad(st, b0, a0, a3, b3, -dir)
	_add_quad(st, a1, b1, b2, a2, dir)

func _add_tri(st: SurfaceTool, v0: Vector3, v1: Vector3, v2: Vector3, n: Vector3) -> void:
	var face_cross := (v1 - v0).cross(v2 - v0)
	var b := v1
	var c := v2
	if face_cross.dot(n) >= 0.0:
		b = v2
		c = v1
	st.set_normal(n)
	st.set_tangent(Plane(1.0, 0.0, 0.0, 1.0))
	st.set_uv(Vector2(v0.x * 0.25, (v0.z + v0.y) * 0.25))
	st.add_vertex(v0)
	st.set_normal(n)
	st.set_tangent(Plane(1.0, 0.0, 0.0, 1.0))
	st.set_uv(Vector2(b.x * 0.25, (b.z + b.y) * 0.25))
	st.add_vertex(b)
	st.set_normal(n)
	st.set_tangent(Plane(1.0, 0.0, 0.0, 1.0))
	st.set_uv(Vector2(c.x * 0.25, (c.z + c.y) * 0.25))
	st.add_vertex(c)

func _add_quad(st: SurfaceTool, v0: Vector3, v1: Vector3, v2: Vector3, v3: Vector3, n: Vector3) -> void:
	_add_tri(st, v0, v1, v2, n)
	_add_tri(st, v0, v2, v3, n)

func _build_sign_header(parent: Node3D, pos: Vector3, rot_deg: Vector3, text_str: String, col: Color = Color(0.98, 0.94, 0.84)) -> void:
	var lbl := Label3D.new()
	lbl.position = pos
	lbl.rotation_degrees = rot_deg
	lbl.text = text_str
	lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	lbl.font_size = 20
	lbl.outline_size = 5
	lbl.modulate = col
	lbl.visibility_range_end = 42.0
	lbl.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
	parent.add_child(lbl)

func _spawn_prop(parent: Node3D, model_path: String, pos: Vector3, col_size: Vector3, with_col: bool = true) -> void:
	if ResourceLoader.exists(model_path):
		var packed := load(model_path) as PackedScene
		if packed:
			var inst := packed.instantiate() as Node3D
			if inst:
				inst.position = pos
				MallPbrMaterialLibrary.beautify_model_materials(inst, model_path, Color(0.76, 0.68, 0.58))
				parent.add_child(inst)
	if with_col and col_size.length_squared() > 0.01:
		var sb := StaticBody3D.new()
		sb.position = pos + Vector3(0.0, col_size.y * 0.5, 0.0)
		parent.add_child(sb)
		var col := CollisionShape3D.new()
		var box := BoxShape3D.new()
		box.size = col_size
		col.shape = box
		sb.add_child(col)

func _add_collider(sb: StaticBody3D, local_pos: Vector3, size: Vector3) -> void:
	var col := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = size
	col.shape = box
	col.position = local_pos
	sb.add_child(col)

func _create_box(parent: Node3D, pos: Vector3, size: Vector3, mat: Material, with_collision: bool = true) -> void:
	if with_collision:
		var sb := StaticBody3D.new()
		sb.position = pos
		parent.add_child(sb)
		var col := CollisionShape3D.new()
		var box := BoxShape3D.new()
		box.size = size
		col.shape = box
		sb.add_child(col)
		var mi := MeshInstance3D.new()
		var bm := BoxMesh.new()
		bm.size = size
		mi.mesh = bm
		mi.material_override = mat
		sb.add_child(mi)
	else:
		var mi := MeshInstance3D.new()
		mi.position = pos
		var bm := BoxMesh.new()
		bm.size = size
		mi.mesh = bm
		mi.material_override = mat
		parent.add_child(mi)
