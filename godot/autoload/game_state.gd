@tool
extends Node
## Master Game State, Team Wallet ($), Evidence Ledger, Randomized Ghost Dispositions,
## Two-Stage Vertical Progression (Escalator Beachhead -> Freight Elevator), & Death/Revival Tally.

signal cash_changed(new_balance: int, delta: int, reason: String)
signal evidence_logged(store_id: String, evidence_type: String, payout: int)
signal lore_discovered(lore_id: String, title: String, text: String)
signal escalator_circuit_powered(circuit_tag: String)
signal freight_elevator_repaired(floor_num: int)
signal store_resolved(store_id: String, method: String, bounty: int)
signal status_message(message: String, color: Color)

var team_cash: int = 40
var current_floor: int = 1
var highest_floor_reached: int = 1
var total_death_tally: int = 0
var sauna_soul_anchor_lit: bool = false

## Ray Gun Power Level System (Starts at Level 1 Weakened State -> Upgradeable up to Level 4 on Higher Floors)
## Level 1 (Starter, 28 DPS, Power 1.0): Tuned for slowing & capturing Hallway Gremlins (120 HP); Store Ghosts have 850–1100 HP.
## Level 2 (Floor 1 Upgrade, $80, 65 DPS, Power 2.5): Overclocked Coil Mk II.
## Level 3 (Floor 2+ Unlock, $140, 120 DPS, Power 4.5): Heavy Containment Array Mk III.
## Level 4 (Floor 3+ Unlock, $220, 185 DPS, Power 6.5): Geodesic Ark Overdrive Mk IV.
var ray_gun_power_level: int = 1

## Ectoplasm Canister Tech Level (Controls Deployed Active Time Limit, Recharge Rate & Max Circle Range)
## Level 1 (Starter): 18.0s deployed active sucking time limit, 2.2m default circle (up to 3.2m).
## Level 2 (Floor 1 Upgrade, $65): 28.0s deployed time limit + faster recharge.
## Level 3 (Floor 2+ Unlock, $110): 40.0s deployed time limit + 4.0m Wide Vortex option + 40% faster capture!
var canister_upgrade_level: int = 1

## Stage 6A (Escalator) & Stage 6B (Service Freight Elevator) states
var powered_circuits: Dictionary = {
	"EscalatorCircuit_F1_To_F2": false,
	"EscalatorCircuit_F2_To_F3": false,
}
var freight_elevator_unlocked_floors: Dictionary = {
	1: true,
	2: false,
	3: false,
}

## Confirmed evidence & resolution state per StoreId
var store_evidence_ledger: Dictionary = {
	"East_F0_02": [],
	"West_F1_03": [],
	"East_F2_01": [],
}
var resolved_stores: Dictionary = {}

## Signature Store Ghost Profiles from Docs/GameDesignBible.md
## Floor 1 ghosts are strictly concerned with their store territory (can_briefly_pursue_outside_store = false);
## only Upper-Floor (Floor 2+) ghosts are comfortable leaving their stores briefly to continue pursuit!
var ghost_profiles: Dictionary = {
	"East_F0_02": {
		"name": "Arthur Pendelton",
		"title": "The Night Inventory Clerk",
		"store_name": "Pendelton's Book & Curio Shop (Onboarding Store)",
		"archetype": "BookAndCurio",
		"floor_level": 1,
		"ghost_weight": 4.5,
		"max_ghost_hp": 850.0,
		"hp_regen_per_second": 3.5,
		"can_briefly_pursue_outside_store": false,
		"territory_aabb": AABB(Vector3(20.2, -0.5, -73.6), Vector3(25.4, 6.5, 31.2)),
		"age": 54,
		"spirit_box_channel": 87.9,
		"required_evidence": ["Ghost Writing", "Spirit Box (87.9 FM)", "EMF-5"],
		"is_corrupted_hostile": false, # Onboarding guarantee: Receptive to Peace
		"receptive_tell": "Neat ledger tally: 'Just one last shelf count before I clock out...'",
		"corrupted_tell": "Violently scratched across both pages: 'DO NOT TOUCH MY STOCK'",
		"peaceful_ritual_hint": "Tune Spirit Box to 87.9 FM & press [F] at the Store Ritual Altar (or speak 'Your shift is over').",
	},
	"West_F1_03": {
		"name": "Static Evelyn",
		"title": "Sheer Terror Hosiery Clerk",
		"store_name": "Sheer Terror Hosiery",
		"archetype": "HosierySpecialty",
		"floor_level": 1,
		"ghost_weight": 4.5,
		"max_ghost_hp": 850.0,
		"hp_regen_per_second": 3.5,
		"can_briefly_pursue_outside_store": false,
		"territory_aabb": AABB(Vector3(-45.6, -0.5, -73.6), Vector3(25.4, 6.5, 31.2)),
		"age": 29,
		"spirit_box_channel": 91.5,
		"required_evidence": ["EMF-5", "UV Prints", "Ghost Writing"],
		"is_corrupted_hostile": true, # Rolled per run in _ready()
		"receptive_tell": "Static-charged plea: 'Unplug the display transformer...'",
		"corrupted_tell": "Ledger smokes with black arcs & hostile static: 'TOUCH THE RACKS AND BURN'",
		"peaceful_ritual_hint": "If Receptive, toggle rear transformer switch [F]. WARNING: If Corrupted, Peaceful Ritual triggers an ENRAGED HUNT — use Proton Ray Gun + Canister instead!",
	},
	"East_F2_01": {
		"name": "The Lost Shopper",
		"title": "Upper Promenade Boutique Stalker",
		"store_name": "Floor 2 East Mezzanine Boutique",
		"archetype": "UselessClothing",
		"floor_level": 2,
		"ghost_weight": 5.5,
		"max_ghost_hp": 1100.0,
		"hp_regen_per_second": 5.0,
		"can_briefly_pursue_outside_store": true,
		"territory_aabb": AABB(Vector3(20.2, 8.0, -73.6), Vector3(25.4, 6.5, 31.2)),
		"age": 37,
		"spirit_box_channel": 98.3,
		"required_evidence": ["Spirit Box (98.3 FM)", "Ghost Writing", "Freezing Temps"],
		"is_corrupted_hostile": false,
		"receptive_tell": "Mournful EVP on 98.3 FM: 'Which way is the fountain exit?'",
		"corrupted_tell": "Mimics distorted fragments of players' own proximity voice chat over 98.3 FM.",
		"peaceful_ritual_hint": "Upper-Floor Ghost: Comfortable leaving its store briefly (6s) to continue pursuing players — block its exit with the Light Cart's Circle of Safety edge!",
	},
}

var discovered_lore: Dictionary = {}
var lore_catalog: Dictionary = {
	"LORE_BRIEFING_00": {
		"title": "Contract #89-AV: Missing Auditor Elena Vance",
		"text": "Target: Elena Vance, Urban Architectural Auditor. Last seen 72h ago entering the condemned Arcadia Oval Megamall. Upon your entry, the street airlock sealed behind you and all clocks froze at 03:17 AM.",
	},
	"LORE_LEDGER_EAST_F0_02": {
		"title": "Arthur Pendelton's Inventory Ledger (1994)",
		"text": "Handwritten in neat clerk ink: 'Just one last shelf count before I clock out... The hallways bend 2.5 degrees every ten meters. Elena Vance climbed the North Escalator toward the Sky Arboretum.'",
	},
	"LORE_UV_CIPHER_01": {
		"title": "Elena's UV Shutter Cipher #1",
		"text": "Fluorescent UV ink painted across the storefront frame: 'INVESTIGATORS: Check the Ledger & Spirit Box tells before attempting a Peaceful Resolution. Corrupted ghosts will Enrage if you try peace—use the Proton Pack & Canister on them!'",
	},
	"LORE_RADIO_104_7": {
		"title": "104.7 FM — Elena Vance Audio Beacon #1",
		"text": "[Static crackle] '...Stepped escalators can't carry your Equipment or Light Carts! When you ascend to Floor 2, haul the Heavy Hydraulic Assembly to the Service Freight Elevator shaft so you can bring your carts up!'",
	},
}

func _ready() -> void:
	randomize()
	# Roll West_F1_03 and East_F2_01 disposition dynamically per run while keeping East_F0_02 Receptive per onboarding rule
	ghost_profiles["East_F0_02"]["is_corrupted_hostile"] = false
	ghost_profiles["West_F1_03"]["is_corrupted_hostile"] = (randf() < 0.65)
	ghost_profiles["East_F2_01"]["is_corrupted_hostile"] = (randf() < 0.50)
	_setup_input_map()
	discover_lore("LORE_BRIEFING_00", false)

func _setup_input_map() -> void:
	var key_bindings := {
		"move_forward": [KEY_W, KEY_UP],
		"move_back": [KEY_S, KEY_DOWN],
		"move_left": [KEY_A, KEY_LEFT],
		"move_right": [KEY_D, KEY_RIGHT],
		"jump": [KEY_SPACE],
		"sprint": [KEY_SHIFT],
		"crouch": [KEY_C, KEY_CTRL],
		"interact": [KEY_F],          # [F] Pick Up Item (Table, Floor, Placed Gadgets, Cart Shelf, or Hands-Only CCTV Monitor)
		"secondary_action": [KEY_E],  # [E] World Interact (Push/Release Cart, Rotate Placed Camera, Elevator Buttons, Benches, Spa, Trader)
		"primary_use": [],            # [LMB] Use Equipped Item (Primary Function: Power ON/OFF, Fire Beam, Snap Photo)
		"tool_secondary": [],         # [RMB] Secondary Function of Equipped Item (°C/°F, FM Channel, Camera Night-Vision, CCTV Monitor Cycle Cam)
		"drop_item": [KEY_G],         # [G] Drop / Place Held Item
		"toggle_flashlight": [KEY_T], # [T] Belt Flashlight ON/OFF
		"push_to_talk": [KEY_B],      # [B] Walkie-Talkie & Voice Press-to-Talk
		"camera_perspective": [KEY_V, KEY_F5], # [V / F5] Toggle 1st/3rd-Person Camera View
		"toggle_voice_menu": [KEY_J],
		"toggle_journal": [KEY_TAB],
		"hotbar_1": [KEY_1],
		"hotbar_2": [KEY_2],
		"hotbar_3": [KEY_3],
		"hotbar_4": [KEY_4],
		"hotbar_5": [KEY_5],
		"voice_cmd_1": [KEY_6], # "Where are you?"
		"voice_cmd_2": [KEY_7], # "Who are you?"
		"voice_cmd_3": [KEY_8], # "Write in the book"
		"voice_cmd_4": [KEY_9], # "Your shift is over" (Peaceful Ritual / Prayer)
		"voice_cmd_5": [KEY_0], # "Leave me alone"
	}
	for action in key_bindings.keys():
		if not InputMap.has_action(action):
			InputMap.add_action(action)
		for keycode in key_bindings[action]:
			var ev := InputEventKey.new()
			ev.physical_keycode = keycode
			if not InputMap.action_has_event(action, ev):
				InputMap.action_add_event(action, ev)

	var lmb := InputEventMouseButton.new()
	lmb.button_index = MOUSE_BUTTON_LEFT
	if not InputMap.action_has_event("primary_use", lmb):
		InputMap.action_add_event("primary_use", lmb)

	var rmb := InputEventMouseButton.new()
	rmb.button_index = MOUSE_BUTTON_RIGHT
	if not InputMap.action_has_event("tool_secondary", rmb):
		InputMap.action_add_event("tool_secondary", rmb)

func _is_net_ready() -> bool:
	if Engine.is_editor_hint() or not is_inside_tree():
		return false
	return multiplayer.has_multiplayer_peer() and not (multiplayer.multiplayer_peer is OfflineMultiplayerPeer)

func add_cash(amount: int, reason: String) -> void:
	_apply_add_cash(amount, reason)
	if _is_net_ready():
		_rpc_add_cash.rpc(amount, reason)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_add_cash(amount: int, reason: String) -> void:
	_apply_add_cash(amount, reason)

func _apply_add_cash(amount: int, reason: String) -> void:
	team_cash += amount
	cash_changed.emit(team_cash, amount, reason)
	status_message.emit("+$%d — %s (Team Balance: $%d)" % [amount, reason, team_cash], Color(0.35, 1.0, 0.55))

func spend_cash(amount: int, reason: String) -> bool:
	if team_cash < amount:
		status_message.emit("Insufficient funds! Need $%d for %s (Have $%d)" % [amount, reason, team_cash], Color(1.0, 0.4, 0.35))
		return false
	_apply_spend_cash(amount, reason)
	if _is_net_ready():
		_rpc_spend_cash.rpc(amount, reason)
	return true

@rpc("any_peer", "call_remote", "reliable")
func _rpc_spend_cash(amount: int, reason: String) -> void:
	_apply_spend_cash(amount, reason)

func _apply_spend_cash(amount: int, reason: String) -> void:
	team_cash = maxi(0, team_cash - amount)
	cash_changed.emit(team_cash, -amount, reason)
	status_message.emit("-$%d — Purchased %s (Balance: $%d)" % [amount, reason, team_cash], Color(0.95, 0.85, 0.35))

func record_evidence(store_id: String, evidence_type: String, bonus_cash: int = 25) -> void:
	if _apply_record_evidence(store_id, evidence_type, bonus_cash) and _is_net_ready():
		_rpc_record_evidence.rpc(store_id, evidence_type, bonus_cash)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_record_evidence(store_id: String, evidence_type: String, bonus_cash: int) -> void:
	_apply_record_evidence(store_id, evidence_type, bonus_cash)

func _apply_record_evidence(store_id: String, evidence_type: String, bonus_cash: int) -> bool:
	if not store_evidence_ledger.has(store_id):
		store_evidence_ledger[store_id] = []
	var list: Array = store_evidence_ledger[store_id]
	if evidence_type not in list:
		list.append(evidence_type)
		_apply_add_cash(bonus_cash, "Confirmed Evidence [%s] in %s" % [evidence_type, store_id])
		evidence_logged.emit(store_id, evidence_type, bonus_cash)
		return true
	return false

func mark_store_resolved(store_id: String, method: String, bounty: int) -> void:
	if _apply_mark_store_resolved(store_id, method, bounty) and _is_net_ready():
		_rpc_mark_store_resolved.rpc(store_id, method, bounty)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_mark_store_resolved(store_id: String, method: String, bounty: int) -> void:
	_apply_mark_store_resolved(store_id, method, bounty)

func _apply_mark_store_resolved(store_id: String, method: String, bounty: int) -> bool:
	if resolved_stores.has(store_id):
		return false
	resolved_stores[store_id] = method
	_apply_add_cash(bounty, "Store [%s] Cleared via %s" % [store_id, method])
	store_resolved.emit(store_id, method, bounty)
	return true

func discover_lore(lore_id: String, grant_bonus: bool = true) -> void:
	if _apply_discover_lore(lore_id, grant_bonus) and _is_net_ready():
		_rpc_discover_lore.rpc(lore_id, grant_bonus)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_discover_lore(lore_id: String, grant_bonus: bool) -> void:
	_apply_discover_lore(lore_id, grant_bonus)

func _apply_discover_lore(lore_id: String, grant_bonus: bool) -> bool:
	if discovered_lore.has(lore_id) or not lore_catalog.has(lore_id):
		return false
	var entry: Dictionary = lore_catalog[lore_id]
	discovered_lore[lore_id] = entry
	lore_discovered.emit(lore_id, entry["title"], entry["text"])
	if grant_bonus:
		_apply_add_cash(50, "Hidden Lore Discovered: %s" % entry["title"])
	return true

func power_escalator_circuit(circuit_tag: String) -> void:
	_apply_power_escalator_circuit(circuit_tag)
	if _is_net_ready():
		_rpc_power_escalator_circuit.rpc(circuit_tag)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_power_escalator_circuit(circuit_tag: String) -> void:
	_apply_power_escalator_circuit(circuit_tag)

func _apply_power_escalator_circuit(circuit_tag: String) -> void:
	powered_circuits[circuit_tag] = true
	if "F1_To_F2" in circuit_tag:
		highest_floor_reached = maxi(highest_floor_reached, 2)
	elif "F2_To_F3" in circuit_tag:
		highest_floor_reached = maxi(highest_floor_reached, 3)
	escalator_circuit_powered.emit(circuit_tag)
	status_message.emit("STAGE 6A COMPLETE: Escalator [%s] Powered! Higher-floor Trader upgrades unlocked at Quartermaster Chen's!" % circuit_tag, Color(0.3, 0.95, 1.0))

func repair_freight_elevator(floor_num: int) -> void:
	_apply_repair_freight_elevator(floor_num)
	if _is_net_ready():
		_rpc_repair_freight_elevator.rpc(floor_num)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_repair_freight_elevator(floor_num: int) -> void:
	_apply_repair_freight_elevator(floor_num)

func _apply_repair_freight_elevator(floor_num: int) -> void:
	freight_elevator_unlocked_floors[floor_num] = true
	highest_floor_reached = maxi(highest_floor_reached, floor_num)
	freight_elevator_repaired.emit(floor_num)
	_apply_add_cash(75, "Stage 6B Complete: Floor %d Service Freight Elevator Repaired!" % floor_num)
	status_message.emit("STAGE 6B COMPLETE: Floor %d Service Freight Elevator Online! Equipment & 18,000 lm Light Carts transported up!" % floor_num, Color(0.35, 1.0, 0.65))

func notify_floor_reached(floor_num: int) -> void:
	current_floor = floor_num
	if floor_num > highest_floor_reached:
		highest_floor_reached = floor_num
		status_message.emit(
			"REACHED FLOOR %d! Quartermaster Chen's Trader Shop now stocks Floor %d+ High-Tier Equipment Upgrades!" % [floor_num, floor_num],
			Color(0.45, 1.0, 0.82)
		)

## Returns the effective Proton Ray Gun power multiplier for the current `ray_gun_power_level`.
func get_ray_gun_power_value() -> float:
	match ray_gun_power_level:
		1:
			return 1.0 # Weakened starter state (ideal vs 120 HP Gremlins; Store Ghosts have 850–1100 HP)
		2:
			return 2.5 # Upgraded Coil Mk II (Floor 1)
		3:
			return 4.5 # Heavy Containment Mk III (Floor 2+ Unlock)
		_:
			return 6.5 # Geodesic Ark Overdrive Mk IV (Floor 3+ Unlock)

## Returns the direct Ghost Health Pool DPS dealt by the Proton Ray Gun at its current level.
func get_ray_gun_dps() -> float:
	match ray_gun_power_level:
		1:
			return 28.0 # 120 HP Gremlin takes ~4.3s; 850 HP Store Ghost takes ~30.4s of beam time
		2:
			return 65.0 # 850 HP Store Ghost takes ~13.1s of beam time (Floor 1 Upgrade)
		3:
			return 120.0 # 850 HP Store Ghost takes ~7.1s of beam time (Floor 2+ Unlock)
		_:
			return 185.0 # Floor 3+ Overdrive

func get_ray_gun_upgrade_cost() -> int:
	match ray_gun_power_level:
		1:
			return 80
		2:
			return 140
		3:
			return 220
		_:
			return 0

func get_ray_gun_next_upgrade_required_floor() -> int:
	match ray_gun_power_level:
		1:
			return 1 # Level 2 available on Floor 1
		2:
			return 2 # Level 3 saved for Floor 2+
		3:
			return 3 # Level 4 saved for Floor 3+
		_:
			return 99

func upgrade_ray_gun(ignore_floor_gate: bool = false) -> bool:
	if ray_gun_power_level >= 4:
		status_message.emit("Proton Ray Gun is already at MAX Power Level 4 (185 DPS)!", Color(0.45, 1.0, 0.72))
		return false
	var req_floor := get_ray_gun_next_upgrade_required_floor()
	if not ignore_floor_gate and highest_floor_reached < req_floor:
		status_message.emit(
			"LOCKED FOR HIGHER FLOORS: Proton Ray Gun Level %d requires reaching Floor %d first! (Current Highest Floor: %d)" % [
				ray_gun_power_level + 1, req_floor, highest_floor_reached
			],
			Color(1.0, 0.55, 0.32)
		)
		return false
	var cost := get_ray_gun_upgrade_cost()
	var next_lvl := ray_gun_power_level + 1
	if not spend_cash(cost, "Proton Ray Gun Power Level %d Upgrade" % next_lvl):
		return false
	_apply_ray_gun_upgrade(next_lvl)
	if _is_net_ready():
		_rpc_upgrade_ray_gun.rpc(next_lvl)
	return true

@rpc("any_peer", "call_remote", "reliable")
func _rpc_upgrade_ray_gun(next_lvl: int) -> void:
	_apply_ray_gun_upgrade(next_lvl)

func _apply_ray_gun_upgrade(next_lvl: int) -> void:
	ray_gun_power_level = clampi(next_lvl, 1, 4)
	status_message.emit(
		"PROTON RAY GUN UPGRADED TO LEVEL %d (%.0f DPS | Power %.1f)! Stronger ghosts can now be damaged and wrangled much faster!" % [
			ray_gun_power_level, get_ray_gun_dps(), get_ray_gun_power_value()
		],
		Color(0.35, 1.0, 0.82)
	)

## Returns the maximum active deployed sucking time (seconds) for an Ectoplasm Canister on the floor.
func get_canister_max_deployed_time() -> float:
	match canister_upgrade_level:
		1:
			return 18.0
		2:
			return 28.0
		_:
			return 40.0

func get_canister_capture_speed_mult() -> float:
	match canister_upgrade_level:
		1:
			return 1.0
		2:
			return 1.15
		_:
			return 1.40

func get_canister_upgrade_cost() -> int:
	match canister_upgrade_level:
		1:
			return 65
		2:
			return 110
		_:
			return 0

func get_canister_next_upgrade_required_floor() -> int:
	match canister_upgrade_level:
		1:
			return 1 # Level 2 Capacitor available on Floor 1
		2:
			return 2 # Level 3 Wide-Vortex Super-Capacitor saved for Floor 2+
		_:
			return 99

func upgrade_canister_tech(ignore_floor_gate: bool = false) -> bool:
	if canister_upgrade_level >= 3:
		status_message.emit("Ectoplasm Canister Tech is already at MAX Level 3 (40s Deployed Time)!", Color(0.45, 1.0, 0.72))
		return false
	var req_floor := get_canister_next_upgrade_required_floor()
	if not ignore_floor_gate and highest_floor_reached < req_floor:
		status_message.emit(
			"LOCKED FOR HIGHER FLOORS: Canister Upgrade Level %d requires reaching Floor %d first!" % [
				canister_upgrade_level + 1, req_floor
			],
			Color(1.0, 0.55, 0.32)
		)
		return false
	var cost := get_canister_upgrade_cost()
	var next_lvl := canister_upgrade_level + 1
	if not spend_cash(cost, "Ectoplasm Canister Capacitor Level %d Upgrade" % next_lvl):
		return false
	_apply_canister_upgrade(next_lvl)
	if _is_net_ready():
		_rpc_upgrade_canister_tech.rpc(next_lvl)
	return true

@rpc("any_peer", "call_remote", "reliable")
func _rpc_upgrade_canister_tech(next_lvl: int) -> void:
	_apply_canister_upgrade(next_lvl)

func _apply_canister_upgrade(next_lvl: int) -> void:
	canister_upgrade_level = clampi(next_lvl, 1, 3)
	status_message.emit(
		"ECTOPLASM CANISTER UPGRADED TO LEVEL %d (%.0fs Deployed Active Time Limit & %.0f%% Capture Speed)!" % [
			canister_upgrade_level, get_canister_max_deployed_time(), get_canister_capture_speed_mult() * 100.0
		],
		Color(0.35, 1.0, 0.82)
	)
