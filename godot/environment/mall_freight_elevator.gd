@tool
class_name MallFreightElevator
extends Node3D
## Physical Rideable Service Freight Elevator connecting Floors 1 through 7 (Y = 0.0m .. 51.0m)
## plus Dynamically Generated Procedural Upper Floors (Floor 8, 9, 10... at +8.50m per floor!).
## - Large 4.6m x 3.8m industrial platform cab that players AND both carts (Equipment Push Cart
##   + Mobile Light Cart) can roll onto and ride back and forth across all floors.
## - Automatic Floor 1 (South) and Upper Floor 2..N (North) safety gates + per-floor shaft barrier gates
##   so players can never walk into an open elevator shaft on any floor.

signal floor_arrived(floor_number: int, floor_elevation_y: float)
signal procedural_floor_requested(new_floor_number: int)

@export var floor_1_y: float = 0.0
@export var floor_2_y: float = 8.50
@export var floor_3_y: float = 17.00
@export var floor_spacing: float = 8.50
@export var max_floors: int = 7
@export var elevator_speed: float = 3.20
@export var cab_width: float = 4.6
@export var cab_depth: float = 3.8
@export var cab_height: float = 3.3

var current_cab_y: float = 0.0
var target_cab_y: float = 0.0
var current_floor_num: int = 1
var is_moving: bool = false
var is_overdrive_repaired: bool = false

var cab_root: Node3D = null
var cab_floor_body: StaticBody3D = null
var south_gate_body: StaticBody3D = null
var south_gate_mesh: MeshInstance3D = null
var north_gate_body: StaticBody3D = null
var north_gate_mesh: MeshInstance3D = null

var f2_shaft_gate_body: StaticBody3D = null
var f2_shaft_gate_mesh: MeshInstance3D = null
var f3_shaft_gate_body: StaticBody3D = null
var f3_shaft_gate_mesh: MeshInstance3D = null

var shaft_gate_bodies: Dictionary = {} # floor_num (int) -> StaticBody3D
var shaft_gate_meshes: Dictionary = {} # floor_num (int) -> MeshInstance3D
var call_labels: Dictionary = {}       # floor_num (int) -> Label3D
var _shaft_posts: Array[MeshInstance3D] = []
var _shaft_top_beams: Array[MeshInstance3D] = []

var cab_light: OmniLight3D = null
var cab_control_label: Label3D = null
var cab_descend_label: Label3D = null
var f1_call_label: Label3D = null
var f2_call_label: Label3D = null
var f3_call_label: Label3D = null
var motor_audio: AudioStreamPlayer3D = null

var _steel_mat: StandardMaterial3D = null
var _hazard_mat: StandardMaterial3D = null
var _glass_mat: StandardMaterial3D = null

func _ready() -> void:
	name = "MallFreightElevator"
	add_to_group("freight_elevators")
	current_cab_y = floor_1_y
	target_cab_y = floor_1_y
	current_floor_num = 1
	_build_elevator_shaft_and_cab()
	_update_gate_states_and_labels()
	if not Engine.is_editor_hint():
		for f in range(1, max_floors + 1):
			GameState.freight_elevator_unlocked_floors[f] = true

func get_floor_y(floor_num: int) -> float:
	return float(maxi(1, floor_num) - 1) * floor_spacing

func get_floor_num_from_y(y_val: float) -> int:
	return maxi(1, int(round(y_val / floor_spacing)) + 1)

func _build_elevator_shaft_and_cab() -> void:
	var half_w := cab_width * 0.5
	var half_d := cab_depth * 0.5

	_steel_mat = StandardMaterial3D.new()
	_steel_mat.albedo_color = Color(0.22, 0.26, 0.31)
	_steel_mat.metallic = 0.68
	_steel_mat.roughness = 0.36

	_hazard_mat = StandardMaterial3D.new()
	_hazard_mat.albedo_color = Color(0.95, 0.76, 0.14)
	_hazard_mat.roughness = 0.42

	_glass_mat = StandardMaterial3D.new()
	_glass_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_glass_mat.albedo_color = Color(0.48, 0.78, 0.92, 0.32)
	_glass_mat.metallic = 0.15
	_glass_mat.roughness = 0.12

	var col_height := get_floor_y(max_floors) + 4.5
	_shaft_posts.clear()
	for sx in [-1.0, 1.0]:
		for sz in [-1.0, 1.0]:
			var post := MeshInstance3D.new()
			var p_box := BoxMesh.new()
			p_box.size = Vector3(0.22, col_height, 0.22)
			post.mesh = p_box
			post.position = Vector3(sx * (half_w + 0.08), col_height * 0.5, sz * (half_d + 0.08))
			post.material_override = _steel_mat
			add_child(post)
			_shaft_posts.append(post)

	_shaft_top_beams.clear()
	for sz in [-1.0, 1.0]:
		var beam := MeshInstance3D.new()
		var b_box := BoxMesh.new()
		b_box.size = Vector3(cab_width + 0.4, 0.28, 0.26)
		beam.mesh = b_box
		beam.position = Vector3(0.0, col_height, sz * (half_d + 0.08))
		beam.material_override = _steel_mat
		add_child(beam)
		_shaft_top_beams.append(beam)

	# Floor 1 South Entry Threshold Strip (Y = 0.008m, Z = +half_d)
	var f1_strip := MeshInstance3D.new()
	var f1_box := BoxMesh.new()
	f1_box.size = Vector3(cab_width, 0.016, 0.32)
	f1_strip.mesh = f1_box
	f1_strip.position = Vector3(0.0, 0.008, half_d + 0.16)
	f1_strip.material_override = _hazard_mat
	add_child(f1_strip)

	_build_call_pedestal(Vector3(-half_w - 0.65, floor_1_y, half_d + 0.35), 1)

	# Build Stops, Safety Gates & Call Pedestals for all built-in Upper Floors (Floors 2..max_floors)
	for f in range(2, max_floors + 1):
		_add_upper_floor_stop(f)

	# Moving Freight Elevator Cab (`cab_root`)
	cab_root = Node3D.new()
	cab_root.name = "ElevatorCabRoot"
	cab_root.position = Vector3(0.0, current_cab_y, 0.0)
	add_child(cab_root)

	cab_floor_body = StaticBody3D.new()
	cab_floor_body.name = "ElevatorCabFloor"
	cab_root.add_child(cab_floor_body)

	var deck_col := CollisionShape3D.new()
	var deck_shape := BoxShape3D.new()
	deck_shape.size = Vector3(cab_width, 0.32, cab_depth + 0.24)
	deck_col.shape = deck_shape
	deck_col.position = Vector3(0.0, -0.16, 0.0)
	cab_floor_body.add_child(deck_col)

	var deck_mi := MeshInstance3D.new()
	var deck_bm := BoxMesh.new()
	deck_bm.size = Vector3(cab_width, 0.30, cab_depth)
	deck_mi.mesh = deck_bm
	deck_mi.position = Vector3(0.0, -0.15, 0.0)
	var deck_mat := StandardMaterial3D.new()
	deck_mat.albedo_color = Color(0.28, 0.31, 0.35)
	deck_mat.metallic = 0.62
	deck_mat.roughness = 0.42
	deck_mi.material_override = deck_mat
	cab_floor_body.add_child(deck_mi)

	# Left (-X) & Right (+X) Cab Walls
	for sx in [-1.0, 1.0]:
		var wall_sb := StaticBody3D.new()
		wall_sb.position = Vector3(sx * (half_w - 0.07), 0.0, 0.0)
		cab_root.add_child(wall_sb)

		var w_col := CollisionShape3D.new()
		var w_shape := BoxShape3D.new()
		w_shape.size = Vector3(0.14, cab_height, cab_depth)
		w_col.shape = w_shape
		w_col.position = Vector3(0.0, cab_height * 0.5, 0.0)
		wall_sb.add_child(w_col)

		var low_mi := MeshInstance3D.new()
		var low_bm := BoxMesh.new()
		low_bm.size = Vector3(0.14, 1.15, cab_depth)
		low_mi.mesh = low_bm
		low_mi.position = Vector3(0.0, 0.575, 0.0)
		low_mi.material_override = _steel_mat
		wall_sb.add_child(low_mi)

		var glass_mi := MeshInstance3D.new()
		var glass_bm := BoxMesh.new()
		glass_bm.size = Vector3(0.06, cab_height - 1.35, cab_depth - 0.3)
		glass_mi.mesh = glass_bm
		glass_mi.position = Vector3(0.0, 1.15 + (cab_height - 1.35) * 0.5, 0.0)
		glass_mi.material_override = _glass_mat
		wall_sb.add_child(glass_mi)

	var roof_mi := MeshInstance3D.new()
	var roof_bm := BoxMesh.new()
	roof_bm.size = Vector3(cab_width, 0.16, cab_depth)
	roof_mi.mesh = roof_bm
	roof_mi.position = Vector3(0.0, cab_height + 0.08, 0.0)
	roof_mi.material_override = _steel_mat
	cab_root.add_child(roof_mi)

	# 1. LEFT PHYSICAL BUTTON PEDESTAL (-X = -1.85m): Dedicated [▲ UP BUTTON]
	var up_ped_mi := MeshInstance3D.new()
	var up_ped_bm := BoxMesh.new()
	up_ped_bm.size = Vector3(0.36, 1.15, 0.42)
	up_ped_mi.mesh = up_ped_bm
	up_ped_mi.position = Vector3(-half_w + 0.45, 0.575, 0.0)
	up_ped_mi.material_override = _steel_mat
	cab_root.add_child(up_ped_mi)

	var up_btn_cap := MeshInstance3D.new()
	var up_btn_cyl := CylinderMesh.new()
	up_btn_cyl.top_radius = 0.14
	up_btn_cyl.bottom_radius = 0.16
	up_btn_cyl.height = 0.10
	up_btn_cap.mesh = up_btn_cyl
	up_btn_cap.position = Vector3(-half_w + 0.45, 1.20, 0.0)
	var console_mat := StandardMaterial3D.new()
	console_mat.albedo_color = Color(0.15, 0.88, 0.42)
	console_mat.emission_enabled = true
	console_mat.emission = Color(0.18, 0.95, 0.48)
	console_mat.emission_energy_multiplier = 1.8
	up_btn_cap.material_override = console_mat
	cab_root.add_child(up_btn_cap)

	cab_control_label = Label3D.new()
	cab_control_label.position = Vector3(-half_w + 0.50, 1.72, 0.0)
	cab_control_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	cab_control_label.font_size = 20
	cab_control_label.outline_size = 6
	cab_control_label.modulate = Color(0.42, 1.0, 0.68)
	cab_root.add_child(cab_control_label)

	# 2. RIGHT PHYSICAL BUTTON PEDESTAL (+X = +1.85m, spaced 3.70m apart!): Dedicated [▼ DOWN BUTTON]
	var down_ped_mi := MeshInstance3D.new()
	var down_ped_bm := BoxMesh.new()
	down_ped_bm.size = Vector3(0.36, 1.15, 0.42)
	down_ped_mi.mesh = down_ped_bm
	down_ped_mi.position = Vector3(half_w - 0.45, 0.575, 0.0)
	down_ped_mi.material_override = _steel_mat
	cab_root.add_child(down_ped_mi)

	var down_btn_cap := MeshInstance3D.new()
	var down_btn_cyl := CylinderMesh.new()
	down_btn_cyl.top_radius = 0.14
	down_btn_cyl.bottom_radius = 0.16
	down_btn_cyl.height = 0.10
	down_btn_cap.mesh = down_btn_cyl
	down_btn_cap.position = Vector3(half_w - 0.45, 1.20, 0.0)
	var desc_console_mat := StandardMaterial3D.new()
	desc_console_mat.albedo_color = Color(0.95, 0.55, 0.16)
	desc_console_mat.emission_enabled = true
	desc_console_mat.emission = Color(1.0, 0.62, 0.18)
	desc_console_mat.emission_energy_multiplier = 1.8
	down_btn_cap.material_override = desc_console_mat
	cab_root.add_child(down_btn_cap)

	cab_descend_label = Label3D.new()
	cab_descend_label.position = Vector3(half_w - 0.50, 1.72, 0.0)
	cab_descend_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	cab_descend_label.font_size = 20
	cab_descend_label.outline_size = 6
	cab_descend_label.modulate = Color(1.0, 0.82, 0.42)
	cab_root.add_child(cab_descend_label)

	south_gate_body = StaticBody3D.new()
	south_gate_body.position = Vector3(0.0, 0.55, half_d - 0.06)
	cab_root.add_child(south_gate_body)
	var sg_col := CollisionShape3D.new()
	var sg_shape := BoxShape3D.new()
	sg_shape.size = Vector3(cab_width - 0.28, 1.1, 0.10)
	sg_col.shape = sg_shape
	south_gate_body.add_child(sg_col)
	south_gate_mesh = MeshInstance3D.new()
	var sg_bm := BoxMesh.new()
	sg_bm.size = Vector3(cab_width - 0.32, 0.95, 0.07)
	south_gate_mesh.mesh = sg_bm
	south_gate_mesh.material_override = _hazard_mat
	south_gate_body.add_child(south_gate_mesh)

	north_gate_body = StaticBody3D.new()
	north_gate_body.position = Vector3(0.0, 0.55, -half_d + 0.06)
	cab_root.add_child(north_gate_body)
	var ng_col := CollisionShape3D.new()
	var ng_shape := BoxShape3D.new()
	ng_shape.size = Vector3(cab_width - 0.28, 1.1, 0.10)
	ng_col.shape = ng_shape
	north_gate_body.add_child(ng_col)
	north_gate_mesh = MeshInstance3D.new()
	var ng_bm := BoxMesh.new()
	ng_bm.size = Vector3(cab_width - 0.32, 0.95, 0.07)
	north_gate_mesh.mesh = ng_bm
	north_gate_mesh.material_override = _hazard_mat
	north_gate_body.add_child(north_gate_mesh)

	cab_light = OmniLight3D.new()
	cab_light.position = Vector3(0.0, cab_height - 0.25, 0.0)
	cab_light.light_color = Color(0.72, 0.95, 1.0)
	cab_light.light_energy = 2.4
	cab_light.omni_range = 7.5
	cab_root.add_child(cab_light)

	motor_audio = AudioStreamPlayer3D.new()
	motor_audio.bus = &"SFX"
	motor_audio.max_distance = 22.0
	motor_audio.unit_size = 4.0
	var click_path := "res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_On.wav"
	if ResourceLoader.exists(click_path):
		motor_audio.stream = load(click_path)
	cab_root.add_child(motor_audio)

func _add_upper_floor_stop(floor_num: int) -> void:
	if shaft_gate_bodies.has(floor_num):
		return
	var half_w := cab_width * 0.5
	var half_d := cab_depth * 0.5
	var fy := get_floor_y(floor_num)

	# Threshold Strip
	var strip := MeshInstance3D.new()
	var s_box := BoxMesh.new()
	s_box.size = Vector3(cab_width, 0.016, 0.32)
	strip.mesh = s_box
	strip.position = Vector3(0.0, fy + 0.008, -half_d - 0.16)
	strip.material_override = _hazard_mat
	add_child(strip)

	# Shaft Safety Gate (blocks open shaft when cab is not docked at floor_num)
	var gate_body := StaticBody3D.new()
	gate_body.position = Vector3(0.0, fy + 0.55, -half_d - 0.08)
	add_child(gate_body)
	var gate_col := CollisionShape3D.new()
	var gate_shape := BoxShape3D.new()
	gate_shape.size = Vector3(cab_width, 1.18, 0.10)
	gate_col.shape = gate_shape
	gate_body.add_child(gate_col)
	var gate_mesh := MeshInstance3D.new()
	var gate_bm := BoxMesh.new()
	gate_bm.size = Vector3(cab_width - 0.1, 1.12, 0.08)
	gate_mesh.mesh = gate_bm
	gate_mesh.material_override = _hazard_mat
	gate_body.add_child(gate_mesh)

	shaft_gate_bodies[floor_num] = gate_body
	shaft_gate_meshes[floor_num] = gate_mesh
	if floor_num == 2:
		f2_shaft_gate_body = gate_body
		f2_shaft_gate_mesh = gate_mesh
	elif floor_num == 3:
		f3_shaft_gate_body = gate_body
		f3_shaft_gate_mesh = gate_mesh

	_build_call_pedestal(Vector3(-half_w - 0.65, fy, -half_d - 0.35), floor_num)

func extend_to_floor(new_max_floor: int) -> void:
	if new_max_floor <= max_floors:
		return
	for f in range(max_floors + 1, new_max_floor + 1):
		_add_upper_floor_stop(f)
		if not Engine.is_editor_hint():
			GameState.freight_elevator_unlocked_floors[f] = true
	max_floors = new_max_floor

	var col_height := get_floor_y(max_floors) + 4.5
	for post in _shaft_posts:
		if is_instance_valid(post) and post.mesh is BoxMesh:
			(post.mesh as BoxMesh).size.y = col_height
			post.position.y = col_height * 0.5
	for beam in _shaft_top_beams:
		if is_instance_valid(beam):
			beam.position.y = col_height
	_update_gate_states_and_labels()

func _build_call_pedestal(local_pos: Vector3, floor_num: int) -> void:
	var ped := MeshInstance3D.new()
	var p_bm := BoxMesh.new()
	p_bm.size = Vector3(0.36, 1.35, 0.36)
	ped.mesh = p_bm
	ped.position = local_pos + Vector3(0.0, 0.675, 0.0)
	var p_mat := StandardMaterial3D.new()
	p_mat.albedo_color = Color(0.20, 0.48, 0.42)
	p_mat.roughness = 0.38
	ped.material_override = p_mat
	add_child(ped)

	var lbl := Label3D.new()
	lbl.position = local_pos + Vector3(0.0, 1.85, 0.0)
	lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	lbl.font_size = 21
	lbl.outline_size = 6
	lbl.modulate = Color(0.45, 1.0, 0.80)
	lbl.visibility_range_end = 18.0
	lbl.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
	add_child(lbl)
	call_labels[floor_num] = lbl
	if floor_num == 1:
		f1_call_label = lbl
	elif floor_num == 2:
		f2_call_label = lbl
	elif floor_num == 3:
		f3_call_label = lbl

func is_point_inside_cab_footprint(world_pt: Vector3, extra_margin: float = 0.25) -> bool:
	var local_pt := to_local(world_pt)
	var hw := cab_width * 0.5 + extra_margin
	var hd := cab_depth * 0.5 + extra_margin
	return absf(local_pt.x) <= hw and absf(local_pt.z) <= hd

func is_point_inside_cab_3d(world_pt: Vector3, extra_margin: float = 0.25) -> bool:
	if not is_point_inside_cab_footprint(world_pt, extra_margin):
		return false
	var dy := world_pt.y - (global_position.y + current_cab_y)
	return dy >= -0.85 and dy <= (cab_height + 0.6)

func is_player_near_any_elevator_control(world_pt: Vector3) -> bool:
	if is_point_inside_cab_3d(world_pt, 0.45):
		return true
	var f1_ped := to_global(Vector3(-cab_width * 0.5 - 0.65, floor_1_y, cab_depth * 0.5 + 0.35))
	if world_pt.distance_to(f1_ped) <= 3.8:
		return true
	for f in range(2, max_floors + 1):
		var ped := to_global(Vector3(-cab_width * 0.5 - 0.65, get_floor_y(f), -cab_depth * 0.5 - 0.35))
		if world_pt.distance_to(ped) <= 3.8:
			return true
	return false

## Determines whether the player is aiming at or standing closer to the Left [▲ UP BUTTON] (-X)
## or the Right [▼ DOWN BUTTON] (+X) inside the Elevator Cab.
func _did_player_press_up_button(p: Node3D) -> bool:
	if p == null or cab_root == null:
		return true
	var up_btn_world := cab_root.to_global(Vector3(-cab_width * 0.5 + 0.45, 1.20, 0.0))
	var down_btn_world := cab_root.to_global(Vector3(cab_width * 0.5 - 0.45, 1.20, 0.0))
	var dist_up := Vector2(p.global_position.x - up_btn_world.x, p.global_position.z - up_btn_world.z).length()
	var dist_down := Vector2(p.global_position.x - down_btn_world.x, p.global_position.z - down_btn_world.z).length()

	# 1. If the player walked over to one of the two spaced-out physical button pedestals, press that button!
	if absf(dist_up - dist_down) >= 0.45:
		return dist_up < dist_down

	# 2. If standing in the middle of the cab between the two buttons, press whichever physical button the camera is looking at!
	var cam := get_viewport().get_camera_3d() if is_inside_tree() and get_viewport() != null else null
	if cam != null and p.global_position.distance_to(cam.global_position) < 4.5:
		var cam_pos := cam.global_position
		var cam_fwd := -cam.global_transform.basis.z.normalized()
		var dir_up := (up_btn_world - cam_pos).normalized()
		var dir_down := (down_btn_world - cam_pos).normalized()
		return cam_fwd.dot(dir_up) >= cam_fwd.dot(dir_down)

	return dist_up <= dist_down

## Called when a player presses [F] on either the Left [▲ UP BUTTON], the Right [▼ DOWN BUTTON], or a Floor Call Pedestal.
func activate_elevator_for_player(p: Node3D, force_up_override: Variant = null) -> void:
	if is_moving:
		var dest_f := get_floor_num_from_y(target_cab_y)
		GameState.status_message.emit(
			"Service Freight Elevator is currently in transit to Floor %d (Y = %.1fm)..." % [dest_f, current_cab_y],
			Color(0.45, 0.95, 1.0)
		)
		return

	var player_floor := 1
	if p != null:
		player_floor = clampi(get_floor_num_from_y(p.global_position.y), 1, max_floors + 1)

	# If player is on a floor and the cab is on a DIFFERENT floor (and player is not inside cab), call cab to player's floor!
	if p != null and not is_point_inside_cab_3d(p.global_position, 0.45) and player_floor != current_floor_num:
		send_to_floor(player_floor, "Called Service Freight Elevator to Floor %d" % player_floor)
		return

	var wants_up := true
	if force_up_override != null:
		wants_up = bool(force_up_override)
	else:
		wants_up = _did_player_press_up_button(p)

	if wants_up:
		var next_floor := current_floor_num + 1
		if next_floor > max_floors:
			procedural_floor_requested.emit(next_floor)
			extend_to_floor(next_floor)
		send_to_floor(next_floor, "Pressed [▲ UP BUTTON] -> Ascending to Floor %d (Y = %.1fm)" % [next_floor, get_floor_y(next_floor)])
	else:
		if current_floor_num <= 1:
			GameState.status_message.emit(
				"Already at Ground Floor 1 (Y = 0.0m)! Step over to the Left Green [▲ UP BUTTON] and press [E] to ascend!",
				Color(1.0, 0.82, 0.42)
			)
			return
		var prev_floor := current_floor_num - 1
		send_to_floor(prev_floor, "Pressed [▼ DOWN BUTTON] -> Descending to Floor %d (Y = %.1fm)" % [prev_floor, get_floor_y(prev_floor)])

func _unhandled_key_input(event: InputEvent) -> void:
	if Engine.is_editor_hint() or not is_inside_tree():
		return
	if not (event is InputEventKey and event.pressed and not event.echo):
		return
	var local_player: Node3D = null
	for p_node in get_tree().get_nodes_in_group("players"):
		if p_node is Node3D and is_point_inside_cab_3d((p_node as Node3D).global_position, 0.45):
			local_player = p_node as Node3D
			break
	if local_player == null:
		return
	if "voice_menu_open" in local_player and bool(local_player.get("voice_menu_open")):
		return

	var kc: int = (event as InputEventKey).physical_keycode if (event as InputEventKey).physical_keycode != 0 else (event as InputEventKey).keycode
	if kc == KEY_PAGEUP or kc == KEY_UP or kc == KEY_EQUAL:
		activate_elevator_for_player(local_player, true)
		get_viewport().set_input_as_handled()
	elif kc == KEY_PAGEDOWN or kc == KEY_DOWN or kc == KEY_MINUS:
		activate_elevator_for_player(local_player, false)
		get_viewport().set_input_as_handled()
	elif kc == KEY_BACKSPACE or kc == KEY_HOME or kc == KEY_0:
		send_to_floor(1, "Express Return to Floor 1 (Ground Hub)")
		get_viewport().set_input_as_handled()
	elif kc >= KEY_5 and kc <= KEY_9:
		var target_fl := (kc - KEY_0)
		send_to_floor(target_fl, "Express Elevator Jump to Floor %d" % target_fl)
		get_viewport().set_input_as_handled()

func send_to_floor(target_floor: int, reason_msg: String = "") -> void:
	if target_floor > max_floors:
		procedural_floor_requested.emit(target_floor)
		extend_to_floor(target_floor)
	target_floor = clampi(target_floor, 1, max_floors)
	target_cab_y = get_floor_y(target_floor)

	if absf(target_cab_y - current_cab_y) <= 0.02:
		current_floor_num = target_floor
		is_moving = false
		_update_gate_states_and_labels()
		return

	is_moving = true
	if motor_audio and motor_audio.stream:
		motor_audio.pitch_scale = clampf(0.68 + float(target_floor - 1) * 0.08, 0.68, 1.35)
		motor_audio.play()
	_update_gate_states_and_labels()

	var carts_aboard := _get_carts_aboard_names()
	var cart_suffix := (" with [%s]" % ", ".join(carts_aboard)) if not carts_aboard.is_empty() else " (Roll carts onto deck to ride together!)"
	GameState.status_message.emit(
		"%s%s!" % [reason_msg if not reason_msg.is_empty() else ("Service Freight Elevator -> Floor %d" % target_floor), cart_suffix],
		Color(0.38, 1.0, 0.78)
	)

func install_hydraulic_overdrive() -> void:
	is_overdrive_repaired = true
	elevator_speed = 4.2
	for f in range(2, max_floors + 1):
		GameState.repair_freight_elevator(f)
	_update_gate_states_and_labels()
	GameState.status_message.emit(
		"INSTALLED HEAVY HYDRAULIC ASSEMBLY! Freight Elevator High-Speed Hoist Overdrive Engaged (4.2 m/s — Floors 1..%d + Procedural Endless Floors Unlocked)!" % max_floors,
		Color(0.35, 1.0, 0.72)
	)

func _get_carts_aboard_names() -> Array[String]:
	var names: Array[String] = []
	if not is_inside_tree():
		return names
	for c_node in get_tree().get_nodes_in_group("mall_carts"):
		var c := c_node as MallCartSystem
		if c and _is_cart_aboard(c):
			names.append(c.cart_type)
	return names

func _is_cart_aboard(c: MallCartSystem) -> bool:
	if is_point_inside_cab_3d(c.global_position, 0.45):
		return true
	if c.is_attached_to_player and is_instance_valid(c.attached_player) and is_point_inside_cab_3d(c.attached_player.global_position, 0.45):
		return true
	return false

func _set_gate_closed(gate_body: StaticBody3D, gate_mesh: MeshInstance3D, closed: bool) -> void:
	if gate_body == null or gate_mesh == null:
		return
	gate_mesh.visible = closed
	gate_body.collision_layer = 1 if closed else 0
	gate_body.collision_mask = 1 if closed else 0

func _update_gate_states_and_labels() -> void:
	var at_f1 := (not is_moving and absf(current_cab_y - floor_1_y) <= 0.05)
	var at_upper := (not is_moving and current_cab_y >= floor_2_y - 0.05)

	_set_gate_closed(south_gate_body, south_gate_mesh, not at_f1)
	_set_gate_closed(north_gate_body, north_gate_mesh, not at_upper)

	for f in shaft_gate_bodies.keys():
		var f_int: int = int(f)
		var fy := get_floor_y(f_int)
		var docked_here := (not is_moving and absf(current_cab_y - fy) <= 0.05)
		_set_gate_closed(shaft_gate_bodies[f_int], shaft_gate_meshes[f_int], not docked_here)

	var status_str := "MOVING (Y = %.1fm)" % current_cab_y if is_moving else ("FLOOR %d/%d+ (Y = %.1fm)" % [current_floor_num, max_floors, current_cab_y])
	var up_fl := current_floor_num + 1
	var up_tag := ("FLOOR %d" % up_fl) if up_fl <= max_floors else ("PROCEDURAL FLOOR %d" % up_fl)
	if cab_control_label:
		cab_control_label.text = (
			"[E] ▲ UP BUTTON [%s]\n" % status_str +
			"Ascend +1 Floor -> %s (%.1fm)" % [up_tag, get_floor_y(up_fl)]
		)
	if cab_descend_label:
		if current_floor_num <= 1:
			cab_descend_label.text = (
				"[E] ▼ DOWN BUTTON [%s]\n" % status_str +
				"(At Ground Floor 1 — Use Left ▲ UP Button to Ascend)"
			)
		else:
			cab_descend_label.text = (
				"[E] ▼ DOWN BUTTON [%s]\n" % status_str +
				"Descend -1 Floor -> FLOOR %d (%.1fm)" % [current_floor_num - 1, get_floor_y(current_floor_num - 1)]
			)

	for f in call_labels.keys():
		var f_int: int = int(f)
		var lbl: Label3D = call_labels[f_int]
		if not is_instance_valid(lbl):
			continue
		var fy := get_floor_y(f_int)
		var docked_here := (not is_moving and absf(current_cab_y - fy) <= 0.05)
		lbl.text = (
			"FLOOR %d ELEVATOR CALL BUTTON [%s]\n" % [f_int, status_str] +
			"%s" % (
				"Cab Docked Here — Step Inside & Press [E] on Left [▲ UP] or Right [▼ DOWN] Button!"
				if docked_here
				else ("[E] Call Elevator Cab to Floor %d" % f_int)
			)
		)

func _physics_process(delta: float) -> void:
	if Engine.is_editor_hint() or not is_moving:
		return

	# Identify riders BEFORE advancing `current_cab_y` so high-speed descent never leaves players/carts behind!
	var riding_players: Array[CharacterBody3D] = []
	for p_node in get_tree().get_nodes_in_group("players"):
		var p := p_node as CharacterBody3D
		if p and is_point_inside_cab_3d(p.global_position, 0.50):
			riding_players.append(p)

	var riding_carts: Array[MallCartSystem] = []
	for c_node in get_tree().get_nodes_in_group("mall_carts"):
		var c := c_node as MallCartSystem
		if c and _is_cart_aboard(c):
			riding_carts.append(c)

	var riding_items: Array[Node3D] = []
	for group_name in ["ectoplasm_canisters", "world_dropped_items", "death_inventory_crates"]:
		for item_node in get_tree().get_nodes_in_group(group_name):
			var n3d := item_node as Node3D
			if n3d and is_point_inside_cab_3d(n3d.global_position, 0.40):
				riding_items.append(n3d)

	var next_y := move_toward(current_cab_y, target_cab_y, elevator_speed * delta)
	var dy := next_y - current_cab_y
	current_cab_y = next_y
	if cab_root:
		cab_root.position.y = current_cab_y

	if absf(dy) > 0.00001:
		var fl := get_floor_num_from_y(current_cab_y)
		for p in riding_players:
			if is_instance_valid(p):
				p.global_position.y = maxf(global_position.y + current_cab_y + 0.02, p.global_position.y + dy)
				if p.velocity.y < 0.0:
					p.velocity.y = 0.0
				GameState.current_floor = fl
				GameState.highest_floor_reached = maxi(GameState.highest_floor_reached, fl)

		for c in riding_carts:
			if is_instance_valid(c):
				c.global_position.y = global_position.y + current_cab_y

		for n3d in riding_items:
			if is_instance_valid(n3d):
				n3d.global_position.y += dy

	if absf(current_cab_y - target_cab_y) <= 0.005:
		current_cab_y = target_cab_y
		if cab_root:
			cab_root.position.y = current_cab_y
		is_moving = false
		current_floor_num = get_floor_num_from_y(current_cab_y)
		GameState.highest_floor_reached = maxi(GameState.highest_floor_reached, current_floor_num)
		if motor_audio and motor_audio.stream:
			motor_audio.pitch_scale = 1.15
			motor_audio.play()
		_update_gate_states_and_labels()
		var carts_aboard := _get_carts_aboard_names()
		var cart_msg := (" with %s" % ", ".join(carts_aboard)) if not carts_aboard.is_empty() else ""
		GameState.status_message.emit(
			"Service Freight Elevator arrived at FLOOR %d (Y = %.1fm)%s! Safety gate opened." % [
				current_floor_num, current_cab_y, cart_msg
			],
			Color(0.45, 1.0, 0.78)
		)
		floor_arrived.emit(current_floor_num, current_cab_y)
	else:
		_update_gate_states_and_labels()
