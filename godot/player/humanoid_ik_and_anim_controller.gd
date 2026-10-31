class_name HumanoidIKAndAnimController
extends Node3D

## Reusable Godot 4 Humanoid Animation & Real-Time In-Game IK Posing Controller.
## Supports:
##  1. Seamless looping & crossfading of embedded/retargeted animations (UAL1, UAL2, and Custom).
##  2. Multi-Bone Spine/Neck/Head Look-At IK tracking the player in real time.
##  3. Analytical Two-Bone Limb IK (Hand/Arm Reach IK & Foot Grounding IK) in Skeleton3D space.
##  4. Interactive Animation Cycling so players can compare UAL vs. Custom animations in-game.

@export var default_animation: String = "Custom_Chen_Idle"
@export var enable_look_at_ik: bool = true
@export var look_at_max_distance: float = 8.5
@export var look_at_max_yaw_deg: float = 68.0
@export var look_at_max_pitch_deg: float = 32.0
@export var look_at_smooth_speed: float = 6.0

@export var enable_right_hand_ik: bool = false
@export var right_hand_ik_target_world: Vector3 = Vector3.ZERO
@export var right_hand_ik_pole_world: Vector3 = Vector3.ZERO
@export var right_hand_ik_weight: float = 0.0

var anim_player: AnimationPlayer = null
var skeleton: Skeleton3D = null
var available_animations: Array[String] = []
var current_anim_index: int = 0

var _idx_spine3: int = -1
var _idx_neck: int = -1
var _idx_head: int = -1
var _idx_upperarm_r: int = -1
var _idx_lowerarm_r: int = -1
var _idx_hand_r: int = -1

var _cur_look_yaw: float = 0.0
var _cur_look_pitch: float = 0.0
var _cur_look_weight: float = 0.0

func _ready() -> void:
	_setup_references()

func _setup_references() -> void:
	var search_root: Node = get_parent() if get_parent() != null else self
	anim_player = _find_anim_player(search_root)
	skeleton = _find_skeleton(search_root)
	if anim_player == null:
		anim_player = _find_anim_player(self)
	if skeleton == null:
		skeleton = _find_skeleton(self)

	if skeleton:
		_idx_spine3 = _find_first_bone(skeleton, ["spine_03.x", "spine_03"])
		_idx_neck = _find_first_bone(skeleton, ["neck.x", "neck_01", "neck"])
		_idx_head = _find_first_bone(skeleton, ["head.x", "Head", "head"])
		_idx_upperarm_r = _find_first_bone(skeleton, ["arm.r", "upperarm_r"])
		_idx_lowerarm_r = _find_first_bone(skeleton, ["forearm.r", "lowerarm_r"])
		_idx_hand_r = _find_first_bone(skeleton, ["hand.r", "hand_r"])

	if anim_player:
		var raw_list := anim_player.get_animation_list()
		available_animations.clear()
		for anim_name in raw_list:
			if anim_name == "RESET":
				continue
			var anim_res := anim_player.get_animation(anim_name)
			if anim_res:
				anim_res.loop_mode = Animation.LOOP_LINEAR
			available_animations.append(anim_name)

		play_named_animation(default_animation, 0.0)

func play_named_animation(requested_name: String, blend_time: float = 0.30) -> String:
	if anim_player == null:
		_setup_references()
	if anim_player == null or available_animations.is_empty():
		return ""

	var stripped_req := requested_name.trim_suffix("_Loop").trim_suffix("_loop")
	var chosen := ""
	# Pass 1: Exact match or _Loop-stripped exact match
	for a_name in available_animations:
		var stripped_a := a_name.trim_suffix("_Loop").trim_suffix("_loop")
		if a_name == requested_name or stripped_a == stripped_req:
			chosen = a_name
			break
	# Pass 2: Prefix fallback if no exact match
	if chosen.is_empty():
		for a_name in available_animations:
			if a_name.begins_with(stripped_req):
				chosen = a_name
				break
	if chosen.is_empty():
		chosen = available_animations[0]

	current_anim_index = maxi(0, available_animations.find(chosen))
	anim_player.play(chosen, blend_time)
	return chosen

func cycle_next_animation() -> String:
	if available_animations.is_empty():
		return ""
	current_anim_index = (current_anim_index + 1) % available_animations.size()
	var next_anim := available_animations[current_anim_index]
	anim_player.play(next_anim, 0.30)
	return next_anim

func _process(delta: float) -> void:
	if anim_player == null or skeleton == null:
		_setup_references()
	if anim_player != null and not anim_player.is_playing() and not available_animations.is_empty():
		anim_player.play(available_animations[current_anim_index])
	if skeleton == null:
		return

	if enable_look_at_ik and _idx_head >= 0:
		_apply_look_at_ik(delta)

	if enable_right_hand_ik and right_hand_ik_weight > 0.001 and _idx_upperarm_r >= 0 and _idx_lowerarm_r >= 0 and _idx_hand_r >= 0:
		solve_two_bone_limb_ik(
			_idx_upperarm_r,
			_idx_lowerarm_r,
			_idx_hand_r,
			right_hand_ik_target_world,
			right_hand_ik_pole_world,
			right_hand_ik_weight
		)

## Layer 1: Multi-Bone Spine/Neck/Head Look-At IK in Skeleton3D Space
func _apply_look_at_ik(delta: float) -> void:
	var target_yaw := 0.0
	var target_pitch := 0.0
	var target_weight := 0.0

	var cam := get_viewport().get_camera_3d() if get_viewport() else null
	if cam:
		var head_global_in_skel := skeleton.get_bone_global_pose(_idx_head).origin
		var head_world := skeleton.to_global(head_global_in_skel)
		var player_eye_world := cam.global_position
		var dist := head_world.distance_to(player_eye_world)

		if dist < look_at_max_distance and dist > 0.25:
			# Express vector to player in Skeleton3D space (+Z = forward, +Y = up, +X = left)
			var local_target := skeleton.to_local(player_eye_world)
			var to_player := local_target - head_global_in_skel
			if to_player.z > -0.15: # Player is in front hemisphere
				var raw_yaw := atan2(to_player.x, maxf(to_player.z, 0.1))
				var horiz_dist := Vector2(to_player.x, to_player.z).length()
				var raw_pitch := atan2(to_player.y, maxf(horiz_dist, 0.1))

				var max_yaw := deg_to_rad(look_at_max_yaw_deg)
				var max_pitch := deg_to_rad(look_at_max_pitch_deg)
				if absf(raw_yaw) <= max_yaw * 1.25:
					target_yaw = clampf(raw_yaw, -max_yaw, max_yaw)
					target_pitch = clampf(raw_pitch, -max_pitch, max_pitch)
					target_weight = clampf(1.0 - (dist / look_at_max_distance) * 0.35, 0.0, 1.0)

	var t := clampf(delta * look_at_smooth_speed, 0.0, 1.0)
	_cur_look_yaw = lerpf(_cur_look_yaw, target_yaw, t)
	_cur_look_pitch = lerpf(_cur_look_pitch, target_pitch, t)
	_cur_look_weight = lerpf(_cur_look_weight, target_weight, t)

	if _cur_look_weight <= 0.002:
		return

	# Distribute Look-At rotation anatomically across Upper Chest (15%), Neck (35%), and Head (50%)
	if _idx_spine3 >= 0:
		_rotate_bone_in_skeleton_space(_idx_spine3, _cur_look_yaw * 0.15 * _cur_look_weight, _cur_look_pitch * 0.10 * _cur_look_weight)
	if _idx_neck >= 0:
		_rotate_bone_in_skeleton_space(_idx_neck, _cur_look_yaw * 0.35 * _cur_look_weight, _cur_look_pitch * 0.35 * _cur_look_weight)
	if _idx_head >= 0:
		_rotate_bone_in_skeleton_space(_idx_head, _cur_look_yaw * 0.50 * _cur_look_weight, _cur_look_pitch * 0.55 * _cur_look_weight)

func _rotate_bone_in_skeleton_space(bone_idx: int, yaw_rad: float, pitch_rad: float) -> void:
	var g_pose := skeleton.get_bone_global_pose(bone_idx)
	# In Skeleton3D space (+Z forward, +Y up, +X left):
	# Rotating around +Y turns horizontally; rotating around -X tilts up/down.
	var delta_basis := Basis(Vector3.UP, yaw_rad) * Basis(Vector3.RIGHT, -pitch_rad)
	var new_global_basis := (delta_basis * g_pose.basis).orthonormalized()

	var parent_idx := skeleton.get_bone_parent(bone_idx)
	var parent_global_basis := skeleton.get_bone_global_pose(parent_idx).basis if parent_idx >= 0 else Basis.IDENTITY
	var new_local_basis := (parent_global_basis.inverse() * new_global_basis).orthonormalized()
	skeleton.set_bone_pose_rotation(bone_idx, new_local_basis.get_rotation_quaternion())

## Layer 2: Analytical Law-of-Cosines Two-Bone Limb IK (e.g. upperarm -> lowerarm -> hand)
func solve_two_bone_limb_ik(
	upper_idx: int,
	lower_idx: int,
	end_idx: int,
	target_world: Vector3,
	pole_world: Vector3,
	weight: float
) -> void:
	var w := clampf(weight, 0.0, 1.0)
	if w <= 0.001:
		return

	var p_root := skeleton.get_bone_global_pose(upper_idx).origin
	var p_mid := skeleton.get_bone_global_pose(lower_idx).origin
	var p_end := skeleton.get_bone_global_pose(end_idx).origin
	var p_target := skeleton.to_local(target_world)
	var p_pole := skeleton.to_local(pole_world)

	var l1 := p_root.distance_to(p_mid)
	var l2 := p_mid.distance_to(p_end)
	if l1 < 0.001 or l2 < 0.001:
		return

	var to_target := p_target - p_root
	var dist := clampf(to_target.length(), absf(l1 - l2) + 0.01, (l1 + l2) - 0.005)
	var dir_target := to_target.normalized() if to_target.length() > 0.001 else Vector3.FORWARD

	# Law of Cosines for angle alpha at shoulder/hip between (p_end - p_root) and (p_mid - p_root)
	var cos_alpha := clampf((l1 * l1 + dist * dist - l2 * l2) / (2.0 * l1 * dist), -1.0, 1.0)
	var alpha := acos(cos_alpha)

	# Bend plane normal from pole vector
	var to_pole := (p_pole - p_root)
	var plane_normal := dir_target.cross(to_pole)
	if plane_normal.length_squared() < 0.0001:
		plane_normal = dir_target.cross(Vector3.UP)
	plane_normal = plane_normal.normalized()

	# Desired mid-joint (elbow/knee) position in Skeleton3D space
	var desired_mid_dir := (Basis(plane_normal, -alpha) * dir_target).normalized()
	var desired_mid := p_root + desired_mid_dir * l1
	var desired_end := p_root + dir_target * dist

	# 1. Rotate upper bone so (p_mid - p_root) aligns with (desired_mid - p_root)
	var cur_upper_vec := (p_mid - p_root).normalized()
	var rot_upper := Quaternion(cur_upper_vec, desired_mid_dir)
	var g_upper := skeleton.get_bone_global_pose(upper_idx)
	var target_upper_basis := (Basis(rot_upper) * g_upper.basis).orthonormalized()
	var blended_upper_basis := Basis(g_upper.basis.get_rotation_quaternion().slerp(target_upper_basis.get_rotation_quaternion(), w))

	var p_upper_parent := skeleton.get_bone_parent(upper_idx)
	var b_upper_parent := skeleton.get_bone_global_pose(p_upper_parent).basis if p_upper_parent >= 0 else Basis.IDENTITY
	skeleton.set_bone_pose_rotation(upper_idx, (b_upper_parent.inverse() * blended_upper_basis).get_rotation_quaternion())

	# 2. Rotate lower bone so (p_end - p_mid) aligns with (desired_end - desired_mid)
	var g_lower := skeleton.get_bone_global_pose(lower_idx)
	var p_mid_now := g_lower.origin
	var p_end_now := skeleton.get_bone_global_pose(end_idx).origin
	var cur_lower_vec := (p_end_now - p_mid_now).normalized()
	var desired_lower_dir := (desired_end - desired_mid).normalized()
	var rot_lower := Quaternion(cur_lower_vec, desired_lower_dir)
	var target_lower_basis := (Basis(rot_lower) * g_lower.basis).orthonormalized()
	var blended_lower_basis := Basis(g_lower.basis.get_rotation_quaternion().slerp(target_lower_basis.get_rotation_quaternion(), w))

	skeleton.set_bone_pose_rotation(lower_idx, (blended_upper_basis.inverse() * blended_lower_basis).get_rotation_quaternion())

static func _find_anim_player(node: Node) -> AnimationPlayer:
	if node is AnimationPlayer:
		return node as AnimationPlayer
	for c in node.get_children():
		var found := _find_anim_player(c)
		if found:
			return found
	return null

static func _find_first_bone(skel: Skeleton3D, names: Array[String]) -> int:
	for bname in names:
		var idx := skel.find_bone(bname)
		if idx >= 0:
			return idx
	return -1

static func _find_skeleton(node: Node) -> Skeleton3D:
	if node is Skeleton3D:
		return node as Skeleton3D
	for c in node.get_children():
		var found := _find_skeleton(c)
		if found:
			return found
	return null
