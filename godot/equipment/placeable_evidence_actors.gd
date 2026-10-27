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
			if ghost and ghost.visible and global_position.distance_to(ghost.global_position) <= 4.8:
				if _cooldown <= 0.0:
					_cooldown = 4.5
					trigger_count += 1
					motion_active_timer = 2.8
					readout_label.text = "IR MOTION ALERT! [Invisible Presence | Hits: %d]" % trigger_count
					readout_label.modulate = Color(0.25, 1.0, 0.55)
					GameState.status_message.emit("IR Motion Sensor [%s] Triggered (#%d) — Spectral presence crossed aisle!" % [store_id, trigger_count], Color(0.3, 1.0, 0.85))


# ------------------------------------------------------------------------------
# 3. Deployable Ectoplasm Canister Trap (Path B: Forceful Ghost Extraction)
# ------------------------------------------------------------------------------
class EctoplasmCanisterTrap extends Node3D:
	var is_captured: bool = false
	var vortex_light: OmniLight3D = null
	var label_3d: Label3D = null

	func _ready() -> void:
		add_to_group("ectoplasm_canisters")
		var glb_path := "res://SourceAssets/Equipment/Prepared/SM_EctoplasmCanister.glb"
		if ResourceLoader.exists(glb_path):
			var packed := load(glb_path) as PackedScene
			if packed:
				add_child(packed.instantiate())

		vortex_light = OmniLight3D.new()
		vortex_light.position = Vector3(0, 0.45, 0)
		vortex_light.light_color = Color(0.25, 1.0, 0.6)
		vortex_light.light_energy = 2.5
		vortex_light.omni_range = 4.5
		add_child(vortex_light)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0, 0.65, 0)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 22
		label_3d.outline_size = 6
		label_3d.text = "ECTOPLASM CANISTER TRAP [ARMED — Weaken Ghost to 100% with Proton Gun]"
		label_3d.modulate = Color(0.35, 1.0, 0.65)
		add_child(label_3d)

	func _physics_process(_delta: float) -> void:
		if Engine.is_editor_hint() or is_captured:
			return
		for g in get_tree().get_nodes_in_group("store_ghosts"):
			var ghost := g as StoreGhost
			if ghost and not ghost.is_resolved and global_position.distance_to(ghost.global_position) <= 5.0:
				if ghost.weakened_percent >= 100.0:
					is_captured = true
					ghost.resolve_forcefully_into_canister(global_position)
					label_3d.text = "ECTOPLASM CANISTER [FILLED: %s — +$150 BOUNTY PAID!]" % ghost.ghost_name
					label_3d.modulate = Color(1.0, 0.92, 0.35)
					vortex_light.light_color = Color(1.0, 0.85, 0.25)


# ------------------------------------------------------------------------------
# 4. Hallway Gremlin (95cm Low-Poly Concourse Pest — Interferes with Light Carts,
#    and when hit with UV Light runs away and disappears into the map walls for a while!)
# ------------------------------------------------------------------------------
class HallwayGremlin extends CharacterBody3D:
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
		label_3d.text = "Hallway Gremlin (95cm Non-Territorial Pest)\n[Shine UV Blacklight to Make Him Run Away into the Walls!]"
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

	## When hit with UV Blacklight (or Flash / Proton), the Gremlin bolts away and disappears into the map walls for 75s!
	func stun_gremlin(reason: String) -> void:
		if is_fleeing_into_wall or is_vanished_in_walls:
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

	func _enter_vanished_in_walls_state() -> void:
		is_fleeing_into_wall = false
		is_vanished_in_walls = true
		vanished_in_walls_timer = 75.0
		visible = false
		velocity = Vector3.ZERO

	func _find_active_light_cart_to_sabotage() -> MallCartSystem:
		if sabotage_cooldown > 0.0 or is_fleeing_into_wall or is_vanished_in_walls:
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
		if Engine.is_editor_hint():
			return
		if sabotage_cooldown > 0.0:
			sabotage_cooldown = maxf(0.0, sabotage_cooldown - delta)

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
				label_3d.text = "Hallway Gremlin (95cm Non-Territorial Pest)"
				label_3d.modulate = Color(1.0, 0.68, 0.32)
				if anim_player and anim_player.has_animation("CharacterArmature|Walk"):
					anim_player.play("CharacterArmature|Walk", -1, 1.0)
			return

		# State 2: Sprinting away toward the nearest map wall to phase/disappear into the wall!
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
				label_3d.text = "Hallway Gremlin [Switched Off Your Light Cart! Shine UV to Chase Off!]"
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

	func _ready() -> void:
		add_to_group("world_dropped_items")
		_bob_phase = randf() * TAU
		_visual_holder = Node3D.new()
		_visual_holder.position = Vector3(0, 0.16, 0)
		add_child(_visual_holder)

		var model_path: String = item_data.get("model_path", "")
		var spawned_mesh := false
		if not model_path.is_empty() and ResourceLoader.exists(model_path):
			var packed := load(model_path) as PackedScene
			if packed:
				var inst := packed.instantiate() as Node3D
				_visual_holder.add_child(inst)
				spawned_mesh = true

		if not spawned_mesh:
			var mi := MeshInstance3D.new()
			var bm := BoxMesh.new()
			bm.size = Vector3(0.28, 0.16, 0.22)
			mi.mesh = bm
			var mat := StandardMaterial3D.new()
			mat.albedo_color = Color(0.35, 0.82, 1.0)
			mi.material_override = mat
			_visual_holder.add_child(mi)

		# Small floor ring pad so dropped items are easy to spot in dark concourses
		var pad := MeshInstance3D.new()
		var cyl := CylinderMesh.new()
		cyl.top_radius = 0.24
		cyl.bottom_radius = 0.26
		cyl.height = 0.02
		pad.mesh = cyl
		pad.position = Vector3(0, 0.01, 0)
		_pad_mat = StandardMaterial3D.new()
		var is_hands_only: bool = not item_data.get("can_store_in_hotbar", true)
		_pad_mat.albedo_color = Color(1.0, 0.68, 0.22) if is_hands_only else Color(0.28, 0.90, 1.0)
		_pad_mat.emission_enabled = true
		_pad_mat.emission = _pad_mat.albedo_color
		_pad_mat.emission_energy_multiplier = 1.6
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
		ground_uv_spotlight.light_energy = 6.5
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
		label_3d.position = Vector3(0, 0.58, 0)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 20
		label_3d.outline_size = 6
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
		if label_3d == null:
			return
		var item_name: String = str(item_data.get("name", "Equipment Item"))
		var is_hands_only: bool = not bool(item_data.get("can_store_in_hotbar", true))
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

