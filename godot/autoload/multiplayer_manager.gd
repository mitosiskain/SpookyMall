extends Node
## Co-op Multiplayer Manager (ENetMultiplayerPeer) for Megamall Anomaly.
## Supports Hosting/Joining Co-op Investigations on Arch Linux & Windows,
## CLI auto-launch flags (--host, --join=<ip>, --port=<port>), and in-game hotkeys ([F8] Host / [F9] Join).

signal peer_joined(peer_id: int)
signal peer_left(peer_id: int)
signal peer_list_updated(peers: Array[int])
signal local_role_changed(is_host: bool, my_peer_id: int)
signal network_status_changed(status_text: String)

const DEFAULT_PORT: int = 7777
const MAX_PLAYERS: int = 4

var is_coop_active: bool = false
var connected_peers: Array[int] = [1]
var current_port: int = DEFAULT_PORT
var target_join_address: String = "127.0.0.1"

func _ready() -> void:
	multiplayer.peer_connected.connect(_on_peer_connected)
	multiplayer.peer_disconnected.connect(_on_peer_disconnected)
	multiplayer.connected_to_server.connect(_on_connected_to_server)
	multiplayer.connection_failed.connect(_on_connection_failed)
	multiplayer.server_disconnected.connect(_on_server_disconnected)

	_setup_coop_hotkeys()
	call_deferred("_parse_cli_multiplayer_args")

func _setup_coop_hotkeys() -> void:
	var bindings := {
		"coop_host_quick": [KEY_F8],
		"coop_join_quick": [KEY_F9],
		"toggle_coop_menu": [KEY_P],
	}
	for action in bindings.keys():
		if not InputMap.has_action(action):
			InputMap.add_action(action)
		for keycode in bindings[action]:
			var ev := InputEventKey.new()
			ev.physical_keycode = keycode
			if not InputMap.action_has_event(action, ev):
				InputMap.action_add_event(action, ev)

func _parse_cli_multiplayer_args() -> void:
	var all_args := OS.get_cmdline_args() + OS.get_cmdline_user_args()
	var auto_host := false
	var auto_join := false
	var coop_verify := false
	var join_ip := "127.0.0.1"
	var port := DEFAULT_PORT

	for i in range(all_args.size()):
		var arg: String = all_args[i]
		if arg == "--host" or arg == "--coop-host":
			auto_host = true
		elif arg == "--join" or arg == "--coop-join":
			auto_join = true
			if i + 1 < all_args.size() and not all_args[i + 1].begins_with("-"):
				join_ip = all_args[i + 1]
		elif arg.begins_with("--join="):
			auto_join = true
			join_ip = arg.substr("--join=".length())
		elif arg.begins_with("--port="):
			port = int(arg.substr("--port=".length()))
		elif arg == "--coop-verify":
			coop_verify = true

	if auto_host:
		host_coop_session(port)
	elif auto_join:
		# Brief delay so if --host and --join are launched simultaneously by a script, the server socket binds first
		await get_tree().create_timer(0.45).timeout
		join_coop_session(join_ip, port)

	if coop_verify:
		_run_coop_verification_sequence(auto_join)

func _run_coop_verification_sequence(is_client_instance: bool) -> void:
	var role := "CLIENT" if is_client_instance else "HOST"
	await get_tree().create_timer(1.2).timeout
	var world := get_tree().current_scene
	print("[%s] Connected Peers: %s | Local Peer ID: %d" % [role, str(connected_peers), get_local_peer_id()])
	if world and "players_by_peer" in world:
		print("[%s] Spawned Investigator Avatars: %s" % [role, str(world.players_by_peer.keys())])

	if is_client_instance and world:
		var my_p := world.get("player") as Node3D
		if my_p:
			my_p.global_position = Vector3(18.0, 0.1, 0.0)
			if my_p.has_method("speak_voice_prompt"):
				my_p.call("speak_voice_prompt", "Where are you?")
		GameState.record_evidence("East_F0_02", "EMF-5", 35)
		var lc := world.get("light_cart") as Node3D
		if lc and lc.has_method("toggle_floodlight"):
			lc.call("toggle_floodlight", "Client Co-op Verification")
		await get_tree().create_timer(0.8).timeout
		print("[CLIENT] Synced Team Cash: $%d | Light Cart Floodlight ON: %s" % [
			GameState.team_cash, str(lc.get("is_floodlight_on")) if lc else "N/A"
		])
	elif world:
		await get_tree().create_timer(1.0).timeout
		var lc := world.get("light_cart") as Node3D
		print("[HOST] Synced Team Cash: $%d | Evidence East_F0_02: %s | Light Cart Floodlight ON: %s" % [
			GameState.team_cash, str(GameState.store_evidence_ledger.get("East_F0_02", [])), str(lc.get("is_floodlight_on")) if lc else "N/A"
		])
		var p_dict: Dictionary = world.get("players_by_peer")
		for pid in p_dict.keys():
			var av := p_dict[pid] as Node3D
			if av:
				print("[HOST] Avatar Peer #%d Pos: %s | Local: %s | 3D Character Mesh Visible: %s" % [
					pid, str(av.global_position), str(av.call("is_local_player")), str(av.get("character_visual_root").visible)
				])
	get_tree().quit(0)

func host_coop_session(port: int = DEFAULT_PORT) -> Error:
	if is_coop_active:
		disconnect_coop_session()
	current_port = port
	var peer := ENetMultiplayerPeer.new()
	var err := peer.create_server(port, MAX_PLAYERS)
	if err != OK:
		GameState.status_message.emit("Failed to host ENet server on UDP port %d (Error %d — is another Host window already running?)" % [port, err], Color(1.0, 0.35, 0.35))
		return err
	multiplayer.multiplayer_peer = peer
	is_coop_active = true
	connected_peers = [1]
	var msg := "HOSTING Co-op Server on UDP Port %d (Peer #1 | Press [F9] in 2nd Window to Join)" % port
	network_status_changed.emit(msg)
	local_role_changed.emit(true, 1)
	peer_list_updated.emit(connected_peers)
	GameState.status_message.emit(msg, Color(0.4, 0.95, 1.0))
	return OK

func join_coop_session(address: String = "127.0.0.1", port: int = DEFAULT_PORT) -> Error:
	if is_coop_active:
		disconnect_coop_session()
	target_join_address = address.strip_edges()
	if target_join_address.is_empty():
		target_join_address = "127.0.0.1"
	current_port = port
	var peer := ENetMultiplayerPeer.new()
	var err := peer.create_client(target_join_address, port)
	if err != OK:
		GameState.status_message.emit("Failed to create ENet client for %s:%d (Error %d)" % [target_join_address, port, err], Color(1.0, 0.35, 0.35))
		return err
	multiplayer.multiplayer_peer = peer
	is_coop_active = true
	var msg := "Joining Co-op Host at %s:%d..." % [target_join_address, port]
	network_status_changed.emit(msg)
	GameState.status_message.emit(msg, Color(0.95, 0.85, 0.40))
	return OK

func disconnect_coop_session() -> void:
	if multiplayer.multiplayer_peer:
		multiplayer.multiplayer_peer.close()
	multiplayer.multiplayer_peer = OfflineMultiplayerPeer.new()
	is_coop_active = false
	connected_peers = [1]
	network_status_changed.emit("Solo / Offline Mode ([F8] Host | [F9] Join | [P] Co-op Menu)")
	local_role_changed.emit(true, 1)
	peer_list_updated.emit(connected_peers)

func is_host() -> bool:
	if not is_coop_active or multiplayer.multiplayer_peer == null:
		return true
	return multiplayer.is_server()

func get_local_peer_id() -> int:
	if not is_coop_active or multiplayer.multiplayer_peer == null:
		return 1
	return multiplayer.get_unique_id()

func _on_peer_connected(id: int) -> void:
	if id not in connected_peers:
		connected_peers.append(id)
		connected_peers.sort()
	peer_joined.emit(id)
	peer_list_updated.emit(connected_peers)
	GameState.status_message.emit("Investigator #%d connected to the Megamall airlock! (%d/%d Players)" % [id, connected_peers.size(), MAX_PLAYERS], Color(0.4, 1.0, 0.65))

	# If we are the Host, push the authoritative peer list and GameState snapshot to the newly connected peer
	if multiplayer.is_server():
		_rpc_sync_session_state.rpc_id(
			id,
			connected_peers,
			GameState.team_cash,
			GameState.total_death_tally,
			GameState.sauna_soul_anchor_lit,
			GameState.powered_circuits,
			GameState.freight_elevator_unlocked_floors,
			GameState.store_evidence_ledger,
			GameState.resolved_stores,
			bool(GameState.ghost_profiles["West_F1_03"].get("is_corrupted_hostile", true)),
			bool(GameState.ghost_profiles["East_F2_01"].get("is_corrupted_hostile", false))
		)

func _on_peer_disconnected(id: int) -> void:
	connected_peers.erase(id)
	peer_left.emit(id)
	peer_list_updated.emit(connected_peers)
	GameState.status_message.emit("Investigator #%d disconnected from Co-op session." % id, Color(1.0, 0.6, 0.35))

func _on_connected_to_server() -> void:
	var my_id := multiplayer.get_unique_id()
	if 1 not in connected_peers:
		connected_peers.append(1)
	if my_id not in connected_peers:
		connected_peers.append(my_id)
	connected_peers.sort()
	var msg := "CONNECTED to Co-op Host %s:%d as Investigator #%d!" % [target_join_address, current_port, my_id]
	network_status_changed.emit(msg)
	local_role_changed.emit(false, my_id)
	peer_list_updated.emit(connected_peers)
	GameState.status_message.emit(msg, Color(0.35, 1.0, 0.68))

func _on_connection_failed() -> void:
	disconnect_coop_session()
	GameState.status_message.emit("Co-op connection to %s:%d failed! Make sure Host pressed [F8] first." % [target_join_address, current_port], Color(1.0, 0.4, 0.4))

func _on_server_disconnected() -> void:
	disconnect_coop_session()
	GameState.status_message.emit("Co-op Host closed the server — returned to Solo Mode.", Color(1.0, 0.55, 0.35))

@rpc("authority", "call_remote", "reliable")
func _rpc_sync_session_state(
	peers: Array,
	cash: int,
	deaths: int,
	sauna_lit: bool,
	circuits: Dictionary,
	elevators: Dictionary,
	ledger: Dictionary,
	resolved: Dictionary,
	west_corrupted: bool,
	f2_corrupted: bool
) -> void:
	connected_peers.clear()
	for p in peers:
		connected_peers.append(int(p))
	connected_peers.sort()

	GameState.team_cash = cash
	GameState.total_death_tally = deaths
	GameState.sauna_soul_anchor_lit = sauna_lit
	GameState.powered_circuits = circuits.duplicate(true)
	GameState.freight_elevator_unlocked_floors = elevators.duplicate(true)
	GameState.store_evidence_ledger = ledger.duplicate(true)
	GameState.resolved_stores = resolved.duplicate(true)
	GameState.ghost_profiles["West_F1_03"]["is_corrupted_hostile"] = west_corrupted
	GameState.ghost_profiles["East_F2_01"]["is_corrupted_hostile"] = f2_corrupted

	peer_list_updated.emit(connected_peers)
	for pid in connected_peers:
		peer_joined.emit(pid)
