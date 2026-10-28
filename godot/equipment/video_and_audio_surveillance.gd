@tool
class_name VideoAndAudioSurveillance
extends Node3D
## Category A (Floors 2–6) Video & Audio Surveillance System from Docs/GameDesignBible.md:
## 1. GhostOrbActor — Floating Lissajous/figure-8 IR anomaly inside haunted upper-floor stores,
##    invisible to the naked eye (`visible = false`), revealed in cyan/green IR Night Vision.
## 2. PlacedVideoCameraActor — Standalone placed `video_camera` (Y + 0.25m) or `assembled_video_rig`
##    (`Video Camera + Collapsible Camera Stand`, 1.45m tall tripod with motorized pan/tilt head).
##    Detects Ghost Orbs (`14.0m` cone) and reveals Camera-Shy Ghosts on the Cart Monitor when
##    0 living players are inside the ghost's `store_territory_aabb`.
## 3. CartComputerMonitor — Mounted at `Vector3(-0.42, 1.16, 0.0)` right at the front of the rear
##    push-handle of every `MallCartSystem` (`PushCart`), leaving the top shelf open while sitting
##    in front of the pusher's face (comedic view obstruction or live night-vision "dashcam" when a
##    switched-on Video Camera hangs on the cart's exterior hook). Cycles CAM 01..04, remotely pans
##    tripod heads (+35°), and captures `$50–$85` video snapshots.
## 4. ParanormalAudioRecorderActor — High-gain directional tape/digital EVP recorder (`AMallAudioRecorder`)
##    that records autonomous sub-audible whispers, weeping, phantom cash-register chatter, and reversed
##    speech over 4.0s within 15.0m of a StoreGhost, and plays back forward/reverse (`[E]`) to confirm
##    `"Recorded EVP Audio"` evidence, reveal Receptive vs. Corrupted vocal tells, and award `+$75` bounty.


# ------------------------------------------------------------------------------
# Static Item Catalog & Visual Builders for Hotbar / Hands / Cart Hooks
# ------------------------------------------------------------------------------
static func make_video_camera_item() -> Dictionary:
	return {
		"id": "video_camera",
		"name": "Night-Vision Video Camera (IR Flip-Out LCD)",
		"price": 65,
		"can_store_in_hotbar": true,
		"can_hang_on_cart": true,
		"can_power_toggle": true,
		"powered_on": true,
		"ir_night_vision_on": true,
		"makes_noise_when_on": false,
		"model_path": "",
	}


static func make_camera_stand_item() -> Dictionary:
	return {
		"id": "camera_stand",
		"name": "Collapsible Camera Stand (145cm Motorized Tripod)",
		"price": 45,
		"can_store_in_hotbar": true,
		"can_hang_on_cart": true,
		"can_power_toggle": false,
		"powered_on": false,
		"makes_noise_when_on": false,
		"model_path": "",
	}


static func make_assembled_video_rig_item() -> Dictionary:
	return {
		"id": "assembled_video_rig",
		"name": "Assembled Video Camera + 145cm Tripod Stand Rig [Hands-Only / Cart Hook]",
		"price": 110,
		"can_store_in_hotbar": false, # Pseudo-5th Hands-Only slot when assembled!
		"can_hang_on_cart": true,
		"can_power_toggle": true,
		"powered_on": true,
		"ir_night_vision_on": true,
		"makes_noise_when_on": false,
		"model_path": "",
	}


static func make_audio_recorder_item() -> Dictionary:
	return {
		"id": "audio_recorder",
		"name": "Paranormal Audio / EVP Tape Recorder",
		"price": 70,
		"can_store_in_hotbar": true,
		"can_hang_on_cart": false,
		"can_power_toggle": true,
		"powered_on": true,
		"makes_noise_when_on": false,
		"recorded_evp_ready": false,
		"recorded_store_id": "",
		"recorded_ghost_name": "",
		"recorded_is_corrupted": false,
		"bounty_claimed": false,
		"model_path": "",
	}


# ------------------------------------------------------------------------------
# 1. Ghost Orb Actor (Floating Lissajous IR Anomaly in Upper-Floor Stores)
# ------------------------------------------------------------------------------
class GhostOrbActor extends Node3D:
	var store_id: String = "East_F2_01"
	var base_position: Vector3 = Vector3.ZERO
	var ir_revealed: bool = false
	var _phase_time: float = 0.0
	var _reveal_grace_timer: float = 0.0
	var _orb_cluster_root: Node3D = null
	var _primary_orb_mat: StandardMaterial3D = null
	var _secondary_orb_mat: StandardMaterial3D = null
	var _orb_light: OmniLight3D = null
	var _label_3d: Label3D = null

	func _ready() -> void:
		add_to_group("ghost_orbs")
		if base_position == Vector3.ZERO:
			base_position = position
		_phase_time = randf() * TAU

		_orb_cluster_root = Node3D.new()
		add_child(_orb_cluster_root)

		# Primary translucent pale-white night-vision orb (emits zero world light!)
		var main_mi := MeshInstance3D.new()
		var main_sph := SphereMesh.new()
		main_sph.radius = 0.085
		main_sph.height = 0.17
		main_mi.mesh = main_sph
		main_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		_primary_orb_mat = StandardMaterial3D.new()
		_primary_orb_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		_primary_orb_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		_primary_orb_mat.albedo_color = Color(0.92, 0.95, 1.0, 0.78)
		main_mi.material_override = _primary_orb_mat
		_orb_cluster_root.add_child(main_mi)

		# Outer soft halo sphere
		var halo_mi := MeshInstance3D.new()
		var halo_sph := SphereMesh.new()
		halo_sph.radius = 0.18
		halo_sph.height = 0.36
		halo_mi.mesh = halo_sph
		halo_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		_secondary_orb_mat = StandardMaterial3D.new()
		_secondary_orb_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		_secondary_orb_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		_secondary_orb_mat.albedo_color = Color(0.85, 0.90, 0.98, 0.28)
		halo_mi.material_override = _secondary_orb_mat
		_orb_cluster_root.add_child(halo_mi)

		# Trailing companion micro-orb
		var trail_mi := MeshInstance3D.new()
		var trail_sph := SphereMesh.new()
		trail_sph.radius = 0.045
		trail_sph.height = 0.09
		trail_mi.mesh = trail_sph
		trail_mi.position = Vector3(0.32, 0.14, -0.22)
		trail_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		trail_mi.material_override = _primary_orb_mat
		_orb_cluster_root.add_child(trail_mi)

		_label_3d = Label3D.new()
		_label_3d.position = Vector3(0.0, 0.34, 0.0)
		_label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		_label_3d.font_size = 18
		_label_3d.outline_size = 5
		_label_3d.text = "GHOST ORB [NIGHT-VISION ONLY — %s]" % store_id
		_label_3d.modulate = Color(0.90, 0.94, 1.0)
		_orb_cluster_root.add_child(_label_3d)

		# Invisible to the naked human eye by default!
		visible = Engine.is_editor_hint()

	func set_ir_revealed(revealed: bool) -> void:
		ir_revealed = revealed
		if revealed:
			_reveal_grace_timer = 1.6
			visible = true
			if _label_3d:
				_label_3d.text = "GHOST ORB [IR NIGHT-VISION FEED — %s]" % store_id
		else:
			_reveal_grace_timer = 0.0
			visible = Engine.is_editor_hint()

	func _process(delta: float) -> void:
		_phase_time += delta * 0.95
		# Smooth figure-8 / 3D Lissajous curve inside the haunted boutique
		var offset := Vector3(
			sin(_phase_time) * 1.35,
			sin(_phase_time * 2.0) * 0.38 + cos(_phase_time * 0.7) * 0.12,
			sin(_phase_time * 2.0 + PI * 0.25) * 0.85
		)
		position = base_position + offset
		if _orb_cluster_root:
			_orb_cluster_root.rotation.y = _phase_time * 0.8

		if Engine.is_editor_hint():
			return

		if _reveal_grace_timer > 0.0:
			_reveal_grace_timer = maxf(0.0, _reveal_grace_timer - delta)
			var pulse := 0.65 + 0.35 * sin(_phase_time * 6.0)
			if _primary_orb_mat:
				_primary_orb_mat.albedo_color.a = 0.70 * pulse
			if _orb_light:
				_orb_light.light_energy = 1.15 * pulse
			if _reveal_grace_timer <= 0.0:
				ir_revealed = false
				visible = false


# ------------------------------------------------------------------------------
# Static Screen Shader & Camcorder Visual Builders (Shared by Handheld & Deployed)
# ------------------------------------------------------------------------------
static var _cached_screen_shader: Shader = null

static func get_camera_screen_shader() -> Shader:
	if _cached_screen_shader != null:
		return _cached_screen_shader
	var sh := Shader.new()
	sh.code = """
shader_type spatial;
render_mode unshaded, cull_disabled;

uniform sampler2D feed_tex : source_color, filter_linear;
uniform bool camera_powered_on = true;
uniform bool night_vision_enabled = true;

void fragment() {
	if (!camera_powered_on) {
		ALBEDO = vec3(0.025, 0.028, 0.032);
	} else {
		vec3 col = texture(feed_tex, UV).rgb;
		if (night_vision_enabled) {
			float lum = dot(col, vec3(0.299, 0.587, 0.114));
			// Pure Black & White with enhanced low-light amplification (zero world light emitted!)
			float boosted = clamp(pow(max(lum, 0.0004), 0.36) * 2.45 + lum * 1.45 + 0.06, 0.0, 1.0);
			float scanline = 0.96 + 0.04 * sin(UV.y * 240.0 * 3.14159);
			ALBEDO = vec3(boosted * scanline);
		} else {
			ALBEDO = col;
		}
	}
}
"""
	_cached_screen_shader = sh
	return sh

static func create_camera_screen_shader_material(tex: Texture2D = null, powered_on: bool = true, nv_on: bool = true) -> ShaderMaterial:
	var sm := ShaderMaterial.new()
	sm.shader = get_camera_screen_shader()
	if tex != null:
		sm.set_shader_parameter("feed_tex", tex)
	sm.set_shader_parameter("camera_powered_on", powered_on)
	sm.set_shader_parameter("night_vision_enabled", nv_on)
	return sm

static func update_camera_screen_shader_material(sm: ShaderMaterial, tex: Texture2D, powered_on: bool, nv_on: bool) -> void:
	if sm == null:
		return
	if tex != null:
		sm.set_shader_parameter("feed_tex", tex)
	sm.set_shader_parameter("camera_powered_on", powered_on)
	sm.set_shader_parameter("night_vision_enabled", nv_on)

## Builds the shared Camcorder Body + Lens Barrel + Flip-Out Side LCD Screen (`-X` side)
## used both when the Video Camera is deployed (`PlacedVideoCameraActor`) and when held in hand (`EquipmentIndicatorRig`).
static func build_camcorder_mesh_hierarchy(head_root: Node3D, initial_powered_on: bool = true, initial_nv_on: bool = true) -> Dictionary:
	var body_mat := StandardMaterial3D.new()
	body_mat.albedo_color = Color(0.15, 0.17, 0.20)
	body_mat.metallic = 0.45
	body_mat.roughness = 0.42

	# Camcorder main body
	var body_mi := MeshInstance3D.new()
	var body_box := BoxMesh.new()
	body_box.size = Vector3(0.13, 0.14, 0.26)
	body_mi.mesh = body_box
	body_mi.material_override = body_mat
	body_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	head_root.add_child(body_mi)

	# Top carry handle arch
	var grip_mi := MeshInstance3D.new()
	var grip_box := BoxMesh.new()
	grip_box.size = Vector3(0.035, 0.032, 0.16)
	grip_mi.mesh = grip_box
	grip_mi.position = Vector3(0.0, 0.086, -0.01)
	grip_mi.material_override = body_mat
	grip_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	head_root.add_child(grip_mi)

	# Forward lens barrel (-Z forward)
	var lens_mi := MeshInstance3D.new()
	var lens_cyl := CylinderMesh.new()
	lens_cyl.top_radius = 0.048
	lens_cyl.bottom_radius = 0.052
	lens_cyl.height = 0.09
	lens_mi.mesh = lens_cyl
	lens_mi.position = Vector3(0.0, 0.01, -0.16)
	lens_mi.rotation_degrees.x = 90.0
	lens_mi.material_override = body_mat
	lens_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	head_root.add_child(lens_mi)

	# Non-illuminating lens bezel ring (emits zero world light!)
	var ring_mi := MeshInstance3D.new()
	var ring_torus := TorusMesh.new()
	ring_torus.inner_radius = 0.036
	ring_torus.outer_radius = 0.050
	ring_torus.rings = 20
	ring_torus.ring_segments = 10
	ring_mi.mesh = ring_torus
	ring_mi.position = Vector3(0.0, 0.01, -0.205)
	ring_mi.rotation_degrees.x = 90.0
	var ring_mat := StandardMaterial3D.new()
	ring_mat.albedo_color = Color(0.28, 0.30, 0.34)
	ring_mat.metallic = 0.65
	ring_mat.roughness = 0.35
	ring_mi.material_override = ring_mat
	ring_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	head_root.add_child(ring_mi)

	# Flip-Out Side LCD Screen (protruding on -X left side of camcorder, angled toward the operator)
	var lcd_frame := MeshInstance3D.new()
	var lcd_box := BoxMesh.new()
	lcd_box.size = Vector3(0.15, 0.10, 0.016)
	lcd_frame.mesh = lcd_box
	lcd_frame.position = Vector3(-0.14, 0.005, 0.02)
	lcd_frame.rotation_degrees.y = 14.0
	lcd_frame.material_override = body_mat
	lcd_frame.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	head_root.add_child(lcd_frame)

	var lcd_screen := MeshInstance3D.new()
	var lcd_quad := QuadMesh.new()
	lcd_quad.size = Vector2(0.136, 0.088)
	lcd_screen.mesh = lcd_quad
	lcd_screen.position = Vector3(0.0, 0.0, 0.010)
	var lcd_shader_mat := create_camera_screen_shader_material(null, initial_powered_on, initial_nv_on)
	lcd_screen.material_override = lcd_shader_mat
	lcd_screen.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	lcd_frame.add_child(lcd_screen)

	# Also place a matching screen quad on the -Z face of the flip-out LCD frame so it is visible from either side
	var lcd_screen_back := MeshInstance3D.new()
	lcd_screen_back.mesh = lcd_quad
	lcd_screen_back.position = Vector3(0.0, 0.0, -0.010)
	lcd_screen_back.rotation_degrees = Vector3(0.0, 180.0, 0.0)
	lcd_screen_back.material_override = lcd_shader_mat
	lcd_screen_back.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	lcd_frame.add_child(lcd_screen_back)

	return {
		"lcd_frame": lcd_frame,
		"lcd_screen": lcd_screen,
		"lcd_shader_mat": lcd_shader_mat,
		"ring_mat": ring_mat,
	}

## Builds the Portable Widescreen CCTV Camera Monitor Tablet (`"cctv_monitor"`, Hands-Only Item)
## with rugged side grips, widescreen 16:9 screen quad, and channel-cycle button bezel.
static func build_portable_cctv_monitor_mesh(target_root: Node3D, initial_powered_on: bool = true) -> Dictionary:
	var casing_mat := StandardMaterial3D.new()
	casing_mat.albedo_color = Color(0.16, 0.18, 0.22)
	casing_mat.metallic = 0.45
	casing_mat.roughness = 0.42

	var grip_mat := StandardMaterial3D.new()
	grip_mat.albedo_color = Color(0.10, 0.11, 0.13)
	grip_mat.roughness = 0.75

	# Rugged widescreen field monitor casing
	var casing_mi := MeshInstance3D.new()
	var casing_box := BoxMesh.new()
	casing_box.size = Vector3(0.34, 0.21, 0.036)
	casing_mi.mesh = casing_box
	casing_mi.material_override = casing_mat
	casing_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	target_root.add_child(casing_mi)

	# Left & Right two-handed rubberized side grips
	for sx in [-1.0, 1.0]:
		var grip_mi := MeshInstance3D.new()
		var grip_box := BoxMesh.new()
		grip_box.size = Vector3(0.032, 0.19, 0.048)
		grip_mi.mesh = grip_box
		grip_mi.position = Vector3(sx * 0.175, 0.0, 0.004)
		grip_mi.material_override = grip_mat
		grip_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		target_root.add_child(grip_mi)

	# Top receiver whip antenna on the field monitor
	var ant_mi := MeshInstance3D.new()
	var ant_cyl := CylinderMesh.new()
	ant_cyl.top_radius = 0.004
	ant_cyl.bottom_radius = 0.006
	ant_cyl.height = 0.11
	ant_mi.mesh = ant_cyl
	ant_mi.position = Vector3(0.13, 0.155, -0.005)
	ant_mi.material_override = casing_mat
	ant_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	target_root.add_child(ant_mi)

	# Widescreen 16:9 Live Screen Quad on the +Z front face of the portable monitor
	var screen_mi := MeshInstance3D.new()
	var screen_quad := QuadMesh.new()
	screen_quad.size = Vector2(0.295, 0.166)
	screen_mi.mesh = screen_quad
	screen_mi.position = Vector3(0.0, 0.008, 0.020)
	var screen_shader_mat := create_camera_screen_shader_material(null, initial_powered_on, true)
	screen_mi.material_override = screen_shader_mat
	screen_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	target_root.add_child(screen_mi)

	return {
		"casing_mi": casing_mi,
		"screen_mi": screen_mi,
		"screen_shader_mat": screen_shader_mat,
	}

static func get_placed_cameras_sorted(tree: SceneTree) -> Array[PlacedVideoCameraActor]:
	var list: Array[PlacedVideoCameraActor] = []
	if tree == null:
		return list
	for node in tree.get_nodes_in_group("placed_video_cameras"):
		var cam := node as PlacedVideoCameraActor
		if cam != null:
			list.append(cam)
	list.sort_custom(func(a: PlacedVideoCameraActor, b: PlacedVideoCameraActor) -> bool:
		return a.camera_index < b.camera_index
	)
	return list


# ------------------------------------------------------------------------------
# 2. Placed Video Camera Actor (Standalone Camera at Y+0.25m OR 1.45m Tripod Rig)
#    Emits ZERO light in the world! Uses the flip-out side LCD screen to display
#    live Black & White enhanced-light Night-Vision (or Standard Color) when ON.
# ------------------------------------------------------------------------------
class PlacedVideoCameraActor extends Node3D:
	var camera_index: int = 1
	var is_on_tripod_stand: bool = false
	var is_powered_on: bool = true
	var ir_night_vision_on: bool = true
	var store_id: String = "East_F2_01"
	var pan_angle_deg: float = 0.0
	var tilt_angle_deg: float = 0.0
	var item_data: Dictionary = {}

	var tripod_root: Node3D = null
	var head_pivot: Node3D = null
	var lcd_shader_mat: ShaderMaterial = null
	var ir_ring_mat: StandardMaterial3D = null
	var cam_viewport: SubViewport = null
	var cam_lens_camera: Camera3D = null
	var cam_lens_attrs: CameraAttributesPractical = null
	var status_label: Label3D = null
	var _scan_accum: float = 0.0
	var _last_scan_result: Dictionary = {}

	func _ready() -> void:
		add_to_group("placed_video_cameras")
		if not item_data.is_empty():
			var item_id: String = str(item_data.get("id", "video_camera"))
			if item_id == "assembled_video_rig":
				is_on_tripod_stand = true
			if item_data.has("powered_on"):
				is_powered_on = bool(item_data.get("powered_on", true))
			if item_data.has("ir_night_vision_on"):
				ir_night_vision_on = bool(item_data.get("ir_night_vision_on", true))
			if item_data.has("store_id") and not str(item_data.get("store_id", "")).is_empty():
				store_id = str(item_data.get("store_id"))

		_assign_unique_camera_index()
		_infer_store_id_from_position()
		_build_camera_and_tripod_visuals()
		_update_camera_orientation_and_label()

	func _assign_unique_camera_index() -> void:
		if not is_inside_tree():
			return
		var used_indices: Array[int] = []
		for node in get_tree().get_nodes_in_group("placed_video_cameras"):
			if node != self and node is PlacedVideoCameraActor:
				used_indices.append((node as PlacedVideoCameraActor).camera_index)
		for idx in range(1, 5):
			if idx not in used_indices:
				camera_index = idx
				return
		camera_index = clampi(camera_index, 1, 4)

	func _infer_store_id_from_position() -> void:
		if not is_inside_tree() or Engine.is_editor_hint():
			return
		if "ghost_profiles" in GameState:
			for sid in GameState.ghost_profiles.keys():
				var prof: Dictionary = GameState.ghost_profiles[sid]
				if prof.has("territory_aabb"):
					var aabb: AABB = prof["territory_aabb"]
					if aabb.grow(3.5).has_point(global_position):
						store_id = str(sid)
						return
		var nearest_d := 22.0
		for g in get_tree().get_nodes_in_group("store_ghosts"):
			var sg := g as StoreGhost
			if sg:
				var d := global_position.distance_to(sg.global_position)
				if d < nearest_d:
					nearest_d = d
					store_id = sg.store_id

	func _build_camera_and_tripod_visuals() -> void:
		for c in get_children():
			c.queue_free()

		# Dedicated SubViewport for the flip-out LCD screen on the side of the camera (Zero World Light!)
		cam_viewport = SubViewport.new()
		cam_viewport.name = "SideLCDViewport"
		cam_viewport.size = Vector2i(240, 150)
		cam_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS if is_powered_on else SubViewport.UPDATE_DISABLED
		cam_viewport.handle_input_locally = false
		add_child(cam_viewport)

		cam_lens_camera = Camera3D.new()
		cam_lens_camera.fov = 76.0
		cam_lens_camera.near = 0.08
		cam_lens_camera.far = 90.0
		cam_lens_camera.current = true
		cam_lens_attrs = CameraAttributesPractical.new()
		cam_lens_attrs.exposure_multiplier = 4.8 if ir_night_vision_on else 1.15
		cam_lens_camera.attributes = cam_lens_attrs
		cam_viewport.add_child(cam_lens_camera)

		var mount_height := 1.45 if is_on_tripod_stand else 0.25

		if is_on_tripod_stand:
			tripod_root = Node3D.new()
			add_child(tripod_root)

			var leg_mat := StandardMaterial3D.new()
			leg_mat.albedo_color = Color(0.16, 0.18, 0.22)
			leg_mat.metallic = 0.78
			leg_mat.roughness = 0.32

			# Central telescoping column up to 1.45m
			var col_mi := MeshInstance3D.new()
			var col_cyl := CylinderMesh.new()
			col_cyl.top_radius = 0.022
			col_cyl.bottom_radius = 0.028
			col_cyl.height = 0.65
			col_mi.mesh = col_cyl
			col_mi.position = Vector3(0.0, 1.12, 0.0)
			col_mi.material_override = leg_mat
			tripod_root.add_child(col_mi)

			# Collar hub at Y = 0.85m
			var hub_mi := MeshInstance3D.new()
			var hub_cyl := CylinderMesh.new()
			hub_cyl.top_radius = 0.055
			hub_cyl.bottom_radius = 0.055
			hub_cyl.height = 0.08
			hub_mi.mesh = hub_cyl
			hub_mi.position = Vector3(0.0, 0.85, 0.0)
			hub_mi.material_override = leg_mat
			tripod_root.add_child(hub_mi)

			# 3 collapsible tripod legs splaying outward from Y=0.85m to the floor
			for i in range(3):
				var angle := TAU * float(i) / 3.0
				var leg_pivot := Node3D.new()
				leg_pivot.position = Vector3(0.0, 0.85, 0.0)
				leg_pivot.rotation.y = angle
				tripod_root.add_child(leg_pivot)

				var leg_mi := MeshInstance3D.new()
				var leg_cyl := CylinderMesh.new()
				leg_cyl.top_radius = 0.014
				leg_cyl.bottom_radius = 0.011
				leg_cyl.height = 0.94
				leg_mi.mesh = leg_cyl
				leg_mi.position = Vector3(0.0, -0.42, 0.18)
				leg_mi.rotation_degrees.x = -22.5
				leg_mi.material_override = leg_mat
				leg_pivot.add_child(leg_mi)

			# Motorized Pan/Tilt Servo Base at Y = 1.41m
			var servo_mi := MeshInstance3D.new()
			var servo_cyl := CylinderMesh.new()
			servo_cyl.top_radius = 0.048
			servo_cyl.bottom_radius = 0.052
			servo_cyl.height = 0.07
			servo_mi.mesh = servo_cyl
			servo_mi.position = Vector3(0.0, 1.41, 0.0)
			var servo_mat := StandardMaterial3D.new()
			servo_mat.albedo_color = Color(0.24, 0.27, 0.32)
			servo_mat.metallic = 0.65
			servo_mat.roughness = 0.38
			servo_mi.material_override = servo_mat
			tripod_root.add_child(servo_mi)
		else:
			# Low floor/shelf rubberized mini-pad at Y = 0.0..0.22m
			var base_mi := MeshInstance3D.new()
			var base_cyl := CylinderMesh.new()
			base_cyl.top_radius = 0.08
			base_cyl.bottom_radius = 0.11
			base_cyl.height = 0.18
			base_mi.mesh = base_cyl
			base_mi.position = Vector3(0.0, 0.09, 0.0)
			var base_mat := StandardMaterial3D.new()
			base_mat.albedo_color = Color(0.18, 0.20, 0.24)
			base_mat.roughness = 0.55
			base_mi.material_override = base_mat
			add_child(base_mi)

		head_pivot = Node3D.new()
		head_pivot.position = Vector3(0.0, mount_height, 0.0)
		add_child(head_pivot)

		var parts := VideoAndAudioSurveillance.build_camcorder_mesh_hierarchy(head_pivot, is_powered_on, ir_night_vision_on)
		lcd_shader_mat = parts.get("lcd_shader_mat") as ShaderMaterial
		ir_ring_mat = parts.get("ring_mat") as StandardMaterial3D
		if lcd_shader_mat and cam_viewport:
			VideoAndAudioSurveillance.update_camera_screen_shader_material(
				lcd_shader_mat, cam_viewport.get_texture(), is_powered_on, ir_night_vision_on
			)

		status_label = Label3D.new()
		status_label.position = Vector3(0.0, mount_height + 0.34, 0.0)
		status_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		status_label.font_size = 19
		status_label.outline_size = 6
		status_label.modulate = Color(0.88, 0.92, 0.96)
		add_child(status_label)

	func _sync_side_lcd_camera() -> void:
		if not is_inside_tree() or cam_viewport == null or cam_lens_camera == null or head_pivot == null:
			return
		var root_vp := get_viewport()
		if root_vp != null and root_vp.world_3d != null and cam_viewport.world_3d != root_vp.world_3d:
			cam_viewport.world_3d = root_vp.world_3d
		cam_lens_camera.global_position = head_pivot.to_global(Vector3(0.0, 0.02, -0.26))
		cam_lens_camera.global_basis = head_pivot.global_transform.basis

	func _update_camera_orientation_and_label() -> void:
		if head_pivot:
			head_pivot.rotation_degrees = Vector3(tilt_angle_deg, pan_angle_deg, 0.0)
		if cam_lens_attrs:
			cam_lens_attrs.exposure_multiplier = 4.8 if (is_powered_on and ir_night_vision_on) else 1.15
		if cam_viewport:
			cam_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS if is_powered_on else SubViewport.UPDATE_DISABLED
		if lcd_shader_mat and cam_viewport:
			VideoAndAudioSurveillance.update_camera_screen_shader_material(
				lcd_shader_mat, cam_viewport.get_texture(), is_powered_on, ir_night_vision_on
			)
		_sync_side_lcd_camera()
		if status_label:
			var rig_tag := "145cm Tripod Rig" if is_on_tripod_stand else "Floor/Shelf Cam"
			var state_tag := "OFF" if not is_powered_on else ("NIGHT-VISION (B&W ENHANCED)" if ir_night_vision_on else "STANDARD COLOR")
			var orb_tag := " | ORBS DETECTED!" if _last_scan_result.get("orbs_detected", false) else ""
			var shy_tag := " | SHY ENTITY VISIBLE!" if _last_scan_result.get("shy_ghost_revealed", false) else ""
			status_label.text = (
				"CAM %02d [%s — %s | Pan: %.0f°]%s%s\n" % [camera_index, rig_tag, state_tag, pan_angle_deg, orb_tag, shy_tag] +
				"[LMB] Power ON/OFF | [RMB] Night-Vision | [E] Rotate +35° | [F] Pick Up"
			)

	func toggle_power() -> bool:
		is_powered_on = not is_powered_on
		if not item_data.is_empty():
			item_data["powered_on"] = is_powered_on
		_update_camera_orientation_and_label()
		scan_feed_status()
		GameState.status_message.emit(
			"CAM %02d [%s] Power [LMB]: %s!" % [
				camera_index,
				store_id,
				"ON (Side LCD Screen & Cart CCTV Feed Active)" if is_powered_on else "OFF (Screen Dark)"
			],
			Color(0.88, 0.94, 1.0) if is_powered_on else Color(0.65, 0.70, 0.78)
		)
		return is_powered_on

	func toggle_night_vision() -> bool:
		ir_night_vision_on = not ir_night_vision_on
		if not item_data.is_empty():
			item_data["ir_night_vision_on"] = ir_night_vision_on
		_update_camera_orientation_and_label()
		scan_feed_status()
		GameState.status_message.emit(
			"CAM %02d [%s] Night-Vision [RMB]: %s!" % [
				camera_index,
				store_id,
				"ON (Black & White Enhanced Light on Side Screen & Cart CCTV)" if ir_night_vision_on else "OFF (Standard Color Video Feed)"
			],
			Color(0.90, 0.94, 1.0) if ir_night_vision_on else Color(0.80, 0.86, 0.94)
		)
		return ir_night_vision_on

	func pan_camera(delta_deg: float = 35.0) -> void:
		pan_angle_deg = wrapf(pan_angle_deg + delta_deg, -180.0, 180.0)
		_update_camera_orientation_and_label()
		GameState.status_message.emit(
			"CAM %02d [%s] Rotated on Camera to %.0f° (%s)!" % [
				camera_index,
				store_id,
				pan_angle_deg,
				"145cm Tripod Stand" if is_on_tripod_stand else "Manual Swivel"
			],
			Color(0.88, 0.94, 1.0)
		)

	func attach_tripod_stand() -> bool:
		if is_on_tripod_stand:
			return false
		is_on_tripod_stand = true
		item_data = {
			"id": "assembled_video_rig",
			"name": "Assembled Video Camera + 145cm Tripod Stand Rig [Hands-Only / Cart Hook]",
			"price": 110,
			"can_store_in_hotbar": false,
			"can_hang_on_cart": true,
			"can_power_toggle": true,
			"powered_on": is_powered_on,
			"ir_night_vision_on": ir_night_vision_on,
			"makes_noise_when_on": false,
			"model_path": "",
			"indicator_slot": 9,
		}
		_build_camera_and_tripod_visuals()
		_update_camera_orientation_and_label()
		GameState.status_message.emit(
			"Snapped Video Camera onto 145cm Collapsible Tripod Stand (CAM %02d)! Press [E] on the camera to rotate its angle!" % camera_index,
			Color(0.88, 0.94, 1.0)
		)
		return true

	func to_pickup_item_data() -> Dictionary:
		if is_on_tripod_stand:
			return {
				"id": "assembled_video_rig",
				"name": "Assembled Video Camera + 145cm Tripod Stand Rig [Hands-Only / Cart Hook]",
				"price": 110,
				"can_store_in_hotbar": false,
				"can_hang_on_cart": true,
				"can_power_toggle": true,
				"powered_on": is_powered_on,
				"ir_night_vision_on": ir_night_vision_on,
				"makes_noise_when_on": false,
				"model_path": "",
				"indicator_slot": 9,
			}
		return {
			"id": "video_camera",
			"name": "Night-Vision Video Camera",
			"price": 65,
			"can_store_in_hotbar": true,
			"can_hang_on_cart": true,
			"can_power_toggle": true,
			"powered_on": is_powered_on,
			"ir_night_vision_on": ir_night_vision_on,
			"makes_noise_when_on": false,
			"model_path": "",
			"indicator_slot": 9,
		}

	func scan_feed_status() -> Dictionary:
		var cam_label := "CAM %02d" % camera_index
		if not is_inside_tree() or not is_powered_on:
			return {
				"cam_label": cam_label,
				"store_id": store_id,
				"orbs_detected": false,
				"shy_ghost_revealed": false,
				"ghost_name": "OFFLINE",
				"players_in_store": 0,
			}

		var cam_pos := head_pivot.global_position if head_pivot else (global_position + Vector3(0.0, 1.45 if is_on_tripod_stand else 0.25, 0.0))
		var cam_fwd := (-head_pivot.global_transform.basis.z).normalized() if head_pivot else (-global_transform.basis.z).normalized()

		var orbs_detected := false
		for node in get_tree().get_nodes_in_group("ghost_orbs"):
			var orb := node as GhostOrbActor
			if orb == null:
				continue
			var to_orb := orb.global_position - cam_pos
			var dist := to_orb.length()
			if dist <= 14.0:
				var angle_deg := rad_to_deg(cam_fwd.angle_to(to_orb.normalized())) if dist > 0.05 else 0.0
				if angle_deg <= 62.0 or orb.store_id == store_id:
					orbs_detected = true
					if ir_night_vision_on:
						orb.set_ir_revealed(true)
					if orb.store_id != "":
						store_id = orb.store_id

		var target_ghost: StoreGhost = null
		var best_ghost_d := 14.0
		for g_node in get_tree().get_nodes_in_group("store_ghosts"):
			var sg := g_node as StoreGhost
			if sg == null or sg.is_resolved:
				continue
			var d := cam_pos.distance_to(sg.global_position)
			var in_same_store := (sg.store_id == store_id) or sg.store_territory_aabb.grow(2.5).has_point(cam_pos)
			if d <= 14.0 or in_same_store:
				var to_g := sg.global_position - cam_pos
				var angle_g := rad_to_deg(cam_fwd.angle_to(to_g.normalized())) if to_g.length() > 0.05 else 0.0
				if angle_g <= 68.0 or in_same_store:
					if d < best_ghost_d or target_ghost == null:
						best_ghost_d = d
						target_ghost = sg

		var players_in_store := 0
		var shy_ghost_revealed := false
		var ghost_name_str := "No Entity in Frame"

		if target_ghost != null:
			store_id = target_ghost.store_id
			ghost_name_str = target_ghost.ghost_name
			var territory := target_ghost.store_territory_aabb.grow(0.15)
			for p_node in get_tree().get_nodes_in_group("players"):
				var p3d := p_node as Node3D
				if p3d == null or p3d.get("is_floating_essence") == true:
					continue
				if territory.has_point(p3d.global_position):
					players_in_store += 1

			# Camera-Shy Ghost rule: only manifests on the Cart Monitor IR feed when 0 living players are inside the store!
			if players_in_store == 0 and ir_night_vision_on:
				shy_ghost_revealed = true
				if target_ghost.visual_root:
					target_ghost.visual_root.visible = true

		_last_scan_result = {
			"cam_label": cam_label,
			"store_id": store_id,
			"orbs_detected": orbs_detected,
			"shy_ghost_revealed": shy_ghost_revealed,
			"ghost_name": ghost_name_str,
			"players_in_store": players_in_store,
		}
		return _last_scan_result

	func _physics_process(delta: float) -> void:
		if Engine.is_editor_hint():
			return
		if is_powered_on:
			_sync_side_lcd_camera()
		_scan_accum += delta
		if _scan_accum >= 0.35:
			_scan_accum = 0.0
			scan_feed_status()
			_update_camera_orientation_and_label()


# ------------------------------------------------------------------------------
# 3. Front-Mounted / Detachable Cart Computer Monitor (`UMallCartMonitorComponent` on PushCart)
#    Starts mounted at the front of the cart (`Vector3(0.42, 1.12, 0.0)`) with a FIXED
#    orientation facing the front of the cart (`+X`), away from the rear handle (`-X`).
#    • Press `[F]` while looking at the mounted monitor to PICK IT UP into the Player's HANDS slot!
#    • Press `[G]` while holding the monitor in Hands near the Push Cart to MOUNT IT BACK onto the cart!
#    • Press `[LMB]` for Power ON/OFF and `[RMB]` to cycle camera locations (`CAM 01 -> CAM 02...`)!
#    • NEVER intercepts `[E]` so `[E]` near the cart 100% reliably grabs/pushes or releases the cart!
# ------------------------------------------------------------------------------
class CartComputerMonitor extends Node3D:
	var parent_cart: Node3D = null
	var is_mounted_on_cart: bool = true
	var is_monitor_powered_on: bool = true
	var active_cam_idx: int = 0
	var dashcam_active: bool = false
	var snapshot_cooldown: float = 0.0

	var monitor_head_pivot: Node3D = null
	var empty_mount_label: Label3D = null
	var crt_casing_mi: MeshInstance3D = null
	var crt_screen_mi: MeshInstance3D = null
	var crt_screen_shader_mat: ShaderMaterial = null
	var cctv_viewport: SubViewport = null
	var cctv_camera: Camera3D = null
	var cctv_cam_attrs: CameraAttributesPractical = null
	var screen_label: Label3D = null
	var sub_label: Label3D = null
	var _logged_surveillance_stores: Dictionary = {}
	var _refresh_timer: float = 0.0
	var _feed_source_label: String = "STANDBY"
	var _feed_powered_on: bool = true
	var _feed_nv_on: bool = true

	func _ready() -> void:
		add_to_group("cart_computer_monitors")
		if position == Vector3.ZERO:
			position = Vector3(0.42, 1.12, 0.0)
		_build_crt_monitor_visuals()
		_sync_cctv_camera_transform()
		_refresh_monitor_feed()

	func _build_crt_monitor_visuals() -> void:
		for c in get_children():
			c.queue_free()

		# 1. Real-Time Widescreen 16:9 3D CCTV SubViewport + Camera3D (Emits ZERO world light!)
		cctv_viewport = SubViewport.new()
		cctv_viewport.name = "CCTVSubViewport"
		cctv_viewport.size = Vector2i(512, 288)
		cctv_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS if is_mounted_on_cart else SubViewport.UPDATE_DISABLED
		cctv_viewport.handle_input_locally = false
		add_child(cctv_viewport)

		cctv_camera = Camera3D.new()
		cctv_camera.name = "CCTVCamera3D"
		cctv_camera.fov = 78.0
		cctv_camera.near = 0.08
		cctv_camera.far = 120.0
		cctv_camera.current = true
		cctv_cam_attrs = CameraAttributesPractical.new()
		cctv_cam_attrs.exposure_multiplier = 4.8
		cctv_camera.attributes = cctv_cam_attrs
		cctv_viewport.add_child(cctv_camera)

		# 2. Mounting riser bracket at the front of the Push Cart's top shelf (stays on cart when monitor is unmounted)
		var stand_mi := MeshInstance3D.new()
		var stand_box := BoxMesh.new()
		stand_box.size = Vector3(0.14, 0.16, 0.16)
		stand_mi.mesh = stand_box
		stand_mi.position = Vector3(0.0, -0.20, 0.0)
		var casing_mat := StandardMaterial3D.new()
		casing_mat.albedo_color = Color(0.18, 0.20, 0.24)
		casing_mat.metallic = 0.45
		casing_mat.roughness = 0.45
		stand_mi.material_override = casing_mat
		add_child(stand_mi)

		empty_mount_label = Label3D.new()
		empty_mount_label.position = Vector3(0.0, -0.02, 0.0)
		empty_mount_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		empty_mount_label.font_size = 16
		empty_mount_label.outline_size = 5
		empty_mount_label.modulate = Color(1.0, 0.86, 0.42)
		empty_mount_label.text = "EMPTY MONITOR MOUNT\n[G] Mount Held CCTV Monitor Here"
		empty_mount_label.visible = not is_mounted_on_cart
		add_child(empty_mount_label)

		# 3. Fixed Widescreen Monitor Head Pivot — strictly fixed facing the FRONT of the cart (+X)!
		#    (Never rotates toward the player, and sits at the front of the cart away from the rear handle!)
		monitor_head_pivot = Node3D.new()
		monitor_head_pivot.name = "MonitorHeadPivot"
		monitor_head_pivot.rotation_degrees = Vector3(0.0, 90.0, 0.0)
		monitor_head_pivot.visible = is_mounted_on_cart
		add_child(monitor_head_pivot)

		crt_casing_mi = MeshInstance3D.new()
		var casing_box := BoxMesh.new()
		casing_box.size = Vector3(0.76, 0.42, 0.12)
		crt_casing_mi.mesh = casing_box
		crt_casing_mi.material_override = casing_mat
		monitor_head_pivot.add_child(crt_casing_mi)

		# Physical channel-cycle button on the front bezel of the cart display
		var btn_mi := MeshInstance3D.new()
		var btn_box := BoxMesh.new()
		btn_box.size = Vector3(0.08, 0.032, 0.025)
		btn_mi.mesh = btn_box
		btn_mi.position = Vector3(0.28, -0.185, 0.065)
		var btn_mat := StandardMaterial3D.new()
		btn_mat.albedo_color = Color(0.32, 0.78, 0.95)
		btn_mi.material_override = btn_mat
		monitor_head_pivot.add_child(btn_mi)

		# 4. Wide 16:9 Fixed Front Screen Quad (0.70m wide x 0.36m tall facing the front of the cart +X)
		crt_screen_shader_mat = VideoAndAudioSurveillance.create_camera_screen_shader_material(
			cctv_viewport.get_texture(), true, true
		)

		crt_screen_mi = MeshInstance3D.new()
		var screen_quad := QuadMesh.new()
		screen_quad.size = Vector2(0.70, 0.36)
		crt_screen_mi.mesh = screen_quad
		crt_screen_mi.position = Vector3(0.0, 0.015, 0.063)
		crt_screen_mi.material_override = crt_screen_shader_mat
		crt_screen_mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		monitor_head_pivot.add_child(crt_screen_mi)

		# 5. Live CCTV Status Bar fixed along the bottom bezel of the front-facing display
		screen_label = Label3D.new()
		screen_label.position = Vector3(0.0, -0.25, 0.070)
		screen_label.font_size = 14
		screen_label.outline_size = 5
		screen_label.modulate = Color(0.88, 0.92, 0.96)
		screen_label.text = "CART CCTV MONITOR [FIXED FRONT]"
		monitor_head_pivot.add_child(screen_label)

		# Fixed header prompt on the front of the monitor (no billboard swivel)
		sub_label = Label3D.new()
		sub_label.position = Vector3(0.0, 0.25, 0.070)
		sub_label.font_size = 15
		sub_label.outline_size = 5
		sub_label.modulate = Color(0.82, 0.90, 0.98)
		sub_label.text = "Cart CCTV Monitor [F: Pick Up Into Hands | LMB: Power | RMB: Next Cam]"
		monitor_head_pivot.add_child(sub_label)

	## Unmounts the CCTV Monitor from the Push Cart's front mount and returns the Hands-Only `"cctv_monitor"` Dictionary!
	func unmount_to_hand_item() -> Dictionary:
		if not is_mounted_on_cart:
			return {}
		is_mounted_on_cart = false
		if monitor_head_pivot != null:
			monitor_head_pivot.visible = false
		if empty_mount_label != null:
			empty_mount_label.visible = true
		if cctv_viewport != null:
			cctv_viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
		var data := ExpandedMallGadgets.get_expanded_item_catalog_entry("cctv_monitor")
		data["powered_on"] = is_monitor_powered_on
		data["active_cam_idx"] = active_cam_idx
		return data

	## Mounts a carried `"cctv_monitor"` item from the Player's Hands back onto the Push Cart's front mount!
	func mount_from_hand_item(item_data: Dictionary) -> bool:
		if is_mounted_on_cart:
			return false
		is_mounted_on_cart = true
		is_monitor_powered_on = bool(item_data.get("powered_on", true))
		active_cam_idx = int(item_data.get("active_cam_idx", active_cam_idx))
		if monitor_head_pivot != null:
			monitor_head_pivot.visible = true
		if empty_mount_label != null:
			empty_mount_label.visible = false
		if cctv_viewport != null:
			cctv_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		_sync_cctv_camera_transform()
		_refresh_monitor_feed()
		return true

	## Toggles power ON/OFF on the mounted Cart Monitor (`[LMB]` when aiming directly at the screen).
	func toggle_monitor_power() -> bool:
		if not is_mounted_on_cart:
			return false
		is_monitor_powered_on = not is_monitor_powered_on
		_sync_cctv_camera_transform()
		_refresh_monitor_feed()
		if is_monitor_powered_on and snapshot_cooldown <= 0.0:
			capture_video_snapshot()
		GameState.status_message.emit(
			"Cart CCTV Monitor Power [LMB]: %s! (Press [RMB] to cycle cameras, or [F] to pick up monitor into Hands)" % ("ON" if is_monitor_powered_on else "OFF"),
			Color(0.88, 0.94, 1.0) if is_monitor_powered_on else Color(0.75, 0.80, 0.88)
		)
		return is_monitor_powered_on

	func _get_placed_cameras() -> Array[PlacedVideoCameraActor]:
		var list: Array[PlacedVideoCameraActor] = []
		if not is_inside_tree():
			return list
		for node in get_tree().get_nodes_in_group("placed_video_cameras"):
			var cam := node as PlacedVideoCameraActor
			if cam != null:
				list.append(cam)
		list.sort_custom(func(a: PlacedVideoCameraActor, b: PlacedVideoCameraActor) -> bool:
			return a.camera_index < b.camera_index
		)
		return list

	func _check_dashcam_hook_state() -> bool:
		if parent_cart == null:
			parent_cart = get_parent() as Node3D
		if parent_cart == null:
			return false
		var hooks_val: Variant = parent_cart.get("exterior_hook_items")
		if hooks_val is Array:
			for item in (hooks_val as Array):
				if item is Dictionary:
					var id_str: String = str(item.get("id", ""))
					if (id_str == "video_camera" or id_str == "assembled_video_rig") and bool(item.get("powered_on", true)):
						return true
		var slots_val: Variant = parent_cart.get("cart_slot_items")
		if slots_val is Array:
			for item in (slots_val as Array):
				if item is Dictionary:
					var id_str: String = str(item.get("id", ""))
					if (id_str == "video_camera" or id_str == "assembled_video_rig") and bool(item.get("powered_on", true)):
						return true
		return false

	## Syncs the SubViewport's 3D Camera (`cctv_camera`) every frame to the active Video Camera location.
	## The monitor head remains strictly FIXED facing the front of the cart (+X).
	func _sync_cctv_camera_transform() -> void:
		if not is_inside_tree() or not is_mounted_on_cart or cctv_viewport == null or cctv_camera == null:
			return
		var root_vp := get_viewport()
		if root_vp != null and root_vp.world_3d != null and cctv_viewport.world_3d != root_vp.world_3d:
			cctv_viewport.world_3d = root_vp.world_3d

		# Keep monitor strictly fixed facing the front of the cart (+X)
		if monitor_head_pivot != null:
			monitor_head_pivot.rotation_degrees = Vector3(0.0, 90.0, 0.0)

		if not is_monitor_powered_on:
			_feed_powered_on = false
			_feed_nv_on = false
			VideoAndAudioSurveillance.update_camera_screen_shader_material(
				crt_screen_shader_mat, cctv_viewport.get_texture(), false, false
			)
			_feed_source_label = "MONITOR OFF [LMB: Turn ON]"
			return

		# Priority 1: Placed Video Cameras (`PlacedVideoCameraActor` in store/hallway)
		var cams := _get_placed_cameras()
		if not cams.is_empty():
			active_cam_idx = clampi(active_cam_idx, 0, cams.size() - 1)
			var cam := cams[active_cam_idx]
			if cam != null and cam.head_pivot != null:
				cctv_camera.global_position = cam.head_pivot.to_global(Vector3(0.0, 0.02, -0.26))
				cctv_camera.global_basis = cam.head_pivot.global_transform.basis
				_feed_powered_on = cam.is_powered_on
				_feed_nv_on = cam.ir_night_vision_on
				if cctv_cam_attrs:
					cctv_cam_attrs.exposure_multiplier = 4.8 if (_feed_powered_on and _feed_nv_on) else 1.15
				VideoAndAudioSurveillance.update_camera_screen_shader_material(
					crt_screen_shader_mat, cctv_viewport.get_texture(), _feed_powered_on, _feed_nv_on
				)
				_feed_source_label = "CAM %02d [%s]" % [cam.camera_index, cam.store_id]
				return

		# Priority 2: Push Cart Dashcam (Video Camera hung on Cart Hook or placed on Cart Shelf)
		dashcam_active = _check_dashcam_hook_state()
		if dashcam_active and parent_cart != null:
			cctv_camera.global_position = parent_cart.to_global(Vector3(0.62, 0.96, 0.0))
			cctv_camera.global_basis = parent_cart.global_transform.basis.rotated(Vector3.UP, -PI * 0.5)
			_feed_powered_on = true
			_feed_nv_on = true
			if cctv_cam_attrs:
				cctv_cam_attrs.exposure_multiplier = 4.8
			VideoAndAudioSurveillance.update_camera_screen_shader_material(
				crt_screen_shader_mat, cctv_viewport.get_texture(), true, true
			)
			_feed_source_label = "CART FRONT DASHCAM"
			return

		# Priority 3: Handheld / Hotbar Video Camera carried by any Player (only when powered ON)
		for p_node in get_tree().get_nodes_in_group("players"):
			var p3d := p_node as Node3D
			if p3d != null and p3d.has_method("is_carrying_item_id"):
				if bool(p3d.call("is_carrying_item_id", "video_camera")) or bool(p3d.call("is_carrying_item_id", "assembled_video_rig")):
					var p_cam := p3d.get("main_camera") as Camera3D
					if p_cam != null:
						var fwd := -p_cam.global_transform.basis.z.normalized()
						cctv_camera.global_position = p_cam.global_position + fwd * 0.45 + Vector3(0.0, -0.12, 0.0)
						cctv_camera.global_basis = p_cam.global_transform.basis
					else:
						var fwd2 := -p3d.global_transform.basis.z.normalized()
						cctv_camera.global_position = p3d.global_position + Vector3(0.0, 1.45, 0.0) + fwd2 * 0.45
						cctv_camera.global_basis = p3d.global_transform.basis
					var held_on := true
					if p3d.has_method("is_carrying_powered_on_item_id"):
						held_on = bool(p3d.call("is_carrying_powered_on_item_id", "video_camera")) or bool(p3d.call("is_carrying_powered_on_item_id", "assembled_video_rig"))
					var held_nv := true
					if p3d.has_method("get_carried_camera_night_vision"):
						held_nv = bool(p3d.call("get_carried_camera_night_vision"))
					_feed_powered_on = held_on
					_feed_nv_on = held_nv
					if cctv_cam_attrs:
						cctv_cam_attrs.exposure_multiplier = 4.8 if (held_on and held_nv) else 1.15
					VideoAndAudioSurveillance.update_camera_screen_shader_material(
						crt_screen_shader_mat, cctv_viewport.get_texture(), held_on, held_nv
					)
					_feed_source_label = "HANDHELD VIDEO CAM [%s]" % (("B&W NV" if held_nv else "COLOR") if held_on else "OFF")
					return

		# Priority 4: No active deployed camera -> dark standby screen until a camera is turned ON / deployed
		_feed_powered_on = false
		_feed_nv_on = false
		VideoAndAudioSurveillance.update_camera_screen_shader_material(
			crt_screen_shader_mat, cctv_viewport.get_texture(), false, false
		)
		_feed_source_label = "NO DEPLOYED CAMERA [Place Cam with G]"

	## Cycles through deployed Video Camera locations (`[RMB]` while aiming at mounted monitor or holding monitor in Hands)!
	## Rotating a camera can only be done directly on the PlacedVideoCameraActor itself (`[E]`).
	func cycle_camera_or_pan() -> void:
		var cams := _get_placed_cameras()
		if cams.is_empty():
			dashcam_active = _check_dashcam_hook_state()
			_sync_cctv_camera_transform()
			_refresh_monitor_feed()
			if dashcam_active:
				capture_video_snapshot()
				GameState.status_message.emit(
					"CCTV MONITOR [DASHCAM]: Viewing Cart Dashcam Feed!",
					Color(0.85, 0.92, 1.0)
				)
			else:
				GameState.status_message.emit(
					"CCTV MONITOR: No deployed Video Cameras found — pick up a Video Camera [F], turn it ON [LMB], and place it [G]!",
					Color(0.85, 0.92, 1.0)
				)
			return

		if snapshot_cooldown <= 0.0:
			capture_video_snapshot()

		if cams.size() > 1:
			active_cam_idx = (active_cam_idx + 1) % cams.size()
			var current_cam := cams[active_cam_idx]
			GameState.status_message.emit(
				"CCTV MONITOR [RMB]: Cycled to Camera Location CAM %02d [%s]! (To rotate a camera, press [E] directly on the camera itself)" % [
					current_cam.camera_index, current_cam.store_id
				],
				Color(0.88, 0.94, 1.0)
			)
		else:
			active_cam_idx = 0
			var only_cam := cams[0]
			GameState.status_message.emit(
				"CCTV MONITOR [RMB]: Viewing CAM %02d [%s] (1 camera deployed — deploy more cameras to cycle locations; rotate on the camera itself with [E])!" % [
					only_cam.camera_index, only_cam.store_id
				],
				Color(0.88, 0.94, 1.0)
			)
		_sync_cctv_camera_transform()
		_refresh_monitor_feed()

	func capture_video_snapshot() -> int:
		if snapshot_cooldown > 0.0:
			GameState.status_message.emit(
				"Cart Monitor DVR buffer writing... Wait %.1fs before next capture." % snapshot_cooldown,
				Color(0.95, 0.82, 0.38)
			)
			return 0

		var cams := _get_placed_cameras()
		if cams.is_empty():
			dashcam_active = _check_dashcam_hook_state()
			if dashcam_active:
				snapshot_cooldown = 3.0
				if not _logged_surveillance_stores.has("Dashcam_Concourse"):
					_logged_surveillance_stores["Dashcam_Concourse"] = true
					GameState.add_cash(50, "Cart Night-Vision Dashcam Surveillance Log")
					return 50
				GameState.status_message.emit("Dashcam concourse clip already logged ($50 claimed).", Color(0.75, 0.88, 0.95))
				return 0
			GameState.status_message.emit(
				"Place a Video Camera or Tripod Stand inside a haunted store ([LMB] or [G]) to capture store CCTV bounties!",
				Color(1.0, 0.78, 0.38)
			)
			return 0

		active_cam_idx = clampi(active_cam_idx, 0, cams.size() - 1)
		var cam := cams[active_cam_idx]
		var status := cam.scan_feed_status()
		var sid: String = str(status.get("store_id", cam.store_id))
		snapshot_cooldown = 3.5

		if bool(status.get("shy_ghost_revealed", false)):
			GameState.record_evidence(sid, "Ghost Orbs / Shy Video Entity", 45)
			var payout_key := "%s_shy" % sid
			if not _logged_surveillance_stores.has(payout_key):
				_logged_surveillance_stores[payout_key] = true
				GameState.add_cash(85, "Camera-Shy Entity CCTV Capture [%s on %s]" % [status.get("ghost_name", "Entity"), status.get("cam_label", "CAM 01")])
				return 85
			GameState.status_message.emit(
				"Camera-Shy Entity [%s] verified on %s! (Evidence & +$85 Bounty already recorded for %s)" % [
					status.get("ghost_name", "Entity"), status.get("cam_label", "CAM"), sid
				],
				Color(0.38, 1.0, 0.78)
			)
			return 0

		if bool(status.get("orbs_detected", false)):
			GameState.record_evidence(sid, "Ghost Orbs / Shy Video Entity", 45)
			var orb_key := "%s_orb" % sid
			if not _logged_surveillance_stores.has(orb_key):
				_logged_surveillance_stores[orb_key] = true
				GameState.add_cash(60, "Ghost Orb IR Video Capture [%s in %s]" % [status.get("cam_label", "CAM 01"), sid])
				return 60
			GameState.status_message.emit(
				"Ghost Orbs verified on %s in %s! (Step ALL players outside the store to reveal Camera-Shy Entity for +$85!)" % [
					status.get("cam_label", "CAM"), sid
				],
				Color(0.42, 1.0, 0.82)
			)
			return 0

		var surv_key := "%s_surv" % sid
		if not _logged_surveillance_stores.has(surv_key):
			_logged_surveillance_stores[surv_key] = true
			GameState.add_cash(50, "Store Security CCTV Calibration [%s in %s]" % [status.get("cam_label", "CAM 01"), sid])
			return 50

		var p_in_store: int = int(status.get("players_in_store", 0))
		if p_in_store > 0:
			GameState.status_message.emit(
				"CCTV %s [%s]: %d investigator(s) inside store — vacate store to reveal Camera-Shy Ghost on monitor!" % [
					status.get("cam_label", "CAM"), sid, p_in_store
				],
				Color(1.0, 0.82, 0.38)
			)
		else:
			GameState.status_message.emit(
				"CCTV %s [%s]: Pan camera [E] toward haunted aisle to spot Ghost Orbs or Camera-Shy Entity!" % [
					status.get("cam_label", "CAM"), sid
				],
				Color(0.75, 0.92, 1.0)
			)
		return 0

	func _refresh_monitor_feed() -> void:
		if screen_label == null or not is_mounted_on_cart:
			return
		if not is_monitor_powered_on:
			screen_label.text = (
				"CART CCTV MONITOR [POWERED OFF]\n" +
				"[LMB] Turn ON | [F] Pick Up Into Hands"
			)
			screen_label.modulate = Color(0.70, 0.76, 0.84)
			return

		dashcam_active = _check_dashcam_hook_state()
		var cams := _get_placed_cameras()

		if cams.is_empty():
			if dashcam_active:
				screen_label.text = (
					"LIVE 3D DASHCAM [CART CAM]\n" +
					"[LMB] Log $50 Clip | [F] Pick Up Into Hands"
				)
				screen_label.modulate = Color(0.88, 0.94, 1.0)
			else:
				screen_label.text = (
					"LIVE 3D FEED: %s\n" % _feed_source_label +
					"[F] Pick Up Into Hands | Turn Cam ON [LMB] & Place [G]"
				)
				screen_label.modulate = Color(0.82, 0.88, 0.96)
			return

		active_cam_idx = clampi(active_cam_idx, 0, cams.size() - 1)
		var cam := cams[active_cam_idx]
		var st := cam.scan_feed_status()
		var nv_str := "NV: B&W ENHANCED" if cam.ir_night_vision_on else "NV: OFF [RMB on Cam]"
		var orb_str := "YES (ORBS!)" if bool(st.get("orbs_detected", false)) else "No"
		var p_count: int = int(st.get("players_in_store", 0))
		var shy_str := ""
		if not cam.is_powered_on:
			shy_str = "Camera Powered OFF — Press [LMB] on Camera to Turn ON!"
			screen_label.modulate = Color(0.75, 0.80, 0.88)
		elif not cam.ir_night_vision_on:
			shy_str = "Night-Vision OFF — Right-Click [RMB] on Camera for B&W Night-Vision!"
			screen_label.modulate = Color(0.88, 0.92, 0.98)
		elif bool(st.get("shy_ghost_revealed", false)):
			shy_str = "SHY ENTITY MANIFESTED! [LMB/RMB: +$85!]"
			screen_label.modulate = Color(0.95, 0.98, 1.0)
		elif p_count > 0:
			shy_str = "Players in Store (%d) — Exit Store to Reveal Shy Ghost" % p_count
			screen_label.modulate = Color(1.0, 0.88, 0.42)
		else:
			shy_str = "Room Empty (0 Players) — Live B&W Night-Vision Scanning..."
			screen_label.modulate = Color(0.88, 0.94, 1.0)

		screen_label.text = (
			"%s [%s | %s] Angle:%.0f° | Orbs: %s\n" % [st.get("cam_label", "CAM 01"), st.get("store_id", ""), nv_str, cam.pan_angle_deg, orb_str] +
			"%s | [RMB] Next Cam (%d/%d) | [F] Pick Up" % [shy_str, active_cam_idx + 1, cams.size()]
		)

	func _physics_process(delta: float) -> void:
		if Engine.is_editor_hint() or not is_mounted_on_cart:
			return
		_sync_cctv_camera_transform()
		if snapshot_cooldown > 0.0:
			snapshot_cooldown = maxf(0.0, snapshot_cooldown - delta)
		_refresh_timer += delta
		if _refresh_timer >= 0.35:
			_refresh_timer = 0.0
			_refresh_monitor_feed()


# ------------------------------------------------------------------------------
# 4. Paranormal Audio / EVP Recorder (`AMallAudioRecorder`)
#    Captures autonomous sub-audible room whispers, weeping, phantom cash-register
#    chatter, and reversed speech over 4.0s within 15.0m of an active StoreGhost.
#    Playback (`trigger_playback`) reveals Receptive vs. Corrupted tells, logs
#    `"Recorded EVP Audio"` evidence, and awards `+$75` Verified EVP Audio Bounty!
# ------------------------------------------------------------------------------
class ParanormalAudioRecorderActor extends Node3D:
	var is_recording: bool = true
	var is_playing_back: bool = false
	var reverse_playback: bool = false
	var recorded_evp_ready: bool = false
	var recorded_store_id: String = ""
	var recorded_ghost_name: String = ""
	var recorded_is_corrupted: bool = false
	var bounty_claimed: bool = false
	var record_progress: float = 0.0
	var item_data: Dictionary = {}

	var left_reel_mi: MeshInstance3D = null
	var right_reel_mi: MeshInstance3D = null
	var rec_led_mat: StandardMaterial3D = null
	var status_light: OmniLight3D = null
	var label_3d: Label3D = null
	var tape_audio_player: AudioStreamPlayer3D = null
	var _playback_timer: float = 0.0

	func _ready() -> void:
		add_to_group("audio_recorders")
		if not item_data.is_empty():
			recorded_evp_ready = bool(item_data.get("recorded_evp_ready", recorded_evp_ready))
			recorded_store_id = str(item_data.get("recorded_store_id", recorded_store_id))
			recorded_ghost_name = str(item_data.get("recorded_ghost_name", recorded_ghost_name))
			recorded_is_corrupted = bool(item_data.get("recorded_is_corrupted", recorded_is_corrupted))
			bounty_claimed = bool(item_data.get("bounty_claimed", bounty_claimed))
			if recorded_evp_ready:
				record_progress = 100.0

		_build_recorder_visuals()
		_refresh_recorder_label()

	func _build_recorder_visuals() -> void:
		for c in get_children():
			c.queue_free()

		# High-gain directional field tape recorder chassis
		var body_mi := MeshInstance3D.new()
		var body_box := BoxMesh.new()
		body_box.size = Vector3(0.22, 0.075, 0.30)
		body_mi.mesh = body_box
		body_mi.position = Vector3(0.0, 0.04, 0.0)
		var body_mat := StandardMaterial3D.new()
		body_mat.albedo_color = Color(0.18, 0.20, 0.23)
		body_mat.metallic = 0.55
		body_mat.roughness = 0.38
		body_mi.material_override = body_mat
		add_child(body_mi)

		# Directional stereo X/Y condenser microphones at the front (-Z)
		var mic_mat := StandardMaterial3D.new()
		mic_mat.albedo_color = Color(0.65, 0.68, 0.72)
		mic_mat.metallic = 0.85
		mic_mat.roughness = 0.25
		for side in [-1.0, 1.0]:
			var mic_mi := MeshInstance3D.new()
			var mic_cyl := CylinderMesh.new()
			mic_cyl.top_radius = 0.016
			mic_cyl.bottom_radius = 0.014
			mic_cyl.height = 0.09
			mic_mi.mesh = mic_cyl
			mic_mi.position = Vector3(side * 0.055, 0.055, -0.18)
			mic_mi.rotation_degrees = Vector3(90.0, side * -18.0, 0.0)
			mic_mi.material_override = mic_mat
			add_child(mic_mi)

		# Twin spinning magnetic tape reels on top deck
		var reel_mat := StandardMaterial3D.new()
		reel_mat.albedo_color = Color(0.82, 0.76, 0.62)
		reel_mat.metallic = 0.35
		reel_mat.roughness = 0.45

		var reel_cyl := CylinderMesh.new()
		reel_cyl.top_radius = 0.045
		reel_cyl.bottom_radius = 0.045
		reel_cyl.height = 0.014

		left_reel_mi = MeshInstance3D.new()
		left_reel_mi.mesh = reel_cyl
		left_reel_mi.position = Vector3(-0.05, 0.085, 0.02)
		left_reel_mi.material_override = reel_mat
		add_child(left_reel_mi)

		right_reel_mi = MeshInstance3D.new()
		right_reel_mi.mesh = reel_cyl
		right_reel_mi.position = Vector3(0.05, 0.085, 0.02)
		right_reel_mi.material_override = reel_mat
		add_child(right_reel_mi)

		# Peak VU / REC LED indicator
		var led_mi := MeshInstance3D.new()
		var led_sph := SphereMesh.new()
		led_sph.radius = 0.016
		led_sph.height = 0.032
		led_mi.mesh = led_sph
		led_mi.position = Vector3(0.0, 0.085, -0.11)
		rec_led_mat = StandardMaterial3D.new()
		rec_led_mat.albedo_color = Color(1.0, 0.22, 0.18)
		rec_led_mat.emission_enabled = true
		rec_led_mat.emission = Color(1.0, 0.22, 0.18)
		rec_led_mat.emission_energy_multiplier = 2.8
		led_mi.material_override = rec_led_mat
		add_child(led_mi)

		status_light = OmniLight3D.new()
		status_light.position = Vector3(0.0, 0.20, 0.0)
		status_light.light_color = Color(1.0, 0.28, 0.22)
		status_light.light_energy = 0.9
		status_light.omni_range = 2.4
		add_child(status_light)

		tape_audio_player = AudioStreamPlayer3D.new()
		tape_audio_player.bus = &"GhostAudio"
		tape_audio_player.max_distance = 18.0
		tape_audio_player.unit_size = 3.2
		add_child(tape_audio_player)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0.0, 0.42, 0.0)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 19
		label_3d.outline_size = 6
		add_child(label_3d)

	func _find_nearest_store_ghost() -> StoreGhost:
		if not is_inside_tree():
			return null
		var best: StoreGhost = null
		var best_d := 15.0
		for node in get_tree().get_nodes_in_group("store_ghosts"):
			var sg := node as StoreGhost
			if sg and not sg.is_resolved:
				var d := global_position.distance_to(sg.global_position)
				if d <= best_d:
					best_d = d
					best = sg
		return best

	func trigger_playback(reverse_mode: bool = true) -> int:
		reverse_playback = reverse_mode
		is_playing_back = true
		_playback_timer = 4.5

		if not recorded_evp_ready:
			var near_g := _find_nearest_store_ghost()
			if near_g != null:
				recorded_evp_ready = true
				record_progress = 100.0
				recorded_store_id = near_g.store_id
				recorded_ghost_name = near_g.ghost_name
				recorded_is_corrupted = near_g.is_corrupted_hostile
			else:
				GameState.status_message.emit(
					"EVP TAPE RECORDER: Only ambient mall hum on tape (%.0f%%). Place or hold within 15m of a Store Ghost for 4.0s to capture EVP!" % record_progress,
					Color(0.95, 0.78, 0.35)
				)
				_refresh_recorder_label()
				return 0

		# Play isolated reversed/amplified ghost voice audio
		if tape_audio_player:
			var wav_path := "res://SourceAssets/GhostSounds/SW_Ghost_HuntGrowl.wav" if recorded_is_corrupted else "res://SourceAssets/GhostSounds/SW_Ghost_InvisibleMoan.wav"
			if ResourceLoader.exists(wav_path):
				tape_audio_player.stream = load(wav_path)
				tape_audio_player.pitch_scale = 0.74 if (recorded_is_corrupted or reverse_mode) else 1.06
				tape_audio_player.play()

		var store_key := recorded_store_id if not recorded_store_id.is_empty() else "East_F2_01"
		GameState.record_evidence(store_key, "Recorded EVP Audio", 45)

		var transcript := ""
		if recorded_is_corrupted:
			transcript = "CORRUPTED REVERSED SPEECH: \"...KSAR EHT HCUOT DNA NRUB — LEAVE NONE ALIVE...\" [HOSTILE-ONLY TELL!]"
		else:
			transcript = "RECEPTIVE EVP WHISPER: \"...Cash register drawer #4... I only wanted to finish closing count...\" [PEACEFUL TELL!]"

		var bounty_awarded := 0
		if not bounty_claimed:
			bounty_claimed = true
			bounty_awarded = 75
			GameState.add_cash(75, "Verified EVP Tape Recording [%s in %s]" % [recorded_ghost_name, store_key])

		GameState.status_message.emit(
			"EVP PLAYBACK [%s | %s]: %s" % [
				"REVERSE DECODE" if reverse_mode else "HIGH-GAIN FORWARD",
				store_key,
				transcript
			],
			Color(1.0, 0.42, 0.38) if recorded_is_corrupted else Color(0.38, 1.0, 0.82)
		)
		_sync_to_item_data()
		_refresh_recorder_label()
		return bounty_awarded

	func _sync_to_item_data() -> void:
		if item_data.is_empty():
			item_data = {
				"id": "audio_recorder",
				"name": "Paranormal Audio / EVP Tape Recorder",
				"price": 70,
				"can_store_in_hotbar": true,
				"can_hang_on_cart": false,
				"can_power_toggle": true,
				"powered_on": true,
				"makes_noise_when_on": false,
				"model_path": "",
			}
		item_data["recorded_evp_ready"] = recorded_evp_ready
		item_data["recorded_store_id"] = recorded_store_id
		item_data["recorded_ghost_name"] = recorded_ghost_name
		item_data["recorded_is_corrupted"] = recorded_is_corrupted
		item_data["bounty_claimed"] = bounty_claimed

	func to_pickup_item_data() -> Dictionary:
		_sync_to_item_data()
		return item_data.duplicate(true)

	func _refresh_recorder_label() -> void:
		if label_3d == null:
			return
		if is_playing_back:
			var tell_tag := "CORRUPTED REVERSED SPEECH (Hostile Tell!)" if recorded_is_corrupted else "RECEPTIVE EVP WHISPER (Peaceful Tell!)"
			label_3d.text = (
				"PARANORMAL EVP RECORDER [PLAYING BACK — %s]\n" % ("REVERSE" if reverse_playback else "FORWARD") +
				"%s [%s]\n" % [tell_tag, recorded_store_id] +
				"[F] Pick Up | [E] Replay EVP Tape"
			)
			label_3d.modulate = Color(1.0, 0.48, 0.42) if recorded_is_corrupted else Color(0.38, 1.0, 0.78)
		elif recorded_evp_ready:
			var bounty_str := "CLAIMED" if bounty_claimed else "+$75 BOUNTY READY!"
			label_3d.text = (
				"PARANORMAL EVP RECORDER [EVP CAPTURED 100%% — %s]\n" % bounty_str +
				"Source: %s (%s)\n" % [recorded_ghost_name, recorded_store_id] +
				"[E] Play Back Reversed EVP Audio | [F] Pick Up"
			)
			label_3d.modulate = Color(0.35, 1.0, 0.65)
		else:
			label_3d.text = (
				"PARANORMAL EVP RECORDER [RECORDING SUB-AUDIBLE ROOM AUDIO: %.0f%%]\n" % record_progress +
				"[E] Play Back Tape (Forward/Reverse) | [F] Pick Up"
			)
			label_3d.modulate = Color(1.0, 0.75, 0.35)

	func _physics_process(delta: float) -> void:
		if Engine.is_editor_hint():
			return

		if is_recording or is_playing_back:
			var spin_dir := -4.5 if (is_playing_back and reverse_playback) else 3.2
			if left_reel_mi:
				left_reel_mi.rotation.y += delta * spin_dir
			if right_reel_mi:
				right_reel_mi.rotation.y += delta * spin_dir

		if is_playing_back:
			_playback_timer = maxf(0.0, _playback_timer - delta)
			if _playback_timer <= 0.0:
				is_playing_back = false
				_refresh_recorder_label()

		if is_recording and not recorded_evp_ready:
			var g := _find_nearest_store_ghost()
			if g != null:
				# Accumulates over 4.0s (25% per second) within 15.0m of an active StoreGhost
				record_progress = minf(100.0, record_progress + 25.0 * delta)
				if status_light:
					status_light.light_color = Color(1.0, 0.55, 0.18)
					status_light.light_energy = 1.35
				if record_progress >= 100.0:
					recorded_evp_ready = true
					recorded_store_id = g.store_id
					recorded_ghost_name = g.ghost_name
					recorded_is_corrupted = g.is_corrupted_hostile
					_sync_to_item_data()
					if status_light:
						status_light.light_color = Color(0.25, 1.0, 0.55)
						status_light.light_energy = 1.8
					if rec_led_mat:
						rec_led_mat.albedo_color = Color(0.25, 1.0, 0.55)
						rec_led_mat.emission = Color(0.25, 1.0, 0.55)
					GameState.status_message.emit(
						"EVP AUDIO CAPTURED (100%%) in [%s]! Press [E] on the Paranormal Audio Recorder to play back & decode (+$75 Bounty)!" % recorded_store_id,
						Color(0.38, 1.0, 0.72)
					)
				_refresh_recorder_label()
