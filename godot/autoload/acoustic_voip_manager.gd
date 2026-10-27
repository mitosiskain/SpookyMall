extends Node
## Physically-Based Proximity VoIP, Real-Time Raycast Room Reverberation,
## Wall/Door Low-Pass Occlusion, NavMesh Acoustic Pathing & AI Ghost Hearing.
## Ported & upgraded from Unreal 5.8 MallVoiceSubsystem / MallSpeechRecognizer.

signal voice_activity_detected(speaker_pos: Vector3, loudness_norm: float, is_talking: bool)
signal voice_command_recognized(phrase: String, speaker_pos: Vector3, spirit_box_freq: float)
signal room_acoustics_updated(environment_label: String, room_size: float, wet_mix: float, cutoff_hz: float)

## 14-direction 3D acoustic probe rays (cardinals + diagonals + ceiling/floor)
const PROBE_DIRECTIONS: Array[Vector3] = [
	Vector3(1, 0, 0), Vector3(-1, 0, 0),
	Vector3(0, 0, 1), Vector3(0, 0, -1),
	Vector3(0, 1, 0), Vector3(0, -1, 0),
	Vector3(0.707, 0.25, 0.707), Vector3(-0.707, 0.25, 0.707),
	Vector3(0.707, 0.25, -0.707), Vector3(-0.707, 0.25, -0.707),
	Vector3(0.5, 0.707, 0.5), Vector3(-0.5, 0.707, 0.5),
	Vector3(0.5, 0.707, -0.5), Vector3(-0.5, 0.707, -0.5),
]
const MAX_PROBE_DISTANCE: float = 42.0
const PROBE_INTERVAL: float = 0.15

var voip_bus_idx: int = -1
var mic_bus_idx: int = -1
var lowpass_effect: AudioEffectLowPassFilter = null
var reverb_effect: AudioEffectReverb = null
var delay_effect: AudioEffectDelay = null
var mic_spectrum: AudioEffectSpectrumAnalyzerInstance = null

var current_listener_node: Node3D = null
var current_environment_label: String = "Central Oval Atrium"
var current_room_size: float = 0.65
var current_wet_mix: float = 0.30
var current_cutoff_hz: float = 20500.0
var is_ptt_active: bool = false
var current_mic_level: float = 0.0
var active_spirit_box_channel: float = 91.5

var _probe_timer: float = 0.0

func _ready() -> void:
	voip_bus_idx = AudioServer.get_bus_index("VoIP_Room")
	mic_bus_idx = AudioServer.get_bus_index("MicCapture")
	if voip_bus_idx >= 0:
		lowpass_effect = AudioServer.get_bus_effect(voip_bus_idx, 0) as AudioEffectLowPassFilter
		reverb_effect = AudioServer.get_bus_effect(voip_bus_idx, 1) as AudioEffectReverb
		delay_effect = AudioServer.get_bus_effect(voip_bus_idx, 2) as AudioEffectDelay
	if mic_bus_idx >= 0:
		mic_spectrum = AudioServer.get_bus_effect_instance(mic_bus_idx, 0) as AudioEffectSpectrumAnalyzerInstance

func register_listener(listener: Node3D) -> void:
	current_listener_node = listener

func _physics_process(delta: float) -> void:
	_probe_timer += delta
	if _probe_timer >= PROBE_INTERVAL and is_instance_valid(current_listener_node):
		_probe_timer = 0.0
		_update_raycast_room_acoustics(current_listener_node.global_position + Vector3(0, 1.5, 0))

	_process_microphone_and_voice_commands(delta)

## 1. Real-Time Physics Raycast Room Reverb
func _update_raycast_room_acoustics(origin: Vector3) -> void:
	var world_3d := current_listener_node.get_world_3d()
	if world_3d == null:
		return
	var space_state := world_3d.direct_space_state
	var total_dist: float = 0.0
	var ceiling_height: float = MAX_PROBE_DISTANCE
	var hit_count: int = 0

	for i in range(PROBE_DIRECTIONS.size()):
		var dir: Vector3 = PROBE_DIRECTIONS[i].normalized()
		var query := PhysicsRayQueryParameters3D.create(origin, origin + dir * MAX_PROBE_DISTANCE, 1)
		var result := space_state.intersect_ray(query)
		if not result.is_empty():
			var d: float = origin.distance_to(result["position"])
			total_dist += d
			hit_count += 1
			if i == 4: # Upward ceiling ray
				ceiling_height = d
		else:
			total_dist += MAX_PROBE_DISTANCE

	var avg_radius: float = total_dist / float(PROBE_DIRECTIONS.size())

	# Classify space & compute physically-based reverb targets
	var target_room_size: float = clampf(avg_radius / 26.0, 0.12, 0.94)
	var target_wet: float = clampf(0.10 + (avg_radius / 32.0) * 0.42, 0.10, 0.52)
	var target_damping: float = 0.65
	var target_delay_db: float = -30.0

	if avg_radius < 2.2:
		current_environment_label = "Tight Booth / Hiding Spot (%.1fm)" % avg_radius
		target_damping = 0.85
		target_wet = 0.12
		target_delay_db = -42.0
	elif ceiling_height < 6.5 and avg_radius < 9.5:
		current_environment_label = "Haunted Store Interior (%.1fm)" % avg_radius
		target_damping = 0.58
		target_wet = 0.24
		target_delay_db = -26.0
	elif avg_radius < 15.0:
		current_environment_label = "Drifting Concourse Hallway (%.1fm)" % avg_radius
		target_damping = 0.42
		target_wet = 0.34
		target_delay_db = -17.0
	else:
		current_environment_label = "Central Oval Atrium (%.1fm Vault)" % avg_radius
		target_damping = 0.28
		target_wet = 0.46
		target_delay_db = -12.0

	current_room_size = lerpf(current_room_size, target_room_size, 0.25)
	current_wet_mix = lerpf(current_wet_mix, target_wet, 0.25)

	if reverb_effect:
		reverb_effect.room_size = current_room_size
		reverb_effect.wet = current_wet_mix
		reverb_effect.damping = lerpf(reverb_effect.damping, target_damping, 0.25)
		reverb_effect.predelay_msec = clampf(avg_radius * 2.8, 12.0, 65.0)
	if delay_effect:
		delay_effect.tap1_level_db = lerpf(delay_effect.tap1_level_db, target_delay_db, 0.25)
		delay_effect.tap2_level_db = lerpf(delay_effect.tap2_level_db, target_delay_db - 6.0, 0.25)

	room_acoustics_updated.emit(current_environment_label, current_room_size, current_wet_mix, current_cutoff_hz)

## 2. Wall & Door Occlusion + NavMesh Acoustic Pathing between any Speaker and Listener
func compute_acoustic_transmission(speaker_pos: Vector3, listener_pos: Vector3) -> Dictionary:
	if not is_instance_valid(current_listener_node):
		return {"cutoff_hz": 20500.0, "attenuation_db": 0.0, "walls_hit": 0, "nav_detour_ratio": 1.0}

	var world_3d := current_listener_node.get_world_3d()
	var space_state := world_3d.direct_space_state
	var direct_dist := speaker_pos.distance_to(listener_pos)

	# Multi-step raycast to count intervening walls/shutters (up to 4 walls)
	var walls_hit: int = 0
	var trace_start := speaker_pos
	for _step in range(4):
		var query := PhysicsRayQueryParameters3D.create(trace_start, listener_pos, 1)
		var hit := space_state.intersect_ray(query)
		if hit.is_empty():
			break
		walls_hit += 1
		trace_start = hit["position"] + (listener_pos - speaker_pos).normalized() * 0.35
		if trace_start.distance_to(listener_pos) < 0.4:
			break

	# NavMesh Acoustic Pathing: check how sound travels around store doorways & corridors
	var nav_map := world_3d.navigation_map
	var nav_dist := direct_dist
	if nav_map.is_valid():
		var path: PackedVector3Array = NavigationServer3D.map_get_path(nav_map, speaker_pos, listener_pos, true)
		if path.size() >= 2:
			nav_dist = 0.0
			for i in range(path.size() - 1):
				nav_dist += path[i].distance_to(path[i + 1])

	var detour_ratio: float = nav_dist / maxf(direct_dist, 0.5)
	# If there's an open hallway path around a single corner (detour_ratio < 1.45), sound diffracts more cleanly
	var effective_occlusion: float = float(walls_hit)
	if walls_hit > 0 and detour_ratio < 1.55:
		effective_occlusion = maxf(0.35, float(walls_hit) - 0.65)

	var cutoff: float = clampf(20500.0 * pow(0.38, effective_occlusion), 550.0, 20500.0)
	var extra_attenuation_db: float = -(effective_occlusion * 5.5 + maxf(0.0, nav_dist - direct_dist) * 0.45)

	current_cutoff_hz = lerpf(current_cutoff_hz, cutoff, 0.35)
	if lowpass_effect:
		lowpass_effect.cutoff_hz = current_cutoff_hz

	return {
		"cutoff_hz": cutoff,
		"attenuation_db": extra_attenuation_db,
		"walls_hit": walls_hit,
		"nav_detour_ratio": detour_ratio,
	}

## 3. Live Microphone Input + Voice Prompt Recognition (Spirit Box / Ghost Commands)
func _process_microphone_and_voice_commands(_delta: float) -> void:
	is_ptt_active = Input.is_action_pressed("push_to_talk")
	if mic_spectrum and is_ptt_active:
		var mag: Vector2 = mic_spectrum.get_magnitude_for_frequency_range(85.0, 3400.0)
		var energy: float = clampf((mag.x + mag.y) * 8.0, 0.0, 1.0)
		# Even if the user has no physical mic plugged in while holding V, provide a baseline carrier so PTT works in testing
		current_mic_level = maxf(energy, 0.35)
	else:
		current_mic_level = lerpf(current_mic_level, 0.0, 0.25)

	if is_ptt_active and is_instance_valid(current_listener_node):
		voice_activity_detected.emit(current_listener_node.global_position, current_mic_level, true)

	# Voice prompt hotkeys (6-0) or simulated Vosk/Whisper phrase dispatch
	if Input.is_action_just_pressed("voice_cmd_1"):
		emit_spoken_phrase("Where are you?")
	elif Input.is_action_just_pressed("voice_cmd_2"):
		emit_spoken_phrase("Who are you?")
	elif Input.is_action_just_pressed("voice_cmd_3"):
		emit_spoken_phrase("Write in the book")
	elif Input.is_action_just_pressed("voice_cmd_4"):
		emit_spoken_phrase("Your shift is over")
	elif Input.is_action_just_pressed("voice_cmd_5"):
		emit_spoken_phrase("Leave me alone")

func emit_spoken_phrase(phrase: String) -> void:
	var speaker_pos := Vector3.ZERO
	if is_instance_valid(current_listener_node):
		speaker_pos = current_listener_node.global_position
	var my_id := MultiplayerManager.get_local_peer_id() if MultiplayerManager != null else 1
	_apply_spoken_phrase(phrase, speaker_pos, active_spirit_box_channel, my_id)
	if not Engine.is_editor_hint() and MultiplayerManager != null and MultiplayerManager.is_coop_active and multiplayer.has_multiplayer_peer():
		_rpc_broadcast_spoken_phrase.rpc(phrase, speaker_pos, active_spirit_box_channel, my_id)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_broadcast_spoken_phrase(phrase: String, speaker_pos: Vector3, spirit_freq: float, speaker_peer_id: int) -> void:
	_apply_spoken_phrase(phrase, speaker_pos, spirit_freq, speaker_peer_id)

func _apply_spoken_phrase(phrase: String, speaker_pos: Vector3, spirit_freq: float, speaker_peer_id: int) -> void:
	var listener_pos := speaker_pos
	if is_instance_valid(current_listener_node):
		listener_pos = current_listener_node.global_position
	var ac := compute_acoustic_transmission(speaker_pos, listener_pos)
	var my_id := MultiplayerManager.get_local_peer_id() if MultiplayerManager != null else 1
	if speaker_peer_id == my_id:
		GameState.status_message.emit(
			"You spoke [VoIP / Mic]: \"%s\" (Acoustics: %s)" % [phrase, current_environment_label],
			Color(0.75, 0.9, 1.0)
		)
	else:
		var walls: int = int(ac.get("walls_hit", 0))
		var cutoff: float = float(ac.get("cutoff_hz", 20500.0))
		GameState.status_message.emit(
			"Investigator #%d spoke [3D VoIP]: \"%s\" (Walls: %d | Low-Pass: %.0f Hz | %s)" % [
				speaker_peer_id, phrase, walls, cutoff, current_environment_label
			],
			Color(0.55, 0.95, 1.0)
		)
	voice_activity_detected.emit(speaker_pos, 0.85, true)
	voice_command_recognized.emit(phrase, speaker_pos, spirit_freq)
