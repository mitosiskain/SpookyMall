extends Node
## Co-op Multiplayer Manager (ENetMultiplayerPeer + Native UPnP + Invite Codes) for Megamall Anomaly.
## Supports:
## - 1-Click Shareable Invite Codes (e.g. "MALL-XXXX-XXXX" or "host:port") so testers never deal with raw IPs
## - Automatic Router Port Forwarding via Godot 4's built-in UPNP class (runs on a background thread)
## - Local Split-Screen (127.0.0.1), LAN, playit.gg tunnels (host:port), and Tailscale (100.x.y.z)
## - CLI auto-launch flags (--host, --join=<code_or_ip>, --port=<port>) & in-game hotkeys ([F8] Host / [F9] Join / [P] Lobby).

signal peer_joined(peer_id: int)
signal peer_left(peer_id: int)
signal peer_list_updated(peers: Array[int])
signal local_role_changed(is_host: bool, my_peer_id: int)
signal network_status_changed(status_text: String)
signal invite_code_updated(invite_code: String, detail_text: String)

const DEFAULT_PORT: int = 7777
const MAX_PLAYERS: int = 4

var is_coop_active: bool = false
var connected_peers: Array[int] = [1]
var current_port: int = DEFAULT_PORT
var target_join_address: String = "127.0.0.1"
var active_invite_code: String = "MALL-7F00-0001"
var lan_invite_code: String = "MALL-7F00-0001"
var upnp_status_text: String = "UPnP Idle (Click 'Auto-Forward Port via UPnP' in [P] Menu for Internet Hosting)"

var _upnp_thread: Thread = null
var _upnp_mapped_port: int = 0

func _ready() -> void:
	multiplayer.peer_connected.connect(_on_peer_connected)
	multiplayer.peer_disconnected.connect(_on_peer_disconnected)
	multiplayer.connected_to_server.connect(_on_connected_to_server)
	multiplayer.connection_failed.connect(_on_connection_failed)
	multiplayer.server_disconnected.connect(_on_server_disconnected)

	_detect_local_and_lan_codes()
	_setup_coop_hotkeys()
	call_deferred("_parse_cli_multiplayer_args")

func _exit_tree() -> void:
	if _upnp_thread and _upnp_thread.is_started():
		_upnp_thread.wait_to_finish()

func _detect_local_and_lan_codes() -> void:
	var best_ip := "127.0.0.1"
	for addr in IP.get_local_addresses():
		if ":" in addr or addr == "127.0.0.1":
			continue
		if addr.begins_with("100."): # Tailscale IP takes priority if installed
			best_ip = addr
			break
		elif addr.begins_with("192.168.") or addr.begins_with("10.") or addr.begins_with("172."):
			best_ip = addr
	lan_invite_code = encode_invite_code(best_ip, current_port)
	active_invite_code = lan_invite_code

## Encodes an IPv4 address + port into a clean shareable Invite Code (e.g. "MALL-C0A8-0132" or "MALL-93B9-DD14-D431").
static func encode_invite_code(ip: String, port: int = DEFAULT_PORT) -> String:
	var parts := ip.strip_edges().split(".")
	if parts.size() == 4:
		var valid := true
		var b: Array[int] = []
		for p in parts:
			if not p.is_valid_int():
				valid = false
				break
			var v := int(p)
			if v < 0 or v > 255:
				valid = false
				break
			b.append(v)
		if valid:
			if port == DEFAULT_PORT:
				return "MALL-%02X%02X-%02X%02X" % [b[0], b[1], b[2], b[3]]
			else:
				return "MALL-%02X%02X-%02X%02X-%04X" % [b[0], b[1], b[2], b[3], clampi(port, 1, 65535)]
	return "%s:%d" % [ip.strip_edges(), port]

## Decodes a "MALL-XXXX-XXXX" Invite Code, a "host:port" tunnel string (e.g. playit.gg), or a plain IP address!
static func decode_invite_code_or_address(raw_input: String, fallback_port: int = DEFAULT_PORT) -> Dictionary:
	var cleaned := raw_input.strip_edges()
	if cleaned.is_empty():
		return {"ip": "127.0.0.1", "port": fallback_port}

	var upper := cleaned.to_upper()
	if upper.begins_with("MALL-"):
		var tokens := upper.split("-")
		if tokens.size() >= 3 and tokens[1].length() == 4 and tokens[2].length() == 4:
			var hi := tokens[1].hex_to_int()
			var lo := tokens[2].hex_to_int()
			var b0 := (hi >> 8) & 0xFF
			var b1 := hi & 0xFF
			var b2 := (lo >> 8) & 0xFF
			var b3 := lo & 0xFF
			var decoded_port := fallback_port
			if tokens.size() >= 4 and tokens[3].length() > 0:
				decoded_port = tokens[3].hex_to_int()
			return {
				"ip": "%d.%d.%d.%d" % [b0, b1, b2, b3],
				"port": decoded_port,
			}

	# Support "host:port" (such as playit.gg tunnel addresses like "147.185.221.20:54321" or "tunnel.playit.gg:54321")
	if ":" in cleaned:
		var colon_idx := cleaned.rfind(":")
		var host_part := cleaned.left(colon_idx).strip_edges()
		var port_part := cleaned.substr(colon_idx + 1).strip_edges()
		if port_part.is_valid_int():
			return {"ip": host_part, "port": int(port_part)}

	return {"ip": cleaned, "port": fallback_port}

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
		host_coop_session(port, false)
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
	print("[%s] Connected Peers: %s | Local Peer ID: %d | Invite Code: %s" % [role, str(connected_peers), get_local_peer_id(), active_invite_code])
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

func host_coop_session(port: int = DEFAULT_PORT, try_upnp_auto_forward: bool = false) -> Error:
	if is_coop_active:
		disconnect_coop_session()
	current_port = port
	_detect_local_and_lan_codes()
	var peer := ENetMultiplayerPeer.new()
	var err := peer.create_server(port, MAX_PLAYERS)
	if err != OK:
		GameState.status_message.emit("Failed to host ENet server on UDP port %d (Error %d — is another Host window already running?)" % [port, err], Color(1.0, 0.35, 0.35))
		return err
	multiplayer.multiplayer_peer = peer
	is_coop_active = true
	connected_peers = [1]
	var msg := "HOSTING Co-op Server (UDP :%d) | Invite Code: %s (Press [P] to Copy Code or Enable UPnP)" % [port, active_invite_code]
	network_status_changed.emit(msg)
	invite_code_updated.emit(active_invite_code, upnp_status_text)
	local_role_changed.emit(true, 1)
	peer_list_updated.emit(connected_peers)
	GameState.status_message.emit(msg, Color(0.4, 0.95, 1.0))

	if try_upnp_auto_forward:
		start_upnp_port_forwarding(port)
	return OK

## Automatically opens UDP `port` on the Host's router via Godot 4's built-in UPNP class (on a background thread!)
## and generates a public Internet Invite Code (`MALL-XXXX-XXXX`) that testers can paste directly into `[P]`.
func start_upnp_port_forwarding(port: int = DEFAULT_PORT) -> void:
	if _upnp_thread and _upnp_thread.is_started():
		return
	upnp_status_text = "UPnP: Discovering router gateway for UDP port %d..." % port
	invite_code_updated.emit(active_invite_code, upnp_status_text)
	GameState.status_message.emit(upnp_status_text, Color(0.95, 0.88, 0.42))
	_upnp_thread = Thread.new()
	_upnp_thread.start(_thread_run_upnp.bind(port))

func _thread_run_upnp(port: int) -> void:
	var upnp := UPNP.new()
	var discover_err := upnp.discover(2000, 2, "InternetGatewayDevice")
	if discover_err != UPNP.UPNP_RESULT_SUCCESS or upnp.get_gateway() == null or not upnp.get_gateway().is_valid_gateway():
		call_deferred("_on_upnp_completed", false, "", port, "Router UPnP disabled or unavailable — use LAN Code (%s) or free playit.gg tunnel." % lan_invite_code)
		return

	var map_err := upnp.add_port_mapping(port, port, "MegamallAnomaly_Coop", "UDP", 0)
	if map_err != UPNP.UPNP_RESULT_SUCCESS:
		call_deferred("_on_upnp_completed", false, "", port, "UPnP port mapping rejected (Code %d) — use LAN Code or playit.gg tunnel." % map_err)
		return

	var ext_ip := upnp.query_external_address()
	if ext_ip.is_empty():
		call_deferred("_on_upnp_completed", false, "", port, "UPnP mapped UDP %d, but could not query external IP." % port)
		return

	call_deferred("_on_upnp_completed", true, ext_ip, port, "UPnP Internet Port Forwarding ACTIVE (UDP %d)!" % port)

func _on_upnp_completed(success: bool, ext_ip: String, port: int, status_msg: String) -> void:
	if _upnp_thread and _upnp_thread.is_started():
		_upnp_thread.wait_to_finish()
	_upnp_thread = null
	upnp_status_text = status_msg
	if success and not ext_ip.is_empty():
		_upnp_mapped_port = port
		active_invite_code = encode_invite_code(ext_ip, port)
		DisplayServer.clipboard_set(active_invite_code)
		var msg := "UPnP SUCCESS! Internet Invite Code [%s] copied to clipboard — send it to your testers!" % active_invite_code
		network_status_changed.emit(msg)
		invite_code_updated.emit(active_invite_code, upnp_status_text)
		GameState.status_message.emit(msg, Color(0.35, 1.0, 0.65))
	else:
		invite_code_updated.emit(active_invite_code, upnp_status_text)
		GameState.status_message.emit(status_msg, Color(1.0, 0.72, 0.38))

func join_coop_session(code_or_address: String = "127.0.0.1", fallback_port: int = DEFAULT_PORT) -> Error:
	if is_coop_active:
		disconnect_coop_session()
	var decoded := decode_invite_code_or_address(code_or_address, fallback_port)
	target_join_address = str(decoded.get("ip", "127.0.0.1"))
	current_port = int(decoded.get("port", fallback_port))

	var peer := ENetMultiplayerPeer.new()
	var err := peer.create_client(target_join_address, current_port)
	if err != OK:
		GameState.status_message.emit("Failed to create ENet client for %s:%d (Error %d)" % [target_join_address, current_port, err], Color(1.0, 0.35, 0.35))
		return err
	multiplayer.multiplayer_peer = peer
	is_coop_active = true
	var msg := "Joining Co-op Host [%s] (%s:%d)..." % [code_or_address.strip_edges(), target_join_address, current_port]
	network_status_changed.emit(msg)
	GameState.status_message.emit(msg, Color(0.95, 0.85, 0.40))
	return OK

func disconnect_coop_session() -> void:
	if multiplayer.multiplayer_peer:
		multiplayer.multiplayer_peer.close()
	multiplayer.multiplayer_peer = OfflineMultiplayerPeer.new()
	is_coop_active = false
	connected_peers = [1]
	network_status_changed.emit("Solo / Offline Mode ([F8] Host | [F9] Join | [P] Co-op Invite Menu)")
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
	var msg := "CONNECTED to Co-op Host (%s:%d) as Investigator #%d!" % [target_join_address, current_port, my_id]
	network_status_changed.emit(msg)
	local_role_changed.emit(false, my_id)
	peer_list_updated.emit(connected_peers)
	GameState.status_message.emit(msg, Color(0.35, 1.0, 0.68))

func _on_connection_failed() -> void:
	disconnect_coop_session()
	GameState.status_message.emit("Co-op connection to %s:%d failed! Check Invite Code or Host UPnP status." % [target_join_address, current_port], Color(1.0, 0.4, 0.4))

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
