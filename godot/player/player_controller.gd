class_name PlayerController
extends CharacterBody3D
## Megamall Anomaly — Co-op Investigator Controller.
## Features:
## - 4-Slot Hotbar ([1]–[4]) + "Hands" Pseudo-5th Slot (carries Hands-Only items like Death Inventory Crate,
##   Heavy Hydraulic Assembly, Escalator Relay Fuse Box, or a 5th overflow tool when slots 1-4 are full).
## - Switching to [1]–[4] while holding an item in Hands automatically drops the Hands item if there is no slot for it.
## - On death: stores all player inventory inside a Death Inventory Crate (unusable until opened; opening spills
##   items onto the floor near where opened to prevent 4-slot hotbar overrides) and locks camera to 3rd-Person Ectoplasm Form.
## - Overhauled Stair & Escalator Slope Collision (52° max slope, 0.35m floor snap, and 0.42m kinematic stair step-up).

signal hotbar_changed(slot_index: int, tool_name: String, mode_detail: String)
signal vitality_updated(hp: float, stamina: float, ward_sec: float, active_buff: String)
signal camera_mode_changed(is_third_person: bool, context_reason: String)
signal player_died_with_inventory(death_pos: Vector3, killer_name: String, stored_items: Array)
signal meal_finished()

const WALK_SPEED: float = 4.5
const SPRINT_SPEED: float = 6.9
const JUMP_VELOCITY: float = 4.6
const MOUSE_SENSITIVITY: float = 0.0024
const MAX_STEP_HEIGHT: float = 0.42

## Co-op Multiplayer Peer ID (1 = Host / Solo, >1 = Joined Co-op Client)
var peer_id: int = 1
var nametag_label: Label3D = null
var _net_target_pos: Vector3 = Vector3(2.0, 0.1, 0.0)
var _net_target_yaw: float = 0.0
var _net_target_pitch: float = 0.0
var _net_velocity: Vector3 = Vector3.ZERO
var _net_held_item_label: String = "EMF Reader"
var _net_emitting_noise: bool = true
var _net_sync_timer: float = 0.0

var health: float = 100.0
var stamina: float = 100.0
var pink_salt_ward_timer: float = 0.0
var food_buff_timer: float = 0.0
var food_buff_name: String = "None"

## Death & Locked 3rd-Person Ectoplasm Form state
var is_floating_essence: bool = false
var last_death_position: Vector3 = Vector3.ZERO

## Camera state: First-Person default with smooth SpringArm3D transition to Third-Person
var is_third_person: bool = false
var contextual_seat_active: bool = false
var contextual_reason: String = "First-Person Investigator View"
var is_eating_meal: bool = false
var eating_timer: float = 0.0
var eating_duration: float = 5.0
var _camera_blend: float = 0.0
var _stride_phase: float = 0.0

## Hiding spot witness state (read by StoreGhost)
var is_hidden_without_witness: bool = false

## Strict 4-Slot Hotbar ([1]-[4]) + "Hands" Pseudo-5th Slot
var active_slot: int = 0
var hotbar_slots: Array[Dictionary] = [{}, {}, {}, {}]
var hand_slot_item: Dictionary = {}

var use_fahrenheit: bool = false
var photo_exposures_left: int = 10
var spirit_box_channels: Array[float] = [87.9, 91.5, 98.3, 104.7]
var spirit_box_channel_idx: int = 0 # 87.9 FM default
var spirit_box_power_on: bool = true
var uv_light_power_on: bool = true
var uv_narrow_beam: bool = true
var uv_prints_discovered: int = 0
var proton_heat: float = 0.0
var is_firing_proton: bool = false
var voice_menu_open: bool = false

## Nodes built on _ready
var head_pivot: Node3D = null
var fp_eye_marker: Marker3D = null
var spring_arm: SpringArm3D = null
var main_camera: Camera3D = null
var belt_flashlight: SpotLight3D = null
var uv_spotlight: SpotLight3D = null
var flash_omni: OmniLight3D = null
var character_visual_root: Node3D = null
var ectoplasm_visual_root: Node3D = null
var ectoplasm_anim_player: AnimationPlayer = null
var indicator_rig: EquipmentIndicatorRig = null
var hand_crate_visual: Node3D = null
var heavy_part_visual: Node3D = null
var fuse_box_visual: Node3D = null
var voip_emitter: AudioStreamPlayer3D = null

## Dedicated Equipment & Flashlight Audio Players
var emf_beep_player: AudioStreamPlayer3D = null
var spirit_static_player: AudioStreamPlayer3D = null
var spirit_evp_player: AudioStreamPlayer3D = null
var flashlight_click_player: AudioStreamPlayer3D = null
var tool_sfx_player: AudioStreamPlayer3D = null
var _emf_beep_timer: float = 0.0
var _thermal_beep_timer: float = 0.0
var _current_emf_reading: int = 1
var _current_temp_reading_c: float = 20.5
var _sfx_click_on: AudioStream = null
var _sfx_click_off: AudioStream = null
var _sfx_emf_beep: AudioStream = null
var _sfx_thermal_beep: AudioStream = null
var _sfx_camera_shutter: AudioStream = null
var _sfx_spirit_static: AudioStream = null
var _sfx_spirit_evp_receptive: AudioStream = null
var _sfx_spirit_evp_corrupted: AudioStream = null

var _yaw: float = 0.0
var _pitch: float = 0.0
var _flash_decay: float = 0.0
var test_override_wish_dir: Vector3 = Vector3.ZERO

static func make_item_data(item_id: String) -> Dictionary:
	match item_id:
		"emf_reader", "emf_thermal":
			return {
				"id": "emf_reader",
				"name": "EMF Reader",
				"short_label": "EMF READER",
				"icon_symbol": "((o))",
				"accent_color": Color(0.28, 0.96, 0.56),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_EMFReader.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 0,
				"powered_on": true,
				"can_power_toggle": true,
				"makes_noise_when_on": true,
			}
		"thermal_scanner":
			return {
				"id": "thermal_scanner",
				"name": "Thermal Scanner",
				"short_label": "THERMOMETER",
				"icon_symbol": "[°C]",
				"accent_color": Color(0.32, 0.90, 1.0),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_Thermometer.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 1,
				"powered_on": false,
				"can_power_toggle": true,
				"makes_noise_when_on": true,
			}
		"spirit_box", "spirit_uv":
			return {
				"id": "spirit_box",
				"name": "Spirit Box",
				"short_label": "SPIRIT BOX",
				"icon_symbol": "~FM~",
				"accent_color": Color(0.96, 0.68, 0.28),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_SpiritBox_CC0.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 2,
				"powered_on": false,
				"can_power_toggle": true,
				"makes_noise_when_on": true,
				"spirit_freq": 87.9,
			}
		"uv_blacklight":
			return {
				"id": "uv_blacklight",
				"name": "UV Blacklight",
				"short_label": "UV LIGHT",
				"icon_symbol": "(UV)",
				"accent_color": Color(0.75, 0.45, 1.0),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_UVLight.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 3,
				"powered_on": false,
				"can_power_toggle": true,
				"makes_noise_when_on": false,
				"uv_narrow": true,
			}
		"photo_camera":
			return {
				"id": "photo_camera",
				"name": "Photo Camera (10 Exp)",
				"short_label": "PHOTO CAM",
				"icon_symbol": "[O]",
				"accent_color": Color(0.45, 0.85, 1.0),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_PhotoCamera.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 4,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
			}
		"light_remote":
			return {
				"id": "light_remote",
				"name": "Light Remote Gun (60m)",
				"short_label": "LIGHT GUN",
				"icon_symbol": "=IR>",
				"accent_color": Color(1.0, 0.88, 0.35),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_MotionSensor.fbx",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 5,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
			}
		"proton_gun":
			return {
				"id": "proton_gun",
				"name": "Proton Ray Gun & Trap",
				"short_label": "PROTON GUN",
				"icon_symbol": "⚡>>",
				"accent_color": Color(1.0, 0.45, 0.25),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_ProtonRayGun_T1.glb",
				"can_store_in_hotbar": true,
				"two_handed_slow": false,
				"indicator_slot": 6,
				"powered_on": true,
				"can_power_toggle": false,
				"makes_noise_when_on": false,
			}
		"heavy_hydraulic":
			return {
				"id": "heavy_hydraulic",
				"name": "Heavy Hydraulic Assembly",
				"short_label": "HYDRAULIC",
				"icon_symbol": "[HVY]",
				"accent_color": Color(1.0, 0.65, 0.22),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_HeavyHydraulicAssembly.glb",
				"can_store_in_hotbar": false,
				"two_handed_slow": true,
				"indicator_slot": -1,
			}
		"escalator_fuse":
			return {
				"id": "escalator_fuse",
				"name": "Escalator Relay Fuse Crate",
				"short_label": "FUSE CRATE",
				"icon_symbol": "[FUSE]",
				"accent_color": Color(0.32, 0.95, 0.68),
				"model_path": "res://SourceAssets/Equipment/Prepared/SM_MotionSensor.fbx",
				"can_store_in_hotbar": false,
				"two_handed_slow": false,
				"indicator_slot": -1,
			}
		_:
			return {
				"id": item_id,
				"name": item_id,
				"short_label": item_id.to_upper(),
				"icon_symbol": "[*]",
				"accent_color": Color(0.75, 0.82, 0.90),
				"model_path": "",
				"can_store_in_hotbar": false,
				"two_handed_slow": false,
				"indicator_slot": -1,
			}

func is_local_player() -> bool:
	if Engine.is_editor_hint() or MultiplayerManager == null or not MultiplayerManager.is_coop_active:
		return true
	return peer_id == MultiplayerManager.get_local_peer_id()

func configure_for_peer(id: int) -> void:
	peer_id = id
	name = "Player_%d" % id
	_net_target_pos = global_position if is_inside_tree() else position
	_net_target_yaw = rotation.y
	if is_inside_tree():
		_apply_peer_role_visuals()

func _ready() -> void:
	add_to_group("players")
	# Configure CharacterBody3D slope & stair parameters for smooth escalators and platform stairs
	floor_stop_on_slope = true
	floor_constant_speed = true
	floor_block_on_wall = true
	floor_max_angle = deg_to_rad(52.0)
	floor_snap_length = 0.35
	safe_margin = 0.002

	_init_starting_4slot_hotbar()
	_build_player_hierarchy()
	_setup_equipment_audio_players()
	_apply_peer_role_visuals()
	AcousticVoipManager.voice_command_recognized.connect(_on_voice_command_for_revival)
	_refresh_equipped_visuals()

func _apply_peer_role_visuals() -> void:
	var local := is_local_player()
	if main_camera:
		main_camera.current = local
	if local:
		AcousticVoipManager.register_listener(self)
	if character_visual_root:
		character_visual_root.visible = (not local) and (not is_floating_essence)
	if indicator_rig:
		indicator_rig.visible = local
	if nametag_label:
		nametag_label.visible = not local
		_refresh_nametag_text()

func _refresh_nametag_text() -> void:
	if nametag_label == null:
		return
	var role_tag := " [HOST]" if peer_id == 1 else ""
	var item_str := _net_held_item_label if not is_local_player() else str(get_current_active_item().get("name", "Empty Hands"))
	if is_floating_essence:
		nametag_label.text = "INVESTIGATOR #%d%s — [ECTOPLASM FORM]" % [peer_id, role_tag]
		nametag_label.modulate = Color(0.35, 1.0, 0.95)
	else:
		var badge_colors := [
			Color(0.45, 0.96, 1.0),
			Color(1.0, 0.84, 0.35),
			Color(0.45, 1.0, 0.65),
			Color(0.85, 0.58, 1.0),
		]
		var col_idx := posmod(peer_id - 1, badge_colors.size())
		nametag_label.text = "INVESTIGATOR #%d%s\n[%s | HP %.0f%%]" % [peer_id, role_tag, item_str, health]
		nametag_label.modulate = badge_colors[col_idx]

func _init_starting_4slot_hotbar() -> void:
	hotbar_slots = [
		make_item_data("emf_reader"),
		make_item_data("thermal_scanner"),
		make_item_data("spirit_box"),
		make_item_data("uv_blacklight"),
	]
	hand_slot_item = {}
	active_slot = 0

func _build_player_hierarchy() -> void:
	var col := CollisionShape3D.new()
	var cap_shape := CapsuleShape3D.new()
	cap_shape.radius = 0.35
	cap_shape.height = 1.78
	col.shape = cap_shape
	col.position = Vector3(0, 0.89, 0)
	add_child(col)

	# 1. Living Low-Poly Investigator Character (1.78m exact 1:1 scale)
	character_visual_root = Node3D.new()
	add_child(character_visual_root)
	var char_path := "res://SourceAssets/Characters/NormalizedLowPoly/Investigator_LowPoly.glb"
	if ResourceLoader.exists(char_path):
		var packed := load(char_path) as PackedScene
		if packed:
			var inst := packed.instantiate() as Node3D
			inst.scale = Vector3.ONE
			inst.rotation_degrees = Vector3(0.0, 180.0, 0.0)
			character_visual_root.add_child(inst)
	character_visual_root.visible = false

	# 2. Dead Locked 3rd-Person Ectoplasm Form (1.55m floating ghost mesh + cyan aura + overhead death notice)
	ectoplasm_visual_root = Node3D.new()
	add_child(ectoplasm_visual_root)
	var ecto_path := "res://SourceAssets/Characters/NormalizedLowPoly/Player_Ectoplasm_Form.glb"
	if ResourceLoader.exists(ecto_path):
		var ecto_packed := load(ecto_path) as PackedScene
		if ecto_packed:
			var ecto_inst := ecto_packed.instantiate() as Node3D
			ecto_inst.scale = Vector3.ONE
			ecto_inst.rotation_degrees = Vector3(0.0, 180.0, 0.0)
			ectoplasm_visual_root.add_child(ecto_inst)
			ectoplasm_anim_player = _find_anim_player(ecto_inst)
			_apply_ectoplasm_material_recursive(ecto_inst)

	var ecto_light := OmniLight3D.new()
	ecto_light.position = Vector3(0, 1.1, 0)
	ecto_light.light_color = Color(0.25, 0.95, 1.0)
	ecto_light.light_energy = 2.6
	ecto_light.omni_range = 5.5
	ectoplasm_visual_root.add_child(ecto_light)

	var ecto_label := Label3D.new()
	ecto_label.position = Vector3(0, 1.95, 0)
	ecto_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	ecto_label.font_size = 22
	ecto_label.outline_size = 6
	ecto_label.text = "ECTOPLASM FORM (YOU ARE DEAD)\n[Locked 3rd-Person View — Float to South Sauna & Press F or 9 to Revive]"
	ecto_label.modulate = Color(0.35, 1.0, 0.95)
	ectoplasm_visual_root.add_child(ecto_label)
	ectoplasm_visual_root.visible = false

	head_pivot = Node3D.new()
	head_pivot.position = Vector3(0, 1.60, 0)
	add_child(head_pivot)

	fp_eye_marker = Marker3D.new()
	head_pivot.add_child(fp_eye_marker)

	spring_arm = SpringArm3D.new()
	spring_arm.spring_length = 2.85
	spring_arm.margin = 0.25
	spring_arm.position = Vector3(0.35, 0.25, 0.0)
	head_pivot.add_child(spring_arm)

	main_camera = Camera3D.new()
	main_camera.fov = 75.0
	main_camera.current = true
	head_pivot.add_child(main_camera)

	# Belt Flashlight (T) — positioned forward of the first-person viewmodel plane on the left-chest side
	# so equipped right-hand tools (EMF Reader, Camera, Spirit Box, etc.) never occlude or shadow the beam.
	belt_flashlight = SpotLight3D.new()
	belt_flashlight.position = Vector3(-0.12, -0.08, -0.58)
	belt_flashlight.light_color = Color(1.0, 0.96, 0.88)
	belt_flashlight.light_energy = 3.8
	belt_flashlight.light_volumetric_fog_energy = 1.8
	belt_flashlight.spot_range = 20.0
	belt_flashlight.spot_angle = 30.0
	belt_flashlight.shadow_enabled = true
	head_pivot.add_child(belt_flashlight)

	# UV Blacklight Cone — positioned at the front emitter nozzle ahead of the viewmodel
	uv_spotlight = SpotLight3D.new()
	uv_spotlight.position = Vector3(0.26, -0.18, -0.60)
	uv_spotlight.light_color = Color(0.58, 0.22, 1.0)
	uv_spotlight.light_energy = 4.5
	uv_spotlight.light_volumetric_fog_energy = 2.8
	uv_spotlight.spot_range = 9.5
	uv_spotlight.spot_angle = 22.0
	uv_spotlight.visible = false
	head_pivot.add_child(uv_spotlight)

	# Photo Camera Xenon Flash burst
	flash_omni = OmniLight3D.new()
	flash_omni.light_color = Color(0.92, 0.97, 1.0)
	flash_omni.light_energy = 0.0
	flash_omni.omni_range = 9.5
	flash_omni.visible = false
	head_pivot.add_child(flash_omni)

	# Diegetic 3D Equipment Indicator Rig (tucked cleanly in lower-right view with shadow casting disabled)
	indicator_rig = EquipmentIndicatorRig.new()
	indicator_rig.position = Vector3(0.30, -0.25, -0.44)
	head_pivot.add_child(indicator_rig)

	# 1. Hands-Only Visual: Death Inventory Crate held in Hands
	hand_crate_visual = Node3D.new()
	hand_crate_visual.position = Vector3(0.0, -0.34, -0.56)
	PlaceableEvidenceActors.DeathInventoryCrate._build_crate_visuals(hand_crate_visual)
	_disable_shadows_recursive(hand_crate_visual)
	hand_crate_visual.visible = false
	head_pivot.add_child(hand_crate_visual)

	# 2. Hands-Only Visual: Two-Handed Heavy Hydraulic Assembly
	heavy_part_visual = Node3D.new()
	heavy_part_visual.position = Vector3(0.0, -0.30, -0.58)
	var heavy_path := "res://SourceAssets/Equipment/Prepared/SM_HeavyHydraulicAssembly.glb"
	if ResourceLoader.exists(heavy_path):
		var hp_packed := load(heavy_path) as PackedScene
		if hp_packed:
			heavy_part_visual.add_child(hp_packed.instantiate())
	_disable_shadows_recursive(heavy_part_visual)
	heavy_part_visual.visible = false
	head_pivot.add_child(heavy_part_visual)

	# 3. Hands-Only Visual: Escalator Relay Fuse Crate
	fuse_box_visual = Node3D.new()
	fuse_box_visual.position = Vector3(0.18, -0.26, -0.48)
	var fuse_mi := MeshInstance3D.new()
	var fuse_bm := BoxMesh.new()
	fuse_bm.size = Vector3(0.26, 0.18, 0.22)
	fuse_mi.mesh = fuse_bm
	var fuse_mat := StandardMaterial3D.new()
	fuse_mat.albedo_color = Color(0.22, 0.85, 0.55)
	fuse_mat.emission_enabled = true
	fuse_mat.emission = Color(0.15, 0.75, 0.45)
	fuse_mat.emission_energy_multiplier = 1.2
	fuse_mi.material_override = fuse_mat
	fuse_box_visual.add_child(fuse_mi)
	_disable_shadows_recursive(fuse_box_visual)
	fuse_box_visual.visible = false
	head_pivot.add_child(fuse_box_visual)

	_disable_shadows_recursive(character_visual_root)
	_disable_shadows_recursive(ectoplasm_visual_root)

	voip_emitter = AudioStreamPlayer3D.new()
	voip_emitter.bus = &"VoIP_Room"
	voip_emitter.max_distance = 25.0
	add_child(voip_emitter)

	# Overhead Investigator Co-op Nametag (visible to teammates and in 3rd-person view)
	nametag_label = Label3D.new()
	nametag_label.position = Vector3(0.0, 2.02, 0.0)
	nametag_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	nametag_label.font_size = 20
	nametag_label.outline_size = 6
	nametag_label.visible = false
	add_child(nametag_label)

func _setup_equipment_audio_players() -> void:
	if ResourceLoader.exists("res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_On.wav"):
		_sfx_click_on = load("res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_On.wav")
	if ResourceLoader.exists("res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_Off.wav"):
		_sfx_click_off = load("res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_Off.wav")
	if ResourceLoader.exists("res://SourceAssets/EquipmentSounds/SW_EMF_Beep.wav"):
		_sfx_emf_beep = load("res://SourceAssets/EquipmentSounds/SW_EMF_Beep.wav")
	if ResourceLoader.exists("res://SourceAssets/EquipmentSounds/SW_Thermal_Beep.wav"):
		_sfx_thermal_beep = load("res://SourceAssets/EquipmentSounds/SW_Thermal_Beep.wav")
	if ResourceLoader.exists("res://SourceAssets/EquipmentSounds/SW_Camera_FlashShutter.wav"):
		_sfx_camera_shutter = load("res://SourceAssets/EquipmentSounds/SW_Camera_FlashShutter.wav")
	if ResourceLoader.exists("res://SourceAssets/SpiritBox/SW_SpiritBox_Scan.wav"):
		_sfx_spirit_static = load("res://SourceAssets/SpiritBox/SW_SpiritBox_Scan.wav")
	if ResourceLoader.exists("res://SourceAssets/SpiritBox/SW_SpiritBox_Placeholder.wav"):
		_sfx_spirit_evp_receptive = load("res://SourceAssets/SpiritBox/SW_SpiritBox_Placeholder.wav")
	if ResourceLoader.exists("res://SourceAssets/GhostSounds/SW_Ghost_HuntGrowl.wav"):
		_sfx_spirit_evp_corrupted = load("res://SourceAssets/GhostSounds/SW_Ghost_HuntGrowl.wav")

	emf_beep_player = AudioStreamPlayer3D.new()
	emf_beep_player.stream = _sfx_emf_beep
	emf_beep_player.max_distance = 14.0
	head_pivot.add_child(emf_beep_player)

	spirit_static_player = AudioStreamPlayer3D.new()
	spirit_static_player.stream = _sfx_spirit_static
	spirit_static_player.volume_db = -7.0
	spirit_static_player.max_distance = 14.0
	head_pivot.add_child(spirit_static_player)

	spirit_evp_player = AudioStreamPlayer3D.new()
	spirit_evp_player.stream = _sfx_spirit_evp_receptive
	spirit_evp_player.volume_db = 2.0
	spirit_evp_player.max_distance = 18.0
	head_pivot.add_child(spirit_evp_player)

	flashlight_click_player = AudioStreamPlayer3D.new()
	flashlight_click_player.stream = _sfx_click_on
	flashlight_click_player.volume_db = 0.0
	flashlight_click_player.max_distance = 12.0
	head_pivot.add_child(flashlight_click_player)

	tool_sfx_player = AudioStreamPlayer3D.new()
	tool_sfx_player.max_distance = 16.0
	head_pivot.add_child(tool_sfx_player)

func play_flashlight_click(turned_on: bool) -> void:
	if flashlight_click_player == null:
		return
	flashlight_click_player.stream = _sfx_click_on if turned_on else _sfx_click_off
	if flashlight_click_player.stream:
		flashlight_click_player.pitch_scale = randf_range(0.96, 1.04)
		flashlight_click_player.play()

func _disable_shadows_recursive(node: Node) -> void:
	if node == null:
		return
	if node is GeometryInstance3D:
		(node as GeometryInstance3D).cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	for c in node.get_children():
		_disable_shadows_recursive(c)

func _find_anim_player(node: Node) -> AnimationPlayer:
	if node is AnimationPlayer:
		return node
	for c in node.get_children():
		var found := _find_anim_player(c)
		if found:
			return found
	return null

func _apply_ectoplasm_material_recursive(node: Node) -> void:
	if node is MeshInstance3D:
		var mi := node as MeshInstance3D
		var mat := StandardMaterial3D.new()
		mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		mat.albedo_color = Color(0.32, 0.95, 0.92, 0.72)
		mat.emission_enabled = true
		mat.emission = Color(0.20, 0.88, 0.95)
		mat.emission_energy_multiplier = 2.5
		mi.material_override = mat
	for c in node.get_children():
		_apply_ectoplasm_material_recursive(c)

func _play_ecto_anim(anim_name: String) -> void:
	if ectoplasm_anim_player and ectoplasm_anim_player.has_animation(anim_name):
		if ectoplasm_anim_player.current_animation != anim_name:
			ectoplasm_anim_player.play(anim_name)

## Returns the currently held item dictionary (Hands pseudo-5th slot takes priority when occupied; otherwise hotbar_slots[active_slot])
func get_current_active_item() -> Dictionary:
	if not hand_slot_item.is_empty():
		return hand_slot_item
	if active_slot >= 0 and active_slot < 4:
		return hotbar_slots[active_slot]
	return {}

func is_item_powered_on(item_data: Dictionary) -> bool:
	if item_data.is_empty():
		return false
	return bool(item_data.get("powered_on", true))

## Returns true if the player is carrying `item_id` (in Hands or any of the 4 hotbar slots) AND it is currently powered ON.
func is_carrying_powered_on_item_id(item_id: String) -> bool:
	if not hand_slot_item.is_empty() and str(hand_slot_item.get("id", "")) == item_id and bool(hand_slot_item.get("powered_on", false)):
		return true
	for s in hotbar_slots:
		if not s.is_empty() and str(s.get("id", "")) == item_id and bool(s.get("powered_on", false)):
			return true
	return false

## Returns the list of noise-making gadgets currently left ON on the player's body (which ghosts can hear to locate the player!).
func get_active_noisy_device_names() -> Array[String]:
	var noisy: Array[String] = []
	if is_floating_essence:
		return noisy
	if not hand_slot_item.is_empty() and bool(hand_slot_item.get("powered_on", false)) and bool(hand_slot_item.get("makes_noise_when_on", false)):
		noisy.append(str(hand_slot_item.get("name", "Gadget")))
	for s in hotbar_slots:
		if not s.is_empty() and bool(s.get("powered_on", false)) and bool(s.get("makes_noise_when_on", false)):
			noisy.append(str(s.get("name", "Gadget")))
	if is_firing_proton:
		noisy.append("Proton Stream")
	return noisy

## Returns true if any noise-making gadget (EMF Reader, Spirit Box, Thermal Scanner) is currently left ON on the player.
func is_emitting_device_noise() -> bool:
	if not is_local_player():
		return _net_emitting_noise and not is_floating_essence
	return not get_active_noisy_device_names().is_empty()

## Toggles the power state (ON / OFF) of the currently held gadget in hand/hotbar.
func toggle_active_gadget_power() -> void:
	var cur := get_current_active_item()
	if cur.is_empty() or not bool(cur.get("can_power_toggle", false)):
		return
	var next_on := not bool(cur.get("powered_on", true))
	if not hand_slot_item.is_empty():
		hand_slot_item["powered_on"] = next_on
	elif active_slot >= 0 and active_slot < 4 and not hotbar_slots[active_slot].is_empty():
		hotbar_slots[active_slot]["powered_on"] = next_on

	var cur_id: String = str(cur.get("id", ""))
	if cur_id == "spirit_box":
		spirit_box_power_on = next_on
	elif cur_id == "uv_blacklight":
		uv_light_power_on = next_on
		if next_on:
			GameState.discover_lore("LORE_UV_CIPHER_01", true)

	play_flashlight_click(next_on)
	_refresh_equipped_visuals()
	var item_name: String = str(cur.get("name", "Gadget"))
	var noise_note := " (Emitting Electronic Sound — Ghosts Can Hear!)" if (next_on and bool(cur.get("makes_noise_when_on", false))) else (" (Silent — Safe to Hide)" if not next_on else "")
	GameState.status_message.emit(
		"%s Power: %s%s" % [item_name, "ON" if next_on else "OFF", noise_note],
		Color(0.42, 1.0, 0.72) if next_on else Color(0.75, 0.82, 0.90)
	)

func is_carrying_item_id(item_id: String) -> bool:
	if hand_slot_item.get("id", "") == item_id:
		return true
	for s in hotbar_slots:
		if s.get("id", "") == item_id:
			return true
	return false

func consume_carried_item_id(item_id: String) -> bool:
	if hand_slot_item.get("id", "") == item_id:
		hand_slot_item = {}
		_refresh_equipped_visuals()
		return true
	for i in range(4):
		if hotbar_slots[i].get("id", "") == item_id:
			hotbar_slots[i] = {}
			_refresh_equipped_visuals()
			return true
	return false

func _find_first_empty_hotbar_slot() -> int:
	for i in range(4):
		if hotbar_slots[i].is_empty():
			return i
	return -1

func _unhandled_input(event: InputEvent) -> void:
	if not is_local_player():
		return
	if event is InputEventMouseMotion and Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
		_yaw -= event.relative.x * MOUSE_SENSITIVITY
		_pitch = clampf(_pitch - event.relative.y * MOUSE_SENSITIVITY, deg_to_rad(-82.0), deg_to_rad(82.0))
		rotation.y = _yaw
		head_pivot.rotation.x = _pitch

	if event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_LEFT and Input.get_mouse_mode() != Input.MOUSE_MODE_CAPTURED and not voice_menu_open:
			Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
		elif event.button_index == MOUSE_BUTTON_WHEEL_UP and not is_floating_essence and not voice_menu_open:
			cycle_hotbar_slot(-1)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and not is_floating_essence and not voice_menu_open:
			cycle_hotbar_slot(1)

	if event.is_action_pressed("ui_cancel"):
		if Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
			Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
		else:
			Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

## Returns true if the player is currently attached to and pushing any cart (Equipment Push Cart or Wheeled Light Cart).
func is_pushing_cart() -> bool:
	if not is_inside_tree():
		return false
	for node in get_tree().get_nodes_in_group("mall_carts"):
		var c := node as MallCartSystem
		if c and c.is_attached_to_player and c.attached_player == self:
			return true
	return false

func _physics_process(delta: float) -> void:
	if not is_local_player():
		_tick_remote_peer_interpolation(delta)
		return

	_update_camera_transition(delta)
	_update_vitality_and_buffs(delta)
	_handle_tool_and_hotbar_inputs(delta)
	_tick_live_equipment_indicators(delta)

	if _flash_decay > 0.0:
		_flash_decay = maxf(0.0, _flash_decay - delta * 6.0)
		flash_omni.light_energy = _flash_decay * 14.0
		flash_omni.visible = _flash_decay > 0.02

	var input_dir := Input.get_vector("move_left", "move_right", "move_forward", "move_back")
	if input_dir.length() > 0.1 and contextual_seat_active and not is_floating_essence:
		if not is_eating_meal:
			exit_contextual_third_person()

	var is_slow_heavy := bool(hand_slot_item.get("two_handed_slow", false))
	var pushing_cart := is_pushing_cart()
	var wish_dir := test_override_wish_dir if test_override_wish_dir != Vector3.ZERO else (transform.basis * Vector3(input_dir.x, 0.0, input_dir.y)).normalized()
	var is_moving := (wish_dir != Vector3.ZERO and not contextual_seat_active)

	if not is_on_floor():
		velocity.y -= 9.8 * delta
	elif Input.is_action_just_pressed("jump") and not contextual_seat_active and not is_slow_heavy and not pushing_cart:
		velocity.y = JUMP_VELOCITY

	var speed := WALK_SPEED
	var is_sprinting := false
	if is_floating_essence:
		speed = 6.2
	elif pushing_cart:
		# Cannot sprint while pushing a cart, and moving a cart consumes extra stamina (18.0/s)!
		is_sprinting = false
		if is_moving:
			stamina = maxf(0.0, stamina - 18.0 * delta)
			speed = (WALK_SPEED * 0.82) if stamina > 2.0 else (WALK_SPEED * 0.42)
		else:
			stamina = minf(100.0, stamina + 4.5 * delta)
			speed = WALK_SPEED * 0.82
	elif is_slow_heavy:
		speed = WALK_SPEED * 0.50
		if is_moving:
			stamina = maxf(0.0, stamina - 10.0 * delta)
		else:
			stamina = minf(100.0, stamina + 6.0 * delta)
	elif Input.is_action_pressed("sprint") and stamina > 5.0 and is_moving:
		speed = SPRINT_SPEED
		is_sprinting = true
		stamina = maxf(0.0, stamina - 14.0 * delta)
	else:
		stamina = minf(100.0, stamina + 9.0 * delta)

	if pink_salt_ward_timer > 0.0 and not is_slow_heavy:
		speed *= 1.15
	if food_buff_timer > 0.0 and "Hot Pot" in food_buff_name and not is_slow_heavy:
		speed *= 1.25

	if is_moving:
		velocity.x = wish_dir.x * speed
		velocity.z = wish_dir.z * speed
		_stride_phase += delta * (12.0 if is_sprinting else 8.0)
		character_visual_root.position.y = abs(sin(_stride_phase)) * 0.045
		_play_ecto_anim("sprint" if is_sprinting else "walk")
	else:
		velocity.x = move_toward(velocity.x, 0.0, speed * 8.0 * delta)
		velocity.z = move_toward(velocity.z, 0.0, speed * 8.0 * delta)
		var target_char_y := -0.22 if contextual_seat_active else 0.0
		character_visual_root.position.y = lerpf(character_visual_root.position.y, target_char_y, 0.2)
		_play_ecto_anim("idle")

	if is_floating_essence:
		_stride_phase += delta * 3.2
		ectoplasm_visual_root.position.y = 0.12 + sin(_stride_phase) * 0.08

	_try_stair_step_up(delta, wish_dir)
	move_and_slide()

	if MultiplayerManager != null and MultiplayerManager.is_coop_active and multiplayer.has_multiplayer_peer():
		_net_sync_timer += delta
		if _net_sync_timer >= 0.04:
			_net_sync_timer = 0.0
			var cur_item_name := str(get_current_active_item().get("name", "Empty Hands"))
			_rpc_sync_remote_state.rpc(
				global_position,
				rotation.y,
				head_pivot.rotation.x,
				velocity,
				is_floating_essence,
				belt_flashlight.visible,
				uv_spotlight.visible,
				cur_item_name,
				health,
				is_emitting_device_noise(),
				is_hidden_without_witness,
				contextual_seat_active
			)

func _tick_remote_peer_interpolation(delta: float) -> void:
	if main_camera and main_camera.current:
		main_camera.current = false
	if indicator_rig:
		indicator_rig.visible = false
	global_position = global_position.lerp(_net_target_pos, clampf(delta * 16.0, 0.0, 1.0))
	rotation.y = lerp_angle(rotation.y, _net_target_yaw, clampf(delta * 16.0, 0.0, 1.0))
	if head_pivot:
		head_pivot.rotation.x = lerpf(head_pivot.rotation.x, _net_target_pitch, clampf(delta * 16.0, 0.0, 1.0))
	velocity = _net_velocity

	var horiz_speed := Vector2(_net_velocity.x, _net_velocity.z).length()
	if horiz_speed > 0.25:
		_stride_phase += delta * (12.0 if horiz_speed > 5.2 else 8.0)
		character_visual_root.position.y = abs(sin(_stride_phase)) * 0.045
		_play_ecto_anim("sprint" if horiz_speed > 5.2 else "walk")
	else:
		var target_char_y := -0.22 if contextual_seat_active else 0.0
		character_visual_root.position.y = lerpf(character_visual_root.position.y, target_char_y, 0.2)
		_play_ecto_anim("idle")

	if is_floating_essence:
		_stride_phase += delta * 3.2
		ectoplasm_visual_root.position.y = 0.12 + sin(_stride_phase) * 0.08

	character_visual_root.visible = not is_floating_essence
	ectoplasm_visual_root.visible = is_floating_essence
	if nametag_label:
		nametag_label.visible = true
		_refresh_nametag_text()

@rpc("any_peer", "call_remote", "unreliable_ordered")
func _rpc_sync_remote_state(
	pos: Vector3,
	yaw: float,
	pitch: float,
	vel: Vector3,
	dead_ecto: bool,
	flashlight_on: bool,
	uv_on: bool,
	held_item_label: String,
	hp: float,
	emitting_noise: bool,
	hidden_undetected: bool,
	seated: bool
) -> void:
	if is_local_player():
		return
	_net_target_pos = pos
	_net_target_yaw = yaw
	_net_target_pitch = pitch
	_net_velocity = vel
	is_floating_essence = dead_ecto
	if belt_flashlight:
		belt_flashlight.visible = flashlight_on
	if uv_spotlight:
		uv_spotlight.visible = uv_on
	_net_held_item_label = held_item_label
	health = hp
	_net_emitting_noise = emitting_noise
	is_hidden_without_witness = hidden_undetected
	contextual_seat_active = seated
	_refresh_nametag_text()

## Kinematic Stair Step-Up for small platform stairs and thresholds up to MAX_STEP_HEIGHT (0.42m)
func _try_stair_step_up(delta: float, wish_dir: Vector3) -> void:
	if wish_dir == Vector3.ZERO or not is_on_floor() or velocity.y > 0.1:
		return
	var horiz_motion := Vector3(velocity.x * delta, 0.0, velocity.z * delta)
	if horiz_motion.length_squared() < 0.00001:
		return
	# Only step up if horizontal movement is blocked by a low step riser
	if not test_move(global_transform, horiz_motion):
		return
	var up_vec := Vector3(0.0, MAX_STEP_HEIGHT, 0.0)
	if test_move(global_transform, up_vec):
		return
	var step_xf := global_transform.translated(up_vec)
	var lookahead := horiz_motion.normalized() * maxf(horiz_motion.length(), 0.14)
	if test_move(step_xf, lookahead):
		return
	step_xf = step_xf.translated(lookahead)
	var down_collision := KinematicCollision3D.new()
	if test_move(step_xf, Vector3(0.0, -MAX_STEP_HEIGHT, 0.0), down_collision):
		var normal := down_collision.get_normal()
		if normal.angle_to(Vector3.UP) <= floor_max_angle:
			var step_rise := MAX_STEP_HEIGHT - down_collision.get_travel().length()
			if step_rise > 0.015 and step_rise <= MAX_STEP_HEIGHT:
				global_position.y += step_rise + 0.01

func _update_camera_transition(delta: float) -> void:
	# While dead (is_floating_essence), 3rd-person view on the Ectoplasm Form is strictly locked!
	if is_floating_essence:
		is_third_person = true
	var target_blend: float = 1.0 if is_third_person else 0.0
	_camera_blend = lerpf(_camera_blend, target_blend, clampf(delta * 8.5, 0.0, 1.0))
	var fp_pos := fp_eye_marker.global_position
	var tp_pos := head_pivot.to_global(Vector3(0.35, 0.35, 2.75))
	main_camera.global_position = fp_pos.lerp(tp_pos, _camera_blend)

	character_visual_root.visible = (_camera_blend > 0.18) and not is_floating_essence
	ectoplasm_visual_root.visible = is_floating_essence
	if nametag_label:
		nametag_label.visible = (_camera_blend > 0.35) and not is_floating_essence
		if nametag_label.visible:
			_refresh_nametag_text()

	var cur_item := get_current_active_item()
	var ind_slot: int = cur_item.get("indicator_slot", -1)
	var fp_visible := (_camera_blend < 0.55) and not is_floating_essence
	indicator_rig.visible = fp_visible and (ind_slot >= 0)
	hand_crate_visual.visible = (not is_floating_essence) and (hand_slot_item.get("id", "") == "death_crate")
	heavy_part_visual.visible = (not is_floating_essence) and (hand_slot_item.get("id", "") == "heavy_hydraulic")
	fuse_box_visual.visible = (not is_floating_essence) and (hand_slot_item.get("id", "") == "escalator_fuse")

func enter_contextual_third_person(seat_world_pos: Vector3, reason: String) -> void:
	if is_floating_essence:
		return
	contextual_seat_active = true
	is_third_person = true
	contextual_reason = reason
	global_position = Vector3(seat_world_pos.x, global_position.y, seat_world_pos.z)
	velocity = Vector3.ZERO
	camera_mode_changed.emit(true, reason)
	GameState.status_message.emit("3rd-Person Camera Active: %s (Press WASD or [B] to stand up)" % reason, Color(0.55, 0.95, 1.0))

## Sits the player down at the Food Court table in 3rd-Person View for a minimum of `min_duration` (5.0s) while the eating bar fills up!
func start_eating_at_table(seat_world_pos: Vector3, table_look_target: Vector3, min_duration: float = 5.0) -> void:
	if is_floating_essence:
		return
	is_eating_meal = true
	eating_timer = 0.0
	eating_duration = maxf(5.0, min_duration)
	contextual_seat_active = true
	is_third_person = true
	global_position = Vector3(seat_world_pos.x, global_position.y, seat_world_pos.z)
	velocity = Vector3.ZERO
	var to_table := table_look_target - global_position
	to_table.y = 0.0
	if to_table.length() > 0.05:
		look_at(global_position + to_table.normalized(), Vector3.UP)
		_yaw = rotation.y
	contextual_reason = "Sitting at Food Court Table — Eating Chongqing Hot Pot (0%)"
	camera_mode_changed.emit(true, contextual_reason)
	GameState.status_message.emit(
		"Sitting at Food Court Table — Eating Chongqing Hot Pot (%.0fs minimum seated meal — watch the bar fill up!)." % eating_duration,
		Color(1.0, 0.82, 0.35)
	)

func get_eating_progress_ratio() -> float:
	if not is_eating_meal or eating_duration <= 0.0:
		return 0.0
	return clampf(eating_timer / eating_duration, 0.0, 1.0)

func exit_contextual_third_person() -> void:
	if is_floating_essence or is_eating_meal:
		return
	contextual_seat_active = false
	is_third_person = false
	is_hidden_without_witness = false
	contextual_reason = "First-Person Investigator View"
	camera_mode_changed.emit(false, contextual_reason)

## Pick up a Death Inventory Crate into Hands (Hands-Only — cannot go on the 4-slot hotbar!)
func pickup_death_crate_in_hands(crate: PlaceableEvidenceActors.DeathInventoryCrate) -> void:
	if is_floating_essence or not is_instance_valid(crate):
		return
	if not hand_slot_item.is_empty():
		_drop_hand_item_to_floor("Hands occupied")

	var stored: Array[Dictionary] = []
	for item in crate.stored_items:
		stored.append((item as Dictionary).duplicate(true))

	hand_slot_item = {
		"id": "death_crate",
		"name": "Death Inventory Crate (%d Sealed Items)" % stored.size(),
		"model_path": "",
		"can_store_in_hotbar": false,
		"two_handed_slow": false,
		"indicator_slot": -1,
		"crate_items": stored,
		"killer_name": crate.killer_name,
	}
	crate.remove_from_group("death_inventory_crates")
	crate.queue_free()
	_refresh_equipped_visuals()
	GameState.status_message.emit(
		"CARRYING DEATH INVENTORY CRATE IN HANDS (%d Items Sealed — Unusable Until Opened)! Press [LMB] or [E] to Open & Spill Items to Floor, or [1-4]/[G] to Drop Crate." % stored.size(),
		Color(1.0, 0.72, 0.28)
	)

## Open the Death Inventory Crate currently held in Hands -> spills all stored items onto the floor!
func open_carried_death_crate() -> void:
	if hand_slot_item.get("id", "") != "death_crate":
		return
	var stored: Array = hand_slot_item.get("crate_items", [])
	hand_slot_item = {}
	_refresh_equipped_visuals()
	PlaceableEvidenceActors.DeathInventoryCrate.spill_items_onto_floor(get_parent(), global_position, stored)

## Pick up an individual item off the floor into an empty 1-4 hotbar slot, or into Hands (pseudo-5th slot) if full or Hands-Only!
func pickup_world_item(drop_node: PlaceableEvidenceActors.WorldDroppedItem) -> void:
	if is_floating_essence or not is_instance_valid(drop_node):
		return
	var data: Dictionary = drop_node.item_data.duplicate(true)
	var can_hotbar: bool = data.get("can_store_in_hotbar", true)

	if can_hotbar:
		# Prefer active_slot if empty; otherwise first empty hotbar slot 0..3
		var target_slot := active_slot if hotbar_slots[active_slot].is_empty() else _find_first_empty_hotbar_slot()
		if target_slot != -1:
			hotbar_slots[target_slot] = data
			drop_node.remove_from_group("world_dropped_items")
			drop_node.queue_free()
			if hand_slot_item.is_empty():
				active_slot = target_slot
			_refresh_equipped_visuals()
			GameState.status_message.emit("Picked up %s into 4-Slot Hotbar [%d]!" % [data.get("name", "Item"), target_slot + 1], Color(0.45, 1.0, 0.72))
			return

	# Either all 4 hotbar slots are FULL, or the item is Hands-Only (can_store_in_hotbar == false)!
	if not hand_slot_item.is_empty():
		_drop_hand_item_to_floor("Replaced in Hands")
	hand_slot_item = data
	drop_node.remove_from_group("world_dropped_items")
	drop_node.queue_free()
	_refresh_equipped_visuals()
	if can_hotbar:
		GameState.status_message.emit(
			"4-Slot Hotbar [1-4] is Full! Carrying %s in HANDS (Pseudo-5th Slot) — Switching to [1-4] will drop it onto the floor!" % data.get("name", "Tool"),
			Color(1.0, 0.85, 0.35)
		)
	else:
		GameState.status_message.emit(
			"Carrying %s in HANDS (Hands-Only Item — Cannot be placed on 4-Slot Hotbar; switching to [1-4] or pressing [G] drops it)!" % data.get("name", "Item"),
			Color(1.0, 0.78, 0.32)
		)

func pickup_item_by_id_into_inventory_or_hands(item_id: String) -> void:
	var data := make_item_data(item_id)
	receive_item_from_cart(data)

func receive_item_from_cart(data: Dictionary) -> void:
	if data.is_empty():
		return
	var can_hotbar: bool = data.get("can_store_in_hotbar", true)
	if can_hotbar:
		var target_slot := active_slot if hotbar_slots[active_slot].is_empty() else _find_first_empty_hotbar_slot()
		if target_slot != -1:
			hotbar_slots[target_slot] = data.duplicate(true)
			if hand_slot_item.is_empty():
				active_slot = target_slot
			_refresh_equipped_visuals()
			return
	if not hand_slot_item.is_empty():
		_drop_hand_item_to_floor("Replaced in Hands")
	hand_slot_item = data.duplicate(true)
	_refresh_equipped_visuals()

func _try_place_held_item_on_push_cart() -> bool:
	var cur_item := get_current_active_item()
	if cur_item.is_empty() or str(cur_item.get("id", "")) == "death_crate":
		return false
	var best_pc: MallCartSystem = null
	var best_d := 2.95
	for node in get_tree().get_nodes_in_group("push_carts"):
		var pc := node as MallCartSystem
		if pc and pc.can_accept_item_from_player(self):
			var d := global_position.distance_to(pc.global_position)
			if d < best_d:
				best_d = d
				best_pc = pc
	if best_pc == null:
		return false

	var item_copy := cur_item.duplicate(true)
	item_copy["spirit_freq"] = spirit_box_channels[spirit_box_channel_idx]
	item_copy["uv_narrow"] = uv_narrow_beam
	var placed_slot := best_pc.place_item_on_cart(item_copy)
	if placed_slot == -1:
		return false

	if not hand_slot_item.is_empty():
		hand_slot_item = {}
	elif active_slot >= 0 and active_slot < 4:
		hotbar_slots[active_slot] = {}

	_refresh_equipped_visuals()
	GameState.status_message.emit(
		"Placed %s onto Equipment Push Cart (Slot #%d/8 — %d/8 stored | Press [E] near cart to take items back)." % [
			item_copy.get("name", "Tool"), placed_slot + 1, best_pc.get_stored_item_count()
		],
		Color(0.35, 1.0, 0.78)
	)
	return true

func _drop_hand_item_to_floor(reason: String) -> void:
	if hand_slot_item.is_empty():
		return
	var forward := -global_transform.basis.z.normalized()
	var drop_pos := global_position + forward * 0.85 + Vector3(randf_range(-0.2, 0.2), 0.0, randf_range(-0.2, 0.2))
	var item_name: String = hand_slot_item.get("name", "Hand Item")

	if hand_slot_item.get("id", "") == "death_crate":
		var crate := PlaceableEvidenceActors.DeathInventoryCrate.new()
		for it in hand_slot_item.get("crate_items", []):
			crate.stored_items.append((it as Dictionary).duplicate(true))
		crate.killer_name = hand_slot_item.get("killer_name", "Store Ghost")
		crate.position = drop_pos
		get_parent().add_child(crate)
	else:
		hand_slot_item["spirit_freq"] = spirit_box_channels[spirit_box_channel_idx]
		hand_slot_item["uv_narrow"] = uv_narrow_beam
		var drop := PlaceableEvidenceActors.WorldDroppedItem.new()
		drop.item_data = hand_slot_item.duplicate(true)
		drop.position = drop_pos
		drop.rotation.y = rotation.y
		get_parent().add_child(drop)

	hand_slot_item = {}
	_refresh_equipped_visuals()
	GameState.status_message.emit("Dropped %s from Hands onto the floor (%s)." % [item_name, reason], Color(1.0, 0.72, 0.35))

func _drop_active_hotbar_slot_to_floor() -> void:
	if active_slot < 0 or active_slot >= 4 or hotbar_slots[active_slot].is_empty():
		GameState.status_message.emit("Hotbar Slot [%d] is already empty." % (active_slot + 1), Color(0.75, 0.8, 0.85))
		return
	var item_data := hotbar_slots[active_slot]
	item_data["spirit_freq"] = spirit_box_channels[spirit_box_channel_idx]
	item_data["uv_narrow"] = uv_narrow_beam
	var forward := -global_transform.basis.z.normalized()
	var drop := PlaceableEvidenceActors.WorldDroppedItem.new()
	drop.item_data = item_data.duplicate(true)
	drop.position = global_position + forward * 0.90
	drop.rotation.y = rotation.y
	get_parent().add_child(drop)
	var was_on := bool(item_data.get("powered_on", false)) and bool(item_data.get("can_power_toggle", false))
	hotbar_slots[active_slot] = {}
	_refresh_equipped_visuals()
	GameState.status_message.emit(
		"Threw/Dropped %s onto the floor (%s)." % [
			item_data.get("name", "Tool"),
			"LEFT ON — Still functioning & making sound on the ground!" if was_on else "Switched OFF"
		],
		Color(0.45, 1.0, 0.72) if was_on else Color(0.85, 0.9, 1.0)
	)

## Lethal Catch -> Pack all Player Inventory into a Death Inventory Crate + Locked 3rd-Person Ectoplasm Form
func on_caught_by_store_ghost(killer_name: String) -> void:
	if is_floating_essence:
		return
	if pink_salt_ward_timer > 0.0:
		pink_salt_ward_timer = 0.0
		GameState.status_message.emit("HIMALAYAN PINK SALT WARD SHATTERED — Saved you from %s's lethal catch!" % killer_name, Color(1.0, 0.65, 0.88))
		return

	GameState.total_death_tally += 1
	last_death_position = global_position
	contextual_seat_active = false
	uv_spotlight.visible = false
	is_firing_proton = false

	# Pack all 4 hotbar items + any Hands item into the Death Inventory Crate!
	var crate_items: Array[Dictionary] = []
	for i in range(4):
		if not hotbar_slots[i].is_empty():
			crate_items.append(hotbar_slots[i].duplicate(true))
			hotbar_slots[i] = {}
	if not hand_slot_item.is_empty():
		if hand_slot_item.get("id", "") == "death_crate":
			for sub in hand_slot_item.get("crate_items", []):
				crate_items.append((sub as Dictionary).duplicate(true))
		else:
			crate_items.append(hand_slot_item.duplicate(true))
		hand_slot_item = {}

	_refresh_equipped_visuals()
	player_died_with_inventory.emit(last_death_position, killer_name, crate_items)

	if GameState.sauna_soul_anchor_lit:
		GameState.sauna_soul_anchor_lit = false
		global_position = Vector3(-11.0, 0.35, 11.0)
		health = 100.0
		GameState.status_message.emit(
			"SOUL-ANCHOR RESURRECTION at Sauna! Your %d items are sealed inside a Death Inventory Crate at your death site — carry it in Hands [F] & open it [LMB/E]! (Deaths: %d)" % [crate_items.size(), GameState.total_death_tally],
			Color(1.0, 0.55, 0.78)
		)
	else:
		is_floating_essence = true
		is_third_person = true
		contextual_reason = "DEAD — LOCKED 3RD-PERSON ECTOPLASM FORM (Revive at South Sauna [F/9]!)"
		camera_mode_changed.emit(true, contextual_reason)
		GameState.status_message.emit(
			"YOU DIED (Killed by %s)! Inventory sealed in a Death Crate. Camera locked to 3rd-Person Ectoplasm Form — float to South Sauna & press [F/9] to revive! (Deaths: %d)" % [killer_name, GameState.total_death_tally],
			Color(1.0, 0.3, 0.35)
		)

func revive_from_sauna_ritual() -> void:
	if not is_floating_essence:
		return
	is_floating_essence = false
	is_third_person = false
	health = 100.0
	stamina = 100.0
	contextual_reason = "First-Person Investigator View (Find & Open Your Death Inventory Crate!)"
	camera_mode_changed.emit(false, contextual_reason)
	_refresh_equipped_visuals()
	GameState.status_message.emit(
		"SAUNA VOICE RITUAL COMPLETE — Resurrected in 1st-Person View! Return to your Death Inventory Crate [F] to carry it in Hands, then open it [LMB/E] to drop your items!",
		Color(0.4, 1.0, 0.75)
	)

func _on_voice_command_for_revival(phrase: String, _speaker_pos: Vector3, _freq: float) -> void:
	if is_floating_essence and global_position.distance_to(Vector3(-11.0, 0.0, 11.0)) < 7.0:
		if "shift is over" in phrase.to_lower() or "where are you" in phrase.to_lower():
			revive_from_sauna_ritual()

func _update_vitality_and_buffs(delta: float) -> void:
	if pink_salt_ward_timer > 0.0:
		pink_salt_ward_timer = maxf(0.0, pink_salt_ward_timer - delta)
	if food_buff_timer > 0.0:
		food_buff_timer = maxf(0.0, food_buff_timer - delta)
		if food_buff_timer <= 0.0:
			food_buff_name = "None"

	if is_eating_meal:
		eating_timer = minf(eating_duration, eating_timer + delta)
		health = minf(100.0, health + 10.0 * delta)
		stamina = minf(100.0, stamina + 16.0 * delta)
		var pct := int(round(get_eating_progress_ratio() * 100.0))
		contextual_reason = "Sitting at Food Court Table — Eating Hot Pot (%d%% | %.1fs / %.1fs)" % [pct, eating_timer, eating_duration]
		if eating_timer >= eating_duration:
			is_eating_meal = false
			food_buff_timer = 90.0
			food_buff_name = "Chongqing Hot Pot (+25% Speed)"
			contextual_reason = "Finished Eating at Food Court Table (Press WASD or [B] to Stand Up)"
			meal_finished.emit()
			GameState.status_message.emit(
				"Finished Eating Chongqing Hot Pot! (+HP/Stamina Filled, +45% Light Cart Battery, 90s Speed Surge — Press WASD to Stand Up!)",
				Color(0.45, 1.0, 0.72)
			)
	elif contextual_seat_active:
		if "Rest Bench" in contextual_reason:
			stamina = minf(100.0, stamina + 24.0 * delta)
			health = minf(100.0, health + 8.0 * delta)
		elif "Pink Salt" in contextual_reason or "Spa" in contextual_reason:
			health = minf(100.0, health + 18.0 * delta)
			stamina = minf(100.0, stamina + 30.0 * delta)
			pink_salt_ward_timer = 60.0

	vitality_updated.emit(health, stamina, pink_salt_ward_timer, food_buff_name)

func _handle_tool_and_hotbar_inputs(delta: float) -> void:
	if Input.is_action_just_pressed("toggle_flashlight"):
		belt_flashlight.visible = not belt_flashlight.visible
		play_flashlight_click(belt_flashlight.visible)
		GameState.status_message.emit("Belt Flashlight [T]: %s" % ("ON" if belt_flashlight.visible else "OFF"), Color(0.95, 0.95, 0.7))

	if Input.is_action_just_pressed("camera_perspective"):
		if is_floating_essence:
			GameState.status_message.emit("Locked in 3rd-Person Ectoplasm Form while DEAD! Float to the South Sauna Shrine (-11, 0, 11) and press [F] or [J] to resurrect.", Color(0.35, 1.0, 0.95))
		elif is_eating_meal:
			GameState.status_message.emit("Still eating at the Food Court table (%.1fs remaining)!" % maxf(0.0, eating_duration - eating_timer), Color(1.0, 0.85, 0.35))
		else:
			contextual_seat_active = false
			is_third_person = not is_third_person
			contextual_reason = "3rd-Person Overview [B/F5]" if is_third_person else "First-Person Investigator View"
			camera_mode_changed.emit(is_third_person, contextual_reason)

	if is_floating_essence:
		is_firing_proton = false
		if spirit_static_player and spirit_static_player.playing:
			spirit_static_player.stop()
		return

	# [G] Place on Equipment Push Cart (if standing near Push Cart with placement outline active) OR Drop Item on Floor
	if Input.is_action_just_pressed("drop_item"):
		if _try_place_held_item_on_push_cart():
			return
		if not hand_slot_item.is_empty():
			_drop_hand_item_to_floor("Pressed [G] Drop")
		else:
			_drop_active_hotbar_slot_to_floor()
		return

	if voice_menu_open:
		return

	# Strict 4-Slot Hotbar selection [1]–[4]
	if Input.is_action_just_pressed("hotbar_1"):
		_select_hotbar_slot(0)
	elif Input.is_action_just_pressed("hotbar_2"):
		_select_hotbar_slot(1)
	elif Input.is_action_just_pressed("hotbar_3"):
		_select_hotbar_slot(2)
	elif Input.is_action_just_pressed("hotbar_4"):
		_select_hotbar_slot(3)

	var cur_item := get_current_active_item()
	var cur_id: String = cur_item.get("id", "")

	# If holding the Death Inventory Crate in Hands: stored items are NOT usable until opened!
	# Pressing Primary Use [LMB/R] or Secondary [E] opens the crate and spills all items onto the floor!
	if cur_id == "death_crate":
		if Input.is_action_just_pressed("secondary_action") or (Input.is_action_just_pressed("primary_use") and Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED):
			open_carried_death_crate()
		return

	if Input.is_action_just_pressed("secondary_action"):
		_use_secondary_mode_toggle(cur_id)

	if cur_id == "proton_gun" and Input.is_action_pressed("primary_use") and Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
		if proton_heat < 99.0:
			is_firing_proton = true
			proton_heat = minf(100.0, proton_heat + 22.0 * delta)
			_pitch = clampf(_pitch + randf_range(-0.003, 0.004), deg_to_rad(-82.0), deg_to_rad(82.0))
			head_pivot.rotation.x = _pitch
			_tick_proton_beam_targets(delta)
		else:
			is_firing_proton = false
	else:
		is_firing_proton = false
		proton_heat = maxf(0.0, proton_heat - 28.0 * delta)

	if cur_id != "proton_gun" and Input.is_action_just_pressed("primary_use") and Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
		_use_primary_tool_action(cur_id)

## Cycles through the 4 hotbar slots [1]-[4] via Mouse Wheel Up (-1) or Mouse Wheel Down (+1).
func cycle_hotbar_slot(step: int) -> void:
	var next_slot := (active_slot + step) % 4
	if next_slot < 0:
		next_slot += 4
	_select_hotbar_slot(next_slot)

## Switching to a 1-4 hotbar slot while holding an item in Hands:
## - If the Hands item is hotbar-compatible AND the player has an open hotbar slot, it stores it.
## - Otherwise (Hands-Only item like Death Crate / Hydraulic Assembly / Fuse Crate, OR no open hotbar slot),
##   switching to [1]-[4] or scrolling the mouse wheel automatically drops the Hands item onto the floor!
func _select_hotbar_slot(slot: int) -> void:
	var clamped := clampi(slot, 0, 3)
	if not hand_slot_item.is_empty():
		var can_hotbar: bool = hand_slot_item.get("can_store_in_hotbar", false)
		var empty_idx := clamped if hotbar_slots[clamped].is_empty() else _find_first_empty_hotbar_slot()
		if can_hotbar and empty_idx != -1:
			hotbar_slots[empty_idx] = hand_slot_item
			hand_slot_item = {}
			GameState.status_message.emit("Moved %s from Hands into Hotbar Slot [%d]." % [hotbar_slots[empty_idx].get("name", "Tool"), empty_idx + 1], Color(0.55, 0.95, 1.0))
		else:
			_drop_hand_item_to_floor("Switched to Hotbar [%d] with no slot for Hand item" % (clamped + 1))

	active_slot = clamped
	_refresh_equipped_visuals()

func get_slot_subtext(item_data: Dictionary) -> String:
	if item_data.is_empty():
		return "EMPTY"
	var item_id: String = str(item_data.get("id", ""))
	var is_on: bool = bool(item_data.get("powered_on", true))
	match item_id:
		"emf_reader":
			return ("ON (LVL %d)" % _current_emf_reading) if is_on else "OFF (SILENT)"
		"thermal_scanner":
			if not is_on:
				return "OFF (SILENT)"
			return "ON (IR °F)" if use_fahrenheit else "ON (IR °C)"
		"spirit_box":
			return ("ON %.1f FM" % spirit_box_channels[spirit_box_channel_idx]) if is_on else "OFF (SILENT)"
		"uv_blacklight":
			if not is_on:
				return "OFF"
			return "ON 22° UV" if uv_narrow_beam else "ON 40° UV"
		"photo_camera":
			return "%d/10 EXP" % photo_exposures_left
		"light_remote":
			return "60m IR"
		"proton_gun":
			return "HEAT %.0f%%" % proton_heat
		"death_crate":
			var items: Array = item_data.get("crate_items", [])
			return "%d SEALED" % items.size()
		"heavy_hydraulic":
			return "2-HANDED"
		"escalator_fuse":
			return "RELAY FUSE"
		_:
			return "READY"

func _refresh_equipped_visuals() -> void:
	var cur_item := get_current_active_item()
	var ind_slot: int = cur_item.get("indicator_slot", -1)
	var model_path: String = cur_item.get("model_path", "")
	var cur_on: bool = is_item_powered_on(cur_item)
	uv_spotlight.visible = (ind_slot == 3 and cur_on and not is_floating_essence)
	if spirit_static_player:
		var should_play_static := (is_carrying_powered_on_item_id("spirit_box") and not is_floating_essence)
		if should_play_static and not spirit_static_player.playing:
			spirit_static_player.volume_db = -16.5
			spirit_static_player.play()
		elif not should_play_static and spirit_static_player.playing:
			spirit_static_player.stop()
	if ind_slot >= 0 and indicator_rig:
		indicator_rig.setup_for_slot(ind_slot, model_path)
	hotbar_changed.emit(active_slot, cur_item.get("name", "Empty Hands"), get_active_tool_mode_text())

func _tick_live_equipment_indicators(delta: float) -> void:
	var cur_item := get_current_active_item()
	var ind_slot: int = cur_item.get("indicator_slot", -1)
	var cur_on: bool = is_item_powered_on(cur_item)

	var g := _get_nearest_ghost()
	var emf_lvl: int = 1
	var temp_c: float = 20.5
	var spirit_tell: String = "STATIC SCANNING..."
	var weak_pct: float = 0.0
	var ghost_dist: float = 999.0

	if g and not g.is_resolved:
		ghost_dist = global_position.distance_to(g.global_position)
		weak_pct = g.weakened_percent
		if ghost_dist <= 16.0:
			spirit_tell = g.get_disposition_tell_short()
		if ghost_dist <= 2.6:
			emf_lvl = 5
			temp_c = -6.8
			if ind_slot == 0 and cur_on:
				GameState.record_evidence(g.store_id, "EMF-5", 35)
			elif ind_slot == 1 and cur_on:
				GameState.record_evidence(g.store_id, "Freezing Temps", 35)
		elif ghost_dist <= 3.6 or g.current_phase == StoreGhost.GhostPhase.VISIBLE_HUNT:
			emf_lvl = 4
			temp_c = -3.8
			if ind_slot == 1 and cur_on:
				GameState.record_evidence(g.store_id, "Freezing Temps", 35)
		elif ghost_dist <= 6.5:
			emf_lvl = 3
			temp_c = 1.5
		elif ghost_dist <= 12.0:
			emf_lvl = 2
			temp_c = 5.5

	_current_emf_reading = emf_lvl
	_current_temp_reading_c = temp_c

	# 1. EMF Reader Audio Beeping: plays if the player is carrying a powered-ON EMF Reader (in hand or hotbar)!
	#    Turning the EMF Reader OFF ([LMB]) immediately silences it so ghosts cannot track its beeps!
	if is_carrying_powered_on_item_id("emf_reader") and emf_beep_player and not is_floating_essence:
		var intervals := [1.30, 0.58, 0.30, 0.15, 0.075]
		var pitches := [0.88, 1.05, 1.25, 1.50, 1.80]
		var vols := [-13.0, -8.5, -5.0, -2.0, 0.5]
		var idx := clampi(emf_lvl - 1, 0, 4)
		var stow_offset := 0.0 if (ind_slot == 0 and cur_on) else -4.5
		_emf_beep_timer += delta
		if _emf_beep_timer >= intervals[idx]:
			_emf_beep_timer = 0.0
			emf_beep_player.pitch_scale = pitches[idx]
			emf_beep_player.volume_db = vols[idx] + stow_offset
			emf_beep_player.play()
	else:
		_emf_beep_timer = 0.0

	# 2. Thermal Scanner Freezing Chirp: chirps when a powered-ON Thermometer detects <0°C!
	if is_carrying_powered_on_item_id("thermal_scanner") and temp_c < 0.0 and tool_sfx_player and _sfx_thermal_beep and not is_floating_essence:
		_thermal_beep_timer += delta
		if _thermal_beep_timer >= 1.8:
			_thermal_beep_timer = 0.0
			tool_sfx_player.stream = _sfx_thermal_beep
			tool_sfx_player.pitch_scale = 1.15
			tool_sfx_player.play()
	else:
		_thermal_beep_timer = 0.0

	# 3. Spirit Box Radio Static Loop (Tuned down in volume; plays while Spirit Box is powered ON in hand or hotbar!)
	if spirit_static_player and not is_floating_essence:
		if is_carrying_powered_on_item_id("spirit_box"):
			if not spirit_static_player.playing:
				spirit_static_player.play()
			var stow_atten := 0.0 if (ind_slot == 2 and cur_on) else -5.0
			spirit_static_player.volume_db = lerpf(-13.0, -19.5, clampf(ghost_dist / 16.0, 0.0, 1.0)) + stow_atten
			spirit_static_player.pitch_scale = randf_range(0.96, 1.05) if ghost_dist < 8.0 else 1.0
		elif spirit_static_player.playing:
			spirit_static_player.stop()

	if ind_slot < 0:
		return

	# 4. UV Blacklight Cone Detection
	if uv_spotlight.visible:
		var forward := -main_camera.global_transform.basis.z.normalized()
		var max_r := 9.5 if uv_narrow_beam else 6.2
		var max_ang := 22.0 if uv_narrow_beam else 40.0
		for p in get_tree().get_nodes_in_group("uv_evidence_prints"):
			var uv_p := p as PlaceableEvidenceActors.UVEvidencePrint
			if uv_p:
				var to_p := uv_p.global_position - main_camera.global_position
				if to_p.length() <= max_r and rad_to_deg(forward.angle_to(to_p.normalized())) <= max_ang:
					if uv_p.illuminate_by_uv():
						uv_prints_discovered += 1
		for gr in get_tree().get_nodes_in_group("hallway_gremlins"):
			var grem := gr as PlaceableEvidenceActors.HallwayGremlin
			if grem:
				var to_g := grem.global_position - main_camera.global_position
				if to_g.length() <= max_r and rad_to_deg(forward.angle_to(to_g.normalized())) <= max_ang:
					grem.stun_gremlin("UV Blacklight Beam")

	var photo_lock := "AIM AT GHOST / UV / LEDGER"
	if g and not g.is_resolved and ghost_dist <= 12.0:
		if g.current_phase == StoreGhost.GhostPhase.VISIBLE_WARNING or g.current_phase == StoreGhost.GhostPhase.VISIBLE_HUNT:
			photo_lock = "LOCK: 5-STAR GHOST [$100 + 4s STUN]"
		else:
			photo_lock = "LOCK: 4-STAR ECTOPLASM [$65]"

	var fix_info := "AIM AT FIXTURE / LIGHT CART"
	var aimed_fix := _find_aimed_light_fixture()
	if aimed_fix:
		fix_info = "[%s] Mode: %d" % [aimed_fix.store_id, int(aimed_fix.current_mode)]

	indicator_rig.update_indicators(
		ind_slot,
		emf_lvl,
		temp_c,
		use_fahrenheit,
		photo_exposures_left,
		photo_lock,
		spirit_box_channels[spirit_box_channel_idx],
		spirit_tell,
		uv_prints_discovered,
		uv_narrow_beam,
		fix_info,
		proton_heat,
		is_firing_proton,
		weak_pct,
		cur_on
	)

func _tick_proton_beam_targets(delta: float) -> void:
	var forward := -main_camera.global_transform.basis.z.normalized()
	for node in get_tree().get_nodes_in_group("store_ghosts"):
		var g := node as StoreGhost
		if g and not g.is_resolved:
			var to_g := (g.global_position + Vector3(0, 1.0, 0)) - main_camera.global_position
			if to_g.length() <= 9.5 and rad_to_deg(forward.angle_to(to_g.normalized())) <= 28.0:
				g.apply_proton_beam(delta)
	for gr in get_tree().get_nodes_in_group("hallway_gremlins"):
		var grem := gr as PlaceableEvidenceActors.HallwayGremlin
		if grem:
			var to_gr := grem.global_position - main_camera.global_position
			if to_gr.length() <= 9.5 and rad_to_deg(forward.angle_to(to_gr.normalized())) <= 28.0:
				grem.stun_gremlin("Proton Stream")

func _deploy_ectoplasm_canister() -> void:
	var trap := PlaceableEvidenceActors.EctoplasmCanisterTrap.new()
	var forward := -global_transform.basis.z.normalized()
	trap.position = global_position + forward * 1.5
	get_parent().add_child(trap)
	play_flashlight_click(true)
	GameState.status_message.emit("Deployed Ectoplasm Canister Trap! Weaken the Ghost to 100% with the Proton Ray Gun [LMB] to capture it!", Color(0.35, 1.0, 0.65))

func _use_secondary_mode_toggle(cur_id: String) -> void:
	match cur_id:
		"emf_reader":
			if emf_beep_player:
				emf_beep_player.pitch_scale = 1.15
				emf_beep_player.play()
			GameState.status_message.emit("EMF Reader Calibrated (Current Field: EMF Level %d/5)" % _current_emf_reading, Color(0.45, 1.0, 0.68))
		"thermal_scanner":
			use_fahrenheit = not use_fahrenheit
			if tool_sfx_player and _sfx_thermal_beep:
				tool_sfx_player.stream = _sfx_thermal_beep
				tool_sfx_player.play()
			GameState.status_message.emit("Thermal Scanner Units [E]: %s" % ("Fahrenheit (°F)" if use_fahrenheit else "Celsius (°C)"), Color(0.65, 0.95, 1.0))
		"spirit_box":
			spirit_box_channel_idx = (spirit_box_channel_idx + 1) % spirit_box_channels.size()
			var freq: float = spirit_box_channels[spirit_box_channel_idx]
			AcousticVoipManager.active_spirit_box_channel = freq
			play_flashlight_click(true)
			GameState.status_message.emit("Spirit Box tuned to %.1f FM (Press [J] to choose what to say to the Ghost!)" % freq, Color(0.95, 0.72, 0.35))
			if absf(freq - 104.7) < 0.1:
				GameState.discover_lore("LORE_RADIO_104_7", true)
		"uv_blacklight":
			uv_narrow_beam = not uv_narrow_beam
			uv_spotlight.spot_angle = 22.0 if uv_narrow_beam else 40.0
			uv_spotlight.spot_range = 9.5 if uv_narrow_beam else 6.2
			play_flashlight_click(uv_narrow_beam)
			GameState.status_message.emit("UV Blacklight Beam [E]: %s" % ("22° Focused (9.5m)" if uv_narrow_beam else "40° Flood (6.2m)"), Color(0.75, 0.50, 1.0))
		"light_remote":
			_fire_light_remote_gun()
		"proton_gun":
			_deploy_ectoplasm_canister()

func _use_primary_tool_action(cur_id: String) -> void:
	match cur_id:
		"emf_reader", "thermal_scanner", "spirit_box", "uv_blacklight":
			toggle_active_gadget_power()
		"photo_camera":
			_snap_evidence_photo()
		"light_remote":
			_fire_light_remote_gun()

## Speaks a phrase from the [J] On-Screen Voice Command Menu (plays human voice sample + triggers Spirit Box Ghost EVP audio response!)
func speak_voice_prompt(phrase: String) -> void:
	var lower := phrase.to_lower()
	var sample_path := "res://ExternalAssets/Voice/ValidationSamples/where_are_you.wav"
	if "leave me alone" in lower:
		sample_path = "res://ExternalAssets/Voice/ValidationSamples/leave_me_alone.wav"
	elif "sign" in lower or "write" in lower:
		sample_path = "res://ExternalAssets/Voice/ValidationSamples/give_me_a_sign.wav"
	elif "shift is over" in lower or "who" in lower or "old" in lower:
		sample_path = "res://ExternalAssets/Voice/ValidationSamples/come_here.wav"

	if tool_sfx_player and ResourceLoader.exists(sample_path):
		tool_sfx_player.stream = load(sample_path)
		tool_sfx_player.pitch_scale = 1.0
		tool_sfx_player.play()

	if MultiplayerManager != null and MultiplayerManager.is_coop_active and multiplayer.has_multiplayer_peer():
		_rpc_play_remote_voice_sample.rpc(sample_path)

	AcousticVoipManager.emit_spoken_phrase(phrase)

	# If a Ghost is within 18m, play the Ghost's EVP voice response through the Spirit Box!
	var g := _get_nearest_ghost()
	if g and not g.is_resolved and global_position.distance_to(g.global_position) <= 18.0 and spirit_evp_player:
		spirit_evp_player.stream = _sfx_spirit_evp_corrupted if g.is_corrupted_hostile else _sfx_spirit_evp_receptive
		if spirit_evp_player.stream:
			spirit_evp_player.pitch_scale = randf_range(0.82, 0.94) if g.is_corrupted_hostile else randf_range(0.95, 1.05)
			spirit_evp_player.play()

@rpc("any_peer", "call_remote", "reliable")
func _rpc_play_remote_voice_sample(sample_path: String) -> void:
	if voip_emitter and ResourceLoader.exists(sample_path):
		voip_emitter.stream = load(sample_path)
		voip_emitter.pitch_scale = 1.0
		voip_emitter.play()

func _snap_evidence_photo() -> void:
	if photo_exposures_left <= 0:
		play_flashlight_click(false)
		GameState.status_message.emit("Photo Camera out of film exposures (0/10)!", Color(1.0, 0.4, 0.4))
		return
	photo_exposures_left -= 1
	_flash_decay = 1.0
	flash_omni.visible = true
	if tool_sfx_player and _sfx_camera_shutter:
		tool_sfx_player.stream = _sfx_camera_shutter
		tool_sfx_player.pitch_scale = 1.0
		tool_sfx_player.play()

	for gr in get_tree().get_nodes_in_group("hallway_gremlins"):
		var grem := gr as PlaceableEvidenceActors.HallwayGremlin
		if grem and global_position.distance_to(grem.global_position) <= 7.5:
			grem.stun_gremlin("Camera Xenon Flash")

	var ghost := _get_nearest_ghost()
	if ghost and not ghost.is_resolved:
		var dist := global_position.distance_to(ghost.global_position)
		if (ghost.current_phase == StoreGhost.GhostPhase.VISIBLE_WARNING or ghost.current_phase == StoreGhost.GhostPhase.VISIBLE_HUNT) and dist <= 12.0:
			GameState.add_cash(100, "5-STAR GHOST PHOTOGRAPH (%s)" % ghost.ghost_name)
			if dist <= 6.5:
				ghost.apply_camera_flash_stun(4.0)
			return
		elif dist <= 8.5:
			GameState.add_cash(65, "4-STAR ECTOPLASMIC / UV EVIDENCE PHOTO")
			return

	GameState.add_cash(30, "2-STAR ANOMALY / PROP PHOTO")

func _find_aimed_light_fixture() -> MallLightFixture:
	var origin := main_camera.global_position
	var forward := -main_camera.global_transform.basis.z.normalized()
	var best_fixture: MallLightFixture = null
	var best_dot: float = 0.72
	for node in get_tree().get_nodes_in_group("mall_light_fixtures"):
		var fix := node as MallLightFixture
		if fix == null:
			continue
		var to_fix := fix.global_position - origin
		if to_fix.length() <= 60.0:
			var d := forward.dot(to_fix.normalized())
			if d > best_dot:
				best_dot = d
				best_fixture = fix
	return best_fixture

func _fire_light_remote_gun() -> void:
	play_flashlight_click(true)
	var origin := main_camera.global_position
	var forward := -main_camera.global_transform.basis.z.normalized()
	for node in get_tree().get_nodes_in_group("light_carts"):
		var lc := node as MallCartSystem
		if lc:
			var to_lc := (lc.global_position + Vector3(0.0, 1.4, 0.0)) - origin
			if to_lc.length() <= 60.0 and forward.dot(to_lc.normalized()) > 0.80:
				lc.toggle_floodlight("Light Remote Gun [60m]")
				return

	var best_fixture := _find_aimed_light_fixture()
	if best_fixture:
		var new_mode := best_fixture.cycle_remote_mode()
		GameState.status_message.emit("Light Remote Gun [60m] -> Fixture [%s]: %s" % [best_fixture.store_id, new_mode], Color(1.0, 0.92, 0.45))
	else:
		for node in get_tree().get_nodes_in_group("mall_light_fixtures"):
			var fix := node as MallLightFixture
			if fix and fix.global_position.distance_to(global_position) < 14.0:
				fix.cycle_remote_mode()
		GameState.status_message.emit("Light Remote Gun -> Cycled nearby zone fixtures (100% / 45% Dim / OFF)", Color(1.0, 0.92, 0.45))

func _get_nearest_ghost() -> StoreGhost:
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

func get_hotbar_bar_summary() -> String:
	var parts: Array[String] = []
	for i in range(4):
		var slot_name: String = hotbar_slots[i].get("name", "Empty").split(" (")[0]
		if hand_slot_item.is_empty() and active_slot == i:
			parts.append("[color=#ffe680][b][%d: %s][/b][/color]" % [i + 1, slot_name])
		else:
			parts.append("[%d: %s]" % [i + 1, slot_name])
	var hand_str: String = "Empty"
	if not hand_slot_item.is_empty():
		hand_str = "[color=#ffaa44][b]%s (Switch [1-4] Drops if No Slot)[/b][/color]" % hand_slot_item.get("name", "Item")
	return "%s  |  [b]Hands (5th):[/b] %s" % [" ".join(parts), hand_str]

func get_active_tool_mode_text() -> String:
	if is_floating_essence:
		return "DEAD (Ectoplasm Form) — Float to South Sauna (-11, 0, 11) & Press [F] or [J] to Revive!"
	var cur := get_current_active_item()
	var cur_id: String = cur.get("id", "")
	var cur_on: bool = is_item_powered_on(cur)
	match cur_id:
		"death_crate":
			return "HANDS-ONLY CRATE (Items Sealed & Unusable!) — Press [LMB] or [E] to Open & Spill Items to Floor!"
		"heavy_hydraulic":
			return "HANDS-ONLY TWO-HANDED CARRY (-50% Speed | [G] or [1-4] Drop | [F] Install at F2 Freight Elevator)"
		"escalator_fuse":
			return "HANDS-ONLY RELAY FUSE CRATE ([F] Insert at Escalator Breaker | [G] or [1-4] Drop)"
		"emf_reader":
			return "5-LED EMF Reader [%s] ([LMB] Toggle ON/OFF | [G] Throw/Drop Active on Floor)" % ("ON — Beeping" if cur_on else "OFF — Silent")
		"thermal_scanner":
			return "Digital IR Thermometer [%s] ([LMB] Toggle ON/OFF | [E] °C/°F | [G] Throw/Drop on Floor)" % ("ON" if cur_on else "OFF — Silent")
		"spirit_box":
			return "Spirit Box %.1f FM [%s] ([LMB] Toggle ON/OFF | [E] Tune FM | [J] Speak | [G] Drop)" % [spirit_box_channels[spirit_box_channel_idx], "ON — Static" if cur_on else "OFF — Silent"]
		"uv_blacklight":
			return "UV Blacklight [%s] ([LMB] Toggle ON/OFF | [E] 22°/40° Beam | [G] Throw/Drop on Floor)" % ("ON" if cur_on else "OFF")
		"photo_camera":
			return "Exposures: %d/10 | [LMB/R] Xenon Flash ($30-$100 + 4s Stun | [G] Drop)" % photo_exposures_left
		"light_remote":
			return "60m Light Remote ([LMB/E] Toggle Ceiling Fixtures or Light Cart | [G] Drop)"
		"proton_gun":
			return "Proton Stream [Hold LMB] (Heat %.0f%%) | [E] Deploy Ectoplasm Canister Trap | [G] Drop" % proton_heat
		_:
			return "Empty Slot — Walk up to any Dropped Tool or Crate and press [F] to pick up!"
