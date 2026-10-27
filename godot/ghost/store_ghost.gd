class_name StoreGhost
extends CharacterBody3D
## 4-Phase Store Ghost AI using 1:1 Meter Normalized Low-Poly Quaternius Characters
## (Ghost_Clerk_LowPoly.glb & Ghost_Evelyn_LowPoly.glb), Dual Store Resolution,
## Enraged Backfire Hunt on Corrupted Ghosts, Instant-Kill Catch,
## Store Territory Confinement (Floor 1 ghosts strictly stay inside their store territory;
## only Upper-Floor Floor 2+ ghosts leave their store briefly to continue pursuit),
## and Wheeled Light Cart Circle-of-Safety Edge Blocking.

enum GhostPhase {
	INITIAL_ACTIVITY = 0,
	VISIBLE_WARNING = 1,
	VISIBLE_HUNT = 2,
	COOLDOWN = 3,
}

signal phase_changed(store_id: String, new_phase: GhostPhase, phase_name: String)
signal ghost_manifested_event(store_id: String, world_pos: Vector3)
signal prop_disturbed(store_id: String, world_pos: Vector3)

@export var store_id: String = "East_F0_02"
@export var ghost_name: String = "Arthur Pendelton (The Night Inventory Clerk)"
@export var floor_level: int = 1
@export var home_position: Vector3 = Vector3(21.0, 0.0, 0.0)
@export var doorway_threshold_x: float = 14.2
@export var store_territory_aabb: AABB = AABB(Vector3(14.2, -0.5, -6.6), Vector3(13.4, 6.5, 13.2))
@export var can_briefly_pursue_outside_store: bool = false
@export var max_outside_pursuit_duration: float = 6.0
@export var max_outside_pursuit_distance: float = 10.0
@export var hunt_speed: float = 1.80
@export var is_corrupted_hostile: bool = false
@export var required_spirit_freq: float = 87.9

var current_phase: GhostPhase = GhostPhase.INITIAL_ACTIVITY
var phase_timer: float = 0.0
var phase_duration: float = 32.0
var disturbance_timer: float = 0.0
var stun_timer: float = 0.0
var current_outside_pursuit_timer: float = 0.0
var is_retreating_to_store: bool = false
var is_blocked_by_light_circle_edge: bool = false
var current_emf_level: int = 2
var local_temperature_c: float = 4.5
var weakened_percent: float = 0.0
var is_resolved: bool = false
var is_enraged: bool = false
var _hover_phase: float = 0.0
var _net_ghost_sync_timer: float = 0.0
var _net_target_pos: Vector3 = Vector3.ZERO
var _net_target_yaw: float = 0.0

var visual_root: Node3D = null
var spectral_omni: OmniLight3D = null
var ghost_audio: AudioStreamPlayer3D = null
var linked_push_cart: MallCartSystem = null
var overhead_label: Label3D = null

func _ready() -> void:
	name = "StoreGhost_%s" % store_id
	add_to_group("store_ghosts")
	home_position = global_position
	_net_target_pos = global_position
	if not Engine.is_editor_hint() and "ghost_profiles" in GameState:
		var prof: Dictionary = GameState.ghost_profiles.get(store_id, {})
		if not prof.is_empty():
			ghost_name = "%s (%s)" % [prof.get("name", "Entity"), prof.get("title", "Store Ghost")]
			floor_level = int(prof.get("floor_level", floor_level))
			can_briefly_pursue_outside_store = bool(prof.get("can_briefly_pursue_outside_store", floor_level >= 2))
			if prof.has("territory_aabb"):
				store_territory_aabb = prof["territory_aabb"]
			is_corrupted_hostile = prof.get("is_corrupted_hostile", false)
			required_spirit_freq = prof.get("spirit_box_channel", 87.9)
	else:
		can_briefly_pursue_outside_store = (floor_level >= 2)

	_build_ghost_character_visuals()
	_setup_ghost_audio()
	if not Engine.is_editor_hint():
		AcousticVoipManager.voice_activity_detected.connect(_on_player_voice_activity)
		AcousticVoipManager.voice_command_recognized.connect(_on_voice_command_recognized)
	_enter_phase(GhostPhase.INITIAL_ACTIVITY)

func _build_ghost_character_visuals() -> void:
	visual_root = Node3D.new()
	add_child(visual_root)

	var glb_path := "res://SourceAssets/Characters/NormalizedLowPoly/Ghost_Clerk_LowPoly.glb" if store_id == "East_F0_02" else "res://SourceAssets/Characters/NormalizedLowPoly/Ghost_Evelyn_LowPoly.glb"
	if ResourceLoader.exists(glb_path):
		var packed := load(glb_path) as PackedScene
		if packed:
			var inst := packed.instantiate() as Node3D
			inst.scale = Vector3.ONE # Exact 1.80m / 1.76m normalized height!
			inst.rotation_degrees = Vector3(0.0, 180.0, 0.0)
			visual_root.add_child(inst)
			_apply_spectral_material_recursive(inst)

	spectral_omni = OmniLight3D.new()
	spectral_omni.position = Vector3(0, 1.25, 0)
	spectral_omni.light_color = Color(1.0, 0.25, 0.3) if is_corrupted_hostile else Color(0.3, 0.88, 1.0)
	spectral_omni.light_energy = 1.8
	spectral_omni.omni_range = 4.8
	visual_root.add_child(spectral_omni)

	overhead_label = Label3D.new()
	overhead_label.position = Vector3(0, 2.02, 0)
	overhead_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	overhead_label.font_size = 22
	overhead_label.outline_size = 6
	var territory_tag := "Upper-Floor Brief Pursuit (6s)" if can_briefly_pursue_outside_store else "Store Territory Bound"
	overhead_label.text = "%s\n[%s]" % [ghost_name, territory_tag]
	overhead_label.modulate = Color(1.0, 0.4, 0.4) if is_corrupted_hostile else Color(0.5, 0.95, 1.0)
	visual_root.add_child(overhead_label)

	var col := CollisionShape3D.new()
	var shape := CapsuleShape3D.new()
	shape.radius = 0.35
	shape.height = 1.8
	col.shape = shape
	col.position = Vector3(0, 0.9, 0)
	add_child(col)

func _apply_spectral_material_recursive(node: Node) -> void:
	if node is MeshInstance3D:
		var mi := node as MeshInstance3D
		var mat := StandardMaterial3D.new()
		mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		var tint := Color(1.0, 0.28, 0.32, 0.78) if is_corrupted_hostile else Color(0.38, 0.88, 1.0, 0.76)
		mat.albedo_color = tint
		mat.emission_enabled = true
		mat.emission = Color(0.95, 0.18, 0.22) if is_corrupted_hostile else Color(0.22, 0.78, 1.0)
		mat.emission_energy_multiplier = 2.0
		mi.material_override = mat
	for c in node.get_children():
		_apply_spectral_material_recursive(c)

func _setup_ghost_audio() -> void:
	ghost_audio = AudioStreamPlayer3D.new()
	ghost_audio.bus = &"GhostAudio"
	ghost_audio.max_distance = 24.0
	ghost_audio.unit_size = 4.0
	var wav_path := "res://SourceAssets/GhostSounds/SW_Ghost_InvisibleMoan.wav"
	if ResourceLoader.exists(wav_path):
		ghost_audio.stream = load(wav_path)
	add_child(ghost_audio)

## Returns true if `pt` (XZ + floor Y) is inside this ghost's store territory box.
func is_point_in_store_territory(pt: Vector3, margin: float = 0.0) -> bool:
	if absf(pt.y - home_position.y) > 3.2:
		return false
	var min_x := store_territory_aabb.position.x - margin
	var max_x := store_territory_aabb.end.x + margin
	var min_z := store_territory_aabb.position.z - margin
	var max_z := store_territory_aabb.end.z + margin
	return pt.x >= min_x and pt.x <= max_x and pt.z >= min_z and pt.z <= max_z

## Clamps a world position so it remains strictly inside this ghost's store territory box.
func clamp_point_to_store_territory(pt: Vector3) -> Vector3:
	return Vector3(
		clampf(pt.x, store_territory_aabb.position.x, store_territory_aabb.end.x),
		pt.y,
		clampf(pt.z, store_territory_aabb.position.z, store_territory_aabb.end.z)
	)

## Returns the horizontal distance from `pt` outside the nearest edge of `store_territory_aabb` (0.0 if inside).
func get_distance_outside_store_territory(pt: Vector3) -> float:
	var clamped := clamp_point_to_store_territory(pt)
	return Vector2(pt.x - clamped.x, pt.z - clamped.z).length()

func _physics_process(delta: float) -> void:
	if is_resolved:
		visual_root.visible = false
		return

	if is_instance_valid(linked_push_cart) and linked_push_cart.current_seal_state == MallCartSystem.DoorwaySealState.DORMANT:
		visual_root.visible = false
		current_emf_level = 1
		return

	_hover_phase += delta * 4.5
	visual_root.position.y = 0.06 + sin(_hover_phase) * 0.05

	# In Co-op Multiplayer, the Host runs the authoritative Ghost AI & State Machine and streams to Clients
	if MultiplayerManager != null and MultiplayerManager.is_coop_active and not MultiplayerManager.is_host():
		global_position = global_position.lerp(_net_target_pos, clampf(delta * 14.0, 0.0, 1.0))
		rotation.y = lerp_angle(rotation.y, _net_target_yaw, clampf(delta * 14.0, 0.0, 1.0))
		return

	# Enforce Light Cart Circle of Safety exclusion even if a player wheels the Light Cart into the ghost!
	_enforce_light_cart_circle_exclusion()

	if stun_timer > 0.0:
		stun_timer = maxf(0.0, stun_timer - delta)
		velocity = Vector3.ZERO
		_sync_ghost_to_clients(delta)
		return

	phase_timer += delta
	if phase_timer >= phase_duration:
		_advance_phase()

	match current_phase:
		GhostPhase.INITIAL_ACTIVITY:
			_tick_initial_activity(delta)
		GhostPhase.VISIBLE_WARNING:
			_tick_visible_warning(delta)
		GhostPhase.VISIBLE_HUNT:
			_tick_visible_hunt(delta)
		GhostPhase.COOLDOWN:
			_tick_cooldown(delta)

	_sync_ghost_to_clients(delta)

func _sync_ghost_to_clients(delta: float) -> void:
	if MultiplayerManager == null or not MultiplayerManager.is_coop_active or not multiplayer.has_multiplayer_peer() or not MultiplayerManager.is_host():
		return
	_net_ghost_sync_timer += delta
	if _net_ghost_sync_timer >= 0.05:
		_net_ghost_sync_timer = 0.0
		_rpc_sync_ghost_state.rpc(
			global_position,
			rotation.y,
			int(current_phase),
			weakened_percent,
			is_resolved,
			is_enraged,
			is_blocked_by_light_circle_edge,
			visual_root.visible
		)

@rpc("authority", "call_remote", "unreliable_ordered")
func _rpc_sync_ghost_state(
	pos: Vector3,
	yaw: float,
	phase_int: int,
	weak_pct: float,
	resolved: bool,
	enraged: bool,
	blocked_edge: bool,
	vis: bool
) -> void:
	_net_target_pos = pos
	_net_target_yaw = yaw
	var next_phase := phase_int as GhostPhase
	if next_phase != current_phase and not resolved:
		_enter_phase(next_phase)
	weakened_percent = weak_pct
	is_resolved = resolved
	is_enraged = enraged
	is_blocked_by_light_circle_edge = blocked_edge
	if visual_root:
		visual_root.visible = vis and not resolved
	if weak_pct > 0.0 and overhead_label:
		overhead_label.text = "%s [WEAKENED: %.0f%%]" % [ghost_name, weak_pct]

func _enforce_light_cart_circle_exclusion() -> void:
	is_blocked_by_light_circle_edge = false
	for cart in get_tree().get_nodes_in_group("light_carts"):
		var lc := cart as MallCartSystem
		if lc and lc.is_light_circle_active():
			var res := lc.block_ghost_at_circle_edge(global_position, global_position, 0.38)
			if res.get("blocked", false):
				is_blocked_by_light_circle_edge = true
				var pushed: Vector3 = res["position"]
				if not can_briefly_pursue_outside_store:
					pushed = clamp_point_to_store_territory(pushed)
				global_position = pushed

func _enter_phase(next_phase: GhostPhase) -> void:
	if is_resolved:
		return
	current_phase = next_phase
	phase_timer = 0.0
	match current_phase:
		GhostPhase.INITIAL_ACTIVITY:
			phase_duration = randf_range(28.0, 42.0)
			visual_root.visible = false
			current_outside_pursuit_timer = 0.0
			is_retreating_to_store = false
			current_emf_level = 3
			local_temperature_c = -1.8
			_set_store_lights_override(false, 1.0, 0.0)
		GhostPhase.VISIBLE_WARNING:
			phase_duration = 3.0
			visual_root.visible = true
			current_emf_level = 4
			local_temperature_c = -4.2
			_set_store_lights_override(true, 0.10, 0.35)
			if ghost_audio and ghost_audio.stream:
				ghost_audio.play()
			ghost_manifested_event.emit(store_id, global_position)
		GhostPhase.VISIBLE_HUNT:
			phase_duration = 20.0
			visual_root.visible = true
			current_outside_pursuit_timer = 0.0
			is_retreating_to_store = false
			current_emf_level = 5
			local_temperature_c = -6.8
			_set_store_lights_override(true, 0.0, 0.0)
			var growl_path := "res://SourceAssets/GhostSounds/SW_Ghost_HuntGrowl.wav"
			if ResourceLoader.exists(growl_path):
				ghost_audio.stream = load(growl_path)
				ghost_audio.play()
			GameState.status_message.emit("LETHAL HUNT ACTIVE (20s): %s is hunting in [%s]!" % [ghost_name, store_id], Color(1.0, 0.25, 0.25))
		GhostPhase.COOLDOWN:
			phase_duration = randf_range(35.0, 50.0)
			visual_root.visible = false
			is_enraged = false
			current_outside_pursuit_timer = 0.0
			is_retreating_to_store = false
			current_emf_level = 2
			local_temperature_c = 5.5
			_set_store_lights_override(false, 1.0, 0.0)
			global_position = home_position

	phase_changed.emit(store_id, current_phase, get_phase_label())

func _advance_phase() -> void:
	match current_phase:
		GhostPhase.INITIAL_ACTIVITY:
			_enter_phase(GhostPhase.VISIBLE_WARNING)
		GhostPhase.VISIBLE_WARNING:
			_enter_phase(GhostPhase.VISIBLE_HUNT)
		GhostPhase.VISIBLE_HUNT:
			_enter_phase(GhostPhase.COOLDOWN)
		GhostPhase.COOLDOWN:
			_enter_phase(GhostPhase.INITIAL_ACTIVITY)

func _tick_initial_activity(delta: float) -> void:
	disturbance_timer += delta
	if disturbance_timer >= 10.0:
		disturbance_timer = 0.0
		current_emf_level = 5
		prop_disturbed.emit(store_id, global_position)

func _tick_visible_warning(_delta: float) -> void:
	pass

func _tick_visible_hunt(delta: float) -> void:
	var active_speed := (hunt_speed * 1.32) if is_enraged else hunt_speed

	# Check if an upper-floor ghost is currently outside its store territory
	var currently_outside_store := not is_point_in_store_territory(global_position, 0.05)
	if currently_outside_store:
		if not can_briefly_pursue_outside_store:
			# Floor 1 ghosts never leave their store territory — snap back inside immediately
			global_position = clamp_point_to_store_territory(global_position)
			currently_outside_store = false
		else:
			current_outside_pursuit_timer += delta
			if current_outside_pursuit_timer >= max_outside_pursuit_duration or get_distance_outside_store_territory(global_position) > max_outside_pursuit_distance:
				is_retreating_to_store = true
	else:
		if is_retreating_to_store and global_position.distance_to(home_position) < 2.0:
			is_retreating_to_store = false
			current_outside_pursuit_timer = 0.0

	var target_player := _find_hunt_target()
	var desired_goal := home_position
	var chasing_player := false

	if not is_retreating_to_store and target_player != null:
		var p_pos := target_player.global_position
		var player_in_store := is_point_in_store_territory(p_pos, 0.15)
		if player_in_store:
			desired_goal = p_pos
			chasing_player = true
		elif can_briefly_pursue_outside_store and current_outside_pursuit_timer < max_outside_pursuit_duration and get_distance_outside_store_territory(p_pos) <= max_outside_pursuit_distance:
			# Upper-Floor Ghost (Floor 2+): Comfortable leaving its store briefly to continue pursuit!
			desired_goal = p_pos
			chasing_player = true
		else:
			# Floor 1 Store Ghost (concerned only with its store territory):
			# Stalks up to the interior edge of its store territory facing the player, but NEVER leaves its store!
			desired_goal = clamp_point_to_store_territory(p_pos)
			chasing_player = false
	elif not is_retreating_to_store:
		# If no player is visible/audible, check if a player left an active noise-making gadget ON on the floor inside the store!
		var noisy_drop := _find_noisy_dropped_item_in_store()
		if noisy_drop != null:
			desired_goal = clamp_point_to_store_territory(noisy_drop.global_position)
			chasing_player = false
		elif currently_outside_store:
			desired_goal = home_position
		else:
			velocity = Vector3.ZERO
			return
	elif currently_outside_store:
		desired_goal = home_position
	else:
		velocity = Vector3.ZERO
		return

	# Central Oval Safe Hub Sanctuary boundary on Floor 1: ghosts cannot enter the sunlit central plaza
	if floor_level == 1 and absf(desired_goal.x) < 10.5 and absf(desired_goal.z) < 8.5 and not is_enraged:
		velocity = Vector3.ZERO
		return

	var to_goal := desired_goal - global_position
	to_goal.y = 0.0
	if to_goal.length() <= 0.12:
		velocity = Vector3.ZERO
		return

	var dir := to_goal.normalized()
	var step_vec := dir * active_speed * delta
	if step_vec.length() > to_goal.length():
		step_vec = to_goal
	var candidate_pos := global_position + step_vec

	# 1. Enforce Floor 1 Store Territory Boundary (Floor 1 ghosts never leave their store territory!)
	if not can_briefly_pursue_outside_store:
		candidate_pos = clamp_point_to_store_territory(candidate_pos)

	# 2. Enforce Equipment Push Cart Doorway Seal
	var is_sealed := is_instance_valid(linked_push_cart) and linked_push_cart.current_seal_state == MallCartSystem.DoorwaySealState.SEALED
	if is_sealed and doorway_threshold_x > 0.0 and candidate_pos.x < doorway_threshold_x:
		candidate_pos.x = maxf(global_position.x, doorway_threshold_x)

	# 3. Enforce Wheeled Mobile Light Cart Circle of Safety & Circle-Edge Blocking!
	#    Ghosts cannot enter the downward light circle, and the edge of the circle blocks ghosts from leaving or entering areas!
	var blocked_by_circle := false
	for cart in get_tree().get_nodes_in_group("light_carts"):
		var lc := cart as MallCartSystem
		if lc and lc.is_light_circle_active():
			var block_res := lc.block_ghost_at_circle_edge(global_position, candidate_pos, 0.38)
			if block_res.get("blocked", false):
				blocked_by_circle = true
				candidate_pos = block_res["position"]
				if not can_briefly_pursue_outside_store:
					candidate_pos = clamp_point_to_store_territory(candidate_pos)

	is_blocked_by_light_circle_edge = blocked_by_circle
	var actual_move := candidate_pos - global_position
	actual_move.y = 0.0

	if blocked_by_circle or actual_move.length() < 0.002:
		velocity = Vector3.ZERO
		if to_goal.length() > 0.1:
			look_at(global_position + dir, Vector3.UP)
		return

	velocity = (actual_move / maxf(delta, 0.001))
	look_at(global_position + dir, Vector3.UP)
	move_and_slide()

	# Re-clamp after physics slide so Jolt never nudges a Floor 1 ghost outside its store territory or into a Light Cart circle
	if not can_briefly_pursue_outside_store:
		global_position = clamp_point_to_store_territory(global_position)
	_enforce_light_cart_circle_exclusion()

	# Lethal catch check (only if player is NOT inside a Light Cart's Circle of Safety and ghost is allowed at player's position)
	if chasing_player and is_instance_valid(target_player):
		for cart in get_tree().get_nodes_in_group("light_carts"):
			var lc := cart as MallCartSystem
			if lc and lc.is_point_inside_safety_circle(target_player.global_position, 0.05):
				return
		if not can_briefly_pursue_outside_store and not is_point_in_store_territory(target_player.global_position, 0.15):
			return
		if global_position.distance_to(target_player.global_position) < 1.35:
			if target_player.has_method("on_caught_by_store_ghost"):
				if target_player.has_method("is_local_player") and not bool(target_player.call("is_local_player")):
					var remote_pid: int = int(target_player.get("peer_id"))
					_rpc_notify_remote_player_caught.rpc_id(remote_pid, ghost_name)
				else:
					target_player.on_caught_by_store_ghost(ghost_name)
				_enter_phase(GhostPhase.COOLDOWN)

@rpc("authority", "call_remote", "reliable")
func _rpc_notify_remote_player_caught(killer_name: String) -> void:
	for p in get_tree().get_nodes_in_group("players"):
		if p and p.has_method("is_local_player") and bool(p.call("is_local_player")):
			p.call("on_caught_by_store_ghost", killer_name)
			break

func _tick_cooldown(_delta: float) -> void:
	pass

func _find_hunt_target() -> Node3D:
	var best_p: Node3D = null
	var best_score := 999.0
	for p in get_tree().get_nodes_in_group("players"):
		var p3d := p as Node3D
		if p3d == null or p3d.get("is_floating_essence") == true:
			continue
		if absf(p3d.global_position.y - home_position.y) > 3.2:
			continue
		var d := global_position.distance_to(p3d.global_position)
		var emitting_noise: bool = p3d.has_method("is_emitting_device_noise") and bool(p3d.call("is_emitting_device_noise"))

		# If the player is hiding inside a booth, they stay safe ONLY if their noise-making gadgets are turned OFF!
		# Leaving an EMF Reader, Spirit Box, or Thermal Scanner ON within 18m gives away their hiding spot to the ghost!
		if p3d.get("is_hidden_without_witness") == true:
			if emitting_noise and d <= 18.0:
				p3d.set("is_hidden_without_witness", false)
				var dev_list: Array = p3d.call("get_active_noisy_device_names") if p3d.has_method("get_active_noisy_device_names") else ["Active Gadget"]
				GameState.status_message.emit(
					"GHOST HEARD YOUR ACTIVE GADGET! %s located your hiding spot via [%s] — turn gadgets OFF [LMB] before hiding!" % [ghost_name, ", ".join(dev_list)],
					Color(1.0, 0.30, 0.25)
				)
			else:
				continue

		if not can_briefly_pursue_outside_store:
			# Floor 1 territorial ghost: targets players inside its store territory (or within 3.5m of doorway, or 7.0m if making device noise)
			var outside_detect := 7.0 if emitting_noise else 3.5
			if not is_point_in_store_territory(p3d.global_position, 0.15) and get_distance_outside_store_territory(p3d.global_position) > outside_detect:
				continue
		else:
			if get_distance_outside_store_territory(p3d.global_position) > max_outside_pursuit_distance:
				continue

		# Players with active noise-making gadgets attract the ghost more strongly
		var effective_score := (d * 0.65) if emitting_noise else d
		if d <= 24.0 and effective_score < best_score:
			best_score = effective_score
			best_p = p3d
	return best_p

func _find_noisy_dropped_item_in_store() -> Node3D:
	var best_drop: Node3D = null
	var best_d := 18.0
	for d_node in get_tree().get_nodes_in_group("world_dropped_items"):
		var drop := d_node as Node3D
		if drop and drop.has_method("is_emitting_ground_noise") and bool(drop.call("is_emitting_ground_noise")):
			if is_point_in_store_territory(drop.global_position, 1.5):
				var d := global_position.distance_to(drop.global_position)
				if d < best_d:
					best_d = d
					best_drop = drop
	return best_drop

func try_peaceful_resolution() -> void:
	_apply_peaceful_resolution_local()
	if MultiplayerManager != null and MultiplayerManager.is_coop_active and multiplayer.has_multiplayer_peer():
		_rpc_try_peaceful_resolution.rpc()

@rpc("any_peer", "call_remote", "reliable")
func _rpc_try_peaceful_resolution() -> void:
	_apply_peaceful_resolution_local()

func _apply_peaceful_resolution_local() -> void:
	if is_resolved:
		return
	if is_corrupted_hostile:
		is_enraged = true
		_enter_phase(GhostPhase.VISIBLE_HUNT)
		GameState.status_message.emit("PEACEFUL RITUAL BACKFIRED! %s is CORRUPTED (Hostile-Only) — ENRAGED HUNT TRIGGERED! Use Proton Ray Gun!" % ghost_name, Color(1.0, 0.2, 0.2))
		return

	is_resolved = true
	visual_root.visible = false
	_set_store_lights_override(false, 1.0, 0.0)
	GameState.mark_store_resolved(store_id, "Peaceful Lore Resolution (Saved Proton Charge!)", 120)

func apply_proton_beam(delta: float) -> void:
	_apply_proton_beam_local(delta)
	if MultiplayerManager != null and MultiplayerManager.is_coop_active and multiplayer.has_multiplayer_peer():
		_rpc_apply_proton_beam.rpc(delta)

@rpc("any_peer", "call_remote", "unreliable")
func _rpc_apply_proton_beam(delta: float) -> void:
	_apply_proton_beam_local(delta)

func _apply_proton_beam_local(delta: float) -> void:
	if is_resolved:
		return
	visual_root.visible = true
	weakened_percent = clampf(weakened_percent + 38.0 * delta, 0.0, 100.0)
	stun_timer = maxf(stun_timer, 0.35)
	overhead_label.text = "%s [WEAKENED: %.0f%%]" % [ghost_name, weakened_percent]
	if weakened_percent >= 100.0:
		overhead_label.text = "%s [100%% WEAKENED — DEPLOY ECTOPLASM CANISTER (E/G)!]" % ghost_name
		overhead_label.modulate = Color(0.3, 1.0, 0.55)

func resolve_forcefully_into_canister(canister_pos: Vector3) -> void:
	_apply_forceful_resolution_local(canister_pos)
	if MultiplayerManager != null and MultiplayerManager.is_coop_active and multiplayer.has_multiplayer_peer():
		_rpc_resolve_forcefully.rpc(canister_pos)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_resolve_forcefully(canister_pos: Vector3) -> void:
	_apply_forceful_resolution_local(canister_pos)

func _apply_forceful_resolution_local(_canister_pos: Vector3) -> void:
	if is_resolved:
		return
	is_resolved = true
	visual_root.visible = false
	_set_store_lights_override(false, 1.0, 0.0)
	GameState.mark_store_resolved(store_id, "Forceful Ectoplasm Canister Extraction", 150)

func apply_camera_flash_stun(duration: float = 4.0) -> void:
	_apply_camera_flash_stun_local(duration)
	if MultiplayerManager != null and MultiplayerManager.is_coop_active and multiplayer.has_multiplayer_peer():
		_rpc_apply_camera_flash_stun.rpc(duration)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_apply_camera_flash_stun(duration: float) -> void:
	_apply_camera_flash_stun_local(duration)

func _apply_camera_flash_stun_local(duration: float) -> void:
	stun_timer = maxf(stun_timer, duration)
	GameState.status_message.emit("XENON FLASH STUN! %s halted for %.1fs!" % [ghost_name, duration], Color(1.0, 0.95, 0.35))

func force_trigger_hunt_warning() -> void:
	if not is_resolved:
		_enter_phase(GhostPhase.VISIBLE_WARNING)

func _set_store_lights_override(active: bool, mult: float, flicker: float) -> void:
	for node in get_tree().get_nodes_in_group("mall_light_fixtures"):
		var fixture := node as MallLightFixture
		if fixture and fixture.store_id == store_id:
			fixture.set_ghost_flicker_override(active, mult, flicker)

func _on_player_voice_activity(speaker_pos: Vector3, loudness_norm: float, _is_talking: bool) -> void:
	if is_resolved or global_position.distance_to(speaker_pos) > 22.0:
		return
	var ac := AcousticVoipManager.compute_acoustic_transmission(speaker_pos, global_position)
	if ac.get("walls_hit", 0) <= 2 and loudness_norm > 0.25 and current_phase == GhostPhase.INITIAL_ACTIVITY:
		current_emf_level = 5

func _on_voice_command_recognized(phrase: String, speaker_pos: Vector3, spirit_box_freq: float) -> void:
	if is_resolved or global_position.distance_to(speaker_pos) > 16.0:
		return
	var lower := phrase.to_lower()
	_play_ghost_voice_response(lower)
	if "leave me alone" in lower:
		stun_timer = maxf(stun_timer, 3.0)
		GameState.status_message.emit("Voice Counterplay [\"Leave me alone\"]: %s paused disturbances for 3.0s!" % ghost_name, Color(0.5, 1.0, 0.8))
	elif "your shift is over" in lower:
		try_peaceful_resolution()
	elif "write in the book" in lower:
		GameState.record_evidence(store_id, "Ghost Writing", 40)
		GameState.discover_lore("LORE_LEDGER_EAST_F0_02", true)
	elif "give me a sign" in lower:
		current_emf_level = 5
		_set_store_lights_override(true, 0.45, 0.38)
		GameState.status_message.emit("Voice Prompt [\"Give me a sign\"]: %s spiked EMF Level 5 and flickered store lights!" % ghost_name, Color(1.0, 0.72, 0.35))
	elif "where are you" in lower or "who are you" in lower or "how old are you" in lower:
		current_emf_level = 5
		if absf(spirit_box_freq - required_spirit_freq) < 0.25:
			GameState.record_evidence(store_id, "Spirit Box (%.1f FM)" % required_spirit_freq, 45)
			var tell_msg := ""
			if "how old are you" in lower:
				tell_msg = "HOSTILE EVP: \"OLD ENOUGH TO BURY YOU IN STORAGE!\" [CORRUPTED TELL!]" if is_corrupted_hostile else "MOURNFUL EVP: \"Fifty-eight... worked here since Opening Day...\" [RECEPTIVE TELL!]"
			elif "where are you" in lower:
				tell_msg = "HOSTILE EVP: \"RIGHT BEHIND THE RACKS!\" [CORRUPTED TELL!]" if is_corrupted_hostile else "MOURNFUL EVP: \"Behind the checkout counter... counting inventory...\" [RECEPTIVE TELL!]"
			else:
				tell_msg = "HOSTILE STATIC THREAT: \"TOUCH MY RACKS AND BURN!\" [CORRUPTED TELL!]" if is_corrupted_hostile else "MOURNFUL EVP: \"Just one last shelf count before I clock out...\" [RECEPTIVE TELL!]"
			GameState.status_message.emit("SPIRIT BOX [%.1f FM] %s" % [required_spirit_freq, tell_msg], Color(1.0, 0.45, 0.45) if is_corrupted_hostile else Color(0.4, 1.0, 0.9))
		elif absf(spirit_box_freq - 104.7) < 0.2:
			GameState.discover_lore("LORE_RADIO_104_7", true)

func _play_ghost_voice_response(lower_phrase: String) -> void:
	if ghost_audio == null:
		return
	var wav_path := "res://SourceAssets/GhostSounds/SW_Ghost_InvisibleMoan.wav"
	if is_corrupted_hostile or "leave me alone" in lower_phrase:
		wav_path = "res://SourceAssets/GhostSounds/SW_Ghost_HuntGrowl.wav" if is_corrupted_hostile else "res://SourceAssets/GhostSounds/SW_Ghost_InvisibleBreath.wav"
	elif "where are you" in lower_phrase or "how old are you" in lower_phrase:
		wav_path = "res://SourceAssets/GhostSounds/SW_Ghost_InvisibleBreath.wav"
	if ResourceLoader.exists(wav_path):
		ghost_audio.stream = load(wav_path)
		ghost_audio.pitch_scale = 0.86 if is_corrupted_hostile else 1.04
		ghost_audio.play()

func get_disposition_tell_short() -> String:
	if is_resolved:
		return "RESOLVED"
	return "TELL: CORRUPTED (Hostile)" if is_corrupted_hostile else "TELL: RECEPTIVE (Peaceful)"

func get_phase_label() -> String:
	if is_resolved:
		return "CLEARED / AT REST"
	match current_phase:
		GhostPhase.INITIAL_ACTIVITY:
			return "Initial Activity (Store Territory)"
		GhostPhase.VISIBLE_WARNING:
			return "Visible Warning (3s Light Flicker 10%)"
		GhostPhase.VISIBLE_HUNT:
			if is_blocked_by_light_circle_edge:
				return "HUNT BLOCKED AT LIGHT CART CIRCLE EDGE!"
			return "ENRAGED HUNT!" if is_enraged else "VISIBLE HUNT (20s Pursuit)"
		GhostPhase.COOLDOWN:
			return "Cooldown (Safe Recovery)"
	return "Unknown"
