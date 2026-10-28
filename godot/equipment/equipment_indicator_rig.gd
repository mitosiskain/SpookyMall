class_name EquipmentIndicatorRig
extends Node3D
## Diegetic 3D Viewmodel & Equipment Indicator Rig for Megamall Anomaly.
## Attaches 3D emissive LED arrays, backlit LCD Label3D screens, thermal color bars,
## camera viewfinder target-lock readouts, and Proton Ray Gun beam/heat indicators
## directly onto the 3D equipment models in the player's hand.

var slot_index: int = 0
var device_model_root: Node3D = null
var lcd_screen_mesh: MeshInstance3D = null
var lcd_screen_mat: StandardMaterial3D = null
var lcd_label: Label3D = null
var sub_label: Label3D = null
var status_omni: OmniLight3D = null

## 5-LED array for EMF Reader (Green, Yellow-Green, Yellow, Orange, Red)
var emf_led_meshes: Array[MeshInstance3D] = []
var emf_led_mats: Array[StandardMaterial3D] = []
const EMF_LED_COLORS: Array[Color] = [
	Color(0.15, 0.95, 0.30), # Level 1 Green
	Color(0.65, 0.95, 0.20), # Level 2 Yellow-Green
	Color(1.00, 0.88, 0.15), # Level 3 Yellow
	Color(1.00, 0.50, 0.10), # Level 4 Orange
	Color(1.00, 0.12, 0.18), # Level 5 Red (EMF-5 Evidence)
]

## Thermal / Proton Heat / Signal Bar mesh
var bar_fill_mesh: MeshInstance3D = null
var bar_fill_mat: StandardMaterial3D = null

## Proton Ray Gun 3D Containment Beam (visible while firing in Slot 4/5)
var proton_beam_mesh: MeshInstance3D = null
var proton_beam_mat: StandardMaterial3D = null

## Handheld Video Camera (Slot 9) Flip-Out Side LCD Screen + Dedicated Zero-Light SubViewport
var handheld_cam_viewport: SubViewport = null
var handheld_cam_3d: Camera3D = null
var handheld_cam_attrs: CameraAttributesPractical = null
var handheld_lcd_shader_mat: ShaderMaterial = null
var handheld_side_label: Label3D = null

func setup_for_slot(slot: int, model_path: String) -> void:
	slot_index = slot
	for c in get_children():
		c.queue_free()
	emf_led_meshes.clear()
	emf_led_mats.clear()
	handheld_cam_viewport = null
	handheld_cam_3d = null
	handheld_cam_attrs = null
	handheld_lcd_shader_mat = null
	handheld_side_label = null

	device_model_root = Node3D.new()
	add_child(device_model_root)

	if slot == 9:
		# Build the exact same Camcorder + Flip-Out Side LCD Screen as PlacedVideoCameraActor,
		# squared straight with the player's view and scaled/positioned to take ~1.8x screen space!
		device_model_root.position = Vector3(-0.02, 0.09, 0.10)
		device_model_root.rotation_degrees = Vector3.ZERO
		device_model_root.scale = Vector3(1.48, 1.48, 1.48)

		handheld_cam_viewport = SubViewport.new()
		handheld_cam_viewport.name = "HandheldCamSideLCDViewport"
		handheld_cam_viewport.size = Vector2i(320, 200)
		handheld_cam_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		handheld_cam_viewport.handle_input_locally = false
		add_child(handheld_cam_viewport)

		handheld_cam_3d = Camera3D.new()
		handheld_cam_3d.fov = 76.0
		handheld_cam_3d.near = 0.08
		handheld_cam_3d.far = 90.0
		handheld_cam_3d.current = true
		handheld_cam_attrs = CameraAttributesPractical.new()
		handheld_cam_attrs.exposure_multiplier = 4.8
		handheld_cam_3d.attributes = handheld_cam_attrs
		handheld_cam_viewport.add_child(handheld_cam_3d)

		var parts := VideoAndAudioSurveillance.build_camcorder_mesh_hierarchy(device_model_root, true, true)
		handheld_lcd_shader_mat = parts.get("lcd_shader_mat") as ShaderMaterial
		var lcd_frame := parts.get("lcd_frame") as Node3D
		if lcd_frame:
			# Keep the flip-out side LCD screen 100% squared with the direction the player is looking!
			lcd_frame.position = Vector3(-0.145, 0.015, 0.03)
			lcd_frame.rotation_degrees = Vector3.ZERO
			handheld_side_label = Label3D.new()
			handheld_side_label.position = Vector3(0.0, -0.058, 0.012)
			handheld_side_label.font_size = 12
			handheld_side_label.outline_size = 4
			handheld_side_label.pixel_size = 0.00070
			handheld_side_label.modulate = Color(0.88, 0.92, 0.96)
			handheld_side_label.text = "B&W NIGHT-VISION [LMB:ON/OFF | RMB:NV]"
			lcd_frame.add_child(handheld_side_label)
		if handheld_lcd_shader_mat and handheld_cam_viewport:
			VideoAndAudioSurveillance.update_camera_screen_shader_material(
				handheld_lcd_shader_mat, handheld_cam_viewport.get_texture(), true, true
			)
	elif slot == 10:
		# Portable Widescreen CCTV Camera Monitor (Hands-Only Item) squared with player view at ~1.8x screen space!
		device_model_root.position = Vector3(-0.18, 0.10, 0.08)
		device_model_root.rotation_degrees = Vector3.ZERO
		device_model_root.scale = Vector3(1.48, 1.48, 1.48)

		handheld_cam_viewport = SubViewport.new()
		handheld_cam_viewport.name = "PortableCCTVMonitorViewport"
		handheld_cam_viewport.size = Vector2i(384, 216)
		handheld_cam_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		handheld_cam_viewport.handle_input_locally = false
		add_child(handheld_cam_viewport)

		handheld_cam_3d = Camera3D.new()
		handheld_cam_3d.fov = 78.0
		handheld_cam_3d.near = 0.08
		handheld_cam_3d.far = 110.0
		handheld_cam_3d.current = true
		handheld_cam_attrs = CameraAttributesPractical.new()
		handheld_cam_attrs.exposure_multiplier = 4.8
		handheld_cam_3d.attributes = handheld_cam_attrs
		handheld_cam_viewport.add_child(handheld_cam_3d)

		var mon_parts := VideoAndAudioSurveillance.build_portable_cctv_monitor_mesh(device_model_root, true)
		handheld_lcd_shader_mat = mon_parts.get("screen_shader_mat") as ShaderMaterial
		handheld_side_label = Label3D.new()
		handheld_side_label.position = Vector3(0.0, -0.095, 0.022)
		handheld_side_label.font_size = 12
		handheld_side_label.outline_size = 4
		handheld_side_label.pixel_size = 0.00068
		handheld_side_label.modulate = Color(0.88, 0.94, 1.0)
		handheld_side_label.text = "CCTV MONITOR [LMB:ON/OFF | RMB:NEXT CAM | G:PUT DOWN]"
		device_model_root.add_child(handheld_side_label)
		if handheld_lcd_shader_mat and handheld_cam_viewport:
			VideoAndAudioSurveillance.update_camera_screen_shader_material(
				handheld_lcd_shader_mat, handheld_cam_viewport.get_texture(), false, true
			)
	elif ResourceLoader.exists(model_path):
		var packed := load(model_path) as PackedScene
		if packed:
			var inst := packed.instantiate()
			if slot == 4:
				device_model_root.position = Vector3(-0.04, 0.06, 0.08)
				device_model_root.rotation_degrees = Vector3.ZERO
				inst.scale = Vector3(1.32, 1.32, 1.32)
			else:
				inst.scale = Vector3(0.82, 0.82, 0.82)
			device_model_root.add_child(inst)

	# Backlit diegetic 3D LCD display panel facing the player's eyes
	lcd_screen_mesh = MeshInstance3D.new()
	var quad := BoxMesh.new()
	quad.size = Vector3(0.16, 0.075, 0.008)
	lcd_screen_mesh.mesh = quad
	lcd_screen_mesh.position = Vector3(-0.02, 0.085, 0.06)
	lcd_screen_mesh.rotation_degrees = Vector3(-12.0, 14.0, 0.0)
	lcd_screen_mat = StandardMaterial3D.new()
	lcd_screen_mat.albedo_color = Color(0.04, 0.09, 0.12)
	lcd_screen_mat.emission_enabled = true
	lcd_screen_mat.emission = Color(0.05, 0.18, 0.24)
	lcd_screen_mat.emission_energy_multiplier = 1.4
	lcd_screen_mesh.material_override = lcd_screen_mat
	lcd_screen_mesh.visible = (slot != 9 and slot != 10)
	add_child(lcd_screen_mesh)

	lcd_label = Label3D.new()
	lcd_label.position = Vector3(0.0, 0.012, 0.006)
	lcd_label.font_size = 18
	lcd_label.outline_size = 4
	lcd_label.pixel_size = 0.0009
	lcd_label.modulate = Color(0.45, 1.0, 0.75)
	lcd_screen_mesh.add_child(lcd_label)

	sub_label = Label3D.new()
	sub_label.position = Vector3(0.0, -0.018, 0.006)
	sub_label.font_size = 13
	sub_label.outline_size = 3
	sub_label.pixel_size = 0.00085
	sub_label.modulate = Color(0.85, 0.95, 1.0)
	lcd_screen_mesh.add_child(sub_label)

	# 5 Physical 3D LED Domes along the top bezel of the screen
	for i in range(5):
		var led := MeshInstance3D.new()
		var sph := SphereMesh.new()
		sph.radius = 0.0085
		sph.height = 0.017
		led.mesh = sph
		led.position = Vector3(-0.056 + float(i) * 0.028, 0.044, 0.005)
		var lmat := StandardMaterial3D.new()
		lmat.albedo_color = EMF_LED_COLORS[i].darkened(0.75)
		lmat.emission_enabled = true
		lmat.emission = EMF_LED_COLORS[i]
		lmat.emission_energy_multiplier = 0.0
		led.material_override = lmat
		lcd_screen_mesh.add_child(led)
		emf_led_meshes.append(led)
		emf_led_mats.append(lmat)

	# 3D Bar indicator along the bottom bezel (Thermal bar / Spirit Box signal / Proton heat)
	bar_fill_mesh = MeshInstance3D.new()
	var bar_box := BoxMesh.new()
	bar_box.size = Vector3(0.14, 0.009, 0.004)
	bar_fill_mesh.mesh = bar_box
	bar_fill_mesh.position = Vector3(0.0, -0.033, 0.006)
	bar_fill_mat = StandardMaterial3D.new()
	bar_fill_mat.albedo_color = Color(0.2, 0.85, 1.0)
	bar_fill_mat.emission_enabled = true
	bar_fill_mat.emission = Color(0.2, 0.85, 1.0)
	bar_fill_mat.emission_energy_multiplier = 2.0
	bar_fill_mesh.material_override = bar_fill_mat
	lcd_screen_mesh.add_child(bar_fill_mesh)

	# Subtle hand-illuminating status OmniLight3D (strictly 0.0 for Video Camera Slot 9 & CCTV Monitor Slot 10 so they emit zero world light!)
	status_omni = OmniLight3D.new()
	status_omni.position = Vector3(-0.02, 0.09, 0.10)
	status_omni.omni_range = 1.4
	status_omni.light_energy = 0.0 if (slot == 9 or slot == 10) else 0.35
	status_omni.light_color = Color(0.3, 0.95, 0.5)
	add_child(status_omni)

	# Proton Ray Gun 3D Containment Stream (hidden unless firing; unit height 1.0 scaled dynamically to track target!)
	proton_beam_mesh = MeshInstance3D.new()
	var cyl := CylinderMesh.new()
	cyl.top_radius = 0.022
	cyl.bottom_radius = 0.035
	cyl.height = 1.0
	proton_beam_mesh.mesh = cyl
	proton_beam_mesh.position = Vector3(0.0, 0.02, -3.85)
	proton_beam_mesh.rotation_degrees = Vector3(90.0, 0.0, 0.0)
	proton_beam_mesh.scale = Vector3(1.0, 7.5, 1.0)
	proton_beam_mat = StandardMaterial3D.new()
	proton_beam_mat.albedo_color = Color(1.0, 0.38, 0.15)
	proton_beam_mat.emission_enabled = true
	proton_beam_mat.emission = Color(0.25, 0.92, 1.0)
	proton_beam_mat.emission_energy_multiplier = 6.5
	proton_beam_mesh.material_override = proton_beam_mat
	proton_beam_mesh.visible = false
	add_child(proton_beam_mesh)

	# Ensure no first-person viewmodel mesh or 3D indicator ever casts shadows into the player's flashlight or UV beam
	_disable_viewmodel_shadows_recursive(self)

func _disable_viewmodel_shadows_recursive(node: Node) -> void:
	if node is GeometryInstance3D:
		(node as GeometryInstance3D).cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	for child in node.get_children():
		_disable_viewmodel_shadows_recursive(child)

func update_indicators(
	active_slot: int,
	emf_level: int,
	temp_c: float,
	use_fahrenheit: bool,
	photo_left: int,
	photo_target_label: String,
	spirit_freq: float,
	spirit_tell: String,
	uv_prints_count: int,
	uv_narrow: bool,
	aimed_fixture_info: String,
	proton_heat: float,
	proton_firing: bool,
	ghost_weakened_pct: float,
	is_powered_on: bool = true,
	beam_track_world_pos: Vector3 = Vector3.ZERO,
	ghost_hp_summary: String = "",
	canister_radius_m: float = 2.2,
	canister_charge_sec: float = 18.0,
	filled_canister_label: String = "",
	video_cam_nv_on: bool = true,
	cctv_cam_idx: int = 0
) -> void:
	if active_slot == 9:
		if lcd_screen_mesh:
			lcd_screen_mesh.visible = false
		if status_omni:
			status_omni.light_energy = 0.0
		if handheld_cam_viewport and handheld_cam_3d and device_model_root and is_inside_tree():
			var root_vp := get_viewport()
			if root_vp != null and root_vp.world_3d != null and handheld_cam_viewport.world_3d != root_vp.world_3d:
				handheld_cam_viewport.world_3d = root_vp.world_3d
			handheld_cam_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS if is_powered_on else SubViewport.UPDATE_DISABLED
			handheld_cam_3d.global_position = device_model_root.to_global(Vector3(0.0, 0.02, -0.28))
			handheld_cam_3d.global_basis = global_transform.basis
			if handheld_cam_attrs:
				handheld_cam_attrs.exposure_multiplier = 4.8 if (is_powered_on and video_cam_nv_on) else 1.15
			if handheld_lcd_shader_mat:
				VideoAndAudioSurveillance.update_camera_screen_shader_material(
					handheld_lcd_shader_mat, handheld_cam_viewport.get_texture(), is_powered_on, video_cam_nv_on
				)
		if handheld_side_label:
			if not is_powered_on:
				handheld_side_label.text = "CAM OFF [LMB: Turn ON | G: Place]"
				handheld_side_label.modulate = Color(0.55, 0.60, 0.68)
			elif video_cam_nv_on:
				handheld_side_label.text = "NIGHT-VISION B&W [RMB: Color | G: Place]"
				handheld_side_label.modulate = Color(0.90, 0.94, 0.98)
			else:
				handheld_side_label.text = "COLOR MODE [RMB: Night-Vision | G: Place]"
				handheld_side_label.modulate = Color(0.85, 0.90, 0.96)
		return
	elif active_slot == 10:
		if lcd_screen_mesh:
			lcd_screen_mesh.visible = false
		if status_omni:
			status_omni.light_energy = 0.0
		var cams := VideoAndAudioSurveillance.get_placed_cameras_sorted(get_tree()) if is_inside_tree() else []
		if handheld_cam_viewport and handheld_cam_3d and is_inside_tree():
			var root_vp2 := get_viewport()
			if root_vp2 != null and root_vp2.world_3d != null and handheld_cam_viewport.world_3d != root_vp2.world_3d:
				handheld_cam_viewport.world_3d = root_vp2.world_3d
			if not is_powered_on or cams.is_empty():
				handheld_cam_viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
				if handheld_lcd_shader_mat:
					VideoAndAudioSurveillance.update_camera_screen_shader_material(
						handheld_lcd_shader_mat, handheld_cam_viewport.get_texture(), false, true
					)
				if handheld_side_label:
					if not is_powered_on:
						handheld_side_label.text = "MONITOR OFF [LMB: Turn ON | G: Put Down]"
						handheld_side_label.modulate = Color(0.55, 0.60, 0.68)
					else:
						handheld_side_label.text = "NO CAMERAS DEPLOYED [Place Video Cam with G]"
						handheld_side_label.modulate = Color(0.78, 0.85, 0.95)
			else:
				var sel_idx := posmod(cctv_cam_idx, cams.size())
				var sel_cam := cams[sel_idx]
				var feed_on := (sel_cam != null and sel_cam.is_powered_on)
				var feed_nv := (sel_cam != null and sel_cam.ir_night_vision_on)
				handheld_cam_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS if feed_on else SubViewport.UPDATE_DISABLED
				if sel_cam != null and sel_cam.head_pivot != null:
					handheld_cam_3d.global_position = sel_cam.head_pivot.to_global(Vector3(0.0, 0.02, -0.26))
					handheld_cam_3d.global_basis = sel_cam.head_pivot.global_transform.basis
				if handheld_cam_attrs:
					handheld_cam_attrs.exposure_multiplier = 4.8 if (feed_on and feed_nv) else 1.15
				if handheld_lcd_shader_mat:
					VideoAndAudioSurveillance.update_camera_screen_shader_material(
						handheld_lcd_shader_mat, handheld_cam_viewport.get_texture(), feed_on, feed_nv
					)
				if handheld_side_label and sel_cam != null:
					var mode_tag := "OFF" if not feed_on else ("B&W NV" if feed_nv else "COLOR")
					handheld_side_label.text = "CAM %02d [%s | %s] (%d/%d) | [LMB] ON/OFF | [RMB] Next Cam | [G] Put Down" % [
						sel_cam.camera_index, sel_cam.store_id, mode_tag, sel_idx + 1, cams.size()
					]
					handheld_side_label.modulate = Color(0.92, 0.96, 1.0)
		return
	proton_beam_mesh.visible = (active_slot == 6 and proton_firing)
	if proton_beam_mesh.visible:
		# Wobble & pulse the proton stream, and dynamically track locked ghost movement!
		var jitter := randf_range(-0.022, 0.022)
		if beam_track_world_pos != Vector3.ZERO and is_inside_tree():
			var nozzle_local := Vector3(0.0, 0.02, -0.12)
			var target_local := to_local(beam_track_world_pos)
			var span := target_local - nozzle_local
			var dist := clampf(span.length(), 0.45, 10.5)
			var dir := span.normalized()
			var up_axis := dir
			var right_axis := up_axis.cross(Vector3.UP)
			if right_axis.length_squared() < 0.001:
				right_axis = up_axis.cross(Vector3.RIGHT)
			right_axis = right_axis.normalized()
			var fwd_axis := right_axis.cross(up_axis).normalized()
			proton_beam_mesh.transform.basis = Basis(right_axis, up_axis, fwd_axis).scaled(Vector3(1.0, dist, 1.0))
			proton_beam_mesh.position = nozzle_local + span * 0.5 + Vector3(jitter, jitter * 0.5, 0.0)
		else:
			proton_beam_mesh.rotation_degrees = Vector3(90.0, 0.0, 0.0)
			proton_beam_mesh.scale = Vector3(1.0, 7.5, 1.0)
			proton_beam_mesh.position = Vector3(jitter, 0.02, -3.85)
		proton_beam_mat.emission = Color(1.0, 0.35, 0.15) if randf() < 0.45 else Color(0.2, 0.9, 1.0)

	# If an electronic gadget (EMF Reader, Thermal Scanner, Spirit Box, UV Blacklight) is powered OFF:
	if active_slot in [0, 1, 2, 3] and not is_powered_on:
		for i in range(emf_led_mats.size()):
			emf_led_mats[i].emission_energy_multiplier = 0.0
			emf_led_mats[i].albedo_color = EMF_LED_COLORS[i].darkened(0.85)
		lcd_label.text = "POWER: OFF (SILENT)"
		sub_label.text = "[LMB] Turn ON | [G] Throw/Drop"
		var off_col := Color(0.42, 0.48, 0.55)
		lcd_label.modulate = off_col
		sub_label.modulate = Color(0.55, 0.62, 0.70)
		status_omni.light_energy = 0.0
		bar_fill_mesh.scale.x = 0.05
		bar_fill_mat.emission_energy_multiplier = 0.0
		return

	sub_label.modulate = Color(0.85, 0.95, 1.0)
	bar_fill_mat.emission_energy_multiplier = 2.0

	# Update 5-LED bar
	for i in range(emf_led_mats.size()):
		var lit := false
		if active_slot == 0:
			lit = (i < emf_level)
		elif active_slot == 6:
			lit = (float(i) / 5.0) < (proton_heat / 100.0)
		else:
			lit = (i == 0)
		emf_led_mats[i].emission_energy_multiplier = 4.2 if lit else 0.0
		emf_led_mats[i].albedo_color = EMF_LED_COLORS[i] if lit else EMF_LED_COLORS[i].darkened(0.8)

	match active_slot:
		0: # Separate Tool #1: EMF Reader (5-LED EMF Level 1-5 + Audio Beeping)
			lcd_label.text = "EMF LEVEL %d / 5" % emf_level
			sub_label.text = "EVIDENCE: EMF-5!" if emf_level >= 5 else ("ANOMALY DETECTED" if emf_level >= 2 else "AMBIENT FIELD")
			var c: Color = EMF_LED_COLORS[clampi(emf_level - 1, 0, 4)]
			lcd_label.modulate = c
			status_omni.light_color = c
			status_omni.light_energy = 0.25 + float(emf_level) * 0.18
			bar_fill_mesh.scale.x = clampf(float(emf_level) / 5.0, 0.15, 1.0)
			bar_fill_mat.emission = c
		1: # Separate Tool #2: Thermal Scanner (Digital IR Thermometer)
			var disp_temp := (temp_c * 9.0 / 5.0 + 32.0) if use_fahrenheit else temp_c
			var unit := "°F" if use_fahrenheit else "°C"
			lcd_label.text = "TEMP: %.1f %s" % [disp_temp, unit]
			sub_label.text = "EVIDENCE: FREEZING (<0°C)!" if temp_c < 0.0 else "[E] Toggle °C / °F"
			var t_col := Color(0.28, 0.92, 1.0) if temp_c < 0.0 else Color(0.45, 1.0, 0.65)
			lcd_label.modulate = t_col
			status_omni.light_color = t_col
			var cold_ratio := clampf((20.5 - temp_c) / 28.0, 0.08, 1.0)
			bar_fill_mesh.scale.x = cold_ratio
			bar_fill_mat.emission = t_col
		2: # Separate Tool #3: Spirit Box (FM EVP Radio + Static & Ghost Voice)
			lcd_label.text = "SPIRIT BOX %.1f FM" % spirit_freq
			sub_label.text = "%s | [J] Speak" % spirit_tell
			var sb_col := Color(0.95, 0.65, 0.28)
			lcd_label.modulate = sb_col
			status_omni.light_color = sb_col
			bar_fill_mesh.scale.x = 0.78
			bar_fill_mat.emission = sb_col
		3: # Separate Tool #4: UV Blacklight (Fluorescent Footprint & Cipher Reveal)
			lcd_label.text = "UV LIGHT (%s)" % ("22° FOCUS" if uv_narrow else "40° FLOOD")
			sub_label.text = "UV PRINTS FOUND: %d" % uv_prints_count
			var uv_col := Color(0.72, 0.42, 1.0)
			lcd_label.modulate = uv_col
			status_omni.light_color = uv_col
			bar_fill_mesh.scale.x = 1.0 if uv_prints_count > 0 else 0.35
			bar_fill_mat.emission = Color(0.3, 1.0, 0.45) if uv_prints_count > 0 else uv_col
		4: # Separate Tool #5: Evidence Photo Camera
			lcd_label.text = "FILM: %d/10 EXP" % photo_left
			sub_label.text = photo_target_label
			var lock_col := Color(0.35, 1.0, 0.55) if "STAR" in photo_target_label else Color(0.75, 0.85, 1.0)
			lcd_label.modulate = lock_col
			status_omni.light_color = lock_col
			bar_fill_mesh.scale.x = clampf(float(photo_left) / 10.0, 0.05, 1.0)
			bar_fill_mat.emission = lock_col
		5: # Separate Tool #6: Light Remote Gun (60m)
			lcd_label.text = "LIGHT REMOTE 60m"
			sub_label.text = aimed_fixture_info
			var rem_col := Color(1.0, 0.88, 0.35)
			lcd_label.modulate = rem_col
			status_omni.light_color = rem_col
			bar_fill_mesh.scale.x = 0.85
			bar_fill_mat.emission = rem_col
		6: # Separate Tool #7: Proton Ray Gun (Deals HP Damage, Slows, Tracks & Wrangles Ghosts)
			var r_lvl: int = GameState.ray_gun_power_level if "ray_gun_power_level" in GameState else 1
			lcd_label.text = "RAY LVL %d | HEAT %.0f%%" % [r_lvl, proton_heat]
			sub_label.text = ghost_hp_summary if not ghost_hp_summary.is_empty() else ("WEAK: %.0f%% [E: PULL/PUSH]" % ghost_weakened_pct)
			var p_col := Color(1.0, 0.3, 0.2) if proton_heat > 75.0 else Color(0.3, 0.95, 1.0)
			lcd_label.modulate = p_col
			status_omni.light_color = p_col
			bar_fill_mesh.scale.x = clampf(proton_heat / 100.0, 0.05, 1.0)
			bar_fill_mat.emission = p_col
		7: # Separate Tool #8: Empty Ectoplasm Collection Canister (Rechargeable & Deployable Floor Trap)
			var max_t: float = GameState.get_canister_max_deployed_time() if GameState.has_method("get_canister_max_deployed_time") else 18.0
			lcd_label.text = "CHG %.0fs/%.0fs | R %.1fm" % [canister_charge_sec, max_t, canister_radius_m]
			sub_label.text = "[LMB/G] SET FLOOR | [E] SIZE"
			var c_col := Color(0.35, 1.0, 0.65) if canister_charge_sec >= max_t * 0.95 else Color(0.95, 0.82, 0.32)
			lcd_label.modulate = c_col
			status_omni.light_color = c_col
			bar_fill_mesh.scale.x = clampf(canister_charge_sec / maxf(max_t, 1.0), 0.08, 1.0)
			bar_fill_mat.emission = c_col
		8: # Filled Ectoplasm Canister (Once Full Must Be Sold to Trader Chen!)
			lcd_label.text = filled_canister_label if not filled_canister_label.is_empty() else "CANISTER FULL!"
			sub_label.text = "SELL AT TRADER CHEN [F]!"
			var f_col := Color(1.0, 0.88, 0.28)
			lcd_label.modulate = f_col
			status_omni.light_color = f_col
			bar_fill_mesh.scale.x = 1.0
			bar_fill_mat.emission = f_col
