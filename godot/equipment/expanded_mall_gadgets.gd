@tool
class_name ExpandedMallGadgets
extends Node3D
## Megamall Anomaly — Expanded Player Equipment & Gadget Catalog (Categories A–F, Items 5–29).
## Implements 3D interactive world actors & static item catalog helpers from Docs/GameDesignBible.md Section 5:
## 1. LaserGridDomeActor ("Star-Shower" Laser Matrix Grid Dome)
## 2. PinkSaltLineActor (Himalayan Pink Salt Barrier Line)
## 3. EASPedestalWardActor (EAS Anti-Theft Pedestal Ward / Mall "Crucifix")
## 4. TalkBoyDecoyActor (Acoustic Decoy "Talk-Boy" Cassette Toy)
## 5. UVWallStencilActor (UV Chalk / Neon Spray Can Wayfinding Stencil)
## 6. BrickworkzFloorHazard (BRICKWORKZ Plastic Toy Brick Cluster / Mirror Glass Hazard)
## 7. MallCartChargerStation (Wall-Mounted Light Cart Charger + Gremlin Zapper Cage)
## 8. ZoltarFortuneBooth ("Ask Zoltar" Coin-Op Fortune Card Booth)
## 9. Static Catalog Helper get_expanded_item_catalog_entry(item_id: String) -> Dictionary


# ------------------------------------------------------------------------------
# 1. "Star-Shower" Laser Matrix Grid Dome (Item #6 — `laser_grid_dome`)
#    Casts a glowing green 3D laser dot-matrix cone/grid across a 5.5m store aisle.
#    Outlines invisible StoreGhosts in bright green laser silhouette and logs
#    "SLS / Laser Silhouette" evidence via GameState.record_evidence!
# ------------------------------------------------------------------------------
class LaserGridDomeActor extends Node3D:
	var store_id: String = "East_F0_02"
	var detection_radius: float = 5.5
	var silhouette_reveal_timer: float = 0.0
	var trigger_cooldown: float = 0.0
	var dome_light: OmniLight3D = null
	var dome_mat: StandardMaterial3D = null
	var beam_mat: StandardMaterial3D = null
	var readout_label: Label3D = null
	var beam_root: Node3D = null
	var _active_ghost: StoreGhost = null

	func _ready() -> void:
		add_to_group("laser_grid_domes")
		_build_dome_visuals()

	func _build_dome_visuals() -> void:
		# Base tripod puck
		var base_mi := MeshInstance3D.new()
		var base_cyl := CylinderMesh.new()
		base_cyl.top_radius = 0.14
		base_cyl.bottom_radius = 0.18
		base_cyl.height = 0.08
		base_mi.mesh = base_cyl
		base_mi.position = Vector3(0.0, 0.04, 0.0)
		var base_mat := StandardMaterial3D.new()
		base_mat.albedo_color = Color(0.14, 0.16, 0.20)
		base_mat.metallic = 0.65
		base_mat.roughness = 0.35
		base_mi.material_override = base_mat
		add_child(base_mi)

		# Glowing green Star-Shower laser projector hemisphere
		var dome_mi := MeshInstance3D.new()
		var sph := SphereMesh.new()
		sph.radius = 0.11
		sph.height = 0.22
		dome_mi.mesh = sph
		dome_mi.position = Vector3(0.0, 0.12, 0.0)
		dome_mat = StandardMaterial3D.new()
		dome_mat.albedo_color = Color(0.10, 0.95, 0.32)
		dome_mat.emission_enabled = true
		dome_mat.emission = Color(0.15, 1.0, 0.38)
		dome_mat.emission_energy_multiplier = 2.8
		dome_mi.material_override = dome_mat
		add_child(dome_mi)

		# 3D Laser Dot-Matrix Fan / Beam Lines across 5.5m aisle
		beam_root = Node3D.new()
		beam_root.position = Vector3(0.0, 0.14, 0.0)
		add_child(beam_root)

		beam_mat = StandardMaterial3D.new()
		beam_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		beam_mat.albedo_color = Color(0.18, 1.0, 0.42, 0.55)
		beam_mat.emission_enabled = true
		beam_mat.emission = Color(0.15, 1.0, 0.38)
		beam_mat.emission_energy_multiplier = 3.2

		var ray_count := 12
		for i in range(ray_count):
			var angle := (TAU * float(i)) / float(ray_count)
			var dir := Vector3(cos(angle), 0.18, sin(angle)).normalized()
			var ray_mi := MeshInstance3D.new()
			var box := BoxMesh.new()
			box.size = Vector3(0.012, 0.012, detection_radius * 0.85)
			ray_mi.mesh = box
			ray_mi.material_override = beam_mat
			ray_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			var mid := dir * (detection_radius * 0.42)
			ray_mi.transform = Transform3D(Basis.looking_at(dir, Vector3.UP), mid)
			beam_root.add_child(ray_mi)

			# Laser matrix dot node at the end of each ray
			var dot_mi := MeshInstance3D.new()
			var dot_sph := SphereMesh.new()
			dot_sph.radius = 0.035
			dot_sph.height = 0.07
			dot_mi.mesh = dot_sph
			dot_mi.position = Vector3(cos(angle) * (detection_radius * 0.72), 0.05, sin(angle) * (detection_radius * 0.72))
			dot_mi.material_override = beam_mat
			dot_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			beam_root.add_child(dot_mi)

		dome_light = OmniLight3D.new()
		dome_light.position = Vector3(0.0, 0.35, 0.0)
		dome_light.light_color = Color(0.18, 1.0, 0.42)
		dome_light.light_energy = 1.65
		dome_light.omni_range = detection_radius
		add_child(dome_light)

		readout_label = Label3D.new()
		readout_label.position = Vector3(0.0, 0.58, 0.0)
		readout_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		readout_label.font_size = 20
		readout_label.outline_size = 5
		readout_label.text = "STAR-SHOWER LASER MATRIX DOME [5.5m Grid Active]\n[F] Pick Up | Outlines Invisible Ghosts in Green Laser Dots"
		readout_label.modulate = Color(0.32, 1.0, 0.52)
		add_child(readout_label)

	func _physics_process(delta: float) -> void:
		if Engine.is_editor_hint():
			return
		if beam_root:
			beam_root.rotation.y += delta * 0.45
		if trigger_cooldown > 0.0:
			trigger_cooldown = maxf(0.0, trigger_cooldown - delta)

		var detected_ghost: StoreGhost = null
		for g in get_tree().get_nodes_in_group("store_ghosts"):
			var ghost := g as StoreGhost
			if ghost and not ghost.is_resolved:
				if absf(ghost.global_position.y - global_position.y) <= 3.0 and global_position.distance_to(ghost.global_position) <= detection_radius:
					detected_ghost = ghost
					break

		if detected_ghost != null:
			_active_ghost = detected_ghost
			store_id = detected_ghost.store_id
			silhouette_reveal_timer = 1.5
			if detected_ghost.visual_root:
				detected_ghost.visual_root.visible = true
			if detected_ghost.spectral_omni:
				detected_ghost.spectral_omni.light_color = Color(0.15, 1.0, 0.42)
				detected_ghost.spectral_omni.light_energy = 3.2

			var pulse := 0.5 + 0.5 * sin(Time.get_ticks_msec() * 0.018)
			if dome_light:
				dome_light.light_energy = 2.8 + 1.4 * pulse
			if dome_mat:
				dome_mat.emission_energy_multiplier = 4.8
			if beam_mat:
				beam_mat.emission_energy_multiplier = 5.2

			var disp_str := "TWITCHING HOSTILE SILHOUETTE (Corrupted!)" if detected_ghost.is_corrupted_hostile else "CALM ROUTINE SILHOUETTE (Receptive!)"
			if readout_label:
				readout_label.text = "LASER SILHOUETTE DETECTED: %s!\n[%s]" % [detected_ghost.ghost_name, disp_str]
				readout_label.modulate = Color(0.25, 1.0, 0.45)

			if trigger_cooldown <= 0.0:
				trigger_cooldown = 6.0
				GameState.record_evidence(detected_ghost.store_id, "SLS / Laser Silhouette", 45)
				GameState.status_message.emit(
					"LASER MATRIX DOME [%s]: Outlined %s in green laser dots (%s)!" % [
						detected_ghost.store_id, detected_ghost.ghost_name, disp_str
					],
					Color(0.28, 1.0, 0.55)
				)
		else:
			if silhouette_reveal_timer > 0.0:
				silhouette_reveal_timer = maxf(0.0, silhouette_reveal_timer - delta)
				if silhouette_reveal_timer <= 0.0 and is_instance_valid(_active_ghost):
					if _active_ghost.current_phase == StoreGhost.GhostPhase.INITIAL_ACTIVITY and _active_ghost.weakened_percent <= 0.0:
						if _active_ghost.visual_root:
							_active_ghost.visual_root.visible = false
					_active_ghost = null
			if dome_light:
				dome_light.light_energy = 1.65
			if dome_mat:
				dome_mat.emission_energy_multiplier = 2.8
			if beam_mat:
				beam_mat.emission_energy_multiplier = 3.2
			if readout_label:
				readout_label.text = "STAR-SHOWER LASER MATRIX DOME [5.5m Grid Active]\n[F] Pick Up | Outlines Invisible Ghosts in Green Laser Dots"
				readout_label.modulate = Color(0.32, 1.0, 0.52)


# ------------------------------------------------------------------------------
# 2. Himalayan Pink Salt Barrier Line (Item #11 — `pink_salt_pouch`)
#    1.8m wide glowing crystalline pink salt line poured across a doorway or aisle.
#    If a StoreGhost or HallwayGremlin steps within 1.6m:
#    - Slows/stuns the hunting ghost for 4.0s (`apply_camera_flash_stun(4.0)`),
#    - Spawns a guaranteed glowing `PlaceableEvidenceActors.UVEvidencePrint` right on the salt line,
#    - Consumes the salt line after blocking a hunt!
# ------------------------------------------------------------------------------
class PinkSaltLineActor extends Node3D:
	var store_id: String = "East_F0_02"
	var is_consumed: bool = false
	var salt_mat: StandardMaterial3D = null
	var salt_light: OmniLight3D = null
	var label_3d: Label3D = null
	var crystal_meshes: Array[MeshInstance3D] = []

	func _ready() -> void:
		add_to_group("pink_salt_lines")
		_build_salt_line_visuals()

	func _build_salt_line_visuals() -> void:
		salt_mat = StandardMaterial3D.new()
		salt_mat.albedo_color = Color(1.0, 0.58, 0.76)
		salt_mat.emission_enabled = true
		salt_mat.emission = Color(1.0, 0.45, 0.68)
		salt_mat.emission_energy_multiplier = 2.4
		salt_mat.roughness = 0.45

		# 1.8m wide base salt strip
		var strip_mi := MeshInstance3D.new()
		var strip_box := BoxMesh.new()
		strip_box.size = Vector3(1.80, 0.024, 0.16)
		strip_mi.mesh = strip_box
		strip_mi.position = Vector3(0.0, 0.012, 0.0)
		strip_mi.material_override = salt_mat
		add_child(strip_mi)
		crystal_meshes.append(strip_mi)

		# Crystalline salt mounds along the 1.8m line
		for i in range(9):
			var x_off := -0.80 + float(i) * 0.20
			var mound := MeshInstance3D.new()
			var sph := SphereMesh.new()
			sph.radius = 0.045
			sph.height = 0.055
			mound.mesh = sph
			mound.position = Vector3(x_off, 0.025, (float(i % 2) - 0.5) * 0.04)
			mound.material_override = salt_mat
			add_child(mound)
			crystal_meshes.append(mound)

		salt_light = OmniLight3D.new()
		salt_light.position = Vector3(0.0, 0.25, 0.0)
		salt_light.light_color = Color(1.0, 0.52, 0.75)
		salt_light.light_energy = 1.6
		salt_light.omni_range = 2.8
		add_child(salt_light)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0.0, 0.48, 0.0)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 19
		label_3d.outline_size = 5
		label_3d.text = "HIMALAYAN PINK SALT LINE [1.8m Ward Active]\n[Halts Hunting Ghosts 4.0s & Reveals Guaranteed UV Footprints!]"
		label_3d.modulate = Color(1.0, 0.68, 0.84)
		add_child(label_3d)

	func _spawn_guaranteed_uv_print(target_store_id: String, is_corrupted: bool) -> void:
		var uvp := PlaceableEvidenceActors.UVEvidencePrint.new()
		uvp.store_id = target_store_id
		uvp.is_corrupted_tell = is_corrupted
		uvp.print_title = "Pink Salt UV Footprint (%s)" % ("Clawed Step [CORRUPTED]" if is_corrupted else "Neat Boot Step [RECEPTIVE]")
		var parent_node := get_parent() if get_parent() != null else self
		parent_node.add_child(uvp)
		uvp.global_position = global_position + Vector3(0.0, 0.01, 0.0)
		uvp.illuminate_by_uv()

	func _consume_salt_line(reason: String) -> void:
		is_consumed = true
		if salt_mat:
			salt_mat.albedo_color = Color(0.45, 0.35, 0.40)
			salt_mat.emission_energy_multiplier = 0.2
		if salt_light:
			salt_light.light_energy = 0.15
		if label_3d:
			label_3d.text = "PINK SALT LINE [CONSUMED — %s]\n[UV Footprint Illuminated on Floor!]" % reason
			label_3d.modulate = Color(0.65, 0.55, 0.62)

	func _physics_process(_delta: float) -> void:
		if Engine.is_editor_hint() or is_consumed:
			return

		# 1. Check StoreGhosts stepping within 1.6m
		for g in get_tree().get_nodes_in_group("store_ghosts"):
			var ghost := g as StoreGhost
			if ghost and not ghost.is_resolved:
				if absf(ghost.global_position.y - global_position.y) <= 2.5 and global_position.distance_to(ghost.global_position) <= 1.6:
					ghost.apply_camera_flash_stun(4.0)
					_spawn_guaranteed_uv_print(ghost.store_id, ghost.is_corrupted_hostile)
					_consume_salt_line("Blocked %s (4.0s Stun)" % ghost.ghost_name)
					GameState.status_message.emit(
						"PINK SALT BARRIER TRIGGERED! %s stepped in Himalayan Pink Salt — stunned for 4.0s & guaranteed UV Footprint revealed!" % ghost.ghost_name,
						Color(1.0, 0.62, 0.82)
					)
					return

		# 2. Check HallwayGremlins stepping within 1.6m
		for gr in get_tree().get_nodes_in_group("hallway_gremlins"):
			var grem := gr as PlaceableEvidenceActors.HallwayGremlin
			if grem and not grem.is_captured and not grem.is_fleeing_into_wall and not grem.is_vanished_in_walls:
				if absf(grem.global_position.y - global_position.y) <= 2.5 and global_position.distance_to(grem.global_position) <= 1.6:
					grem.stun_gremlin("Himalayan Pink Salt Line")
					_spawn_guaranteed_uv_print(store_id, false)
					_consume_salt_line("Repelled Hallway Gremlin")
					return


# ------------------------------------------------------------------------------
# 3. EAS Anti-Theft Pedestal Ward / Mall "Crucifix" (Item #13 — `eas_pedestal_ward`)
#    Hacked retail anti-shoplifting gate pedestal (1.45m tall) placed on the floor.
#    If any StoreGhost within 6.5m enters VISIBLE_WARNING or VISIBLE_HUNT while not
#    burned out: blares retail alarm, cancels the hunt immediately (COOLDOWN + 5.0s stun),
#    and burns out its internal fuse!
# ------------------------------------------------------------------------------
class EASPedestalWardActor extends Node3D:
	var ward_radius: float = 6.5
	var is_burned_out: bool = false
	var led_mat: StandardMaterial3D = null
	var status_light: OmniLight3D = null
	var alarm_audio: AudioStreamPlayer3D = null
	var label_3d: Label3D = null

	func _ready() -> void:
		add_to_group("eas_pedestal_wards")
		_build_pedestal_visuals()

	func _build_pedestal_visuals() -> void:
		var frame_mat := StandardMaterial3D.new()
		frame_mat.albedo_color = Color(0.82, 0.84, 0.88)
		frame_mat.metallic = 0.45
		frame_mat.roughness = 0.35

		# Base plate
		var base_mi := MeshInstance3D.new()
		var base_box := BoxMesh.new()
		base_box.size = Vector3(0.46, 0.06, 0.24)
		base_mi.mesh = base_box
		base_mi.position = Vector3(0.0, 0.03, 0.0)
		base_mi.material_override = frame_mat
		add_child(base_mi)

		# Left & Right vertical retail security gate pillars (1.45m tall)
		for side_x in [-0.18, 0.18]:
			var pillar := MeshInstance3D.new()
			var p_box := BoxMesh.new()
			p_box.size = Vector3(0.06, 1.40, 0.12)
			pillar.mesh = p_box
			pillar.position = Vector3(side_x, 0.73, 0.0)
			pillar.material_override = frame_mat
			add_child(pillar)

		# Top crossbar + center sensor panel
		var top_bar := MeshInstance3D.new()
		var t_box := BoxMesh.new()
		t_box.size = Vector3(0.42, 0.07, 0.13)
		top_bar.mesh = t_box
		top_bar.position = Vector3(0.0, 1.42, 0.0)
		top_bar.material_override = frame_mat
		add_child(top_bar)

		# Top Status LED Dome
		var led_mi := MeshInstance3D.new()
		var sph := SphereMesh.new()
		sph.radius = 0.055
		sph.height = 0.11
		led_mi.mesh = sph
		led_mi.position = Vector3(0.0, 1.50, 0.0)
		led_mat = StandardMaterial3D.new()
		led_mat.albedo_color = Color(0.18, 1.0, 0.55)
		led_mat.emission_enabled = true
		led_mat.emission = Color(0.18, 1.0, 0.55)
		led_mat.emission_energy_multiplier = 3.2
		led_mi.material_override = led_mat
		add_child(led_mi)

		status_light = OmniLight3D.new()
		status_light.position = Vector3(0.0, 1.52, 0.0)
		status_light.light_color = Color(0.20, 1.0, 0.58)
		status_light.light_energy = 1.8
		status_light.omni_range = 4.5
		add_child(status_light)

		alarm_audio = AudioStreamPlayer3D.new()
		alarm_audio.bus = &"SFX"
		alarm_audio.max_distance = 26.0
		alarm_audio.unit_size = 4.0
		if ResourceLoader.exists("res://SourceAssets/EquipmentSounds/SW_EMF_Beep.wav"):
			alarm_audio.stream = load("res://SourceAssets/EquipmentSounds/SW_EMF_Beep.wav")
		add_child(alarm_audio)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0.0, 1.74, 0.0)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 20
		label_3d.outline_size = 6
		label_3d.text = "EAS ANTI-THEFT PEDESTAL WARD [ARMED — 6.5m Range]\n[Cancels 1 Ghost Hunt Immediately & Blows Internal Fuse]"
		label_3d.modulate = Color(0.35, 1.0, 0.68)
		add_child(label_3d)

	func _physics_process(_delta: float) -> void:
		if Engine.is_editor_hint() or is_burned_out:
			return

		for g in get_tree().get_nodes_in_group("store_ghosts"):
			var ghost := g as StoreGhost
			if ghost and not ghost.is_resolved:
				if ghost.current_phase == StoreGhost.GhostPhase.VISIBLE_WARNING or ghost.current_phase == StoreGhost.GhostPhase.VISIBLE_HUNT:
					if absf(ghost.global_position.y - global_position.y) <= 3.2 and global_position.distance_to(ghost.global_position) <= ward_radius:
						_trigger_hunt_cancellation(ghost)
						return

	func _trigger_hunt_cancellation(ghost: StoreGhost) -> void:
		is_burned_out = true
		ghost._enter_phase(StoreGhost.GhostPhase.COOLDOWN)
		ghost.stun_timer = 5.0

		if alarm_audio and alarm_audio.stream:
			alarm_audio.pitch_scale = 1.65
			alarm_audio.volume_db = 2.0
			alarm_audio.play()

		if led_mat:
			led_mat.albedo_color = Color(0.85, 0.42, 0.12)
			led_mat.emission = Color(1.0, 0.45, 0.12)
			led_mat.emission_energy_multiplier = 1.2
		if status_light:
			status_light.light_color = Color(1.0, 0.48, 0.15)
			status_light.light_energy = 0.85
		if label_3d:
			label_3d.text = "EAS ANTI-THEFT WARD [FUSE BURNED OUT — Hunt Aborted!]\n[Blocked %s!]" % ghost.ghost_name
			label_3d.modulate = Color(1.0, 0.62, 0.28)

		GameState.status_message.emit(
			"EAS ANTI-THEFT PEDESTAL WARD TRIGGERED! Retail alarm blared and CANCELLED %s's hunt (5.0s Stun | Internal Fuse Burned Out)!" % ghost.ghost_name,
			Color(1.0, 0.88, 0.32)
		)


# ------------------------------------------------------------------------------
# 4. Acoustic Decoy "Talk-Boy" Cassette Toy (Item #14 — `talkboy_decoy`)
#    Added to `"world_dropped_items"` and `"acoustic_decoys"`.
#    Loops a 4.0s recorded player voice clip for 25.0s (`is_emitting_ground_noise() -> true`),
#    luring sound-hunting StoreGhosts and HallwayGremlins toward the decoy's position!
# ------------------------------------------------------------------------------
class TalkBoyDecoyActor extends Node3D:
	var item_data: Dictionary = {}
	var active_timer: float = 25.0
	var loop_timer: float = 0.0
	var recorded_phrase: String = "Hey! Over here in this aisle!"
	var decoy_light: OmniLight3D = null
	var decoy_audio: AudioStreamPlayer3D = null
	var label_3d: Label3D = null

	func _ready() -> void:
		add_to_group("world_dropped_items")
		add_to_group("acoustic_decoys")
		if item_data.is_empty():
			item_data = ExpandedMallGadgets.get_expanded_item_catalog_entry("talkboy_decoy")
		_build_decoy_visuals()

	func _build_decoy_visuals() -> void:
		var body := MeshInstance3D.new()
		var box := BoxMesh.new()
		box.size = Vector3(0.26, 0.10, 0.18)
		body.mesh = box
		body.position = Vector3(0.0, 0.05, 0.0)
		var mat := StandardMaterial3D.new()
		mat.albedo_color = Color(0.72, 0.76, 0.82)
		mat.metallic = 0.45
		mat.roughness = 0.35
		body.material_override = mat
		add_child(body)

		decoy_light = OmniLight3D.new()
		decoy_light.position = Vector3(0.0, 0.22, 0.0)
		decoy_light.light_color = Color(1.0, 0.55, 0.22)
		decoy_light.light_energy = 1.8
		decoy_light.omni_range = 3.5
		add_child(decoy_light)

		decoy_audio = AudioStreamPlayer3D.new()
		decoy_audio.bus = &"VoIP_Room"
		decoy_audio.max_distance = 22.0
		decoy_audio.unit_size = 3.5
		if ResourceLoader.exists("res://SourceAssets/SpiritBox/SW_SpiritBox_Placeholder.wav"):
			decoy_audio.stream = load("res://SourceAssets/SpiritBox/SW_SpiritBox_Placeholder.wav")
		add_child(decoy_audio)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0.0, 0.48, 0.0)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 19
		label_3d.outline_size = 5
		label_3d.modulate = Color(1.0, 0.76, 0.35)
		add_child(label_3d)
		_refresh_label()

	func is_emitting_ground_noise() -> bool:
		return active_timer > 0.0

	func is_powered_on() -> bool:
		return active_timer > 0.0

	func can_power_toggle() -> bool:
		return true

	func toggle_ground_power() -> void:
		if active_timer > 0.0:
			active_timer = 0.0
		else:
			active_timer = 25.0
			loop_timer = 0.0
		_refresh_label()

	func _refresh_label() -> void:
		if label_3d == null:
			return
		if active_timer > 0.0:
			label_3d.text = "TALK-BOY ACOUSTIC DECOY [LOOPING VOICE: %.1fs Left]\n\"%s\" (Luring Ghosts & Gremlins!)" % [
				active_timer, recorded_phrase
			]
			label_3d.modulate = Color(1.0, 0.78, 0.32)
		else:
			label_3d.text = "TALK-BOY ACOUSTIC DECOY [TAPE FINISHED]\n[F] Pick Up | [E] Rewind & Replay (25s)"
			label_3d.modulate = Color(0.70, 0.76, 0.84)

	func _physics_process(delta: float) -> void:
		if Engine.is_editor_hint():
			return
		if active_timer <= 0.0:
			if decoy_light:
				decoy_light.light_energy = 0.0
			return

		active_timer = maxf(0.0, active_timer - delta)
		loop_timer += delta
		if decoy_light:
			decoy_light.light_energy = 1.4 + 0.8 * absf(sin(loop_timer * 6.0))

		if loop_timer >= 4.0:
			loop_timer = 0.0
			if decoy_audio and decoy_audio.stream:
				decoy_audio.pitch_scale = 1.12
				decoy_audio.play()
			if AcousticVoipManager:
				AcousticVoipManager.voice_activity_detected.emit(global_position, 0.92, true)

		# Lure nearby HallwayGremlins toward the Talk-Boy Decoy!
		for gr in get_tree().get_nodes_in_group("hallway_gremlins"):
			var grem := gr as PlaceableEvidenceActors.HallwayGremlin
			if grem and not grem.is_captured and not grem.is_fleeing_into_wall and not grem.is_vanished_in_walls:
				if absf(grem.global_position.y - global_position.y) <= 2.5 and global_position.distance_to(grem.global_position) <= 16.0:
					grem.patrol_origin = global_position

		_refresh_label()


# ------------------------------------------------------------------------------
# 5. UV Chalk / Neon Spray Can Stencil Actor (Item #15 — `uv_chalk_spray`)
#    Cycles through 3 high-emission neon wayfinding stencils on drifting hallway walls/floors:
#    0: "ARROW: -> EXIT TO CENTRAL OVAL" (Neon Cyan)
#    1: "CHECK: [✓] STORE CLEARED" (Neon Green)
#    2: "WARNING: [☠] CORRUPTED GHOST" (Neon Magenta)
# ------------------------------------------------------------------------------
class UVWallStencilActor extends Node3D:
	const STENCIL_TEXTS: Array[String] = [
		"ARROW: -> EXIT TO CENTRAL OVAL",
		"CHECK: [✓] STORE CLEARED",
		"WARNING: [☠] CORRUPTED GHOST",
	]
	const STENCIL_COLORS: Array[Color] = [
		Color(0.18, 0.96, 1.0),
		Color(0.22, 1.0, 0.45),
		Color(1.0, 0.22, 0.68),
	]

	var stencil_mode: int = 0
	var stencil_mat: StandardMaterial3D = null
	var stencil_light: OmniLight3D = null
	var label_3d: Label3D = null

	func _ready() -> void:
		add_to_group("uv_wall_stencils")
		_build_stencil_visuals()
		set_stencil_mode(stencil_mode)

	func _build_stencil_visuals() -> void:
		stencil_mat = StandardMaterial3D.new()
		stencil_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		stencil_mat.emission_enabled = true
		stencil_mat.emission_energy_multiplier = 3.8

		var plate := MeshInstance3D.new()
		var box := BoxMesh.new()
		box.size = Vector3(0.95, 0.32, 0.03)
		plate.mesh = box
		plate.position = Vector3(0.0, 0.22, 0.0)
		plate.material_override = stencil_mat
		plate.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(plate)

		stencil_light = OmniLight3D.new()
		stencil_light.position = Vector3(0.0, 0.26, 0.12)
		stencil_light.light_energy = 1.4
		stencil_light.omni_range = 3.2
		add_child(stencil_light)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0.0, 0.50, 0.05)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 22
		label_3d.outline_size = 6
		add_child(label_3d)

	func set_stencil_mode(mode: int) -> void:
		stencil_mode = posmod(mode, STENCIL_TEXTS.size())
		var col: Color = STENCIL_COLORS[stencil_mode]
		if stencil_mat:
			stencil_mat.albedo_color = Color(col.r, col.g, col.b, 0.65)
			stencil_mat.emission = col
		if stencil_light:
			stencil_light.light_color = col
		if label_3d:
			label_3d.text = "UV NEON STENCIL [%s]" % STENCIL_TEXTS[stencil_mode]
			label_3d.modulate = col

	func cycle_stencil_mode() -> int:
		set_stencil_mode(stencil_mode + 1)
		return stencil_mode


# ------------------------------------------------------------------------------
# 6. BRICKWORKZ Plastic Toy Brick Cluster / Floor Hazard (Item #18 — `floor_sweeper`)
#    Scattered plastic toy brick clusters on the floor of BRICKWORKZ (or broken glass).
#    Stepping within 1.25m while moving > 2.0 m/s crunches loudly, emits VoIP noise,
#    and spikes the nearest StoreGhost's EMF to 5!
#    Cleared by `sweep_clean()` using the Janitorial Acoustic Floor Sweeper (+$15).
# ------------------------------------------------------------------------------
class BrickworkzFloorHazard extends Node3D:
	var store_id: String = "East_F2_01"
	var is_swept_clean: bool = false
	var crunch_cooldown: float = 0.0
	var crunch_audio: AudioStreamPlayer3D = null
	var label_3d: Label3D = null
	var brick_root: Node3D = null

	func _ready() -> void:
		add_to_group("brickworkz_hazards")
		_build_brick_cluster_visuals()

	func _build_brick_cluster_visuals() -> void:
		brick_root = Node3D.new()
		add_child(brick_root)

		var brick_colors := [
			Color(0.95, 0.18, 0.18),
			Color(0.18, 0.55, 0.98),
			Color(0.98, 0.85, 0.15),
			Color(0.20, 0.85, 0.32),
		]
		for i in range(10):
			var b_mi := MeshInstance3D.new()
			var b_box := BoxMesh.new()
			b_box.size = Vector3(0.11, 0.045, 0.065)
			b_mi.mesh = b_box
			var ang := (TAU * float(i)) / 10.0
			var rad := 0.18 + float(i % 3) * 0.16
			b_mi.position = Vector3(cos(ang) * rad, 0.023, sin(ang) * rad)
			b_mi.rotation.y = ang * 1.7
			var mat := StandardMaterial3D.new()
			mat.albedo_color = brick_colors[i % brick_colors.size()]
			mat.roughness = 0.25
			b_mi.material_override = mat
			brick_root.add_child(b_mi)

		crunch_audio = AudioStreamPlayer3D.new()
		crunch_audio.bus = &"SFX"
		crunch_audio.max_distance = 22.0
		crunch_audio.unit_size = 3.5
		if ResourceLoader.exists("res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_On.wav"):
			crunch_audio.stream = load("res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_On.wav")
		add_child(crunch_audio)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0.0, 0.38, 0.0)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 19
		label_3d.outline_size = 5
		label_3d.text = "BRICKWORKZ PLASTIC FLOOR HAZARD [LOUD NOISE TRAP!]\n[Walk Slowly or Sweep Clean with Janitorial Floor Sweeper (+$15)]"
		label_3d.modulate = Color(1.0, 0.72, 0.28)
		add_child(label_3d)

	func sweep_clean() -> bool:
		if is_swept_clean:
			return false
		is_swept_clean = true
		if brick_root:
			brick_root.visible = false
		if label_3d:
			label_3d.text = "FLOOR SWEPT CLEAN [Silent Aisle]"
			label_3d.modulate = Color(0.45, 1.0, 0.68)
		GameState.add_cash(15, "Swept BRICKWORKZ Acoustic Floor Hazard Clean")
		return true

	func _physics_process(delta: float) -> void:
		if Engine.is_editor_hint() or is_swept_clean:
			return
		if crunch_cooldown > 0.0:
			crunch_cooldown = maxf(0.0, crunch_cooldown - delta)
			return

		for p in get_tree().get_nodes_in_group("players"):
			var player := p as CharacterBody3D
			if player and player.get("is_floating_essence") != true:
				if absf(player.global_position.y - global_position.y) <= 1.8 and global_position.distance_to(player.global_position) <= 1.25:
					var horiz_speed := Vector2(player.velocity.x, player.velocity.z).length()
					if horiz_speed > 2.0:
						crunch_cooldown = 3.5
						if crunch_audio and crunch_audio.stream:
							crunch_audio.pitch_scale = 1.45
							crunch_audio.play()
						if AcousticVoipManager:
							AcousticVoipManager.voice_activity_detected.emit(global_position, 0.95, true)
						for g in get_tree().get_nodes_in_group("store_ghosts"):
							var ghost := g as StoreGhost
							if ghost and not ghost.is_resolved and global_position.distance_to(ghost.global_position) <= 22.0:
								ghost.current_emf_level = 5
						GameState.status_message.emit(
							"CRUNCH! Stepped on BRICKWORKZ plastic bricks at %.1f m/s — alerted nearby entities! (Use Janitorial Floor Sweeper or crouch/walk slowly!)" % horiz_speed,
							Color(1.0, 0.45, 0.28)
						)
						return


# ------------------------------------------------------------------------------
# 7. Wall-Mounted Light Cart Charger Station + Gremlin Zapper Cage (`MallCartChargerStation`)
#    Automatically recharges any `MallCartSystem` (`LightCart`) within 4.2m at +18.0%/s
#    (or instant +50% on `[F]` interaction).
#    Supports `has_zapper_cage: bool` which shocks & stuns any `HallwayGremlin` within 3.8m!
# ------------------------------------------------------------------------------
class MallCartChargerStation extends Node3D:
	var charge_radius: float = 4.2
	var charge_rate_per_sec: float = 18.0
	var has_zapper_cage: bool = false
	var zap_cooldown: float = 0.0
	var charger_light: OmniLight3D = null
	var bar_mat: StandardMaterial3D = null
	var label_3d: Label3D = null

	func _ready() -> void:
		add_to_group("cart_chargers")
		_build_charger_visuals()

	func _build_charger_visuals() -> void:
		var cab := MeshInstance3D.new()
		var box := BoxMesh.new()
		box.size = Vector3(0.55, 0.85, 0.28)
		cab.mesh = box
		cab.position = Vector3(0.0, 1.15, 0.0)
		var cab_mat := StandardMaterial3D.new()
		cab_mat.albedo_color = Color(0.95, 0.76, 0.14)
		cab_mat.metallic = 0.35
		cab_mat.roughness = 0.40
		cab.material_override = cab_mat
		add_child(cab)

		var bar_mi := MeshInstance3D.new()
		var bar_box := BoxMesh.new()
		bar_box.size = Vector3(0.38, 0.14, 0.30)
		bar_mi.mesh = bar_box
		bar_mi.position = Vector3(0.0, 1.32, 0.0)
		bar_mat = StandardMaterial3D.new()
		bar_mat.albedo_color = Color(0.22, 1.0, 0.55)
		bar_mat.emission_enabled = true
		bar_mat.emission = Color(0.22, 1.0, 0.55)
		bar_mat.emission_energy_multiplier = 2.6
		bar_mi.material_override = bar_mat
		add_child(bar_mi)

		charger_light = OmniLight3D.new()
		charger_light.position = Vector3(0.0, 1.35, 0.35)
		charger_light.light_color = Color(0.25, 1.0, 0.62)
		charger_light.light_energy = 1.6
		charger_light.omni_range = 4.5
		add_child(charger_light)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0.0, 1.82, 0.0)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 20
		label_3d.outline_size = 6
		add_child(label_3d)
		_refresh_charger_label(null)

	func install_zapper_cage() -> bool:
		if has_zapper_cage:
			return false
		has_zapper_cage = true
		if bar_mat:
			bar_mat.albedo_color = Color(0.28, 0.92, 1.0)
			bar_mat.emission = Color(0.28, 0.92, 1.0)
		GameState.status_message.emit(
			"INSTALLED GREMLIN ZAPPER CAGE! Electrified mesh guard now shocks & stuns Hallway Gremlins within 3.8m (-10% cart battery per zap)!",
			Color(0.35, 0.95, 1.0)
		)
		_refresh_charger_label(null)
		return true

	func interact_charger() -> void:
		var boosted_any := false
		for node in get_tree().get_nodes_in_group("light_carts"):
			var lc := node as MallCartSystem
			if lc and global_position.distance_to(lc.global_position) <= charge_radius + 1.5:
				lc.boost_battery(50.0)
				lc.set_floodlight_power(true, "Wall Charger Quick-Boost")
				boosted_any = true
				_refresh_charger_label(lc)
		if boosted_any:
			GameState.status_message.emit(
				"HIGH-VOLTAGE WALL CHARGER: Boosted Wheeled Mobile Light Cart battery by +50%!",
				Color(0.35, 1.0, 0.68)
			)
		else:
			GameState.status_message.emit(
				"Wall Charger Ready — wheel the Mobile Light Cart within %.1fm to recharge (+18%%/s passive or press [F] for +50%% Quick-Boost)!" % charge_radius,
				Color(0.95, 0.88, 0.38)
			)

	func _refresh_charger_label(active_cart: MallCartSystem) -> void:
		if label_3d == null:
			return
		var zap_tag := "ZAPPER CAGE: ARMED" if has_zapper_cage else "No Zapper Cage"
		if active_cart != null:
			label_3d.text = "WALL CART CHARGER [CHARGING LIGHT CART: %.0f%% | %s]\n[F] +50%% Instant Quick-Boost" % [
				active_cart.battery_percent, zap_tag
			]
			label_3d.modulate = Color(0.35, 1.0, 0.68)
		else:
			label_3d.text = "HIGH-VOLTAGE LIGHT CART CHARGER [4.2m Range | %s]\n[Park Light Cart Nearby for +18%%/s Charge | Press F for +50%% Boost]" % zap_tag
			label_3d.modulate = Color(0.95, 0.88, 0.38)

	func _physics_process(delta: float) -> void:
		if Engine.is_editor_hint():
			return
		if zap_cooldown > 0.0:
			zap_cooldown = maxf(0.0, zap_cooldown - delta)

		var charging_cart: MallCartSystem = null
		for node in get_tree().get_nodes_in_group("light_carts"):
			var lc := node as MallCartSystem
			if lc and absf(lc.global_position.y - global_position.y) <= 2.5 and global_position.distance_to(lc.global_position) <= charge_radius:
				charging_cart = lc
				if lc.battery_percent < 100.0:
					lc.boost_battery(charge_rate_per_sec * delta)

		# Gremlin Zapper Cage protection around the Charger & Light Cart
		if has_zapper_cage and zap_cooldown <= 0.0:
			for gr in get_tree().get_nodes_in_group("hallway_gremlins"):
				var grem := gr as PlaceableEvidenceActors.HallwayGremlin
				if grem and not grem.is_captured and not grem.is_fleeing_into_wall and not grem.is_vanished_in_walls:
					var d_charger := global_position.distance_to(grem.global_position)
					var d_cart := charging_cart.global_position.distance_to(grem.global_position) if charging_cart else 999.0
					if minf(d_charger, d_cart) <= 3.8:
						zap_cooldown = 4.0
						if charging_cart:
							charging_cart.battery_percent = maxf(0.0, charging_cart.battery_percent - 10.0)
							charging_cart._update_light_cart_active_visuals()
						grem.stun_gremlin("Gremlin Zapper Cage Shock")
						break

		_refresh_charger_label(charging_cart)


# ------------------------------------------------------------------------------
# 8. "Ask Zoltar" Coin-Op Fortune Card Booth (Item #26 — `zoltar_booths`)
#    Spend $20 via `interact_zoltar(player)` to draw 1 of 4 random Fortune Cards:
#    1. "The Auditor's Insight": Reveals 1 unconfirmed evidence in nearest store + $40 rebate!
#    2. "Geothermal Surge": Recharges all LightCart batteries to 150% Overcharge + 60s Pink Salt Ward!
#    3. "Hinoki Soul-Blessing": Lights `GameState.sauna_soul_anchor_lit = true` for free!
#    4. "VOID TRANSACTION — CURSED DRAW": Triggers immediate `force_trigger_hunt_warning()` (+$55 hazard pay)!
# ------------------------------------------------------------------------------
class ZoltarFortuneBooth extends Node3D:
	var draw_cost: int = 20
	var total_draws: int = 0
	var last_card_title: String = "Insert $20 [F] to Draw a Fortune Card!"
	var crystal_light: OmniLight3D = null
	var label_3d: Label3D = null

	func _ready() -> void:
		add_to_group("zoltar_booths")
		_build_zoltar_visuals()

	func _build_zoltar_visuals() -> void:
		# Maroon & gold arcade cabinet
		var cab := MeshInstance3D.new()
		var box := BoxMesh.new()
		box.size = Vector3(0.88, 1.95, 0.78)
		cab.mesh = box
		cab.position = Vector3(0.0, 0.975, 0.0)
		var cab_mat := StandardMaterial3D.new()
		cab_mat.albedo_color = Color(0.42, 0.10, 0.16)
		cab_mat.metallic = 0.25
		cab_mat.roughness = 0.42
		cab.material_override = cab_mat
		add_child(cab)

		# Glowing crystal ball inside the cabinet window
		var ball := MeshInstance3D.new()
		var sph := SphereMesh.new()
		sph.radius = 0.16
		sph.height = 0.32
		ball.mesh = sph
		ball.position = Vector3(0.0, 1.25, 0.22)
		var ball_mat := StandardMaterial3D.new()
		ball_mat.albedo_color = Color(0.75, 0.38, 1.0)
		ball_mat.emission_enabled = true
		ball_mat.emission = Color(0.68, 0.28, 1.0)
		ball_mat.emission_energy_multiplier = 3.4
		ball.material_override = ball_mat
		add_child(ball)

		crystal_light = OmniLight3D.new()
		crystal_light.position = Vector3(0.0, 1.32, 0.42)
		crystal_light.light_color = Color(0.78, 0.42, 1.0)
		crystal_light.light_energy = 2.2
		crystal_light.omni_range = 4.8
		add_child(crystal_light)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0.0, 2.25, 0.0)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 21
		label_3d.outline_size = 6
		label_3d.text = "\"ASK ZOLTAR\" COIN-OP FORTUNE BOOTH [$20 per Draw]\n[Press F to Draw Fortune Card: Evidence Insight / 150% Cart Overcharge / Soul-Anchor / Cursed Hunt!]"
		label_3d.modulate = Color(0.95, 0.78, 1.0)
		add_child(label_3d)

	func interact_zoltar(player: Node3D = null) -> int:
		if not GameState.spend_cash(draw_cost, "Ask Zoltar Coin-Op Fortune Card Draw"):
			return -1
		total_draws += 1
		var card_idx := (total_draws - 1) % 4 if Engine.is_editor_hint() else randi() % 4
		match card_idx:
			0:
				last_card_title = "CARD I: THE AUDITOR'S INSIGHT (+Free Evidence & +$40 Rebate!)"
				var target_store := "East_F0_02"
				var g := _find_nearest_unresolved_ghost()
				if g:
					target_store = g.store_id
				var prof: Dictionary = GameState.ghost_profiles.get(target_store, {})
				var req_ev: Array = prof.get("required_evidence", ["EMF-5", "UV Prints", "Ghost Writing"])
				var confirmed: Array = GameState.store_evidence_ledger.get(target_store, [])
				var chosen_ev := str(req_ev[0]) if not req_ev.is_empty() else "EMF-5"
				for ev in req_ev:
					if ev not in confirmed:
						chosen_ev = str(ev)
						break
				GameState.record_evidence(target_store, chosen_ev, 40)
				GameState.status_message.emit(
					"ZOLTAR FORTUNE [%s]: Revealed [%s] in [%s]!" % [last_card_title, chosen_ev, target_store],
					Color(0.45, 1.0, 0.85)
				)
			1:
				last_card_title = "CARD II: GEOTHERMAL SURGE (150% Light Cart Overcharge + 60s Pink Salt Ward!)"
				for node in get_tree().get_nodes_in_group("light_carts"):
					var lc := node as MallCartSystem
					if lc:
						lc.battery_percent = 150.0
						lc.set_floodlight_power(true, "Zoltar Geothermal Surge")
						lc._update_light_cart_active_visuals()
				if player and "pink_salt_ward_timer" in player:
					player.set("pink_salt_ward_timer", 60.0)
				GameState.status_message.emit(
					"ZOLTAR FORTUNE [%s]: All Light Carts overcharged to 150%% & 60s Pink Salt Ward granted!" % last_card_title,
					Color(1.0, 0.92, 0.38)
				)
			2:
				last_card_title = "CARD III: HINOKI SOUL-BLESSING (Free Sauna Soul-Anchor Lit!)"
				GameState.sauna_soul_anchor_lit = true
				GameState.status_message.emit(
					"ZOLTAR FORTUNE [%s]: Sauna Soul-Anchor Incense lit for free (Auto-Revive on Death)!" % last_card_title,
					Color(0.38, 1.0, 0.68)
				)
			_:
				last_card_title = "CARD IV: VOID TRANSACTION — CURSED DRAW (Immediate Ghost Hunt + $55 Hazard Pay!)"
				GameState.add_cash(55, "Zoltar Cursed Draw Hazard Pay")
				var g := _find_nearest_unresolved_ghost()
				if g:
					g.force_trigger_hunt_warning()
				GameState.status_message.emit(
					"ZOLTAR CURSED DRAW [%s]: Nearest Store Ghost triggered an immediate Hunt Warning (+$55 Hazard Pay)!" % last_card_title,
					Color(1.0, 0.32, 0.32)
				)

		if label_3d:
			label_3d.text = "\"ASK ZOLTAR\" BOOTH [Last Draw: %s]\n[Press F ($20) to Draw Again!]" % last_card_title
		return card_idx

	func _find_nearest_unresolved_ghost() -> StoreGhost:
		var best: StoreGhost = null
		var best_d := 999.0
		for node in get_tree().get_nodes_in_group("store_ghosts"):
			var g := node as StoreGhost
			if g and not g.is_resolved:
				var d := global_position.distance_to(g.global_position)
				if d < best_d:
					best_d = d
					best = g
		return best


# ------------------------------------------------------------------------------
# 9. Static Expanded Item Catalog Helper (`get_expanded_item_catalog_entry`)
#    Returns a full `PlayerController.make_item_data`-compatible Dictionary for
#    all 27 new items in Categories A–F of Docs/GameDesignBible.md Section 5.
# ------------------------------------------------------------------------------
const EXPANDED_ITEM_IDS: Array[String] = [
	"video_camera",
	"cctv_monitor",
	"camera_stand",
	"assembled_video_rig",
	"audio_recorder",
	"sls_camera",
	"laser_grid_dome",
	"barcode_scanner",
	"parabolic_mic",
	"ecto_viewmaster",
	"flashbulb_cube",
	"pink_salt_pouch",
	"hinoki_censer",
	"eas_pedestal_ward",
	"talkboy_decoy",
	"uv_chalk_spray",
	"shutter_hand_crank",
	"master_keyring",
	"floor_sweeper",
	"walkie_talkie",
	"pa_intercom_hack",
	"cart_tow_hook",
	"gremlin_zapper_cage",
	"cart_proximity_horn",
	"jump_starter_pack",
	"helium_balloon",
	"mannequin_disguise",
	"price_slasher_baton",
]

static func has_expanded_item(item_id: String) -> bool:
	return item_id in EXPANDED_ITEM_IDS

static func get_all_expanded_item_ids() -> Array[String]:
	return EXPANDED_ITEM_IDS.duplicate()

static func get_expanded_item_catalog_entry(item_id: String) -> Dictionary:
	match item_id:
		"video_camera":
			return {
				"id": "video_camera",
				"name": "Night-Vision Video Camera",
				"short_label": "VIDEO CAM",
				"icon_symbol": "[NV]",
				"accent_color": Color(0.88, 0.92, 0.98),
				"model_path": "",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 9,
				"powered_on": true,
				"ir_night_vision_on": true,
				"can_power_toggle": true,
				"makes_noise_when_on": false,
				"price": 55,
				"required_floor": 1,
				"category": "Category A: Expanded Investigation",
			}
		"cctv_monitor":
			return {
				"id": "cctv_monitor",
				"name": "Portable CCTV Camera Monitor (Hands-Only)",
				"short_label": "CCTV MON",
				"icon_symbol": "[CCTV]",
				"accent_color": Color(0.82, 0.92, 1.0),
				"model_path": "",
				"can_store_in_hotbar": false,
				"two_handed_slow": false,
				"indicator_slot": 10,
				"powered_on": true,
				"can_power_toggle": true,
				"makes_noise_when_on": false,
				"active_cam_idx": 0,
				"price": 65,
				"required_floor": 1,
				"category": "Category A: Expanded Investigation",
			}
		"camera_stand":
			return {
				"id": "camera_stand",
				"name": "Collapsible Camera Stand (Tripod)",
				"short_label": "CAM STAND",
				"icon_symbol": "/|\\",
				"accent_color": Color(0.68, 0.85, 0.96),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_MotionSensor.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 5,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"price": 35,
				"required_floor": 1,
				"category": "Category A: Expanded Investigation",
			}
		"assembled_video_rig":
			return {
				"id": "assembled_video_rig",
				"name": "Assembled Video Camera + Stand Rig (Hands-Only)",
				"short_label": "CCTV RIG",
				"icon_symbol": "[NV/T]",
				"accent_color": Color(0.88, 0.92, 0.98),
				"model_path": "",
				"can_store_in_hotbar": false,
				"two_handed_slow": false,
				"indicator_slot": 9,
				"powered_on": true,
				"ir_night_vision_on": true,
				"can_power_toggle": true,
				"makes_noise_when_on": false,
				"price": 85,
				"required_floor": 1,
				"category": "Category A: Expanded Investigation",
			}
		"audio_recorder":
			return {
				"id": "audio_recorder",
				"name": "Paranormal Audio / EVP Recorder",
				"short_label": "EVP TAPE",
				"icon_symbol": "[REC]",
				"accent_color": Color(1.0, 0.62, 0.28),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_SpiritBox_CC0.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 2,
				"powered_on": true,
				"can_power_toggle": true,
				"makes_noise_when_on": true,
				"reverse_playback": false,
				"has_recorded_clip": false,
				"price": 60,
				"required_floor": 1,
				"category": "Category A: Expanded Investigation",
			}
		"sls_camera":
			return {
				"id": "sls_camera",
				"name": "SLS Stick-Figure Pose Camera",
				"short_label": "SLS CAM",
				"icon_symbol": "{o+}",
				"accent_color": Color(0.22, 1.0, 0.48),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_PhotoCamera.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 4,
				"powered_on": true,
				"can_power_toggle": true,
				"makes_noise_when_on": false,
				"price": 75,
				"required_floor": 2,
				"category": "Category A: Expanded Investigation",
			}
		"laser_grid_dome":
			return {
				"id": "laser_grid_dome",
				"name": "Laser Matrix Grid Dome (Star-Shower)",
				"short_label": "LASER GRID",
				"icon_symbol": "*#*#",
				"accent_color": Color(0.18, 1.0, 0.42),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_MotionSensor.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 5,
				"powered_on": true,
				"can_power_toggle": true,
				"makes_noise_when_on": false,
				"price": 65,
				"required_floor": 2,
				"category": "Category A: Expanded Investigation",
			}
		"barcode_scanner":
			return {
				"id": "barcode_scanner",
				"name": "Retail Barcode Scanner (SKU-666)",
				"short_label": "SKU SCAN",
				"icon_symbol": "|||",
				"accent_color": Color(1.0, 0.28, 0.36),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_Thermometer.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 1,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": true,
				"price": 55,
				"required_floor": 2,
				"category": "Category A: Expanded Investigation",
			}
		"parabolic_mic":
			return {
				"id": "parabolic_mic",
				"name": "Parabolic Eavesdropper Dish Mic (35m)",
				"short_label": "DISH MIC",
				"icon_symbol": "((<))",
				"accent_color": Color(0.42, 0.92, 1.0),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_EMFReader.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 0,
				"powered_on": true,
				"can_power_toggle": true,
				"makes_noise_when_on": false,
				"price": 70,
				"required_floor": 2,
				"category": "Category A: Expanded Investigation",
			}
		"ecto_viewmaster":
			return {
				"id": "ecto_viewmaster",
				"name": "Ecto-Lens Polarized ViewMaster",
				"short_label": "VIEWMASTER",
				"icon_symbol": "(O-O)",
				"accent_color": Color(0.82, 0.42, 1.0),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_PhotoCamera.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 3,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"price": 65,
				"required_floor": 2,
				"category": "Category A: Expanded Investigation",
			}
		"flashbulb_cube":
			return {
				"id": "flashbulb_cube",
				"name": "Disposable Xenon Flashbulb Cube (2 Uses)",
				"short_label": "FLASH CUBE",
				"icon_symbol": "[*!]",
				"accent_color": Color(1.0, 0.96, 0.42),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_PhotoCamera.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 4,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"charges_left": 2,
				"price": 35,
				"required_floor": 1,
				"category": "Category B: Defensive & Anti-Hunt",
			}
		"pink_salt_pouch":
			return {
				"id": "pink_salt_pouch",
				"name": "Himalayan Pink Salt Pouch (3 Lines)",
				"short_label": "PINK SALT",
				"icon_symbol": "~::~",
				"accent_color": Color(1.0, 0.58, 0.78),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_MotionSensor.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 5,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"charges_left": 3,
				"price": 40,
				"required_floor": 1,
				"category": "Category B: Defensive & Anti-Hunt",
			}
		"hinoki_censer":
			return {
				"id": "hinoki_censer",
				"name": "Hinoki Cedar Smudge Censer (20s)",
				"short_label": "SMUDGE",
				"icon_symbol": "~~~",
				"accent_color": Color(0.95, 0.76, 0.38),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_Thermometer.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 2,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"charges_left": 2,
				"price": 45,
				"required_floor": 1,
				"category": "Category B: Defensive & Anti-Hunt",
			}
		"eas_pedestal_ward":
			return {
				"id": "eas_pedestal_ward",
				"name": "EAS Anti-Theft Pedestal Ward (Crucifix)",
				"short_label": "EAS WARD",
				"icon_symbol": "[||]",
				"accent_color": Color(0.28, 1.0, 0.68),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_MotionSensor.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 5,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"price": 65,
				"required_floor": 1,
				"category": "Category B: Defensive & Anti-Hunt",
			}
		"talkboy_decoy":
			return {
				"id": "talkboy_decoy",
				"name": "Acoustic Decoy Talk-Boy Cassette Toy",
				"short_label": "TALK-BOY",
				"icon_symbol": "[o-o]",
				"accent_color": Color(1.0, 0.72, 0.28),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_SpiritBox_CC0.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 2,
				"powered_on": true,
				"can_power_toggle": true,
				"makes_noise_when_on": true,
				"price": 50,
				"required_floor": 1,
				"category": "Category B: Defensive & Anti-Hunt",
			}
		"uv_chalk_spray":
			return {
				"id": "uv_chalk_spray",
				"name": "UV Chalk / Neon Stencil Spray Can",
				"short_label": "UV SPRAY",
				"icon_symbol": "[->]",
				"accent_color": Color(0.22, 0.96, 1.0),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_UVLight.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 3,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"stencil_mode": 0,
				"price": 25,
				"required_floor": 1,
				"category": "Category C: Utility & Wayfinding",
			}
		"shutter_hand_crank":
			return {
				"id": "shutter_hand_crank",
				"name": "Security Shutter Hand-Crank",
				"short_label": "CRANK",
				"icon_symbol": "(@)",
				"accent_color": Color(0.92, 0.78, 0.32),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_Thermometer.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 5,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": true,
				"price": 55,
				"required_floor": 1,
				"category": "Category C: Utility & Wayfinding",
			}
		"master_keyring":
			return {
				"id": "master_keyring",
				"name": "Mall Maintenance Master Keyring",
				"short_label": "KEYRING",
				"icon_symbol": "O-+",
				"accent_color": Color(1.0, 0.85, 0.32),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_EMFReader.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 0,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": true,
				"price": 45,
				"required_floor": 1,
				"category": "Category C: Utility & Wayfinding",
			}
		"floor_sweeper":
			return {
				"id": "floor_sweeper",
				"name": "Janitorial Acoustic Floor Sweeper",
				"short_label": "SWEEPER",
				"icon_symbol": "[///]",
				"accent_color": Color(0.38, 0.96, 0.65),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_MotionSensor.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 5,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"price": 35,
				"required_floor": 1,
				"category": "Category C: Utility & Wayfinding",
			}
		"walkie_talkie":
			return {
				"id": "walkie_talkie",
				"name": "Mall Security Walkie-Talkie (Cross-Floor)",
				"short_label": "WALKIE",
				"icon_symbol": "((W))",
				"accent_color": Color(0.38, 0.92, 1.0),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_SpiritBox_CC0.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 2,
				"powered_on": true,
				"can_power_toggle": true,
				"makes_noise_when_on": true,
				"price": 40,
				"required_floor": 1,
				"category": "Category D: Co-op Comms & Acoustics",
			}
		"pa_intercom_hack":
			return {
				"id": "pa_intercom_hack",
				"name": "Store PA Intercom Mic Hack",
				"short_label": "PA HACK",
				"icon_symbol": "[PA!]",
				"accent_color": Color(1.0, 0.68, 0.32),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_EMFReader.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 0,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": true,
				"price": 50,
				"required_floor": 2,
				"category": "Category D: Co-op Comms & Acoustics",
			}
		"cart_tow_hook":
			return {
				"id": "cart_tow_hook",
				"name": "Cart Tow-Hook / 2-Cart Train Winch",
				"short_label": "TOW HOOK",
				"icon_symbol": "C==C",
				"accent_color": Color(1.0, 0.84, 0.28),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_MotionSensor.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 5,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"price": 60,
				"required_floor": 1,
				"category": "Category E: Cart Accessories",
			}
		"gremlin_zapper_cage":
			return {
				"id": "gremlin_zapper_cage",
				"name": "Gremlin Zapper Cage (Light Cart Guard)",
				"short_label": "ZAP CAGE",
				"icon_symbol": "[⚡#]",
				"accent_color": Color(0.32, 0.96, 1.0),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_MotionSensor.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 5,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"price": 65,
				"required_floor": 1,
				"category": "Category E: Cart Accessories",
			}
		"cart_proximity_horn":
			return {
				"id": "cart_proximity_horn",
				"name": "Cart Ultrasonic Proximity Horn (3s Hunt Pre-Warn)",
				"short_label": "SONAR HORN",
				"icon_symbol": "((!))",
				"accent_color": Color(1.0, 0.58, 0.25),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_MotionSensor.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 5,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"price": 55,
				"required_floor": 1,
				"category": "Category E: Cart Accessories",
			}
		"jump_starter_pack":
			return {
				"id": "jump_starter_pack",
				"name": "Portable Jump-Starter Pack (+35% Cart Charge)",
				"short_label": "JUMP PACK",
				"icon_symbol": "[+⚡]",
				"accent_color": Color(0.35, 1.0, 0.55),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_EMFReader.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 0,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"charges_left": 2,
				"price": 45,
				"required_floor": 1,
				"category": "Category E: Cart Accessories",
			}
		"helium_balloon":
			return {
				"id": "helium_balloon",
				"name": "Helium Party Balloon (+30% Speed & +8 Semitone Voice)",
				"short_label": "HELIUM",
				"icon_symbol": "(^)",
				"accent_color": Color(1.0, 0.45, 0.85),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_Thermometer.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 1,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"price": 35,
				"required_floor": 1,
				"category": "Category F: Cursed Relics & Co-op Comedy",
			}
		"mannequin_disguise":
			return {
				"id": "mannequin_disguise",
				"name": "Wearable Mannequin Disguise Rig (Hold R to Pose)",
				"short_label": "MANNEQUIN",
				"icon_symbol": "[/\\]",
				"accent_color": Color(0.92, 0.86, 0.74),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_MotionSensor.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 5,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"price": 60,
				"required_floor": 1,
				"category": "Category F: Cursed Relics & Co-op Comedy",
			}
		"price_slasher_baton":
			return {
				"id": "price_slasher_baton",
				"name": "Clearance-Sale \"Price Slasher\" Stun Baton (50% OFF!)",
				"short_label": "STUN BATON",
				"icon_symbol": "/⚡/",
				"accent_color": Color(1.0, 0.92, 0.22),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_UVLight.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 3,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
				"price": 45,
				"required_floor": 1,
				"category": "Category F: Cursed Relics & Co-op Comedy",
			}
		_:
			return {}
