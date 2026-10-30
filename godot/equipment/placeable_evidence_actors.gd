@tool
class_name PlaceableEvidenceActors
extends Node3D
## Contains UVEvidencePrint (2.5s phosphorescent UV afterglow), PlaceableMotionSensor (4.8m IR strobe),
## EctoplasmCanisterTrap (Forceful Ghost Extraction), and HallwayGremlin (95cm concourse nuisance pest).

# ------------------------------------------------------------------------------
# 1. UV Evidence Print (Footprint / Handprint / UV Cipher with 2.5s Afterglow)
# ------------------------------------------------------------------------------
class UVEvidencePrint extends Node3D:
	var store_id: String = "East_F0_02"
	var print_title: String = "UV Footprint (Neat Clerk Boot Step)"
	var is_corrupted_tell: bool = false
	var afterglow_timer: float = 0.0
	var ever_discovered: bool = false
	var mesh_root: Node3D = null
	var glow_light: OmniLight3D = null
	var label_3d: Label3D = null

	func _ready() -> void:
		add_to_group("uv_evidence_prints")
		mesh_root = Node3D.new()
		add_child(mesh_root)

		var fbx_path := "res://SourceAssets/Equipment/Prepared/SM_UVFootprint.fbx"
		if ResourceLoader.exists(fbx_path):
			var packed := load(fbx_path) as PackedScene
			if packed:
				var inst := packed.instantiate()
				mesh_root.add_child(inst)

		var decal_box := MeshInstance3D.new()
		var bm := BoxMesh.new()
		bm.size = Vector3(0.32, 0.015, 0.18)
		decal_box.mesh = bm
		var mat := StandardMaterial3D.new()
		mat.albedo_color = Color(0.2, 1.0, 0.35)
		mat.emission_enabled = true
		mat.emission = Color(0.2, 1.0, 0.35)
		mat.emission_energy_multiplier = 3.8
		decal_box.material_override = mat
		mesh_root.add_child(decal_box)

		glow_light = OmniLight3D.new()
		glow_light.position = Vector3(0, 0.25, 0)
		glow_light.light_color = Color(0.25, 1.0, 0.4)
		glow_light.light_energy = 1.6
		glow_light.omni_range = 2.2
		mesh_root.add_child(glow_light)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0, 0.45, 0)
		label_3d.text = print_title
		label_3d.font_size = 20
		label_3d.outline_size = 6
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.modulate = Color(0.35, 1.0, 0.5)
		mesh_root.add_child(label_3d)

		mesh_root.visible = Engine.is_editor_hint()

	func illuminate_by_uv() -> bool:
		afterglow_timer = 2.5
		mesh_root.visible = true
		if not ever_discovered:
			ever_discovered = true
			GameState.record_evidence(store_id, "UV Prints", 35)
			return true
		return false

	func _process(delta: float) -> void:
		if Engine.is_editor_hint():
			return
		if afterglow_timer > 0.0:
			afterglow_timer = maxf(0.0, afterglow_timer - delta)
			if glow_light:
				glow_light.light_energy = 1.6 * (afterglow_timer / 2.5)
			if afterglow_timer <= 0.0:
				mesh_root.visible = false


# ------------------------------------------------------------------------------
# 2. Placeable Infrared Motion Sensor (Detects Invisible Store Ghosts up to 4.8m)
# ------------------------------------------------------------------------------
class PlaceableMotionSensor extends Node3D:
	var store_id: String = "East_F0_02"
	var trigger_count: int = 0
	var motion_active_timer: float = 0.0
	var strobe_light: OmniLight3D = null
	var strobe_dome_mat: StandardMaterial3D = null
	var readout_label: Label3D = null
	var _cooldown: float = 0.0

	func _ready() -> void:
		add_to_group("motion_sensors")
		add_to_group("placeable_motion_sensors")
		var fbx_path := "res://SourceAssets/Equipment/Prepared/SM_MotionSensor.fbx"
		if ResourceLoader.exists(fbx_path):
			var packed := load(fbx_path) as PackedScene
			if packed:
				add_child(packed.instantiate())

		var dome := MeshInstance3D.new()
		var sph := SphereMesh.new()
		sph.radius = 0.05
		sph.height = 0.10
		dome.mesh = sph
		dome.position = Vector3(0, 0.24, 0)
		strobe_dome_mat = StandardMaterial3D.new()
		strobe_dome_mat.albedo_color = Color(0.1, 0.4, 0.45)
		strobe_dome_mat.emission_enabled = true
		strobe_dome_mat.emission = Color(0.15, 0.95, 1.0)
		strobe_dome_mat.emission_energy_multiplier = 0.4
		dome.material_override = strobe_dome_mat
		add_child(dome)

		strobe_light = OmniLight3D.new()
		strobe_light.position = Vector3(0, 0.30, 0)
		strobe_light.light_color = Color(0.15, 0.95, 1.0)
		strobe_light.light_energy = 0.0
		strobe_light.omni_range = 4.2
		add_child(strobe_light)

		readout_label = Label3D.new()
		readout_label.position = Vector3(0, 0.48, 0)
		readout_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		readout_label.font_size = 20
		readout_label.outline_size = 5
		readout_label.text = "IR MOTION SENSOR [IDLE | Hits: 0]"
		readout_label.modulate = Color(0.45, 0.95, 1.0)
		add_child(readout_label)

	func _physics_process(delta: float) -> void:
		if Engine.is_editor_hint():
			return
		if _cooldown > 0.0:
			_cooldown = maxf(0.0, _cooldown - delta)
		if motion_active_timer > 0.0:
			motion_active_timer = maxf(0.0, motion_active_timer - delta)
			var pulse := 0.5 + 0.5 * sin(motion_active_timer * 24.0)
			strobe_light.light_energy = 3.2 * pulse
			strobe_dome_mat.emission_energy_multiplier = 5.0 * pulse
		else:
			strobe_light.light_energy = 0.0
			strobe_dome_mat.emission_energy_multiplier = 0.35

		for g in get_tree().get_nodes_in_group("store_ghosts"):
			var ghost := g as StoreGhost
			if ghost and not ghost.is_resolved and global_position.distance_to(ghost.global_position) <= 4.8:
				if _cooldown <= 0.0:
					_cooldown = 4.5
					trigger_count += 1
					motion_active_timer = 2.8
					strobe_light.light_color = Color(0.25, 1.0, 0.55)
					readout_label.text = "IR MOTION ALERT! [Invisible Ghost Presence | Hits: %d]" % trigger_count
					readout_label.modulate = Color(0.25, 1.0, 0.55)
					GameState.status_message.emit("IR Motion Sensor [%s] Triggered (#%d) — Spectral presence crossed aisle!" % [store_id, trigger_count], Color(0.3, 1.0, 0.85))
				return

		# Non-Ghost Presences: Store Mannequin micro-twitches & Investigator/NPC pass-by!
		if motion_active_timer <= 0.0:
			var near_mannequin := false
			for m_node in get_tree().get_nodes_in_group("mall_mannequins"):
				var m3d := m_node as Node3D
				if m3d and absf(m3d.global_position.y - global_position.y) <= 2.2 and global_position.distance_to(m3d.global_position) <= 3.8:
					near_mannequin = true
					break
			if near_mannequin:
				var creep_pulse := 0.5 + 0.5 * sin(Time.get_ticks_msec() * 0.004)
				strobe_light.light_color = Color(1.0, 0.58, 0.18)
				strobe_light.light_energy = 0.85 * creep_pulse
				readout_label.text = "IR SENSOR: MICRO-MOVEMENT (0.4 cm)\n[DISPLAY MANNEQUIN TWITCHED?! | Hits: %d]" % trigger_count
				readout_label.modulate = Color(1.0, 0.68, 0.28)
				return

			var near_human_name := ""
			for p_node in get_tree().get_nodes_in_group("players"):
				var p3d := p_node as Node3D
				if p3d and absf(p3d.global_position.y - global_position.y) <= 2.2 and global_position.distance_to(p3d.global_position) <= 2.6:
					var vel: Vector3 = p3d.get("velocity") if p3d.get("velocity") != null else Vector3.ZERO
					if Vector2(vel.x, vel.z).length() > 0.25:
						near_human_name = "Investigator"
						break
			if near_human_name.is_empty():
				for n_node in get_tree().get_nodes_in_group("mall_npcs"):
					var n3d := n_node as Node3D
					if n3d and absf(n3d.global_position.y - global_position.y) <= 2.2 and global_position.distance_to(n3d.global_position) <= 3.0:
						near_human_name = str(n3d.get_meta("npc_name", "Mall Staff"))
						break
			if not near_human_name.is_empty():
				strobe_light.light_color = Color(0.35, 0.85, 1.0)
				strobe_light.light_energy = 0.65
				readout_label.text = "IR SENSOR: NON-GHOST PASS-BY [%s | Hits: %d]" % [near_human_name, trigger_count]
				readout_label.modulate = Color(0.55, 0.90, 1.0)
			else:
				strobe_light.light_color = Color(0.15, 0.95, 1.0)
				readout_label.text = "IR MOTION SENSOR [IDLE | Hits: %d]" % trigger_count
				readout_label.modulate = Color(0.45, 0.95, 1.0)


# ------------------------------------------------------------------------------
# 3. Deployable Ectoplasm Canister Trap (Separate Equipment Item:
#    • Set on the floor in CLOSED state.
#    • Kick & Open [E] to slide it forward & start sucking the ghost in!
#    • Has an active deployed time limit (`deployed_seconds_left`), and can be picked up [F] & recharged.
#    • Once FULL (`is_captured == true`), cannot be reused — pick it up [F] and sell it to Trader Chen,
#      then buy a new Empty Canister from the Trader!)
# ------------------------------------------------------------------------------
class EctoplasmCanisterTrap extends Node3D:
	const CAPTURE_RADIUS_PRESETS: Array[float] = [1.5, 2.2, 3.2, 4.0]

	@export_range(0.8, 6.0, 0.1) var capture_radius: float = 2.2:
		set = set_capture_radius
	@export_range(0.0, 1.0, 0.05) var multi_kick_random_move_chance: float = 0.35

	var radius_preset_idx: int = 1
	var is_open_sucking: bool = false
	var kick_count: int = 0
	var max_deployed_seconds: float = 18.0
	var deployed_seconds_left: float = 18.0
	var _kick_slide_vel: Vector3 = Vector3.ZERO
	var is_captured: bool = false
	var captured_entity_name: String = ""
	var captured_bounty_value: int = 180
	var captured_store_id: String = ""
	var capture_progress: float = 0.0
	var vortex_light: OmniLight3D = null
	var label_3d: Label3D = null
	var capture_ring_root: Node3D = null
	var ring_mat: StandardMaterial3D = null
	var disc_mat: StandardMaterial3D = null

	func _ready() -> void:
		add_to_group("ectoplasm_canisters")
		if not Engine.is_editor_hint() and GameState.has_method("get_canister_max_deployed_time"):
			max_deployed_seconds = GameState.get_canister_max_deployed_time()
			if deployed_seconds_left > max_deployed_seconds or deployed_seconds_left <= 0.0:
				deployed_seconds_left = max_deployed_seconds

		var glb_path := "res://SourceAssets/Equipment/Prepared/SM_EctoplasmCanister.glb"
		if ResourceLoader.exists(glb_path):
			var packed := load(glb_path) as PackedScene
			if packed:
				add_child(packed.instantiate())

		_build_circular_capture_range_visual()

		vortex_light = OmniLight3D.new()
		vortex_light.position = Vector3(0, 0.45, 0)
		vortex_light.light_color = Color(0.25, 1.0, 0.6)
		vortex_light.light_energy = 1.1
		vortex_light.omni_range = capture_radius + 1.5
		add_child(vortex_light)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0, 0.76, 0)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 22
		label_3d.outline_size = 6
		label_3d.modulate = Color(0.35, 1.0, 0.65)
		add_child(label_3d)
		_refresh_idle_label()

	func _build_circular_capture_range_visual() -> void:
		capture_ring_root = Node3D.new()
		capture_ring_root.position = Vector3(0.0, 0.03, 0.0)
		capture_ring_root.scale = Vector3(capture_radius, 1.0, capture_radius)
		add_child(capture_ring_root)

		# 1. Outer Glowing Circular Boundary Ring (Unit radius 1.0 -> scaled by `capture_radius`)
		var ring_mi := MeshInstance3D.new()
		var torus := TorusMesh.new()
		torus.inner_radius = 0.96
		torus.outer_radius = 1.02
		torus.rings = 48
		torus.ring_segments = 12
		ring_mi.mesh = torus
		ring_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		ring_mat = StandardMaterial3D.new()
		ring_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		ring_mat.albedo_color = Color(0.35, 0.85, 0.65, 0.55)
		ring_mat.emission_enabled = true
		ring_mat.emission = Color(0.18, 0.82, 0.58)
		ring_mat.emission_energy_multiplier = 1.4
		ring_mi.material_override = ring_mat
		capture_ring_root.add_child(ring_mi)

		# 2. Translucent Circular Floor Field inside the ring
		var disc_mi := MeshInstance3D.new()
		var cyl := CylinderMesh.new()
		cyl.top_radius = 0.98
		cyl.bottom_radius = 0.98
		cyl.height = 0.012
		cyl.radial_segments = 48
		disc_mi.mesh = cyl
		disc_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		disc_mat = StandardMaterial3D.new()
		disc_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		disc_mat.albedo_color = Color(0.15, 0.85, 0.58, 0.08)
		disc_mat.emission_enabled = true
		disc_mat.emission = Color(0.12, 0.75, 0.50)
		disc_mat.emission_energy_multiplier = 0.7
		disc_mi.material_override = disc_mat
		capture_ring_root.add_child(disc_mi)

	## Adjusts the circular capture radius around the catching box and updates the 3D floor ring!
	func set_capture_radius(new_r: float) -> void:
		capture_radius = clampf(new_r, 0.8, 6.0)
		for i in range(CAPTURE_RADIUS_PRESETS.size()):
			if absf(CAPTURE_RADIUS_PRESETS[i] - capture_radius) < 0.08:
				radius_preset_idx = i
				break
		if capture_ring_root:
			capture_ring_root.scale = Vector3(capture_radius, 1.0, capture_radius)
		if vortex_light:
			vortex_light.omni_range = capture_radius + 1.5
		if label_3d and not is_captured:
			_refresh_idle_label()

	## Cycles the circular capture range (1.5m Tight -> 2.2m Standard -> 3.2m Wide -> 4.0m if Canister Level 3).
	func cycle_capture_radius() -> float:
		var max_idx := 3 if (GameState.canister_upgrade_level >= 3) else 2
		radius_preset_idx = (radius_preset_idx + 1) % (max_idx + 1)
		set_capture_radius(CAPTURE_RADIUS_PRESETS[radius_preset_idx])
		GameState.status_message.emit(
			"Adjusted Ectoplasm Canister Circular Capture Range to %.1fm radius!" % capture_radius,
			Color(0.35, 1.0, 0.72)
		)
		return capture_radius

	## Kicks the canister to open its trap doors so it starts sucking ghosts in!
	## Remains stationary on the opening kick, and on multiple kicks (`kick_count >= 2`) only sometimes (`multi_kick_random_move_chance`) jolts randomly.
	func kick_and_open(_kick_dir: Vector3 = Vector3.ZERO) -> bool:
		if is_captured:
			GameState.status_message.emit(
				"This Ectoplasm Canister is FULL (%s — Worth $%d)! Once full it must be sold — press [F] to pick it up and take it to Trader Chen!" % [
					captured_entity_name, captured_bounty_value
				],
				Color(1.0, 0.88, 0.35)
			)
			return false
		if deployed_seconds_left <= 0.05:
			GameState.status_message.emit(
				"Ectoplasm Canister charge is depleted (0.0s)! Press [F] to pick it up and recharge it!",
				Color(1.0, 0.48, 0.32)
			)
			return false

		kick_count += 1
		var moved_randomly := false
		if not is_open_sucking:
			# First kick to open: always remains 100% stationary at its floor position!
			is_open_sucking = true
			_kick_slide_vel = Vector3.ZERO
		else:
			# Multiple kicks on an already-open canister: only sometimes triggers a random movement, otherwise stays stationary!
			if randf() < multi_kick_random_move_chance:
				var rand_ang := randf_range(0.0, TAU)
				_kick_slide_vel = Vector3(cos(rand_ang), 0.0, sin(rand_ang)) * randf_range(3.2, 4.5)
				moved_randomly = true
			else:
				_kick_slide_vel = Vector3.ZERO

		if vortex_light:
			vortex_light.light_energy = 3.4
		_set_ring_color(Color(0.22, 1.0, 0.68), 0.20, 3.0)
		_refresh_idle_label()
		if kick_count == 1:
			GameState.status_message.emit(
				"KICKED & OPENED ECTOPLASM CANISTER! Canister is stationary & sucking (%.1fs deployed time limit | %.1fm circle) — wrangle the ghost in!" % [
					deployed_seconds_left, capture_radius
				],
				Color(0.35, 1.0, 0.78)
			)
		elif moved_randomly:
			GameState.status_message.emit(
				"Kicked open canister again (Kick #%d) — Multiple kicks jolted the canister in a random direction!" % kick_count,
				Color(1.0, 0.85, 0.38)
			)
		else:
			GameState.status_message.emit(
				"Kicked open canister (Kick #%d) — Canister remained stationary." % kick_count,
				Color(0.35, 1.0, 0.78)
			)
		return true

	func is_point_inside_capture_circle(world_pt: Vector3) -> bool:
		if absf(world_pt.y - global_position.y) > 2.5:
			return false
		var horiz_dist := Vector2(world_pt.x - global_position.x, world_pt.z - global_position.z).length()
		return horiz_dist <= capture_radius

	func _refresh_idle_label() -> void:
		if label_3d == null:
			return
		if is_captured:
			label_3d.text = "FILLED ECTOPLASM CANISTER [%s — Sell Value: $%d]\n[Once Full Must Be Sold! Press F to Pick Up & Sell to Trader Chen!]" % [
				captured_entity_name, captured_bounty_value
			]
			label_3d.modulate = Color(1.0, 0.92, 0.35)
		elif is_open_sucking:
			label_3d.text = "ECTOPLASM CANISTER [OPEN & SUCKING — %.1fs / %.0fs Left | %.1fm Circle]\n[Wrangle Weakened Ghost Inside Circle | E: Kick Again | F: Pick Up & Recharge]" % [
				deployed_seconds_left, max_deployed_seconds, capture_radius
			]
			label_3d.modulate = Color(0.35, 1.0, 0.75)
		elif deployed_seconds_left <= 0.05:
			label_3d.text = "ECTOPLASM CANISTER [TIME EXPIRED (0.0s) — CLOSED]\n[Press F to Pick Up & Recharge Battery!]"
			label_3d.modulate = Color(1.0, 0.52, 0.35)
		else:
			label_3d.text = "ECTOPLASM CANISTER [SET ON FLOOR (CLOSED) — %.0fs Charge | %.1fm Circle]\n[Press E to KICK & OPEN to Start Sucking Ghost In! | F: Pick Up]" % [
				deployed_seconds_left, capture_radius
			]
			label_3d.modulate = Color(0.65, 0.95, 0.80)

	func _set_ring_color(col: Color, disc_alpha: float = 0.18, energy: float = 2.8) -> void:
		if ring_mat:
			ring_mat.albedo_color = Color(col.r, col.g, col.b, 0.90)
			ring_mat.emission = col
			ring_mat.emission_energy_multiplier = energy
		if disc_mat:
			disc_mat.albedo_color = Color(col.r, col.g, col.b, disc_alpha)
			disc_mat.emission = col

	func _mark_filled_with_ghost(entity_name: String, bounty_val: int, store_tag: String = "") -> void:
		is_captured = true
		is_open_sucking = false
		captured_entity_name = entity_name
		captured_bounty_value = bounty_val
		captured_store_id = store_tag
		if vortex_light:
			vortex_light.light_color = Color(1.0, 0.85, 0.25)
			vortex_light.light_energy = 3.2
		_set_ring_color(Color(1.0, 0.85, 0.25), 0.26, 3.6)
		_refresh_idle_label()

	func _physics_process(delta: float) -> void:
		if Engine.is_editor_hint():
			return

		# Handle physical floor slide when the player kicks the canister!
		if _kick_slide_vel.length_squared() > 0.01:
			global_position += _kick_slide_vel * delta
			_kick_slide_vel = _kick_slide_vel.move_toward(Vector3.ZERO, 10.5 * delta)

		if is_captured:
			return

		# While CLOSED on the floor, the canister does not drain its deployed time limit and does not suck ghosts in yet!
		if not is_open_sucking:
			return

		# Drain the active deployed time limit while OPEN & SUCKING
		deployed_seconds_left = maxf(0.0, deployed_seconds_left - delta)
		if deployed_seconds_left <= 0.0:
			is_open_sucking = false
			capture_progress = 0.0
			if vortex_light:
				vortex_light.light_energy = 0.5
			_set_ring_color(Color(0.65, 0.35, 0.25), 0.06, 0.8)
			_refresh_idle_label()
			GameState.status_message.emit(
				"Ectoplasm Canister deployed time limit ran out (0.0s)! Press [F] to pick it up and recharge it!",
				Color(1.0, 0.52, 0.32)
			)
			return

		var speed_mult: float = GameState.get_canister_capture_speed_mult() if GameState.has_method("get_canister_capture_speed_mult") else 1.0
		var active_target_name := ""
		var capturing_now := false

		# 1. Check Hallway Gremlins inside the Open Canister's Circular Capture Range
		for gr in get_tree().get_nodes_in_group("hallway_gremlins"):
			var grem := gr as HallwayGremlin
			if grem and not grem.is_captured and not grem.is_vanished_in_walls:
				if is_point_inside_capture_circle(grem.global_position):
					if grem.ghost_hp <= grem.max_ghost_hp * 0.25 or grem.weakened_percent >= 75.0:
						capturing_now = true
						active_target_name = "Hallway Gremlin (HP %.0f/%.0f)" % [grem.ghost_hp, grem.max_ghost_hp]
						var pull_dir := (global_position - grem.global_position)
						pull_dir.y = 0.0
						if pull_dir.length() > 0.12:
							grem.global_position += pull_dir.normalized() * minf(1.55 * speed_mult * delta, pull_dir.length())
						capture_progress = minf(100.0, capture_progress + 58.0 * speed_mult * delta)
						if capture_progress >= 100.0:
							grem.capture_into_canister(global_position)
							_mark_filled_with_ghost("Hallway Gremlin", 85, "")
							return
					else:
						active_target_name = "Hallway Gremlin (HP %.0f/%.0f — Damage Below %.0f HP with Ray Gun!)" % [
							grem.ghost_hp, grem.max_ghost_hp, grem.max_ghost_hp * 0.25
						]

		# 2. Check Store Ghosts inside the Open Canister's Circular Capture Range (ONLY inside `capture_radius`, NOT the whole store!)
		if not capturing_now:
			for g in get_tree().get_nodes_in_group("store_ghosts"):
				var ghost := g as StoreGhost
				if ghost and not ghost.is_resolved:
					if is_point_inside_capture_circle(ghost.global_position):
						if ghost.ghost_hp <= ghost.max_ghost_hp * 0.15 or ghost.weakened_percent >= 85.0:
							capturing_now = true
							active_target_name = "%s (HP %.0f/%.0f)" % [ghost.ghost_name, ghost.ghost_hp, ghost.max_ghost_hp]
							var pull_dir := (global_position - ghost.global_position)
							pull_dir.y = 0.0
							if pull_dir.length() > 0.12:
								var cand := ghost.global_position + pull_dir.normalized() * minf(1.30 * speed_mult * delta, pull_dir.length())
								if not ghost.can_briefly_pursue_outside_store:
									cand = ghost.clamp_point_to_store_territory(cand)
								ghost.global_position = cand
							capture_progress = minf(100.0, capture_progress + 48.0 * speed_mult * delta)
							if capture_progress >= 100.0:
								var bounty := 260 if ghost.floor_level >= 2 else 180
								ghost.resolve_forcefully_into_canister(global_position)
								_mark_filled_with_ghost(ghost.ghost_name, bounty, ghost.store_id)
								return
						else:
							active_target_name = "%s (HP %.0f/%.0f — Damage Below %.0f HP with Ray Gun!)" % [
								ghost.ghost_name, ghost.ghost_hp, ghost.max_ghost_hp, ghost.max_ghost_hp * 0.15
							]

		if capturing_now:
			vortex_light.light_energy = 4.4
			_set_ring_color(Color(0.35, 1.0, 0.92), 0.28, 4.2)
			label_3d.text = "ECTOPLASM CANISTER [SUCKING IN %s: %.0f%% | %.1fs Left]" % [
				active_target_name, capture_progress, deployed_seconds_left
			]
			label_3d.modulate = Color(0.45, 1.0, 0.88)
		else:
			capture_progress = maxf(0.0, capture_progress - 25.0 * delta)
			vortex_light.light_energy = 3.0
			if not active_target_name.is_empty():
				_set_ring_color(Color(1.0, 0.78, 0.28), 0.22, 3.2)
				label_3d.text = "ECTOPLASM CANISTER [OPEN (%.1fs Left) — INSIDE %.1fm CIRCLE: %s]" % [
					deployed_seconds_left, capture_radius, active_target_name
				]
				label_3d.modulate = Color(1.0, 0.84, 0.38)
			else:
				_set_ring_color(Color(0.22, 1.0, 0.68), 0.20, 3.0)
				_refresh_idle_label()


# ------------------------------------------------------------------------------
# 4. Hallway Gremlin (95cm Low-Poly Concourse Pest — Weight 1.0, Health Pool 85 HP:
#    • Interferes with Light Carts.
#    • UV Blacklight / Camera Flash scares him into the walls of the map for 75s.
#    • Proton Ray Gun deals HP damage, ALWAYS slows him down, tracks him, and pulls
#      him into the Ectoplasm Canister's circular capture range!)
# ------------------------------------------------------------------------------
class HallwayGremlin extends CharacterBody3D:
	@export var ghost_weight: float = 1.0
	@export var max_ghost_hp: float = 85.0
	@export var hp_regen_per_second: float = 1.5
	@export var regen_delay_after_beam: float = 4.5

	var ghost_hp: float = 85.0
	var weakened_percent: float = 0.0
	var is_captured: bool = false
	var _beamed_timer: float = 0.0
	var _regen_delay_timer: float = 0.0
	var _wrangle_target: Vector3 = Vector3.ZERO
	var _wrangle_strength: float = 0.0
	var _wander_jitter_phase: float = 0.0
	var patrol_origin: Vector3 = Vector3(6.0, 0.0, 4.0)
	var patrol_phase: float = 0.0
	var stunned_timer: float = 0.0
	var sabotage_cooldown: float = 25.0
	var is_fleeing_into_wall: bool = false
	var is_vanished_in_walls: bool = false
	var flee_timer: float = 0.0
	var vanished_in_walls_timer: float = 0.0
	var flee_wall_target: Vector3 = Vector3(28.0, 0.0, -14.0)
	var anim_player: AnimationPlayer = null
	var label_3d: Label3D = null
	var gremlin_audio: AudioStreamPlayer3D = null

	func _ready() -> void:
		add_to_group("hallway_gremlins")
		patrol_origin = global_position
		ghost_hp = max_ghost_hp

		var col := CollisionShape3D.new()
		var shape := CapsuleShape3D.new()
		shape.radius = 0.28
		shape.height = 0.95
		col.shape = shape
		col.position = Vector3(0, 0.48, 0)
		add_child(col)

		var glb_path := "res://SourceAssets/Characters/NormalizedLowPoly/Hallway_Gremlin_LowPoly.glb"
		if ResourceLoader.exists(glb_path):
			var packed := load(glb_path) as PackedScene
			if packed:
				var inst := packed.instantiate() as Node3D
				inst.scale = Vector3.ONE # Exact 95cm normalized height!
				add_child(inst)
				anim_player = _find_anim_player(inst)
				if anim_player and anim_player.has_animation("CharacterArmature|Walk"):
					anim_player.play("CharacterArmature|Walk")

		gremlin_audio = AudioStreamPlayer3D.new()
		gremlin_audio.bus = &"GhostAudio"
		gremlin_audio.max_distance = 18.0
		gremlin_audio.unit_size = 3.0
		var breath_path := "res://SourceAssets/GhostSounds/SW_Ghost_InvisibleBreath.wav"
		if ResourceLoader.exists(breath_path):
			gremlin_audio.stream = load(breath_path)
		add_child(gremlin_audio)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0, 1.15, 0)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 20
		label_3d.outline_size = 5
		label_3d.text = "Hallway Gremlin [HP: %.0f / %.0f | Weight: %.1f]\n[UV Light: Chase into Walls | Proton Ray Gun: Damage, Slow & Wrangle to Canister!]" % [
			ghost_hp, max_ghost_hp, ghost_weight
		]
		label_3d.modulate = Color(1.0, 0.68, 0.32)
		add_child(label_3d)

	func _find_anim_player(node: Node) -> AnimationPlayer:
		if node is AnimationPlayer:
			return node
		for c in node.get_children():
			var found := _find_anim_player(c)
			if found:
				return found
		return null

	func _choose_wall_escape_point() -> Vector3:
		var wall_candidates := [
			Vector3(28.5, global_position.y, global_position.z),
			Vector3(-28.5, global_position.y, global_position.z),
			Vector3(global_position.x, global_position.y, -23.0),
			Vector3(global_position.x, global_position.y, 19.5),
		]
		var best_pt: Vector3 = wall_candidates[0]
		var best_d := 999.0
		for pt in wall_candidates:
			var d := global_position.distance_to(pt)
			if d < best_d:
				best_d = d
				best_pt = pt
		return best_pt

	## Called ONLY by UV Blacklight or Camera Xenon Flash — scares the Gremlin into running away into the map walls for 75s!
	func stun_gremlin(reason: String) -> void:
		if is_captured or is_fleeing_into_wall or is_vanished_in_walls:
			return
		GameState.add_cash(25, "Repelled Hallway Gremlin (%s)" % reason)
		is_fleeing_into_wall = true
		flee_timer = 1.85
		stunned_timer = 75.0
		sabotage_cooldown = 95.0
		flee_wall_target = _choose_wall_escape_point()
		collision_layer = 0
		collision_mask = 0
		if gremlin_audio and gremlin_audio.stream:
			gremlin_audio.pitch_scale = 1.85
			gremlin_audio.play()
		if anim_player:
			if anim_player.has_animation("CharacterArmature|Run"):
				anim_player.play("CharacterArmature|Run")
			elif anim_player.has_animation("CharacterArmature|Walk"):
				anim_player.play("CharacterArmature|Walk", -1, 2.2)
		label_3d.text = "Hallway Gremlin [HIT BY %s — RUNNING INTO THE WALLS!]" % reason
		label_3d.modulate = Color(0.45, 1.0, 0.65)
		GameState.status_message.emit(
			"Hit Hallway Gremlin with %s! He is running away and disappearing into the mall walls for a while!" % reason,
			Color(0.45, 1.0, 0.72)
		)

	## Called by the Proton Ray Gun — deals direct HP damage to `ghost_hp`, ALWAYS slows the Gremlin down (-65% speed),
	## cancels wall fleeing, allows him to move around slightly on his own, and pulls/herds him toward `beam_target_pos`!
	func apply_proton_beam(delta: float, beam_target_pos: Vector3, ray_power: float = 1.0, ray_dps: float = 28.0) -> void:
		if is_captured or is_vanished_in_walls:
			return
		if is_fleeing_into_wall:
			is_fleeing_into_wall = false
			collision_layer = 1
			collision_mask = 1
		_beamed_timer = 0.25
		_regen_delay_timer = regen_delay_after_beam
		_wrangle_target = beam_target_pos
		ghost_hp = maxf(0.0, ghost_hp - ray_dps * delta)
		weakened_percent = clampf((1.0 - (ghost_hp / maxf(max_ghost_hp, 1.0))) * 100.0, 0.0, 100.0)
		var ratio := clampf(ray_power / maxf(ghost_weight, 0.1), 0.25, 3.0)
		_wrangle_strength = clampf(ratio * (1.85 + 2.05 * (weakened_percent / 100.0)), 0.75, 4.8)
		var ready_capture := (ghost_hp <= max_ghost_hp * 0.25 or weakened_percent >= 75.0)
		var state_hint := "WEAKENED — WRANGLE INTO CANISTER CIRCLE!" if ready_capture else "SLOWED & TAKING RAY DAMAGE!"
		label_3d.text = "Hallway Gremlin [HP: %.0f / %.0f | WEAKENED: %.0f%%]\n[%s]" % [ghost_hp, max_ghost_hp, weakened_percent, state_hint]
		label_3d.modulate = Color(0.35, 1.0, 0.68) if ready_capture else Color(0.42, 0.92, 1.0)

	func capture_into_canister(_canister_pos: Vector3) -> void:
		if is_captured:
			return
		is_captured = true
		is_fleeing_into_wall = false
		is_vanished_in_walls = false
		visible = false
		collision_layer = 0
		collision_mask = 0
		velocity = Vector3.ZERO
		GameState.status_message.emit(
			"CAPTURED HALLWAY GREMLIN IN ECTOPLASM CANISTER! Canister is now FULL — press [F] to pick it up and sell it to Trader Chen (+$85)!",
			Color(1.0, 0.88, 0.35)
		)

	func _enter_vanished_in_walls_state() -> void:
		is_fleeing_into_wall = false
		is_vanished_in_walls = true
		vanished_in_walls_timer = 75.0
		visible = false
		velocity = Vector3.ZERO

	func _find_active_light_cart_to_sabotage() -> MallCartSystem:
		if sabotage_cooldown > 0.0 or is_fleeing_into_wall or is_vanished_in_walls or is_captured:
			return null
		var best_cart: MallCartSystem = null
		var best_dist: float = 20.0
		for node in get_tree().get_nodes_in_group("light_carts"):
			var lc := node as MallCartSystem
			if lc and lc.is_light_circle_active() and absf(lc.global_position.y - global_position.y) < 2.5:
				var d := global_position.distance_to(lc.global_position)
				if d < best_dist:
					best_dist = d
					best_cart = lc
		return best_cart

	func _physics_process(delta: float) -> void:
		if Engine.is_editor_hint() or is_captured:
			return
		if sabotage_cooldown > 0.0:
			sabotage_cooldown = maxf(0.0, sabotage_cooldown - delta)

		# State 0: Being Beamed by Proton Ray Gun — Slowed Down, Moving Around a Little on His Own, & Wrangled Toward Target!
		if _beamed_timer > 0.0:
			_beamed_timer = maxf(0.0, _beamed_timer - delta)
			_wander_jitter_phase += delta * 4.2
			# Autonomous struggling movement (slowed down to ~0.50-0.85 m/s so he can still move around on his own a little!)
			var struggle_dir := Vector3(cos(_wander_jitter_phase * 1.3), 0.0, sin(_wander_jitter_phase * 1.9)).normalized()
			var autonomous_speed := lerpf(0.85, 0.45, weakened_percent / 100.0)
			var autonomous_vel := struggle_dir * autonomous_speed

			# Wrangling pull/push velocity toward the Proton Ray Gun's tether point
			var to_tether := _wrangle_target - global_position
			to_tether.y = 0.0
			var wrangle_vel := Vector3.ZERO
			if to_tether.length() > 0.15:
				wrangle_vel = to_tether.normalized() * minf(_wrangle_strength, to_tether.length() * 3.2)

			velocity = autonomous_vel + wrangle_vel
			if velocity.length() > 0.08:
				look_at(global_position + velocity.normalized(), Vector3.UP)
			move_and_slide()
			return

		# When not beamed, wait `regen_delay_after_beam` before regenerating `hp_regen_per_second`!
		if _regen_delay_timer > 0.0:
			_regen_delay_timer = maxf(0.0, _regen_delay_timer - delta)
		elif ghost_hp < max_ghost_hp:
			ghost_hp = minf(max_ghost_hp, ghost_hp + hp_regen_per_second * delta)
			weakened_percent = clampf((1.0 - (ghost_hp / maxf(max_ghost_hp, 1.0))) * 100.0, 0.0, 100.0)
			if ghost_hp >= max_ghost_hp and not is_fleeing_into_wall and not is_vanished_in_walls:
				label_3d.text = "Hallway Gremlin [HP: %.0f / %.0f | Weight: %.1f]\n[UV Light: Chase into Walls | Proton Ray Gun: Damage, Slow & Wrangle to Canister!]" % [
					ghost_hp, max_ghost_hp, ghost_weight
				]
				label_3d.modulate = Color(1.0, 0.68, 0.32)

		# State 1: Vanished inside the walls of the map for a long while (75s)
		if is_vanished_in_walls:
			vanished_in_walls_timer = maxf(0.0, vanished_in_walls_timer - delta)
			stunned_timer = vanished_in_walls_timer
			if vanished_in_walls_timer <= 0.0:
				is_vanished_in_walls = false
				global_position = patrol_origin
				visible = true
				collision_layer = 1
				collision_mask = 1
				sabotage_cooldown = 45.0
				label_3d.text = "Hallway Gremlin (Weight: 1.0 | Non-Territorial Pest)"
				label_3d.modulate = Color(1.0, 0.68, 0.32)
				if anim_player and anim_player.has_animation("CharacterArmature|Walk"):
					anim_player.play("CharacterArmature|Walk", -1, 1.0)
			return

		# State 2: Sprinting away toward the nearest map wall (triggered ONLY by UV Light / Camera Flash)
		if is_fleeing_into_wall:
			var to_wall := flee_wall_target - global_position
			to_wall.y = 0.0
			if to_wall.length() > 0.15:
				var run_dir := to_wall.normalized()
				var step_len := minf(6.5 * delta, to_wall.length())
				global_position += run_dir * step_len
				look_at(global_position + run_dir, Vector3.UP)
			flee_timer = maxf(0.0, flee_timer - delta)
			if flee_timer <= 0.0 or (flee_wall_target - global_position).length() <= 0.25:
				_enter_vanished_in_walls_state()
			return

		# State 2.5: Groggy / Weakened by Proton Ray Gun — stays right where he is so the player can set/kick open the Canister and catch him!
		if ghost_hp <= max_ghost_hp * 0.25 or _regen_delay_timer > 0.0:
			patrol_phase += delta * 1.6
			velocity = Vector3(cos(patrol_phase), 0.0, sin(patrol_phase * 1.3)) * 0.24
			move_and_slide()
			return

		# State 3: Occasional non-territorial Gremlin interference (45s cooldown between attempts)
		var target_cart := _find_active_light_cart_to_sabotage()
		if target_cart != null:
			var to_cart := target_cart.global_position - global_position
			to_cart.y = 0.0
			var dist_to_cart := to_cart.length()
			if dist_to_cart <= 3.55:
				target_cart.set_floodlight_power(false, "Hallway Gremlin Interference")
				sabotage_cooldown = 45.0
				if gremlin_audio and gremlin_audio.stream:
					gremlin_audio.pitch_scale = 1.68
					gremlin_audio.play()
				label_3d.text = "Hallway Gremlin [Switched Off Your Light Cart! Use UV or Proton Gun!]"
				label_3d.modulate = Color(1.0, 0.42, 0.28)
				return
			else:
				label_3d.text = "Hallway Gremlin [Sneaking to Turn Off Light Cart!]"
				label_3d.modulate = Color(1.0, 0.58, 0.25)
				velocity = to_cart.normalized() * 2.35
				look_at(global_position + to_cart.normalized(), Vector3.UP)
				move_and_slide()
				return

		patrol_phase += delta * 1.35
		var target := patrol_origin + Vector3(cos(patrol_phase) * 3.2, 0.0, sin(patrol_phase * 1.5) * 2.4)
		var dir := (target - global_position)
		dir.y = 0.0
		if dir.length() > 0.1:
			velocity = dir.normalized() * 1.9
			look_at(global_position + dir.normalized(), Vector3.UP)
			move_and_slide()


# ------------------------------------------------------------------------------
# 5. World Dropped Item (Individual Tool or Hands-Only Item on the Floor)
#    Gadgets left ON and thrown/dropped on the ground continue functioning in 3D space!
# ------------------------------------------------------------------------------
class WorldDroppedItem extends Node3D:
	var item_data: Dictionary = {}
	var label_3d: Label3D = null
	var ground_audio: AudioStreamPlayer3D = null
	var ground_evp_audio: AudioStreamPlayer3D = null
	var ground_status_light: OmniLight3D = null
	var ground_uv_spotlight: SpotLight3D = null
	var _pad_mat: StandardMaterial3D = null
	var _bob_phase: float = 0.0
	var _visual_holder: Node3D = null
	var _ground_beep_timer: float = 0.0
	var _current_ground_emf: int = 1
	var _current_ground_temp_c: float = 20.5
	var _is_closest_focus: bool = false
	var _last_live_detail: String = ""

	func _ready() -> void:
		add_to_group("world_dropped_items")
		_bob_phase = randf() * TAU
		_visual_holder = Node3D.new()
		_visual_holder.position = Vector3(0, 0.16, 0)
		add_child(_visual_holder)

		var item_id_str: String = str(item_data.get("id", ""))
		var model_path: String = item_data.get("model_path", "")
		var spawned_mesh := false
		if item_id_str == "cctv_monitor":
			VideoAndAudioSurveillance.build_portable_cctv_monitor_mesh(_visual_holder, false)
			spawned_mesh = true
		elif not model_path.is_empty() and ResourceLoader.exists(model_path):
			var packed := load(model_path) as PackedScene
			if packed:
				var inst := packed.instantiate() as Node3D
				MallPbrMaterialLibrary.beautify_model_materials(inst, model_path)
				_visual_holder.add_child(inst)
				spawned_mesh = true

		if not spawned_mesh:
			var mi := MeshInstance3D.new()
			var bm := BoxMesh.new()
			bm.size = Vector3(0.24, 0.12, 0.18)
			mi.mesh = bm
			mi.material_override = MallPbrMaterialLibrary.get_brushed_steel_material(Color(0.55, 0.62, 0.68))
			_visual_holder.add_child(mi)

		# Sleek low-profile equipment tray pad (no blinding neon floor blobs)
		var pad := MeshInstance3D.new()
		var cyl := CylinderMesh.new()
		cyl.top_radius = 0.15
		cyl.bottom_radius = 0.17
		cyl.height = 0.016
		pad.mesh = cyl
		pad.position = Vector3(0, 0.008, 0)
		_pad_mat = StandardMaterial3D.new()
		var is_hands_only: bool = not item_data.get("can_store_in_hotbar", true)
		_pad_mat.albedo_color = Color(0.75, 0.56, 0.28) if is_hands_only else Color(0.38, 0.48, 0.54)
		_pad_mat.metallic = 0.65
		_pad_mat.roughness = 0.32
		_pad_mat.emission_enabled = true
		_pad_mat.emission = _pad_mat.albedo_color
		_pad_mat.emission_energy_multiplier = 0.22
		pad.material_override = _pad_mat
		add_child(pad)

		ground_status_light = OmniLight3D.new()
		ground_status_light.position = Vector3(0.0, 0.28, 0.0)
		ground_status_light.omni_range = 2.6
		ground_status_light.light_energy = 0.0
		add_child(ground_status_light)

		ground_uv_spotlight = SpotLight3D.new()
		ground_uv_spotlight.position = Vector3(0.0, 0.22, -0.10)
		ground_uv_spotlight.light_color = Color(0.58, 0.18, 1.0)
		ground_uv_spotlight.light_energy = 4.2
		ground_uv_spotlight.spot_range = 8.5
		ground_uv_spotlight.spot_angle = 26.0
		ground_uv_spotlight.visible = false
		add_child(ground_uv_spotlight)

		ground_audio = AudioStreamPlayer3D.new()
		ground_audio.bus = &"SFX"
		ground_audio.max_distance = 18.0
		ground_audio.unit_size = 3.0
		add_child(ground_audio)

		ground_evp_audio = AudioStreamPlayer3D.new()
		ground_evp_audio.bus = &"GhostAudio"
		ground_evp_audio.max_distance = 18.0
		ground_evp_audio.unit_size = 3.2
		add_child(ground_evp_audio)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0, 0.48, 0)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 18
		label_3d.outline_size = 5
		label_3d.visibility_range_end = 7.0
		label_3d.visibility_range_end_margin = 1.5
		label_3d.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
		add_child(label_3d)

		if not Engine.is_editor_hint() and AcousticVoipManager:
			if not AcousticVoipManager.voice_command_recognized.is_connected(_on_ground_voice_command):
				AcousticVoipManager.voice_command_recognized.connect(_on_ground_voice_command)

		_configure_ground_gadget_state()

	func is_powered_on() -> bool:
		return bool(item_data.get("powered_on", false))

	func can_power_toggle() -> bool:
		return bool(item_data.get("can_power_toggle", false))

	func is_emitting_ground_noise() -> bool:
		return is_powered_on() and bool(item_data.get("makes_noise_when_on", false))

	func toggle_ground_power() -> void:
		if not can_power_toggle():
			return
		var next_on := not is_powered_on()
		item_data["powered_on"] = next_on
		var click_path := "res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_On.wav" if next_on else "res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_Off.wav"
		if ground_evp_audio and ResourceLoader.exists(click_path):
			ground_evp_audio.stream = load(click_path)
			ground_evp_audio.pitch_scale = 1.0
			ground_evp_audio.play()
		_configure_ground_gadget_state()
		GameState.status_message.emit(
			"Floor Gadget [%s] Switched %s!" % [item_data.get("name", "Gadget"), "ON (Active on Floor)" if next_on else "OFF (Silent)"],
			Color(0.45, 1.0, 0.72) if next_on else Color(0.75, 0.80, 0.88)
		)

	func _configure_ground_gadget_state() -> void:
		var item_id: String = str(item_data.get("id", ""))
		var on := is_powered_on()
		if ground_uv_spotlight:
			ground_uv_spotlight.visible = (item_id == "uv_blacklight" and on)
		if ground_audio:
			ground_audio.stop()
			if on:
				if item_id == "emf_reader" and ResourceLoader.exists("res://SourceAssets/EquipmentSounds/SW_EMF_Beep.wav"):
					ground_audio.stream = load("res://SourceAssets/EquipmentSounds/SW_EMF_Beep.wav")
				elif item_id == "thermal_scanner" and ResourceLoader.exists("res://SourceAssets/EquipmentSounds/SW_Thermal_Beep.wav"):
					ground_audio.stream = load("res://SourceAssets/EquipmentSounds/SW_Thermal_Beep.wav")
				elif item_id == "spirit_box" and ResourceLoader.exists("res://SourceAssets/SpiritBox/SW_SpiritBox_Scan.wav"):
					ground_audio.stream = load("res://SourceAssets/SpiritBox/SW_SpiritBox_Scan.wav")
					ground_audio.volume_db = -18.5
					ground_audio.play()
		if not on and ground_status_light:
			ground_status_light.light_energy = 0.0
		_refresh_ground_label("")

	func _refresh_ground_label(live_detail: String) -> void:
		_last_live_detail = live_detail
		if label_3d == null:
			return
		var item_name: String = str(item_data.get("name", "Equipment Item"))
		var is_hands_only: bool = not bool(item_data.get("can_store_in_hotbar", true))

		# When not the single closest focused item to the player, display a crisp 1-line name tag right over the item
		# so items lined up along a table never overlap each other's text!
		if not _is_closest_focus:
			label_3d.position = Vector3(0.0, 0.34, 0.0)
			label_3d.font_size = 14
			label_3d.text = "[F] %s" % item_name
			label_3d.modulate = Color(1.0, 0.82, 0.38) if is_hands_only else Color(0.78, 0.94, 1.0)
			return

		label_3d.position = Vector3(0.0, 0.52, 0.0)
		label_3d.font_size = 18
		if is_hands_only:
			label_3d.text = "[F] Carry in Hands (Hands-Only):\n%s" % item_name
			label_3d.modulate = Color(1.0, 0.80, 0.35)
		elif can_power_toggle():
			var on := is_powered_on()
			if on:
				label_3d.text = "[ON — %s] %s\n[F] Pick Up | [E] Turn OFF" % [live_detail if not live_detail.is_empty() else "ACTIVE", item_name]
				label_3d.modulate = Color(0.38, 1.0, 0.68)
			else:
				label_3d.text = "[OFF — SILENT] %s\n[F] Pick Up | [E] Turn ON" % item_name
				label_3d.modulate = Color(0.65, 0.78, 0.88)
		else:
			label_3d.text = "[F] Pick Up: %s\n(Slots into 1-4 or Hands if Full)" % item_name
			label_3d.modulate = Color(0.55, 0.96, 1.0)

	func _on_ground_voice_command(_phrase: String, _speaker_pos: Vector3, _freq: float) -> void:
		if not is_powered_on() or str(item_data.get("id", "")) != "spirit_box":
			return
		var g := _find_nearest_ghost()
		if g and global_position.distance_to(g.global_position) <= 16.0 and ground_evp_audio:
			var evp_path := "res://SourceAssets/GhostSounds/SW_Ghost_HuntGrowl.wav" if g.is_corrupted_hostile else "res://SourceAssets/SpiritBox/SW_SpiritBox_Placeholder.wav"
			if ResourceLoader.exists(evp_path):
				ground_evp_audio.stream = load(evp_path)
				ground_evp_audio.pitch_scale = 0.88 if g.is_corrupted_hostile else 1.0
				ground_evp_audio.play()

	func _find_nearest_ghost() -> StoreGhost:
		var best: StoreGhost = null
		var best_d: float = 999.0
		for node in get_tree().get_nodes_in_group("store_ghosts"):
			var g := node as StoreGhost
			if g and not g.is_resolved:
				var d := global_position.distance_to(g.global_position)
				if d < best_d:
					best_d = d
					best = g
		return best

	func _physics_process(delta: float) -> void:
		if Engine.is_editor_hint():
			return
		var now_focus := false
		for p_node in get_tree().get_nodes_in_group("players"):
			var p3d := p_node as Node3D
			if p3d:
				var my_d := global_position.distance_to(p3d.global_position)
				if my_d <= 2.75:
					var closer_exists := false
					for other_node in get_tree().get_nodes_in_group("world_dropped_items"):
						if other_node != self and other_node is Node3D:
							if (other_node as Node3D).global_position.distance_to(p3d.global_position) < my_d - 0.02:
								closer_exists = true
								break
					if not closer_exists:
						now_focus = true
						break
		if now_focus != _is_closest_focus:
			_is_closest_focus = now_focus
			_refresh_ground_label(_last_live_detail)

		var item_id: String = str(item_data.get("id", ""))
		var on := is_powered_on()
		_bob_phase += delta * 2.2
		if _visual_holder:
			if not (item_id == "uv_blacklight" and on):
				_visual_holder.rotation.y += delta * 0.85
			_visual_holder.position.y = 0.16 + sin(_bob_phase) * 0.025

		if not on:
			return

		var g := _find_nearest_ghost()
		var ghost_dist := global_position.distance_to(g.global_position) if g else 999.0
		var emf_lvl: int = 1
		var temp_c: float = 20.5
		if g:
			if ghost_dist <= 2.6:
				emf_lvl = 5
				temp_c = -6.8
			elif ghost_dist <= 3.6 or g.current_phase == StoreGhost.GhostPhase.VISIBLE_HUNT:
				emf_lvl = 4
				temp_c = -3.8
			elif ghost_dist <= 6.5:
				emf_lvl = 3
				temp_c = 1.5
			elif ghost_dist <= 12.0:
				emf_lvl = 2
				temp_c = 5.5

		_current_ground_emf = emf_lvl
		_current_ground_temp_c = temp_c

		match item_id:
			"emf_reader":
				if g and emf_lvl >= 5:
					GameState.record_evidence(g.store_id, "EMF-5", 35)
				var intervals := [1.30, 0.58, 0.30, 0.15, 0.075]
				var pitches := [0.88, 1.05, 1.25, 1.50, 1.80]
				var colors := [
					Color(0.15, 0.95, 0.30),
					Color(0.65, 0.95, 0.20),
					Color(1.00, 0.88, 0.15),
					Color(1.00, 0.50, 0.10),
					Color(1.00, 0.12, 0.18),
				]
				var idx := clampi(emf_lvl - 1, 0, 4)
				ground_status_light.light_color = colors[idx]
				ground_status_light.light_energy = 0.8 + float(emf_lvl) * 0.35
				_ground_beep_timer += delta
				if _ground_beep_timer >= intervals[idx]:
					_ground_beep_timer = 0.0
					if ground_audio and ground_audio.stream:
						ground_audio.pitch_scale = pitches[idx]
						ground_audio.volume_db = -8.0 + float(idx) * 2.0
						ground_audio.play()
				_refresh_ground_label("EMF LVL %d/5" % emf_lvl)

			"thermal_scanner":
				if g and temp_c < 0.0:
					GameState.record_evidence(g.store_id, "Freezing Temps", 35)
				ground_status_light.light_color = Color(0.25, 0.92, 1.0) if temp_c < 0.0 else Color(0.35, 1.0, 0.65)
				ground_status_light.light_energy = 1.2
				if temp_c < 0.0:
					_ground_beep_timer += delta
					if _ground_beep_timer >= 1.8:
						_ground_beep_timer = 0.0
						if ground_audio and ground_audio.stream:
							ground_audio.pitch_scale = 1.15
							ground_audio.volume_db = -6.0
							ground_audio.play()
				_refresh_ground_label("%.1f°C %s" % [temp_c, "FREEZING!" if temp_c < 0.0 else "AMBIENT"])

			"spirit_box":
				var freq: float = float(item_data.get("spirit_freq", 87.9))
				ground_status_light.light_color = Color(0.95, 0.65, 0.28)
				ground_status_light.light_energy = 1.1
				if ground_audio and ground_audio.stream:
					if not ground_audio.playing:
						ground_audio.play()
					ground_audio.volume_db = lerpf(-13.5, -19.5, clampf(ghost_dist / 16.0, 0.0, 1.0))
				var tell_str := g.get_disposition_tell_short() if (g and ghost_dist <= 16.0) else "STATIC SCAN"
				_refresh_ground_label("%.1f FM | %s" % [freq, tell_str])

			"uv_blacklight":
				ground_status_light.light_color = Color(0.68, 0.35, 1.0)
				ground_status_light.light_energy = 1.4
				var forward := -global_transform.basis.z.normalized()
				for p in get_tree().get_nodes_in_group("uv_evidence_prints"):
					var uv_p := p as UVEvidencePrint
					if uv_p:
						var to_p := uv_p.global_position - global_position
						if to_p.length() <= 8.5 and rad_to_deg(forward.angle_to(to_p.normalized())) <= 28.0:
							uv_p.illuminate_by_uv()
				for gr in get_tree().get_nodes_in_group("hallway_gremlins"):
					var grem := gr as HallwayGremlin
					if grem:
						var to_g := grem.global_position - global_position
						if to_g.length() <= 8.5 and rad_to_deg(forward.angle_to(to_g.normalized())) <= 28.0:
							grem.stun_gremlin("Floor UV Blacklight")
				_refresh_ground_label("UV BEAM ACTIVE")


# ------------------------------------------------------------------------------
# 6. Player Death Inventory Crate (Hands-Only Carry, Unusable Until Opened ->
#    Spills All Stored Player Items onto the Floor Near Where Opened!)
# ------------------------------------------------------------------------------
class DeathInventoryCrate extends Node3D:
	var stored_items: Array[Dictionary] = []
	var killer_name: String = "Store Ghost"
	var label_3d: Label3D = null

	func _ready() -> void:
		add_to_group("death_inventory_crates")
		_build_crate_visuals(self)

		var beacon := OmniLight3D.new()
		beacon.position = Vector3(0, 0.65, 0)
		beacon.light_color = Color(1.0, 0.45, 0.18)
		beacon.light_energy = 2.8
		beacon.omni_range = 6.0
		add_child(beacon)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0, 0.92, 0)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 21
		label_3d.outline_size = 6
		var count := stored_items.size()
		label_3d.text = (
			"DEATH INVENTORY CRATE (%d Items Sealed — Unusable Until Opened)\n" % count +
			"[F] Pick Up Crate in Hands (Hands-Only) | [E] Open Crate & Spill Items to Floor"
		)
		label_3d.modulate = Color(1.0, 0.62, 0.28)
		add_child(label_3d)

	static func _build_crate_visuals(parent: Node3D) -> void:
		var body_mi := MeshInstance3D.new()
		var body_box := BoxMesh.new()
		body_box.size = Vector3(0.56, 0.34, 0.40)
		body_mi.mesh = body_box
		body_mi.position = Vector3(0, 0.17, 0)
		var body_mat := StandardMaterial3D.new()
		body_mat.albedo_color = Color(0.85, 0.40, 0.14)
		body_mat.roughness = 0.45
		body_mat.metallic = 0.25
		body_mi.material_override = body_mat
		parent.add_child(body_mi)

		var lid_mi := MeshInstance3D.new()
		var lid_box := BoxMesh.new()
		lid_box.size = Vector3(0.59, 0.06, 0.43)
		lid_mi.mesh = lid_box
		lid_mi.position = Vector3(0, 0.36, 0)
		var lid_mat := StandardMaterial3D.new()
		lid_mat.albedo_color = Color(0.18, 0.20, 0.24)
		lid_mat.metallic = 0.65
		lid_mat.roughness = 0.35
		lid_mi.material_override = lid_mat
		parent.add_child(lid_mi)

		# Glowing amber latch strip
		var latch_mi := MeshInstance3D.new()
		var latch_box := BoxMesh.new()
		latch_box.size = Vector3(0.22, 0.08, 0.44)
		latch_mi.mesh = latch_box
		latch_mi.position = Vector3(0, 0.33, 0)
		var latch_mat := StandardMaterial3D.new()
		latch_mat.albedo_color = Color(1.0, 0.78, 0.20)
		latch_mat.emission_enabled = true
		latch_mat.emission = Color(1.0, 0.65, 0.15)
		latch_mat.emission_energy_multiplier = 2.0
		latch_mi.material_override = latch_mat
		parent.add_child(latch_mi)

	func open_and_spill_to_floor(world_parent: Node) -> void:
		DeathInventoryCrate.spill_items_onto_floor(world_parent, global_position, stored_items)
		queue_free()

	static func spill_items_onto_floor(world_parent: Node, center_pos: Vector3, items: Array) -> void:
		var count := items.size()
		if count == 0:
			GameState.status_message.emit("Opened Death Inventory Crate (Crate was empty).", Color(0.8, 0.85, 0.9))
			return
		var radius := 0.90
		for i in range(count):
			var angle := (TAU * float(i)) / float(max(count, 1))
			var drop_pos := center_pos + Vector3(cos(angle) * radius, 0.0, sin(angle) * radius)
			var drop := WorldDroppedItem.new()
			drop.item_data = (items[i] as Dictionary).duplicate(true)
			drop.position = drop_pos
			world_parent.add_child(drop)
		GameState.status_message.emit(
			"OPENED DEATH INVENTORY CRATE! %d items dropped onto the floor around you — press [F] on each item to equip without overriding your 4-slot hotbar!" % count,
			Color(0.45, 1.0, 0.68)
		)

