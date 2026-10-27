@tool
class_name MallEscalator
extends Node3D
## Megamall Anomaly — Full Godot 4 Port of AMallEscalator (UnrealSourceReference/MyProject2/MallEscalator.h/.cpp).
## Features:
## - Exact 8.50m floor-to-floor geometry (SM_MallEscalatorShell_850.fbx + 49 animated SM_MallEscalatorStep.fbx treads)
## - Watertight bevelled StaticBody3D walking deck (Lower Landing -> Smooth Fillet -> 30° Incline -> Smooth Fillet -> Upper Landing)
## - Solid Left/Right Balustrade SideGuards so players never fall through the sides
## - Automatic belt rider transport (uphill or downhill) + walkable stairs when stalled/unpowered
## - Stage 6A Cart Blocker: automatically detaches pushed carts at the escalator entrance.

enum MaintenanceState {
	OPERATIONAL,
	STALLED_WALKABLE,
	BROKEN_BLOCKED
}

@export var escalator_title: String = "Concourse Escalator (F1 <-> F2)"
@export var circuit_tag: String = "EscalatorCircuit_F1_To_F2"
@export var run_m: float = 14.722432
@export var rise_m: float = 8.50
@export var landing_length_m: float = 1.30
@export var belt_width_m: float = 1.06
@export var step_spacing_m: float = 0.40
@export var stationary_end_m: float = 0.45
@export var travel_speed_mps: float = 2.15
@export var running: bool = true
@export var reverse_direction: bool = false
@export var maintenance_state: MaintenanceState = MaintenanceState.OPERATIONAL

var belt_distance: float = 0.0
var tread_count: int = 0
var actual_tread_spacing: float = 0.40
var _step_nodes: Array[Node3D] = []
var _status_beacon: OmniLight3D = null
var _label_3d: Label3D = null
var _caution_bar_root: Node3D = null

func _ready() -> void:
	add_to_group("mall_escalators")
	_rebuild_escalator()

func incline_length() -> float:
	return sqrt(run_m * run_m + rise_m * rise_m)

func get_travel_length() -> float:
	return 2.0 * landing_length_m + incline_length()

func total_run() -> float:
	return 2.0 * landing_length_m + run_m

## Local coordinates: +X is horizontal uphill travel, +Y is UP, Z is cross-tread width.
func deck_local_point(dist: float) -> Vector3:
	var slope_len := incline_length()
	var total_len := get_travel_length()
	var d := clampf(dist, 0.0, total_len)
	if d <= landing_length_m:
		return Vector3(d, 0.0, 0.0)
	if d < landing_length_m + slope_len:
		var frac := (d - landing_length_m) / slope_len
		return Vector3(landing_length_m + run_m * frac, rise_m * frac, 0.0)
	var upper_d := d - landing_length_m - slope_len
	return Vector3(landing_length_m + run_m + upper_d, rise_m, 0.0)

func distance_at_local_x(lx: float) -> float:
	if lx <= landing_length_m:
		return maxf(0.0, lx)
	if lx < landing_length_m + run_m:
		return landing_length_m + (lx - landing_length_m) * incline_length() / run_m
	return minf(get_travel_length(), landing_length_m + incline_length() + (lx - landing_length_m - run_m))

func eval_extended_deck_local_point(dist: float) -> Vector3:
	var total_len := get_travel_length()
	if dist < 0.0:
		return Vector3(dist, 0.0, 0.0)
	if dist > total_len:
		return Vector3(total_run() + (dist - total_len), rise_m, 0.0)
	return deck_local_point(dist)

func _rebuild_escalator() -> void:
	for c in get_children():
		c.queue_free()
	_step_nodes.clear()

	# 1. Visual Shell (SM_MallEscalatorShell_850.fbx) with PBR Materials
	var shell_path := "res://ExternalAssets/Mall/Escalator850/SM_MallEscalatorShell_850.fbx" if rise_m > 5.0 else "res://ExternalAssets/Escalator/SM_MallEscalatorShell.fbx"
	if ResourceLoader.exists(shell_path):
		var shell_scene := load(shell_path) as PackedScene
		if shell_scene:
			var shell_inst := shell_scene.instantiate() as Node3D
			_apply_shell_materials_recursive(shell_inst)
			add_child(shell_inst)

	# 2. Moving Step Treads (SM_MallEscalatorStep.fbx)
	var total_len := get_travel_length()
	tread_count = clampi(ceili(total_len / step_spacing_m), 8, 128)
	actual_tread_spacing = total_len / float(tread_count)

	var step_path := "res://ExternalAssets/Escalator/SM_MallEscalatorStep.fbx"
	var step_scene: PackedScene = load(step_path) as PackedScene if ResourceLoader.exists(step_path) else null
	var step_root := Node3D.new()
	step_root.name = "StepsPool"
	add_child(step_root)

	for i in range(tread_count):
		var step_node: Node3D = null
		if step_scene:
			step_node = step_scene.instantiate() as Node3D
			_apply_step_materials_recursive(step_node)
		else:
			step_node = Node3D.new()
			var mi := MeshInstance3D.new()
			var bm := BoxMesh.new()
			bm.size = Vector3(0.39, 0.22, belt_width_m)
			mi.mesh = bm
			mi.position = Vector3(0.0, -0.11, 0.0)
			step_node.add_child(mi)
		step_root.add_child(step_node)
		_step_nodes.append(step_node)

	# 3. Watertight Bevelled StaticBody3D Walking Deck + Side Balustrade Guards
	_build_deck_and_balustrade_collision()

	# 4. Caution Barricade & Service Beacon Light
	_caution_bar_root = Node3D.new()
	add_child(_caution_bar_root)
	var bar_mat := StandardMaterial3D.new()
	bar_mat.albedo_color = Color(1.0, 0.72, 0.08)
	bar_mat.emission_enabled = true
	bar_mat.emission = Color(1.0, 0.55, 0.05)
	bar_mat.emission_energy_multiplier = 1.4
	for h in [0.52, 0.84]:
		var bar_mi := MeshInstance3D.new()
		var bar_box := BoxMesh.new()
		bar_box.size = Vector3(0.12, 0.10, belt_width_m + 0.08)
		bar_mi.mesh = bar_box
		bar_mi.position = Vector3(0.75, h, 0.0)
		bar_mi.material_override = bar_mat
		_caution_bar_root.add_child(bar_mi)

	_status_beacon = OmniLight3D.new()
	_status_beacon.position = Vector3(0.55, 1.15, belt_width_m * 0.5 + 0.24)
	_status_beacon.omni_range = 3.5
	add_child(_status_beacon)

	_label_3d = Label3D.new()
	_label_3d.position = Vector3(0.55, 1.95, 0.0)
	_label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_label_3d.font_size = 22
	_label_3d.outline_size = 6
	add_child(_label_3d)

	_refresh_maintenance_visuals()
	_update_treads()

func _build_deck_and_balustrade_collision() -> void:
	var sb := StaticBody3D.new()
	sb.name = "EscalatorDeckCollision"
	add_child(sb)

	var deck_thick := 0.24
	var slope_len := incline_length()
	var pitch_rad := atan2(rise_m, run_m)
	var slope_normal := Vector3(-sin(pitch_rad), cos(pitch_rad), 0.0)

	# 1. Lower Flat Landing Deck (X = 0 .. landing_length_m, top surface at Y = 0.0)
	var low_col := CollisionShape3D.new()
	var low_box := BoxShape3D.new()
	low_box.size = Vector3(landing_length_m, deck_thick, belt_width_m)
	low_col.shape = low_box
	low_col.position = Vector3(landing_length_m * 0.5, -deck_thick * 0.5, 0.0)
	sb.add_child(low_col)

	# 2. Inclined 30° Walking Deck (X = landing_length_m .. landing_length_m + run_m, Y = 0 .. rise_m)
	var inc_col := CollisionShape3D.new()
	var inc_box := BoxShape3D.new()
	inc_box.size = Vector3(slope_len + 0.12, deck_thick, belt_width_m)
	inc_col.shape = inc_box
	inc_col.position = Vector3(landing_length_m + run_m * 0.5, rise_m * 0.5, 0.0) - slope_normal * (deck_thick * 0.5)
	inc_col.rotation = Vector3(0.0, 0.0, pitch_rad)
	sb.add_child(inc_col)

	# 3. Upper Flat Landing Deck (X = landing_length_m + run_m .. total_run(), top surface at Y = rise_m)
	var up_col := CollisionShape3D.new()
	var up_box := BoxShape3D.new()
	up_box.size = Vector3(landing_length_m + 0.20, deck_thick, belt_width_m)
	up_col.shape = up_box
	up_col.position = Vector3(landing_length_m + run_m + landing_length_m * 0.5, rise_m - deck_thick * 0.5, 0.0)
	sb.add_child(up_col)

	# 4. Left & Right Solid Balustrade Side-Guard Colliders
	var guard_h := 1.08
	var guard_thick := 0.14
	for side_sign in [-1.0, 1.0]:
		var side_z: float = side_sign * (belt_width_m * 0.5 + guard_thick * 0.5)

		var g_low := CollisionShape3D.new()
		var g_low_box := BoxShape3D.new()
		g_low_box.size = Vector3(landing_length_m, guard_h, guard_thick)
		g_low.shape = g_low_box
		g_low.position = Vector3(landing_length_m * 0.5, guard_h * 0.5, side_z)
		sb.add_child(g_low)

		var g_inc := CollisionShape3D.new()
		var g_inc_box := BoxShape3D.new()
		g_inc_box.size = Vector3(slope_len, guard_h, guard_thick)
		g_inc.shape = g_inc_box
		g_inc.position = Vector3(landing_length_m + run_m * 0.5, rise_m * 0.5, side_z) + slope_normal * (guard_h * 0.5)
		g_inc.rotation = Vector3(0.0, 0.0, pitch_rad)
		sb.add_child(g_inc)

		var g_up := CollisionShape3D.new()
		var g_up_box := BoxShape3D.new()
		g_up_box.size = Vector3(landing_length_m, guard_h, guard_thick)
		g_up.shape = g_up_box
		g_up.position = Vector3(landing_length_m + run_m + landing_length_m * 0.5, rise_m + guard_h * 0.5, side_z)
		sb.add_child(g_up)

func _apply_shell_materials_recursive(node: Node) -> void:
	if node is MeshInstance3D:
		var mi := node as MeshInstance3D
		var mat_metal := StandardMaterial3D.new()
		mat_metal.albedo_color = Color(0.68, 0.72, 0.76)
		mat_metal.metallic = 0.75
		mat_metal.roughness = 0.28

		var mat_glass := StandardMaterial3D.new()
		mat_glass.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		mat_glass.albedo_color = Color(0.55, 0.82, 0.95, 0.35)
		mat_glass.roughness = 0.1

		var mat_dark := StandardMaterial3D.new()
		mat_dark.albedo_color = Color(0.10, 0.11, 0.13)
		mat_dark.roughness = 0.65

		var mat_yellow := StandardMaterial3D.new()
		mat_yellow.albedo_color = Color(0.95, 0.78, 0.12)
		mat_yellow.emission_enabled = true
		mat_yellow.emission = Color(0.95, 0.72, 0.08)
		mat_yellow.emission_energy_multiplier = 0.5

		var slots := [mat_metal, mat_glass, mat_dark, mat_yellow]
		if mi.mesh:
			for s in range(mini(mi.mesh.get_surface_count(), slots.size())):
				mi.set_surface_override_material(s, slots[s])
	for c in node.get_children():
		_apply_shell_materials_recursive(c)

func _apply_step_materials_recursive(node: Node) -> void:
	if node is MeshInstance3D:
		var mi := node as MeshInstance3D
		var mat_tread_dark := StandardMaterial3D.new()
		mat_tread_dark.albedo_color = Color(0.16, 0.18, 0.21)
		mat_tread_dark.metallic = 0.45
		mat_tread_dark.roughness = 0.5

		var mat_tread_metal := StandardMaterial3D.new()
		mat_tread_metal.albedo_color = Color(0.45, 0.49, 0.54)
		mat_tread_metal.metallic = 0.7
		mat_tread_metal.roughness = 0.35

		var mat_yellow := StandardMaterial3D.new()
		mat_yellow.albedo_color = Color(0.95, 0.80, 0.12)

		var slots := [mat_tread_dark, mat_tread_metal, mat_yellow]
		if mi.mesh:
			for s in range(mini(mi.mesh.get_surface_count(), slots.size())):
				mi.set_surface_override_material(s, slots[s])
	for c in node.get_children():
		_apply_step_materials_recursive(c)

func set_operational_mode(new_state: MaintenanceState, is_running: bool = true) -> void:
	maintenance_state = new_state
	running = is_running and (maintenance_state == MaintenanceState.OPERATIONAL)
	_refresh_maintenance_visuals()
	_update_treads()

func _refresh_maintenance_visuals() -> void:
	if _caution_bar_root:
		_caution_bar_root.visible = (maintenance_state == MaintenanceState.BROKEN_BLOCKED)
	if _status_beacon:
		if maintenance_state == MaintenanceState.OPERATIONAL and running:
			_status_beacon.light_color = Color(0.15, 0.95, 0.45)
			_status_beacon.light_energy = 1.8
		elif maintenance_state == MaintenanceState.BROKEN_BLOCKED:
			_status_beacon.light_color = Color(1.0, 0.20, 0.10)
			_status_beacon.light_energy = 2.6
		else:
			_status_beacon.light_color = Color(1.0, 0.72, 0.15)
			_status_beacon.light_energy = 1.8
	if _label_3d:
		var dir_str := "DOWNHILL (F2 -> F1)" if reverse_direction else "UPHILL (F1 -> F2)"
		if maintenance_state == MaintenanceState.OPERATIONAL and running:
			_label_3d.text = "%s [%s — MOTORIZED BELT ACTIVE]\n(Walk directly onto steps to ride | No Push Carts Allowed)" % [escalator_title, dir_str]
			_label_3d.modulate = Color(0.45, 1.0, 0.70)
		else:
			_label_3d.text = "%s [%s — STALLED WALKABLE STAIRS]\n(Walk up/down steps directly OR press [F] with Fuse / at Breaker to Motorize!)" % [escalator_title, dir_str]
			_label_3d.modulate = Color(1.0, 0.82, 0.38)

func _update_treads() -> void:
	if tread_count <= 0 or _step_nodes.is_empty():
		return
	var total_len := get_travel_length()
	var pit_center := tread_count / 2
	for i in range(tread_count):
		var d := fposmod(belt_distance + float(i) * actual_tread_spacing, total_len)
		var pt := deck_local_point(d)
		var at_hidden_return := (d < 0.22) or (d > total_len - 0.22)
		var in_pit := (maintenance_state == MaintenanceState.BROKEN_BLOCKED) and (absi(i - pit_center) <= 1)
		var step_node := _step_nodes[i]
		step_node.position = pt
		step_node.visible = not (at_hidden_return or in_pit)

func _physics_process(delta: float) -> void:
	var signed_speed: float = 0.0
	if maintenance_state == MaintenanceState.OPERATIONAL and running:
		signed_speed = travel_speed_mps * (-1.0 if reverse_direction else 1.0)

	if not is_zero_approx(signed_speed):
		belt_distance = fposmod(belt_distance + signed_speed * delta, get_travel_length())
		_update_treads()

	if Engine.is_editor_hint():
		return

	_carry_grounded_riders(delta, signed_speed)

func _carry_grounded_riders(delta: float, signed_speed: float) -> void:
	var total_r := total_run()
	var travel_len := get_travel_length()
	var min_d := 0.08 if signed_speed >= 0.0 else -0.24
	var max_d := (travel_len + 0.24) if signed_speed >= 0.0 else (travel_len - 0.08)

	for node in get_tree().get_nodes_in_group("players"):
		var p := node as PlayerController
		if p == null:
			continue
		var local_pos := to_local(p.global_position)
		if absf(local_pos.z) > belt_width_m * 0.5 + 0.14:
			continue
		if local_pos.x < -0.35 or local_pos.x > total_r + 0.35:
			continue

		var dist: float = local_pos.x if local_pos.x < 0.0 else (
			(travel_len + (local_pos.x - total_r)) if local_pos.x > total_r else distance_at_local_x(local_pos.x)
		)
		var deck_pt := eval_extended_deck_local_point(dist)
		if local_pos.y < deck_pt.y - 0.38 or local_pos.y > deck_pt.y + 0.65:
			continue

		# Track floor elevation as player ascends/descends the escalator
		if p.global_position.y >= 6.5:
			GameState.current_floor = 2
		elif p.global_position.y <= 2.0:
			GameState.current_floor = 1

		# Stage 6A Rule: Stepped escalators cannot carry Equipment Push Carts or Mobile Light Carts!
		for c_node in get_tree().get_nodes_in_group("mall_carts"):
			var cart := c_node as MallCartSystem
			if cart and cart.is_attached_to_player and cart.attached_player == p:
				cart.toggle_attach(p)
				GameState.status_message.emit("STAGE 6A NO-CART BEACHHEAD: Push Carts cannot ride stepped escalators! Repair the Floor 2 Freight Elevator to bring carts up.", Color(1.0, 0.78, 0.3))

		if not is_zero_approx(signed_speed) and p.is_on_floor() and dist >= min_d and dist <= max_d:
			var next_dist := clampf(dist + signed_speed * delta, min_d, max_d)
			var local_delta := eval_extended_deck_local_point(next_dist) - deck_pt
			var world_delta := global_transform.basis * local_delta
			if world_delta.length_squared() > 0.000001:
				# Lift slightly by 0.015m during swept belt carry so capsule rides on top of the 30° incline without digging into the crease
				p.move_and_collide(Vector3(0.0, maxf(0.012, world_delta.y + 0.008), 0.0))
				p.move_and_collide(Vector3(world_delta.x, 0.0, world_delta.z))
				p.apply_floor_snap()
