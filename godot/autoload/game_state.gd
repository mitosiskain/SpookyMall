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
var total_death_tally: int = 0
var sauna_soul_anchor_lit: bool = false

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
		"can_briefly_pursue_outside_store": false,
		"territory_aabb": AABB(Vector3(14.2, -0.5, -6.6), Vector3(13.4, 6.5, 13.2)),
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
		"can_briefly_pursue_outside_store": false,
		"territory_aabb": AABB(Vector3(-27.6, -0.5, -14.5), Vector3(13.4, 6.5, 12.0)),
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
		"can_briefly_pursue_outside_store": true,
		"territory_aabb": AABB(Vector3(12.5, 8.0, -20.5), Vector3(9.0, 5.5, 9.0)),
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
		"crouch": [KEY_CTRL, KEY_C],
		"interact": [KEY_F],
		"secondary_action": [KEY_E],
		"primary_use": [KEY_R],
		"drop_item": [KEY_G],
		"toggle_flashlight": [KEY_T],
		"push_to_talk": [KEY_V],
		"camera_perspective": [KEY_B, KEY_F5],
		"toggle_voice_menu": [KEY_J],
		"toggle_journal": [KEY_TAB],
		"hotbar_1": [KEY_1],
		"hotbar_2": [KEY_2],
		"hotbar_3": [KEY_3],
		"hotbar_4": [KEY_4],
		"hotbar_5": [KEY_5], # Slot 5: Parody Proton Ray Gun & Ectoplasm Canister
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
	escalator_circuit_powered.emit(circuit_tag)
	status_message.emit("STAGE 6A COMPLETE: Escalator [%s] Powered! Ascend on foot (Carts stay on Floor 1 until Freight Elevator is repaired!)." % circuit_tag, Color(0.3, 0.95, 1.0))

func repair_freight_elevator(floor_num: int) -> void:
	_apply_repair_freight_elevator(floor_num)
	if _is_net_ready():
		_rpc_repair_freight_elevator.rpc(floor_num)

@rpc("any_peer", "call_remote", "reliable")
func _rpc_repair_freight_elevator(floor_num: int) -> void:
	_apply_repair_freight_elevator(floor_num)

func _apply_repair_freight_elevator(floor_num: int) -> void:
	freight_elevator_unlocked_floors[floor_num] = true
	freight_elevator_repaired.emit(floor_num)
	_apply_add_cash(75, "Stage 6B Complete: Floor %d Service Freight Elevator Repaired!" % floor_num)
	status_message.emit("STAGE 6B COMPLETE: Floor %d Service Freight Elevator Online! Equipment & 18,000 lm Light Carts transported up!" % floor_num, Color(0.35, 1.0, 0.65))
