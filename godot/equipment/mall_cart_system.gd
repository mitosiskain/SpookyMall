@tool
class_name MallCartSystem
extends Node3D
## Manages the 8-Slot Equipment Push Cart, Doorway Seal Dock (Sealed / Unsealed / Dormant),
## and the Wheeled 18,000 lm Mobile Light Cart (4 rotating caster wheels + overhead downward
## floodlight creating a 3.25m floor Circle of Safety that ghosts cannot enter and whose edge
## physically blocks ghosts from leaving or entering areas).

enum DoorwaySealState {
	UNSEALED = 0,
	SEALED = 1,
	DORMANT = 2,
}

signal doorway_seal_changed(store_id: String, new_state: DoorwaySealState, state_name: String)

@export var cart_type: String = "PushCart" # "PushCart" or "LightCart"
@export var linked_store_id: String = "East_F0_02"
@export var dock_world_position: Vector3 = Vector3(14.0, 0.0, 0.0)
@export var store_interior_center: Vector3 = Vector3(20.0, 0.0, 0.0)

## Wheeled Mobile Light Cart properties (Overhead Downward Light -> Floor Circle of Safety)
var battery_percent: float = 100.0
var is_floodlight_on: bool = true
const SAFETY_CIRCLE_RADIUS: float = 3.25
const OVERHEAD_LAMP_HEIGHT: float = 2.62
const FLOODLIGHT_RANGE: float = 4.8
const FLOODLIGHT_HALF_ANGLE_DEG: float = 52.5
const BATTERY_DRAIN_PER_SEC: float = 100.0 / 900.0 # 15 minutes full runtime

var spot_light: SpotLight3D = null
var safety_circle_ring: MeshInstance3D = null
var safety_circle_disc: MeshInstance3D = null
var lens_mat: StandardMaterial3D = null
var cart_label: Label3D = null
var switch_audio: AudioStreamPlayer3D = null
var wheel_meshes: Array[MeshInstance3D] = []
var _last_world_pos: Vector3 = Vector3.ZERO

var current_seal_state: DoorwaySealState = DoorwaySealState.UNSEALED
var is_attached_to_player: bool = false
var attached_player: Node3D = null

## 8-Slot Equipment Push Cart physical shelf storage + placement outline preview ([G] to place, [E] to take)
const PUSH_CART_SLOT_OFFSETS: Array[Vector3] = [
	# Top Shelf (4 Slots, Y = 0.88m)
	Vector3(-0.34, 0.88, -0.16),
	Vector3( 0.16, 0.88, -0.16),
	Vector3(-0.34, 0.88,  0.16),
	Vector3( 0.16, 0.88,  0.16),
	# Lower Shelf (4 Slots, Y = 0.42m)
	Vector3(-0.34, 0.42, -0.16),
	Vector3( 0.16, 0.42, -0.16),
	Vector3(-0.34, 0.42,  0.16),
	Vector3( 0.16, 0.42,  0.16),
]
var cart_slot_items: Array[Dictionary] = [{}, {}, {}, {}, {}, {}, {}, {}]
var cart_slot_outlines: Array[Node3D] = []
var cart_slot_outline_mats: Array[StandardMaterial3D] = []
var cart_slot_labels: Array[Label3D] = []
var cart_slot_visual_holders: Array[Node3D] = []
var highlighted_place_slot: int = -1
var highlighted_take_slot: int = -1
var _outline_pulse_time: float = 0.0
var _net_cart_sync_timer: float = 0.0

func _ready() -> void:
	name = "MallCart_%s" % cart_type
	add_to_group("mall_carts")
	wheel_meshes.clear()
	if cart_type == "LightCart":
		add_to_group("light_carts")
		_build_light_cart_visuals()
	else:
		add_to_group("push_carts")
		_build_push_cart_visuals()
	_last_world_pos = global_position
	if not Engine.is_editor_hint():
		_evaluate_doorway_seal()

func _build_push_cart_visuals() -> void:
	var spawned_cart := false
	var fbx_path := "res://SourceAssets/Carts/Prepared/SM_EquipmentUtilityCart.fbx"
	if ResourceLoader.exists(fbx_path):
		var packed := load(fbx_path) as PackedScene
		if packed:
			var inst := packed.instantiate()
			add_child(inst)
			spawned_cart = true

	if not spawned_cart:
		var base := MeshInstance3D.new()
		var box := BoxMesh.new()
		box.size = Vector3(1.35, 0.85, 0.72)
		base.mesh = box
		base.position = Vector3(0, 0.48, 0)
		var mat := StandardMaterial3D.new()
		mat.albedo_color = Color(0.22, 0.27, 0.32)
		mat.metallic = 0.55
		mat.roughness = 0.38
		base.material_override = mat
		add_child(base)

	switch_audio = AudioStreamPlayer3D.new()
	switch_audio.bus = &"SFX"
	switch_audio.max_distance = 18.0
	switch_audio.unit_size = 3.0
	add_child(switch_audio)

	cart_label = Label3D.new()
	cart_label.position = Vector3(0.0, 1.55, 0.0)
	cart_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	cart_label.font_size = 20
	cart_label.outline_size = 6
	cart_label.modulate = Color(0.55, 0.96, 1.0)
	add_child(cart_label)

	# Build the 8 physical shelf slot pads, 3D wireframe placement outlines, and 3D item visual holders
	cart_slot_items = [{}, {}, {}, {}, {}, {}, {}, {}]
	cart_slot_outlines.clear()
	cart_slot_outline_mats.clear()
	cart_slot_labels.clear()
	cart_slot_visual_holders.clear()

	var pad_mat := StandardMaterial3D.new()
	pad_mat.albedo_color = Color(0.12, 0.16, 0.22, 0.85)
	pad_mat.roughness = 0.65

	for i in range(PUSH_CART_SLOT_OFFSETS.size()):
		var slot_pos: Vector3 = PUSH_CART_SLOT_OFFSETS[i]

		# Subtle rubberized slot mat on the cart shelf
		var pad := MeshInstance3D.new()
		var pad_box := BoxMesh.new()
		pad_box.size = Vector3(0.28, 0.012, 0.22)
		pad.mesh = pad_box
		pad.position = slot_pos + Vector3(0.0, 0.006, 0.0)
		pad.material_override = pad_mat
		add_child(pad)

		# 3D Wireframe Box Outline (shows the exact spot where [G] will place your held item!)
		var o_mat := StandardMaterial3D.new()
		o_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		o_mat.albedo_color = Color(0.22, 1.0, 0.78, 0.92)
		o_mat.emission_enabled = true
		o_mat.emission = Color(0.22, 1.0, 0.78)
		o_mat.emission_energy_multiplier = 3.2
		var outline_node := _create_slot_wireframe_outline(Vector3(0.30, 0.18, 0.24), o_mat)
		outline_node.position = slot_pos + Vector3(0.0, 0.09, 0.0)
		outline_node.visible = false
		add_child(outline_node)
		cart_slot_outlines.append(outline_node)
		cart_slot_outline_mats.append(o_mat)

		# Floating 3D label above the slot when highlighted
		var s_lbl := Label3D.new()
		s_lbl.position = slot_pos + Vector3(0.0, 0.28, 0.0)
		s_lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		s_lbl.font_size = 17
		s_lbl.outline_size = 5
		s_lbl.visible = false
		add_child(s_lbl)
		cart_slot_labels.append(s_lbl)

		# Visual holder for the stored item's 3D mesh on this cart slot
		var holder := Node3D.new()
		holder.position = slot_pos + Vector3(0.0, 0.03, 0.0)
		add_child(holder)
		cart_slot_visual_holders.append(holder)

	_refresh_push_cart_header_label()

func _create_slot_wireframe_outline(box_size: Vector3, mat: StandardMaterial3D) -> Node3D:
	var root := Node3D.new()
	var t := 0.014
	var hx := box_size.x * 0.5
	var hy := box_size.y * 0.5
	var hz := box_size.z * 0.5
	# 4 X-edges
	for sy in [-1.0, 1.0]:
		for sz in [-1.0, 1.0]:
			var bar := MeshInstance3D.new()
			var bm := BoxMesh.new()
			bm.size = Vector3(box_size.x, t, t)
			bar.mesh = bm
			bar.position = Vector3(0.0, sy * hy, sz * hz)
			bar.material_override = mat
			bar.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			root.add_child(bar)
	# 4 Z-edges
	for sy in [-1.0, 1.0]:
		for sx in [-1.0, 1.0]:
			var bar := MeshInstance3D.new()
			var bm := BoxMesh.new()
			bm.size = Vector3(t, t, box_size.z)
			bar.mesh = bm
			bar.position = Vector3(sx * hx, sy * hy, 0.0)
			bar.material_override = mat
			bar.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			root.add_child(bar)
	# 4 vertical Y-pillars
	for sx in [-1.0, 1.0]:
		for sz in [-1.0, 1.0]:
			var bar := MeshInstance3D.new()
			var bm := BoxMesh.new()
			bm.size = Vector3(t, box_size.y, t)
			bar.mesh = bm
			bar.position = Vector3(sx * hx, 0.0, sz * hz)
			bar.material_override = mat
			bar.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			root.add_child(bar)
	return root

func get_stored_item_count() -> int:
	var c := 0
	for s in cart_slot_items:
		if not s.is_empty():
			c += 1
	return c

func get_first_open_push_cart_slot() -> int:
	for i in range(cart_slot_items.size()):
		if cart_slot_items[i].is_empty():
			return i
	return -1

func can_accept_item_from_player(player: Node3D) -> bool:
	if cart_type != "PushCart" or player == null:
		return false
	if global_position.distance_to(player.global_position) > 2.95:
		return false
	return get_first_open_push_cart_slot() != -1

## Places an item Dictionary into the highlighted (or first open) slot on the Equipment Push Cart!
func place_item_on_cart(item_data: Dictionary, preferred_slot: int = -1) -> int:
	if cart_type != "PushCart" or item_data.is_empty():
		return -1
	var target_slot := preferred_slot
	if target_slot < 0 or target_slot >= cart_slot_items.size() or not cart_slot_items[target_slot].is_empty():
		if highlighted_place_slot >= 0 and highlighted_place_slot < cart_slot_items.size() and cart_slot_items[highlighted_place_slot].is_empty():
			target_slot = highlighted_place_slot
		else:
			target_slot = get_first_open_push_cart_slot()
	if target_slot == -1:
		return -1

	var stored_copy := item_data.duplicate(true)
	# Automatically switch noise-making gadgets OFF when neatly racked onto the Push Cart shelf
	if bool(stored_copy.get("can_power_toggle", false)):
		stored_copy["powered_on"] = false
	cart_slot_items[target_slot] = stored_copy
	_rebuild_slot_item_visual(target_slot)
	_refresh_push_cart_header_label()

	if switch_audio and ResourceLoader.exists("res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_On.wav"):
		switch_audio.stream = load("res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_On.wav")
		switch_audio.pitch_scale = 0.92
		switch_audio.play()
	_broadcast_cart_slots_if_coop()
	return target_slot

## Removes and returns the item Dictionary from the currently highlighted occupied slot (or last occupied slot) on the Push Cart.
func take_highlighted_item_from_cart() -> Dictionary:
	if cart_type != "PushCart":
		return {}
	var slot_idx := highlighted_take_slot
	if slot_idx < 0 or slot_idx >= cart_slot_items.size() or cart_slot_items[slot_idx].is_empty():
		for i in range(cart_slot_items.size() - 1, -1, -1):
			if not cart_slot_items[i].is_empty():
				slot_idx = i
				break
	if slot_idx < 0 or slot_idx >= cart_slot_items.size() or cart_slot_items[slot_idx].is_empty():
		return {}

	var taken := cart_slot_items[slot_idx].duplicate(true)
	cart_slot_items[slot_idx] = {}
	_rebuild_slot_item_visual(slot_idx)
	_refresh_push_cart_header_label()
	if switch_audio and ResourceLoader.exists("res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_Off.wav"):
		switch_audio.stream = load("res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_Off.wav")
		switch_audio.pitch_scale = 1.08
		switch_audio.play()
	_broadcast_cart_slots_if_coop()
	return taken

func _broadcast_cart_slots_if_coop() -> void:
	if not Engine.is_editor_hint() and MultiplayerManager != null and MultiplayerManager.is_coop_active and multiplayer.has_multiplayer_peer():
		_rpc_sync_cart_slots.rpc(cart_slot_items)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_sync_cart_slots(remote_slots: Array) -> void:
	if cart_type != "PushCart":
		return
	for i in range(mini(cart_slot_items.size(), remote_slots.size())):
		var d: Dictionary = remote_slots[i] if remote_slots[i] is Dictionary else {}
		cart_slot_items[i] = d.duplicate(true)
		_rebuild_slot_item_visual(i)
	_refresh_push_cart_header_label()

func _rebuild_slot_item_visual(slot_idx: int) -> void:
	if slot_idx < 0 or slot_idx >= cart_slot_visual_holders.size():
		return
	var holder := cart_slot_visual_holders[slot_idx]
	for c in holder.get_children():
		c.queue_free()
	var item := cart_slot_items[slot_idx]
	if item.is_empty():
		return

	var model_path: String = str(item.get("model_path", ""))
	var spawned := false
	if not model_path.is_empty() and ResourceLoader.exists(model_path):
		var packed := load(model_path) as PackedScene
		if packed:
			var inst := packed.instantiate() as Node3D
			inst.scale = Vector3(0.78, 0.78, 0.78)
			holder.add_child(inst)
			spawned = true
	if not spawned:
		var mi := MeshInstance3D.new()
		var bm := BoxMesh.new()
		bm.size = Vector3(0.22, 0.12, 0.16)
		mi.mesh = bm
		mi.position = Vector3(0.0, 0.06, 0.0)
		var mat := StandardMaterial3D.new()
		mat.albedo_color = item.get("accent_color", Color(0.45, 0.88, 1.0))
		mi.material_override = mat
		holder.add_child(mi)

func _refresh_push_cart_header_label() -> void:
	if cart_type != "PushCart" or cart_label == null:
		return
	var used := get_stored_item_count()
	cart_label.text = (
		"8-Slot Equipment Push Cart [%d/8 Stored | %s]\n" % [used, get_seal_state_label().split(" (")[0]] +
		"[G] Place Held Item on Highlighted Outline | [E] Take Stored Item | [F] Push/Park Cart"
	)

## Builds a Wheeled Mobile Light Cart with 4 rotating caster wheels, a 2.62m overhead mast,
## a downward-pointing floodlight head (-Y), and a visible 3.25m Floor Circle of Safety
## whose perimeter edge blocks ghosts from leaving or entering areas.
func _build_light_cart_visuals() -> void:
	var dark_metal := StandardMaterial3D.new()
	dark_metal.albedo_color = Color(0.18, 0.20, 0.24)
	dark_metal.metallic = 0.65
	dark_metal.roughness = 0.35

	var yellow_paint := StandardMaterial3D.new()
	yellow_paint.albedo_color = Color(0.95, 0.76, 0.14)
	yellow_paint.metallic = 0.25
	yellow_paint.roughness = 0.42

	var tire_mat := StandardMaterial3D.new()
	tire_mat.albedo_color = Color(0.10, 0.11, 0.13)
	tire_mat.roughness = 0.80

	# 1. 4 Caster Wheels (Forks + Rotating Wheel Cylinders at the 4 corners)
	var wheel_offsets := [
		Vector3(-0.34, 0.09, -0.26),
		Vector3(0.34, 0.09, -0.26),
		Vector3(-0.34, 0.09, 0.26),
		Vector3(0.34, 0.09, 0.26),
	]
	for w_pos in wheel_offsets:
		# Caster fork bracket
		var fork := MeshInstance3D.new()
		var fork_box := BoxMesh.new()
		fork_box.size = Vector3(0.07, 0.10, 0.08)
		fork.mesh = fork_box
		fork.position = Vector3(w_pos.x, 0.14, w_pos.z)
		fork.material_override = dark_metal
		add_child(fork)

		# Rotating wheel cylinder (height along local Y, rotated 90 deg on X so axle is along Z)
		var wheel := MeshInstance3D.new()
		var w_cyl := CylinderMesh.new()
		w_cyl.top_radius = 0.09
		w_cyl.bottom_radius = 0.09
		w_cyl.height = 0.055
		wheel.mesh = w_cyl
		wheel.position = w_pos
		wheel.rotation_degrees = Vector3(90.0, 0.0, 0.0)
		wheel.material_override = tire_mat

		# Hubcap bar so wheel rotation is visibly obvious
		var spoke := MeshInstance3D.new()
		var spoke_box := BoxMesh.new()
		spoke_box.size = Vector3(0.14, 0.062, 0.025)
		spoke.mesh = spoke_box
		spoke.material_override = yellow_paint
		wheel.add_child(spoke)

		add_child(wheel)
		wheel_meshes.append(wheel)

	# 2. Wheeled Rolling Cart Chassis Deck (supported at Y = 0.22m above the 4 wheels)
	var deck := MeshInstance3D.new()
	var deck_box := BoxMesh.new()
	deck_box.size = Vector3(0.86, 0.08, 0.66)
	deck.mesh = deck_box
	deck.position = Vector3(0.0, 0.22, 0.0)
	deck.material_override = yellow_paint
	add_child(deck)

	# 3. Heavy-Duty Battery Pack Housing on the Cart Deck
	var battery_box_mi := MeshInstance3D.new()
	var bat_mesh := BoxMesh.new()
	bat_mesh.size = Vector3(0.44, 0.28, 0.44)
	battery_box_mi.mesh = bat_mesh
	battery_box_mi.position = Vector3(-0.10, 0.40, 0.0)
	battery_box_mi.material_override = dark_metal
	add_child(battery_box_mi)

	# 4. Rear Push Handlebar
	for z_side in [-0.26, 0.26]:
		var strut := MeshInstance3D.new()
		var s_box := BoxMesh.new()
		s_box.size = Vector3(0.04, 0.72, 0.04)
		strut.mesh = s_box
		strut.position = Vector3(-0.40, 0.58, z_side)
		strut.rotation_degrees = Vector3(0.0, 0.0, 8.0)
		strut.material_override = dark_metal
		add_child(strut)

	var grip := MeshInstance3D.new()
	var grip_cyl := CylinderMesh.new()
	grip_cyl.top_radius = 0.022
	grip_cyl.bottom_radius = 0.022
	grip_cyl.height = 0.58
	grip.mesh = grip_cyl
	grip.position = Vector3(-0.45, 0.92, 0.0)
	grip.rotation_degrees = Vector3(90.0, 0.0, 0.0)
	grip.material_override = yellow_paint
	add_child(grip)

	# 5. Tall Overhead Telescoping Mast + Cantilevered Overhead Arm (Y = 0.26m -> 2.68m)
	var mast := MeshInstance3D.new()
	var mast_cyl := CylinderMesh.new()
	mast_cyl.top_radius = 0.035
	mast_cyl.bottom_radius = 0.05
	mast_cyl.height = 2.42
	mast.mesh = mast_cyl
	mast.position = Vector3(0.22, 1.45, 0.0)
	mast.material_override = yellow_paint
	add_child(mast)

	var overhead_arm := MeshInstance3D.new()
	var arm_box := BoxMesh.new()
	arm_box.size = Vector3(0.36, 0.06, 0.06)
	overhead_arm.mesh = arm_box
	overhead_arm.position = Vector3(0.08, 2.66, 0.0)
	overhead_arm.material_override = dark_metal
	add_child(overhead_arm)

	# 6. Overhead Floodlight Reflector Hood Pointing Straight Down (-Y)
	var hood := MeshInstance3D.new()
	var hood_cyl := CylinderMesh.new()
	hood_cyl.top_radius = 0.11
	hood_cyl.bottom_radius = 0.28
	hood_cyl.height = 0.16
	hood.mesh = hood_cyl
	hood.position = Vector3(0.0, OVERHEAD_LAMP_HEIGHT, 0.0)
	hood.material_override = dark_metal
	hood.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(hood)

	var lens := MeshInstance3D.new()
	var lens_cyl := CylinderMesh.new()
	lens_cyl.top_radius = 0.25
	lens_cyl.bottom_radius = 0.25
	lens_cyl.height = 0.02
	lens.mesh = lens_cyl
	lens.position = Vector3(0.0, OVERHEAD_LAMP_HEIGHT - 0.08, 0.0)
	lens_mat = StandardMaterial3D.new()
	lens_mat.albedo_color = Color(1.0, 0.97, 0.86)
	lens_mat.emission_enabled = true
	lens_mat.emission = Color(1.0, 0.95, 0.78)
	lens_mat.emission_energy_multiplier = 4.5
	lens.material_override = lens_mat
	lens.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(lens)

	switch_audio = AudioStreamPlayer3D.new()
	switch_audio.bus = &"SFX"
	switch_audio.max_distance = 22.0
	switch_audio.unit_size = 3.5
	add_child(switch_audio)

	# 7. Overhead Downward SpotLight3D (Points Straight Down -Y to project the Circle of Safety!)
	spot_light = SpotLight3D.new()
	spot_light.position = Vector3(0.0, OVERHEAD_LAMP_HEIGHT - 0.10, 0.0)
	spot_light.rotation_degrees = Vector3(-90.0, 0.0, 0.0) # Straight down (-Y)!
	spot_light.light_color = Color(1.0, 0.97, 0.86)
	spot_light.light_energy = 8.5
	spot_light.light_volumetric_fog_energy = 2.6
	spot_light.spot_range = FLOODLIGHT_RANGE
	spot_light.spot_angle = FLOODLIGHT_HALF_ANGLE_DEG
	spot_light.shadow_enabled = true
	add_child(spot_light)

	# 8. Visible Floor Circle of Safety & Perimeter Edge Ring (Radius = 3.25m)
	#    Allows players to clearly see the exact circle edge when blocking store doorways or choke points!
	safety_circle_ring = MeshInstance3D.new()
	var torus := TorusMesh.new()
	torus.inner_radius = SAFETY_CIRCLE_RADIUS - 0.055
	torus.outer_radius = SAFETY_CIRCLE_RADIUS + 0.035
	torus.rings = 48
	torus.ring_segments = 12
	safety_circle_ring.mesh = torus
	safety_circle_ring.position = Vector3(0.0, 0.02, 0.0)
	var ring_mat := StandardMaterial3D.new()
	ring_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	ring_mat.albedo_color = Color(1.0, 0.90, 0.38, 0.78)
	ring_mat.emission_enabled = true
	ring_mat.emission = Color(1.0, 0.86, 0.30)
	ring_mat.emission_energy_multiplier = 2.2
	safety_circle_ring.material_override = ring_mat
	safety_circle_ring.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(safety_circle_ring)

	safety_circle_disc = MeshInstance3D.new()
	var disc_cyl := CylinderMesh.new()
	disc_cyl.top_radius = SAFETY_CIRCLE_RADIUS - 0.05
	disc_cyl.bottom_radius = SAFETY_CIRCLE_RADIUS - 0.05
	disc_cyl.height = 0.006
	safety_circle_disc.mesh = disc_cyl
	safety_circle_disc.position = Vector3(0.0, 0.008, 0.0)
	var disc_mat := StandardMaterial3D.new()
	disc_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	disc_mat.albedo_color = Color(1.0, 0.95, 0.72, 0.11)
	disc_mat.emission_enabled = true
	disc_mat.emission = Color(1.0, 0.92, 0.60)
	disc_mat.emission_energy_multiplier = 0.65
	safety_circle_disc.material_override = disc_mat
	safety_circle_disc.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(safety_circle_disc)

	cart_label = Label3D.new()
	cart_label.position = Vector3(0.0, 2.95, 0.0)
	cart_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	cart_label.font_size = 20
	cart_label.outline_size = 6
	add_child(cart_label)
	_update_light_cart_active_visuals()

func detach_from_player(silent: bool = false) -> void:
	if not is_attached_to_player:
		return
	is_attached_to_player = false
	attached_player = null
	_resolve_cart_overlap()
	_evaluate_doorway_seal()
	if not silent:
		if cart_type == "LightCart":
			GameState.status_message.emit(
				"Parked Wheeled Light Cart (Circle of Safety Radius: %.2fm — Ghosts cannot enter or cross the circle edge!)." % SAFETY_CIRCLE_RADIUS,
				Color(1.0, 0.94, 0.48)
			)
		else:
			GameState.status_message.emit("Parked %s (%s)" % [cart_type, get_seal_state_label()], Color(0.85, 0.9, 1.0))

func toggle_attach(player: Node3D) -> void:
	if is_attached_to_player and attached_player == player:
		detach_from_player(false)
		return

	# Mutual exclusion: a player can NEVER hold/push more than one cart at a time!
	if is_inside_tree():
		for node in get_tree().get_nodes_in_group("mall_carts"):
			var other := node as MallCartSystem
			if other and other != self and other.is_attached_to_player and other.attached_player == player:
				other.detach_from_player(true)

	is_attached_to_player = true
	attached_player = player
	GameState.status_message.emit("Wheeling %s (No Sprinting | Drains Stamina While Pushing — Press [F] to Park)." % cart_type, Color(0.5, 0.95, 1.0))

func _resolve_cart_overlap() -> void:
	if not is_inside_tree():
		return
	for node in get_tree().get_nodes_in_group("mall_carts"):
		var other := node as MallCartSystem
		if other == null or other == self or absf(other.global_position.y - global_position.y) > 1.5:
			continue
		var diff := Vector2(global_position.x - other.global_position.x, global_position.z - other.global_position.z)
		var dist := diff.length()
		var min_sep := 1.25
		if dist < min_sep:
			var push_dir := (diff / dist) if dist > 0.001 else Vector2(1.0, 0.0)
			var resolved_2d := Vector2(other.global_position.x, other.global_position.z) + push_dir * min_sep
			global_position.x = resolved_2d.x
			global_position.z = resolved_2d.y

func set_floodlight_power(turn_on: bool, actor_reason: String = "Player", broadcast_rpc: bool = true) -> void:
	if cart_type != "LightCart":
		return
	if is_floodlight_on == turn_on:
		return
	is_floodlight_on = turn_on
	if broadcast_rpc and not Engine.is_editor_hint() and MultiplayerManager != null and MultiplayerManager.is_coop_active and multiplayer.has_multiplayer_peer():
		_rpc_set_floodlight_power.rpc(turn_on, actor_reason)
	if switch_audio:
		var click_path := "res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_On.wav" if is_floodlight_on else "res://SourceAssets/EquipmentSounds/SW_Flashlight_Click_Off.wav"
		if ResourceLoader.exists(click_path):
			switch_audio.stream = load(click_path)
			switch_audio.pitch_scale = 0.85 if not is_floodlight_on else 0.95
			switch_audio.play()
	_update_light_cart_active_visuals()
	if "Gremlin" in actor_reason:
		GameState.status_message.emit(
			"GREMLIN INTERFERENCE! A Hallway Gremlin switched the Wheeled Light Cart OFF — press [E] near the cart (or use Light Remote Gun) to turn the Circle of Safety back ON!",
			Color(1.0, 0.45, 0.28)
		)
	else:
		GameState.status_message.emit(
			"Wheeled Overhead Light Cart [%s]: %s (Battery: %.0f%% | %.2fm Circle of Safety)" % [
				actor_reason, "ON" if is_floodlight_on else "OFF", battery_percent, SAFETY_CIRCLE_RADIUS
			],
			Color(1.0, 0.95, 0.45) if is_floodlight_on else Color(0.75, 0.80, 0.88)
		)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_set_floodlight_power(turn_on: bool, actor_reason: String) -> void:
	set_floodlight_power(turn_on, actor_reason, false)

func toggle_floodlight(actor_reason: String = "Player") -> void:
	if cart_type != "LightCart":
		return
	set_floodlight_power(not is_floodlight_on, actor_reason)

func boost_battery(percent_bonus: float) -> void:
	battery_percent = clampf(battery_percent + percent_bonus, 0.0, 150.0)
	_update_light_cart_active_visuals()

func _update_light_cart_active_visuals() -> void:
	var active := (cart_type == "LightCart" and is_floodlight_on and battery_percent > 0.0)
	if spot_light:
		spot_light.visible = active
	if safety_circle_ring:
		safety_circle_ring.visible = active
	if safety_circle_disc:
		safety_circle_disc.visible = active
	if lens_mat:
		lens_mat.emission_energy_multiplier = 4.5 if active else 0.0
		lens_mat.albedo_color = Color(1.0, 0.97, 0.86) if active else Color(0.28, 0.30, 0.34)
	if cart_label:
		if active:
			cart_label.text = "Wheeled Mobile Light Cart [LIGHT: ON | %.0f%%]\n[F] Push/Park | [E] Turn Light OFF | 3.25m Circle of Safety" % battery_percent
			cart_label.modulate = Color(1.0, 0.94, 0.52)
		else:
			cart_label.text = "Wheeled Mobile Light Cart [LIGHT: OFF — NO SAFETY CIRCLE!]\n[E] Turn Light ON | [F] Push/Park (Watch out for Gremlin Interference!)"
			cart_label.modulate = Color(1.0, 0.55, 0.35)

func is_light_circle_active() -> bool:
	return cart_type == "LightCart" and is_floodlight_on and battery_percent > 0.0

## Returns true if a world point lies inside the overhead downward light's floor Circle of Safety.
func is_point_inside_safety_circle(world_pt: Vector3, extra_radius: float = 0.0) -> bool:
	if not is_light_circle_active():
		return false
	if absf(world_pt.y - global_position.y) > 2.8:
		return false
	var d2 := Vector2(world_pt.x - global_position.x, world_pt.z - global_position.z).length()
	return d2 <= (SAFETY_CIRCLE_RADIUS + extra_radius)

## Backwards-compatible alias for is_point_inside_safety_circle()
func is_point_inside_safety_cone(world_pt: Vector3) -> bool:
	return is_point_inside_safety_circle(world_pt, 0.0)

## Returns true if a 2D movement segment from `from_pt` to `to_pt` touches or crosses the Circle of Safety edge.
func does_segment_intersect_safety_circle(from_pt: Vector3, to_pt: Vector3, ghost_radius: float = 0.38) -> bool:
	if not is_light_circle_active():
		return false
	if absf(from_pt.y - global_position.y) > 2.8 and absf(to_pt.y - global_position.y) > 2.8:
		return false
	var c := Vector2(global_position.x, global_position.z)
	var a := Vector2(from_pt.x, from_pt.z)
	var b := Vector2(to_pt.x, to_pt.z)
	var closest := Geometry2D.get_closest_point_to_segment(c, a, b)
	return c.distance_to(closest) <= (SAFETY_CIRCLE_RADIUS + ghost_radius)

## Blocks any ghost from entering or crossing the Circle of Safety edge.
## Returns {"blocked": bool, "position": Vector3} clamped outside the circle perimeter!
func block_ghost_at_circle_edge(from_pt: Vector3, candidate_pt: Vector3, ghost_radius: float = 0.38) -> Dictionary:
	if not is_light_circle_active() or absf(from_pt.y - global_position.y) > 2.8:
		return {"blocked": false, "position": candidate_pt}

	var c := Vector2(global_position.x, global_position.z)
	var a := Vector2(from_pt.x, from_pt.z)
	var b := Vector2(candidate_pt.x, candidate_pt.z)
	var boundary_r := SAFETY_CIRCLE_RADIUS + ghost_radius

	# Case 1: Ghost is already inside the circle (e.g. player wheeled the Light Cart toward the ghost) -> push ghost out to the circle edge!
	var dist_a := a.distance_to(c)
	if dist_a < boundary_r:
		var push_dir := (a - c).normalized() if dist_a > 0.001 else Vector2(1.0, 0.0)
		var edge_2d := c + push_dir * (boundary_r + 0.03)
		return {
			"blocked": true,
			"position": Vector3(edge_2d.x, from_pt.y, edge_2d.y)
		}

	# Case 2: Ghost's step from `a` to `b` would enter or cross the Circle of Safety edge -> stop at the circle perimeter!
	var closest := Geometry2D.get_closest_point_to_segment(c, a, b)
	if c.distance_to(closest) <= boundary_r:
		var halt_dir := (a - c).normalized()
		var safe_dist := maxf(dist_a, boundary_r + 0.02)
		var halted_2d := c + halt_dir * minf(dist_a, safe_dist)
		return {
			"blocked": true,
			"position": Vector3(halted_2d.x, from_pt.y, halted_2d.y)
		}

	return {"blocked": false, "position": candidate_pt}

func _physics_process(delta: float) -> void:
	if Engine.is_editor_hint():
		return
	if is_attached_to_player and is_instance_valid(attached_player):
		# Auto-detach if player died, sat down, or stepped onto a stepped escalator
		if attached_player.get("is_floating_essence") == true or attached_player.get("contextual_seat_active") == true:
			detach_from_player(true)
		elif absf(attached_player.global_position.y - global_position.y) > 0.45:
			detach_from_player(true)
			GameState.status_message.emit(
				"Stepped escalators cannot carry carts! Parked %s at the foot of the escalator (Repair the Floor 2 Service Freight Elevator to bring carts up)." % cart_type,
				Color(1.0, 0.78, 0.32)
			)
		else:
			var forward := -attached_player.global_transform.basis.z
			forward.y = 0.0
			forward = forward.normalized()
			var push_dist := 1.65
			var space_state := get_world_3d().direct_space_state if get_world_3d() else null
			if space_state:
				var ray_from := attached_player.global_position + Vector3(0.0, 0.55, 0.0)
				var ray_to := ray_from + forward * 2.15
				var query := PhysicsRayQueryParameters3D.create(ray_from, ray_to)
				if attached_player is CollisionObject3D:
					query.exclude = [(attached_player as CollisionObject3D).get_rid()]
				var hit := space_state.intersect_ray(query)
				if not hit.is_empty():
					var hit_dist := ray_from.distance_to(hit["position"])
					push_dist = clampf(hit_dist - 0.55, 0.75, 1.65)
			var target_pos := attached_player.global_position + forward * push_dist
			target_pos.y = global_position.y
			global_position = global_position.lerp(target_pos, clampf(delta * 12.0, 0.0, 1.0))
			look_at(global_position + forward, Vector3.UP)
			_evaluate_doorway_seal()

			if MultiplayerManager != null and MultiplayerManager.is_coop_active and multiplayer.has_multiplayer_peer():
				if not attached_player.has_method("is_local_player") or bool(attached_player.call("is_local_player")):
					_net_cart_sync_timer += delta
					if _net_cart_sync_timer >= 0.05:
						_net_cart_sync_timer = 0.0
						_rpc_sync_cart_transform.rpc(global_position, rotation.y)

	# Spin the 4 caster wheels proportionally to horizontal travel distance
	var horiz_delta := Vector2(global_position.x - _last_world_pos.x, global_position.z - _last_world_pos.z).length()
	_last_world_pos = global_position
	if horiz_delta > 0.0005 and not wheel_meshes.is_empty():
		var spin_rad := horiz_delta / 0.09
		for w in wheel_meshes:
			if is_instance_valid(w):
				w.rotate_object_local(Vector3.UP, spin_rad)

	if cart_type == "LightCart" and is_floodlight_on and battery_percent > 0.0:
		battery_percent = maxf(0.0, battery_percent - BATTERY_DRAIN_PER_SEC * delta)
		if spot_light:
			spot_light.light_energy = 8.5 * clampf(battery_percent / 20.0, 0.2, 1.0)
		_update_light_cart_active_visuals()
	elif cart_type == "PushCart":
		_update_push_cart_slot_outlines(delta)

@rpc("any_peer", "call_remote", "unreliable_ordered")
func _rpc_sync_cart_transform(pos: Vector3, yaw: float) -> void:
	global_position = pos
	rotation.y = yaw
	_evaluate_doorway_seal()

func _update_push_cart_slot_outlines(delta: float) -> void:
	_outline_pulse_time += delta * 5.5
	var pulse := 2.4 + 1.2 * sin(_outline_pulse_time)
	highlighted_place_slot = -1
	highlighted_take_slot = -1

	var near_player: Node3D = null
	var best_p_dist := 2.95
	for p in get_tree().get_nodes_in_group("players"):
		var p3d := p as Node3D
		if p3d and p3d.get("is_floating_essence") != true:
			if p3d.has_method("is_local_player") and not bool(p3d.call("is_local_player")):
				continue
			var d := global_position.distance_to(p3d.global_position)
			if d < best_p_dist:
				best_p_dist = d
				near_player = p3d

	if near_player != null:
		var held_item: Dictionary = near_player.call("get_current_active_item") if near_player.has_method("get_current_active_item") else {}
		var cam: Camera3D = near_player.get("main_camera") as Camera3D
		var ray_origin := cam.global_position if cam else (near_player.global_position + Vector3(0.0, 1.6, 0.0))
		var ray_dir := (-cam.global_transform.basis.z.normalized()) if cam else (-near_player.global_transform.basis.z.normalized())

		# 1. Find the open slot closest to where the player is looking (to show the [G] Placement Outline!)
		if not held_item.is_empty():
			var best_open_score := 999.0
			for i in range(PUSH_CART_SLOT_OFFSETS.size()):
				if cart_slot_items[i].is_empty():
					var slot_world := to_global(PUSH_CART_SLOT_OFFSETS[i] + Vector3(0.0, 0.08, 0.0))
					var to_slot := slot_world - ray_origin
					var perp_dist := to_slot.cross(ray_dir).length()
					if perp_dist < best_open_score:
						best_open_score = perp_dist
						highlighted_place_slot = i

		# 2. Find the occupied slot closest to where the player is looking (to allow taking with [E]!)
		var best_take_score := 999.0
		for i in range(PUSH_CART_SLOT_OFFSETS.size()):
			if not cart_slot_items[i].is_empty():
				var slot_world := to_global(PUSH_CART_SLOT_OFFSETS[i] + Vector3(0.0, 0.08, 0.0))
				var to_slot := slot_world - ray_origin
				var perp_dist := to_slot.cross(ray_dir).length()
				if perp_dist < best_take_score:
					best_take_score = perp_dist
					highlighted_take_slot = i

		var held_name: String = str(held_item.get("name", "Item"))
		for i in range(cart_slot_outlines.size()):
			if i == highlighted_place_slot:
				cart_slot_outlines[i].visible = true
				cart_slot_outline_mats[i].albedo_color = Color(0.22, 1.0, 0.78, 0.92)
				cart_slot_outline_mats[i].emission = Color(0.22, 1.0, 0.78)
				cart_slot_outline_mats[i].emission_energy_multiplier = pulse
				cart_slot_labels[i].visible = true
				cart_slot_labels[i].text = "[G] Place Here:\n%s (Slot #%d)" % [held_name, i + 1]
				cart_slot_labels[i].modulate = Color(0.35, 1.0, 0.80)
			elif i == highlighted_take_slot:
				var st_name: String = str(cart_slot_items[i].get("name", "Stored Tool"))
				cart_slot_outlines[i].visible = true
				cart_slot_outline_mats[i].albedo_color = Color(1.0, 0.86, 0.28, 0.85)
				cart_slot_outline_mats[i].emission = Color(1.0, 0.82, 0.22)
				cart_slot_outline_mats[i].emission_energy_multiplier = 1.8
				cart_slot_labels[i].visible = true
				cart_slot_labels[i].text = "[E] Take Off Cart:\n%s (Slot #%d)" % [st_name, i + 1]
				cart_slot_labels[i].modulate = Color(1.0, 0.90, 0.42)
			else:
				cart_slot_outlines[i].visible = false
				cart_slot_labels[i].visible = false
	else:
		for i in range(cart_slot_outlines.size()):
			cart_slot_outlines[i].visible = false
			cart_slot_labels[i].visible = false

func _evaluate_doorway_seal() -> void:
	if cart_type != "PushCart":
		return
	var dist_to_dock := Vector2(global_position.x, global_position.z).distance_to(Vector2(dock_world_position.x, dock_world_position.z))
	var dist_to_interior := Vector2(global_position.x, global_position.z).distance_to(Vector2(store_interior_center.x, store_interior_center.z))

	var new_state := DoorwaySealState.UNSEALED
	if dist_to_dock <= 2.1:
		new_state = DoorwaySealState.SEALED
	elif dist_to_interior <= 4.2:
		new_state = DoorwaySealState.DORMANT

	if new_state != current_seal_state:
		current_seal_state = new_state
		var label := get_seal_state_label()
		_refresh_push_cart_header_label()
		doorway_seal_changed.emit(linked_store_id, current_seal_state, label)
		GameState.status_message.emit("Store [%s] Doorway State -> %s" % [linked_store_id, label], Color(0.4, 1.0, 0.75))

func get_seal_state_label() -> String:
	match current_seal_state:
		DoorwaySealState.SEALED:
			return "SEALED (Ghost Confined to Store; Evidence Active)"
		DoorwaySealState.DORMANT:
			return "DORMANT (Cart Inside Store; Ghost Pacified)"
		_:
			return "UNSEALED (Use Light Cart Circle Edge to Block Exit!)"
