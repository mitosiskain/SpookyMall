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

func setup_for_slot(slot: int, model_path: String) -> void:
	slot_index = slot
	for c in get_children():
		c.queue_free()
	emf_led_meshes.clear()
	emf_led_mats.clear()

	device_model_root = Node3D.new()
	add_child(device_model_root)

	if ResourceLoader.exists(model_path):
		var packed := load(model_path) as PackedScene
		if packed:
			var inst := packed.instantiate()
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

	# Subtle hand-illuminating status OmniLight3D
	status_omni = OmniLight3D.new()
	status_omni.position = Vector3(-0.02, 0.09, 0.10)
	status_omni.omni_range = 1.4
	status_omni.light_energy = 0.35
	status_omni.light_color = Color(0.3, 0.95, 0.5)
	add_child(status_omni)

	# Proton Ray Gun 3D Containment Stream (hidden unless firing)
	proton_beam_mesh = MeshInstance3D.new()
	var cyl := CylinderMesh.new()
	cyl.top_radius = 0.022
	cyl.bottom_radius = 0.035
	cyl.height = 7.5
	proton_beam_mesh.mesh = cyl
	proton_beam_mesh.position = Vector3(0.0, 0.02, -3.85)
	proton_beam_mesh.rotation_degrees = Vector3(90.0, 0.0, 0.0)
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
	is_powered_on: bool = true
) -> void:
	proton_beam_mesh.visible = (active_slot == 6 and proton_firing)
	if proton_beam_mesh.visible:
		# Wobble & pulse the proton stream
		var jitter := randf_range(-0.025, 0.025)
		proton_beam_mesh.position.x = jitter
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
		6: # Separate Tool #7: Proton Ray Gun + Ectoplasm Canister
			lcd_label.text = "PROTON HEAT: %.0f%%" % proton_heat
			sub_label.text = "WEAKENED: %.0f%% [E: TRAP]" % ghost_weakened_pct
			var p_col := Color(1.0, 0.3, 0.2) if proton_heat > 75.0 else Color(0.3, 0.95, 1.0)
			lcd_label.modulate = p_col
			status_omni.light_color = p_col
			bar_fill_mesh.scale.x = clampf(proton_heat / 100.0, 0.05, 1.0)
			bar_fill_mat.emission = p_col
