@tool
class_name CampaignFloorsAndLore
extends Node3D
## Megamall Anomaly — 6-Floor Campaign Expansion, All 6 Signature Store Ghosts,
## All 12 Hidden Lore Collectibles, Floor 3 Sky Arboretum & Geodesic Ark Control (Y = 17.0m),
## Drifting H-Concourse Spline Corridors (1.5°–4.0° yaw / 0.8°–2.2° pitch),
## All 5 AMallHidingSpot Types, Automated Delivery Airlock Bay, 4-Meal Food Court Selector,
## Intro Contract Briefing Card, and Endgame Summary / Floors 7+ Endless Escalation Controller.

signal endgame_summary_opened(floor_tier: int, stores_cleared: int, lore_found: int)
signal endless_escalation_launched(new_floor: int)
signal food_court_meal_cycled(meal_index: int, meal_title: String)

# ------------------------------------------------------------------------------
# Inner Class 1: GhostOrbActor (Floor 2+ Video Camera / Cart Monitor Evidence)
# ------------------------------------------------------------------------------
class GhostOrbActor extends Node3D:
	var store_id: String = "West_F2_01"
	var orb_title: String = "Floating IR Ghost Orb (Floor 2+ Video Evidence)"
	var is_camera_shy_entity: bool = false
	var home_pos: Vector3 = Vector3.ZERO
	var _phase: float = 0.0
	var _discovered: bool = false
	var orb_mesh: MeshInstance3D = null
	var orb_mat: StandardMaterial3D = null
	var orb_light: OmniLight3D = null
	var orb_label: Label3D = null

	func _ready() -> void:
		add_to_group("ghost_orbs")
		add_to_group("ghost_orb_actors")
		home_pos = position
		_phase = randf() * TAU

		orb_mesh = MeshInstance3D.new()
		var sph := SphereMesh.new()
		sph.radius = 0.09
		sph.height = 0.18
		orb_mesh.mesh = sph
		orb_mat = StandardMaterial3D.new()
		orb_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		orb_mat.albedo_color = Color(0.45, 0.96, 1.0, 0.78)
		orb_mat.emission_enabled = true
		orb_mat.emission = Color(0.35, 0.92, 1.0)
		orb_mat.emission_energy_multiplier = 3.2
		orb_mesh.material_override = orb_mat
		add_child(orb_mesh)

		orb_light = OmniLight3D.new()
		orb_light.light_color = Color(0.40, 0.95, 1.0)
		orb_light.light_energy = 1.2
		orb_light.omni_range = 2.4
		add_child(orb_light)

		orb_label = Label3D.new()
		orb_label.position = Vector3(0.0, 0.28, 0.0)
		orb_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		orb_label.font_size = 18
		orb_label.outline_size = 5
		orb_label.text = "%s [%s]" % [orb_title, store_id]
		orb_label.modulate = Color(0.55, 0.98, 1.0)
		add_child(orb_label)

	func inspect_or_capture_orb() -> bool:
		if not _discovered:
			_discovered = true
			GameState.record_evidence(store_id, "Ghost Orbs / Shy Video Entity", 45)
			return true
		return false

	func _process(delta: float) -> void:
		if Engine.is_editor_hint():
			return
		_phase += delta * 2.1
		position = home_pos + Vector3(
			cos(_phase * 1.1) * 0.65,
			sin(_phase * 1.7) * 0.28,
			sin(_phase * 0.9) * 0.65
		)


# ------------------------------------------------------------------------------
# Inner Class 2: MallHidingSpotActor (All 5 AMallHidingSpot Types in group "hiding_spots")
# ------------------------------------------------------------------------------
class MallHidingSpotActor extends Node3D:
	var spot_type: String = "FittingRoomBooth"
	var spot_display_name: String = "Enclosed Fitting Room Cubicle Booth"
	var hidden_local_offset: Vector3 = Vector3(0.0, 0.0, 0.0)
	var exit_local_offset: Vector3 = Vector3(0.0, 0.0, 1.35)
	var is_locked: bool = false
	var requires_master_key: bool = false
	var occupied_without_witness: bool = false
	var occupant_player: PlayerController = null
	var indicator_light: OmniLight3D = null
	var label_3d: Label3D = null

	func _ready() -> void:
		add_to_group("hiding_spots")
		indicator_light = OmniLight3D.new()
		indicator_light.position = Vector3(0.0, 1.85, 0.0)
		indicator_light.light_color = Color(1.0, 0.55, 0.25) if is_locked else Color(0.45, 0.88, 1.0)
		indicator_light.light_energy = 1.1
		indicator_light.omni_range = 3.2
		add_child(indicator_light)

		label_3d = Label3D.new()
		label_3d.position = Vector3(0.0, 2.35, 0.0)
		label_3d.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label_3d.font_size = 21
		label_3d.outline_size = 6
		add_child(label_3d)
		_refresh_spot_label()

	func _refresh_spot_label() -> void:
		if label_3d == null:
			return
		var lock_tag := " [LOCKED — Press F with Master Keyring or Latch Override]" if is_locked else ""
		label_3d.text = "[F] Hiding Spot (%s): %s%s\n(Conceals from Lethal Hunts if entered without Ghost Witness)" % [
			spot_type, spot_display_name, lock_tag
		]
		label_3d.modulate = Color(1.0, 0.68, 0.35) if is_locked else Color(0.72, 0.88, 1.0)

	func has_hunting_ghost_witness(entering_player: PlayerController) -> bool:
		if not is_instance_valid(entering_player):
			return false
		var ppos := entering_player.global_position
		for g_node in get_tree().get_nodes_in_group("store_ghosts"):
			var ghost := g_node as StoreGhost
			if ghost and not ghost.is_resolved and ghost.current_phase == StoreGhost.GhostPhase.VISIBLE_HUNT:
				if absf(ghost.global_position.y - ppos.y) <= 3.0 and ghost.global_position.distance_to(ppos) < 5.2:
					return true
		return false

	func try_toggle_hide(p: PlayerController) -> bool:
		if not is_instance_valid(p) or p.is_floating_essence:
			return false
		if is_locked:
			is_locked = false
			if indicator_light:
				indicator_light.light_color = Color(0.45, 0.88, 1.0)
			_refresh_spot_label()
			GameState.status_message.emit(
				"Unlocked %s [%s]! Press [F] again to duck inside and hide." % [spot_display_name, spot_type],
				Color(0.45, 1.0, 0.75)
			)
			return true

		if p.contextual_seat_active and occupant_player == p:
			occupant_player = null
			occupied_without_witness = false
			p.is_hidden_without_witness = false
			p.exit_contextual_third_person()
			p.global_position = to_global(exit_local_offset)
			GameState.status_message.emit("Stepped out of %s." % spot_display_name, Color(0.78, 0.88, 0.98))
			return true

		var witnessed := has_hunting_ghost_witness(p)
		occupant_player = p
		occupied_without_witness = not witnessed
		p.is_hidden_without_witness = occupied_without_witness
		var hide_world_pos := to_global(hidden_local_offset)
		p.enter_contextual_third_person(
			hide_world_pos,
			"Hidden in %s (%s)" % [spot_display_name, "WITNESSED BY HUNTING GHOST!" if witnessed else "Undetected"]
		)
		return true


# ------------------------------------------------------------------------------
# 4 Signature North Night-Market Food Court Meals (Docs/GameDesignBible.md)
# ------------------------------------------------------------------------------
const FOOD_COURT_MEALS: Array[Dictionary] = [
	{
		"title": "Chongqing Hot Pot (+25% Speed & Stamina Surge)",
		"short_name": "Chongqing Hot Pot",
		"speed_bonus_pct": 25,
		"stealth_bonus_pct": 0,
		"cart_battery_boost": 45.0,
		"grants_chill_shield": false,
		"description": "+40 HP, +65 Stamina, +45% Light Cart Battery, & 90s +25% Sprint/Stamina Surge",
	},
	{
		"title": "Geothermal Matcha Boba (-30% Ghost Detection Stealth)",
		"short_name": "Geothermal Matcha Boba",
		"speed_bonus_pct": 10,
		"stealth_bonus_pct": 30,
		"cart_battery_boost": 45.0,
		"grants_chill_shield": false,
		"description": "+40 HP, +65 Stamina, +45% Light Cart Battery, & 90s -30% Ghost Detection Range (Spectral Stealth)",
	},
	{
		"title": "Pocket-Dimension Bento (+50% Light Cart Overcharge)",
		"short_name": "Pocket-Dimension Bento",
		"speed_bonus_pct": 12,
		"stealth_bonus_pct": 0,
		"cart_battery_boost": 50.0,
		"grants_chill_shield": false,
		"description": "+40 HP, +65 Stamina, & +50% Mobile Light Cart Battery Overcharge",
	},
	{
		"title": "Himalayan Salt Broth (Ghost Chill Shield & +15% Speed)",
		"short_name": "Himalayan Salt Broth",
		"speed_bonus_pct": 15,
		"stealth_bonus_pct": 15,
		"cart_battery_boost": 45.0,
		"grants_chill_shield": true,
		"description": "+40 HP, +65 Stamina, 90s Ghost Chill Shield Ward, & +15% Move Speed",
	},
]

var selected_food_meal_index: int = 0
var food_court_menu_label_3d: Label3D = null

## Store Ghosts spawned by this expansion builder
var store_ghost_west_f1_02: StoreGhost = null
var store_ghost_west_f2_01: StoreGhost = null
var store_ghost_center_f3_01: StoreGhost = null

## Escalator connecting Floor 2 (Y = 8.50m) -> Floor 3 (Y = 17.0m)
var escalator_f2_to_f3: MallEscalator = null

## Automated Delivery Airlock Bay state
var delivery_airlock_pos: Vector3 = Vector3(-6.0, 0.0, -34.0)
var _airlock_cooldown: float = 0.0
var _airlock_dispense_count: int = 0
var airlock_label_3d: Label3D = null

## Elena Vance Final Camp & Geodesic Ark Extraction Console on Floor 3
var elena_camp_f3_pos: Vector3 = Vector3(0.0, 17.0, -34.0)
var ark_containment_stabilized: bool = false
var endless_escalation_wave: int = 0
var _east_f2_spoke_where_are_you: bool = false
var _uv_cipher_entries: Array[Dictionary] = []

## UI Controls (Intro Briefing Card & Endgame / Endless Escalation Modal)
var briefing_card_panel: PanelContainer = null
var endgame_modal_panel: PanelContainer = null
var endgame_summary_rtl: RichTextLabel = null
var endgame_escalate_btn: Button = null
var _ui_canvas_layer: CanvasLayer = null

func _ready() -> void:
	name = "CampaignFloorsAndLore"
	add_to_group("campaign_floors_and_lore")
	register_full_bible_profiles_and_lore()
	_build_floor_3_sky_arboretum_and_ark_deck()
	_build_drifting_concourse_wings()
	_build_missing_signature_stores_and_ghosts()
	_build_all_five_hiding_spot_types()
	_build_automated_delivery_airlock_bay()
	_build_food_court_four_meal_selector_sign()
	_spawn_12_hidden_lore_triggers_and_ciphers()

	if Engine.is_editor_hint():
		return

	if AcousticVoipManager != null and not AcousticVoipManager.voice_command_recognized.is_connected(_on_voice_command_for_lore_and_rituals):
		AcousticVoipManager.voice_command_recognized.connect(_on_voice_command_for_lore_and_rituals)

	call_deferred("_deferred_wire_world_and_ui")

# ------------------------------------------------------------------------------
# 1. Complete 6 Signature Store Ghost Profiles & 12 Hidden Lore Catalog Registration
# ------------------------------------------------------------------------------
func register_full_bible_profiles_and_lore() -> void:
	if Engine.is_editor_hint():
		return

	# Register all 6 Signature Store Ghosts from Docs/GameDesignBible.md Section 4 (32m Wide x 26m Deep Bay 0 Stores!)
	var full_profiles: Dictionary = {
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
			"is_corrupted_hostile": false, # Onboarding rule: always Receptive on first run
			"receptive_tell": "Neat ledger tally: 'Just one last shelf count before I clock out...'",
			"corrupted_tell": "Violently scratched across both pages: 'DO NOT TOUCH MY STOCK'",
			"peaceful_ritual_hint": "Return displaced books to checkout stand & speak 'Your shift is over' on 87.9 FM (or press [F] at Ledger).",
		},
		"West_F1_02": {
			"name": "The Display Coordinator",
			"title": "Symmetry-Obsessed Window Dresser",
			"store_name": "Victoria's Phantoms Boutique",
			"archetype": "LingerieParody",
			"floor_level": 1,
			"ghost_weight": 4.5,
			"max_ghost_hp": 880.0,
			"hp_regen_per_second": 3.5,
			"can_briefly_pursue_outside_store": false,
			"territory_aabb": AABB(Vector3(-45.6, -0.5, 42.4), Vector3(25.4, 6.5, 31.2)),
			"age": 31,
			"spirit_box_channel": 91.5,
			"required_evidence": ["UV Prints", "Freezing Temps", "Spirit Box (91.5 FM)"],
			"is_corrupted_hostile": (randf() < 0.45),
			"receptive_tell": "Soft whispered reply on 91.5 FM: 'The spotlight is too harsh on the silk...'",
			"corrupted_tell": "Violently hisses 'DON'T LOOK AT ME' on 91.5 FM and cracks nearby mirrors.",
			"peaceful_ritual_hint": "Dim West_F1_02 interior lights to 45% Gray-Space Dim with Light Remote Gun or press [F] at Spotlight Dimmer Console.",
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
			"is_corrupted_hostile": GameState.ghost_profiles.get("West_F1_03", {}).get("is_corrupted_hostile", randf() < 0.65),
			"receptive_tell": "Static-charged plea: 'Unplug the display transformer...'",
			"corrupted_tell": "Ledger smokes with black arcs & hostile static: 'TOUCH THE RACKS AND BURN'",
			"peaceful_ritual_hint": "Toggle off the store's rear display transformer switch [F] while holding a grounded EMF Reader.",
		},
		"West_F2_01": {
			"name": "The Night-Shift Builder",
			"title": "Master Model Assembler",
			"store_name": "BRICKWORKZ Modular Toy Store",
			"archetype": "ParodyLegoBrickStore",
			"floor_level": 2,
			"ghost_weight": 5.5,
			"max_ghost_hp": 1050.0,
			"hp_regen_per_second": 4.8,
			"can_briefly_pursue_outside_store": true,
			"territory_aabb": AABB(Vector3(-45.6, 8.0, -73.6), Vector3(25.4, 6.0, 31.2)),
			"age": 26,
			"spirit_box_channel": 98.3,
			"required_evidence": ["EMF-5", "Freezing Temps", "UV Prints", "Ghost Orbs / Shy Video Entity"],
			"is_corrupted_hostile": (randf() < 0.45),
			"receptive_tell": "UV Blacklight reveals neat glowing blueprint arrows pointing to the center feature podium.",
			"corrupted_tell": "UV reveals frantic clawed handprints and scattered plastic brick hazard warnings.",
			"peaceful_ritual_hint": "Place the missing modular display piece onto the center feature podium [F].",
		},
		"East_F2_01": {
			"name": "The Lost Shopper",
			"title": "Upper Promenade Boutique Stalker",
			"store_name": "Arcadia Evening Wear & Department Maze",
			"archetype": "UselessClothing",
			"floor_level": 2,
			"ghost_weight": 5.5,
			"max_ghost_hp": 1100.0,
			"hp_regen_per_second": 5.0,
			"can_briefly_pursue_outside_store": true,
			"territory_aabb": AABB(Vector3(20.2, 8.0, -73.6), Vector3(25.4, 6.0, 31.2)),
			"age": 37,
			"spirit_box_channel": 98.3,
			"required_evidence": ["Spirit Box (98.3 FM)", "Ghost Writing", "Freezing Temps", "Ghost Orbs / Shy Video Entity"],
			"is_corrupted_hostile": GameState.ghost_profiles.get("East_F2_01", {}).get("is_corrupted_hostile", randf() < 0.50),
			"receptive_tell": "Mournful EVP on 98.3 FM: 'Which way is the fountain exit?'",
			"corrupted_tell": "Mimics distorted fragments of players' own proximity voice chat over 98.3 FM.",
			"peaceful_ritual_hint": "Speak 'Where are you?' followed by ringing the checkout service bell [F] to guide them out.",
		},
		"Center_F3_01": {
			"name": "The Chief Architect",
			"title": "Geodesic Ark Containment Designer",
			"store_name": "Mall City Civic Facility & Upper Ark Control",
			"archetype": "SurvivorCivicFacility",
			"floor_level": 3,
			"ghost_weight": 6.5,
			"max_ghost_hp": 1350.0,
			"hp_regen_per_second": 6.0,
			"can_briefly_pursue_outside_store": true,
			"territory_aabb": AABB(Vector3(20.2, 16.5, -73.6), Vector3(25.4, 6.0, 31.2)),
			"age": 62,
			"spirit_box_channel": 104.7,
			"required_evidence": [
				"Spirit Box (104.7 FM)",
				"Ghost Writing",
				"UV Prints",
				"Recorded EVP Audio",
				"SLS / Laser Silhouette",
				"Ectoplasmic Residue / SKU Anomaly",
			],
			"is_corrupted_hostile": (randf() < 0.40),
			"receptive_tell": "Writes architectural stabilization coordinates: 'Keep the ring coil balanced.'",
			"corrupted_tell": "Broadcasts deafening number-station sirens on 104.7 FM.",
			"peaceful_ritual_hint": "Tune Spirit Box to 104.7 FM & interact [F] with the Ark Blueprint Drafting Table.",
		},
	}

	for store_id in full_profiles.keys():
		GameState.ghost_profiles[store_id] = full_profiles[store_id]
		if not GameState.store_evidence_ledger.has(store_id):
			GameState.store_evidence_ledger[store_id] = []

	GameState.powered_circuits["EscalatorCircuit_F2_To_F3"] = true
	for f in range(1, 8):
		GameState.freight_elevator_unlocked_floors[f] = true

	# Register all 12 Hidden Lore Collectibles across the 3 Categories (4 Ledgers + 4 Radio 104.7 FM + 4 UV Ciphers)
	var full_lore_catalog: Dictionary = {
		"LORE_BRIEFING_00": {
			"title": "Contract #89-AV: Missing Auditor Elena Vance",
			"text": "Target: Elena Vance, Urban Architectural Auditor. Last seen 72h ago entering the condemned Arcadia Oval Megamall. Upon your entry, the street airlock sealed behind you and all clocks froze at 03:17 AM.",
		},
		# --- 4 Ghost Writing Ledger Pages ---
		"LORE_LEDGER_EAST_F0_02": {
			"title": "Ledger #1: Arthur Pendelton's Inventory Tally (1994)",
			"text": "Handwritten in neat clerk ink: 'Just one last shelf count before I clock out... The hallways bend 2.5 degrees every ten meters. Elena Vance climbed the North Escalator toward the Sky Arboretum.'",
		},
		"LORE_LEDGER_WEST_F1_03": {
			"title": "Ledger #2: Static Evelyn's Transformer Log",
			"text": "Singed stockroom ledger: 'The display step-up transformer is feeding off the Oval Atrium's stabilizing coil. Whenever the voltage spikes, mannequins in Victoria's Phantoms turn their heads toward the ceiling.'",
		},
		"LORE_LEDGER_EAST_F2_01": {
			"title": "Ledger #3: The Lost Shopper's Department Maze Diary",
			"text": "Written on the back of a 1997 layaway receipt: 'I followed the exit signs for three days, but the concourse curved right back to the fountain overlook. A woman with an architectural clipboard told me to listen to 104.7 FM.'",
		},
		"LORE_LEDGER_ARK_F3_01": {
			"title": "Ledger #4: The Chief Architect's Geodesic Ark Dossier (1989)",
			"text": "Stamped CONFIDENTIAL — PROJECT ARCADIA ARK: 'This structure was never a suburban shopping mall. The elliptical gallery rings and geodesic hull ribs form a non-terrestrial spatial containment coil.'",
		},
		# --- 4 Spirit Box 104.7 FM Secret Broadcasts ---
		"LORE_RADIO_104_7": {
			"title": "104.7 FM Broadcast #1: Hebe Fountain Beacon",
			"text": "[Static crackle] 'This is Elena Vance. Stepped escalators can't carry your Equipment or Light Carts! When you ascend to a new floor, locate and repair the Service Freight Elevator shaft so you can bring your carts up!'",
		},
		"LORE_RADIO_AIRLOCK_02": {
			"title": "104.7 FM Broadcast #2: Automated Delivery Airlock Transmission",
			"text": "[Number-station chimes: 0-3-1-7] 'Elena Vance log #2: The North Delivery Airlock drops fresh hot-pot rations and relay fuses from outside our time-slice. Someone on the outside knows Mall City is still inhabited.'",
		},
		"LORE_RADIO_BREAKER_03": {
			"title": "104.7 FM Broadcast #3: Escalator Breaker Harmonics",
			"text": "[High-voltage hum] 'Elena Vance log #3: Every escalator breaker circuit you power up locks another phase of the Oval Atrium's magnetic ring. Keep climbing to Floor 3 — my camp is beside the Sky Arboretum Oculus!'",
		},
		"LORE_RADIO_OCULUS_04": {
			"title": "104.7 FM Broadcast #4: Roof Oculus & Geodesic Ark Core",
			"text": "[Clear voice transmission] 'You made it to the Upper Ark Control Ring! Stabilize the Containment Core console here with me to unseal the Roof Oculus — or ride the endless upper floors if you dare!'",
		},
		# --- 4 UV Hidden Wall Ciphers ---
		"LORE_UV_CIPHER_01": {
			"title": "UV Wall Cipher #1: East Storefront Shutter Warning",
			"text": "Fluorescent UV ink painted across the storefront frame: 'INVESTIGATORS: Check the Ledger & Spirit Box tells before attempting a Peaceful Resolution. Corrupted ghosts will Enrage if you try peace—use the Proton Pack & Canister!'",
		},
		"LORE_UV_CIPHER_02": {
			"title": "UV Wall Cipher #2: West Boutique Backroom Arrow",
			"text": "Glowing ultraviolet schematics behind Victoria's Phantoms: '45% GRAY-SPACE DIM CALMS PHOTOPHOBIC ENTITIES. Use your Light Remote Gun on ceiling troffers before the 3-second Visible Warning turns into a 20-second Hunt!'",
		},
		"LORE_UV_CIPHER_03": {
			"title": "UV Wall Cipher #3: Floor 2 Promenade Boundary Map",
			"text": "Ultraviolet survey markings along the Floor 2 balustrade: 'WARNING: Floor 1 ghosts never cross their doorway threshold, but Floor 2+ ghosts WILL pursue you 10 meters into the concourse for 6 seconds! Park the Light Cart circle edge across the door!'",
		},
		"LORE_UV_CIPHER_04": {
			"title": "UV Wall Cipher #4: Floor 3 Ark Core Stabilization Code",
			"text": "UV architectural cipher across the Geodesic Ark Control bulkhead: 'COIL HARMONIC 104.7 MHz — ELENA VANCEFINAL CAMP AT [0, 17, -18]. Override the drafting table & extraction console to complete Contract #89-AV!'",
		},
	}

	for lore_id in full_lore_catalog.keys():
		GameState.lore_catalog[lore_id] = full_lore_catalog[lore_id]


# ------------------------------------------------------------------------------
# 2. Floor 3 Architecture (Y = 17.0m), Drifting Spline Concourses, Stores #2/#4/#6,
#    Elena Vance's Final Camp, All 5 Hiding Spots, & Automated Delivery Airlock Bay
# ------------------------------------------------------------------------------
func _build_floor_3_sky_arboretum_and_ark_deck() -> void:
	# Floor 3 Outer Concourse Gallery Slab (AtriumRadius (28.5m, 30.0m) -> OuterRectangle (±46m, ±42m))
	# AND the Floor 2 -> Floor 3 Void-Spanning Escalator Pair at Z = +22.20m are already generated by MallCurvedAtriumRing!
	var p := get_parent()
	if p and "curved_atrium" in p and is_instance_valid(p.get("curved_atrium")):
		var ca := p.get("curved_atrium") as MallCurvedAtriumRing
		if ca and ca.escalators_up.has(2):
			escalator_f2_to_f3 = ca.escalators_up[2] as MallEscalator

	# Sky Arboretum Overgrown Biotope Planters on Floor 3 Outer Concourse (Y = 17.0m)
	var arboretum_planters := [
		Vector3(-6.5, 17.0, -34.0),
		Vector3(5.5, 17.0, -34.0),
		Vector3(-34.5, 17.0, 12.0),
		Vector3(34.5, 17.0, 12.0),
	]
	for ppos in arboretum_planters:
		_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25369_LargeAtriumPlanter.fbx", ppos, Vector3(1.9, 0.68, 1.9), Color(0.56, 0.54, 0.50), true)
		_spawn_model_or_box("res://ExternalAssets/Mall/PolyHaven/ImportReady/SM_PachiraD.fbx", ppos + Vector3(0.0, 0.48, 0.0), Vector3(0.95, 2.2, 0.95), Color(0.24, 0.58, 0.32), false)

	# Abandoned Security & Audit Office on Floor 3 West Wing (Vector3(-34.5, 17.0, 3.0))
	_build_store_sign_header(Vector3(-32.0, 21.15, 3.0), Vector3(0, 90, 0), "ABANDONED SECURITY & ARCHITECTURAL AUDIT OFFICE [F3_AUDIT]", Color(0.84, 0.94, 0.98))
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25310_CashDesk120.fbx", Vector3(-34.5, 17.0, 3.0), Vector3(1.3, 1.05, 0.8), Color(0.42, 0.48, 0.54), true)
	_create_floating_label(
		Vector3(-34.5, 18.8, 3.0),
		"Abandoned Security & Architectural Audit Office (Floor 3 — Y = 17.0m)\n• Elena Vance's Inspection Blueprints & Geodesic Ark Stabilization Logs",
		Color(0.84, 0.94, 0.98)
	)

	# Elena Vance's Final Camp & Geodesic Ark Extraction Console at Vector3(0.0, 17.0, -34.0)
	_build_elena_vance_final_camp_and_extraction_console()

	# Floor 3 Ceiling Light Fixtures
	var f3_lights := [
		Vector3(-8.0, 23.8, -34.0),
		Vector3(0.0, 23.8, -34.0),
		Vector3(8.0, 23.8, -34.0),
		Vector3(-34.5, 23.8, 3.0),
		Vector3(34.5, 23.8, 3.0),
	]
	for lpos in f3_lights:
		var fix := MallLightFixture.new()
		fix.store_id = "Floor3_SkyArboretum"
		fix.light_color = Color(0.96, 0.95, 0.90)
		fix.base_energy = 1.45
		fix.position = lpos
		add_child(fix)

func _build_elena_vance_final_camp_and_extraction_console() -> void:
	var camp_root := Node3D.new()
	camp_root.name = "ElenaVanceFinalCampF3"
	camp_root.position = elena_camp_f3_pos
	add_child(camp_root)

	# Geodesic Ark Containment Console Dais
	_create_static_box(camp_root, Vector3(0.0, 0.22, 0.0), Vector3(3.8, 0.44, 3.2), Color(0.68, 0.70, 0.74), 0.3)
	_create_static_box(camp_root, Vector3(0.0, 0.95, -0.45), Vector3(1.6, 1.05, 0.75), Color(0.36, 0.52, 0.60), 0.25)

	# Glowing Containment Core Holo-Ring above console
	var ring_mi := MeshInstance3D.new()
	var torus := TorusMesh.new()
	torus.inner_radius = 0.68
	torus.outer_radius = 0.82
	ring_mi.mesh = torus
	ring_mi.position = Vector3(0.0, 2.15, -0.45)
	ring_mi.rotation_degrees = Vector3(65.0, 0.0, 0.0)
	var holo_mat := StandardMaterial3D.new()
	holo_mat.albedo_color = Color(0.65, 0.92, 0.98)
	holo_mat.emission_enabled = true
	holo_mat.emission = Color(0.45, 0.85, 0.95)
	holo_mat.emission_energy_multiplier = 1.25
	ring_mi.material_override = holo_mat
	camp_root.add_child(ring_mi)

	var camp_light := OmniLight3D.new()
	camp_light.position = Vector3(0.0, 2.4, 0.0)
	camp_light.light_color = Color(0.88, 0.95, 1.0)
	camp_light.light_energy = 1.35
	camp_light.omni_range = 9.5
	camp_root.add_child(camp_light)

	# Elena Vance Character Model at Final Camp (1.74m 1:1 scale)
	_spawn_model_or_box(
		"res://SourceAssets/Characters/NormalizedLowPoly/Elena_Vance_LowPoly.glb",
		elena_camp_f3_pos + Vector3(-1.15, 0.44, 0.25),
		Vector3(0.5, 1.74, 0.5),
		Color(0.55, 0.85, 0.92),
		false
	)

	# Floor 3 Freight Elevator Call & Overdrive Notice near Shaft (X = 7.1, Y = 17.0, Z = -28.9)
	_create_static_box(self, Vector3(7.1, 17.68, -28.9), Vector3(0.38, 1.35, 0.38), Color(0.42, 0.58, 0.50), 0.35)
	_create_floating_label(
		Vector3(7.1, 18.95, -28.9),
		"[F / E] Floor 3 Service Freight Elevator Landing (Y = 17.0m)\n• Call or Ride Freight Elevator with Push Cart & Light Cart between Floors 1..7+!",
		Color(0.82, 0.96, 0.88)
	)

	_create_floating_label(
		elena_camp_f3_pos + Vector3(0.0, 3.15, 0.0),
		"[F] ELENA VANCE'S FINAL CAMP & GEODESIC ARK EXTRACTION CONSOLE (Floor 3 — Y = 17.0m)\n" +
		"• Reunite with Auditor Elena Vance & Stabilize the Geodesic Ark Containment Core\n" +
		"• Open Contract #89-AV Endgame Summary OR Launch Floors 4–7 & Procedural Floors 8+ Endless Escalation Mode! (Or press [F10])",
		Color(0.84, 0.96, 0.94)
	)

## Registers Drifting H-Concourse Spline Corridor landmarks (floor & terrazzo runners are built seamlessly by MallStackedHWings with zero coplanar Z-fighting!).
func _build_drifting_concourse_wings() -> void:
	_build_drifting_concourse_wing(Vector3(-15.5, 0.0, -45.0), Vector3(0.0, 0.0, -1.0), 4, 1337, "North-West Drifting H-Concourse Wing")
	_build_drifting_concourse_wing(Vector3(-15.5, 8.50, -45.0), Vector3(0.0, 0.0, -1.0), 3, 2049, "Floor 2 Upper Promenade Drifting H-Concourse")
	_build_drifting_concourse_wing(Vector3(-15.5, 17.0, -45.0), Vector3(0.0, 0.0, -1.0), 3, 4099, "Floor 3 Sky Arboretum Drifting H-Concourse")

func _build_drifting_concourse_wing(start_pos: Vector3, _dir_vec: Vector3, _num_bays: int, _seed_val: int, wing_label: String) -> Node3D:
	var wing_root := Node3D.new()
	wing_root.name = wing_label.replace(" ", "_")
	add_child(wing_root)

	_create_floating_label(
		start_pos + Vector3(0.0, 2.55, 4.0),
		"%s (AMallSplineCorridor)\n[Curved Sightline Hallway & Stacked-H Concourse]",
		Color(0.84, 0.90, 0.96)
	)
	return wing_root

func _build_missing_signature_stores_and_ghosts() -> void:
	# --------------------------------------------------------------------------
	# Store #2: West_F1_02 — "Victoria's Phantoms" (LingerieParody, South Wing Bay 0 West: X = -46.0..-20.0, Z = +42.0..+74.0, Y = 0.0m — 32m Wide x 26m Deep!)
	# Store walls & portals are built by MallStackedHWings!
	# --------------------------------------------------------------------------
	_create_static_box(self, Vector3(-20.0, 0.0075, 58.0), Vector3(0.9, 0.015, 5.6), Color(0.84, 0.62, 0.76), 0.3)
	_build_store_sign_header(Vector3(-19.65, 4.45, 58.0), Vector3(0, 90, 0), "VICTORIA'S PHANTOMS BOUTIQUE [West_F1_02 — 32m Wide x 26m Deep]", Color(0.96, 0.82, 0.90))

	# Furnish West_F1_02 with dense mannequin cluster, mirrors & Spotlight Dimmer Console across its 32m x 26m bay!
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25257_FemaleMannequin.fbx", Vector3(-22.8, 0.0, 51.2), Vector3(0.55, 1.78, 0.45), Color(0.92, 0.84, 0.90), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25265_FemaleWalkingMannequin.fbx", Vector3(-22.8, 0.0, 64.8), Vector3(0.55, 1.78, 0.55), Color(0.92, 0.84, 0.90), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25267_HeadlessMannequin.fbx", Vector3(-31.5, 0.0, 52.2), Vector3(0.55, 1.62, 0.45), Color(0.88, 0.80, 0.88), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25208_StraightHangingRail.fbx", Vector3(-39.5, 0.0, 52.2), Vector3(1.35, 1.45, 0.60), Color(0.88, 0.80, 0.88), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25208_StraightHangingRail.fbx", Vector3(-39.5, 0.0, 63.8), Vector3(1.35, 1.45, 0.60), Color(0.88, 0.80, 0.88), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25198_WallBayMirror.fbx", Vector3(-45.0, 0.0, 58.0), Vector3(0.45, 2.40, 1.20), Color(0.68, 0.78, 0.92), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25310_CashDesk120.fbx", Vector3(-25.5, 0.0, 58.0), Vector3(1.25, 1.05, 0.75), Color(0.55, 0.38, 0.46), true)

	var wf102_lights := [
		Vector3(-24.5, 5.75, 50.0), Vector3(-24.5, 5.75, 66.0),
		Vector3(-33.0, 5.75, 50.0), Vector3(-33.0, 5.75, 66.0),
		Vector3(-41.5, 5.75, 50.0), Vector3(-41.5, 5.75, 66.0),
	]
	for lpos in wf102_lights:
		var fix := MallLightFixture.new()
		fix.store_id = "West_F1_02"
		fix.light_color = Color(0.98, 0.92, 0.94)
		fix.base_energy = 1.35
		fix.position = lpos
		add_child(fix)

	store_ghost_west_f1_02 = StoreGhost.new()
	store_ghost_west_f1_02.store_id = "West_F1_02"
	store_ghost_west_f1_02.floor_level = 1
	store_ghost_west_f1_02.can_briefly_pursue_outside_store = false
	store_ghost_west_f1_02.doorway_threshold_x = -999.0
	store_ghost_west_f1_02.position = Vector3(-30.0, 0.0, 58.0)
	add_child(store_ghost_west_f1_02)

	_create_floating_label(
		Vector3(-25.5, 2.35, 58.0),
		"[F] Victoria's Phantoms Spotlight Dimmer Console [West_F1_02 — The Display Coordinator]\n" +
		"• Peaceful Ritual: Press [F] (or use Light Remote Gun) to Dim Store Lights to 45% Gray-Space Dim!",
		Color(0.96, 0.84, 0.92)
	)

	# --------------------------------------------------------------------------
	# Store #4: West_F2_01 — "BRICKWORKZ Modular Toy Store" (ParodyLegoBrickStore, Floor 2 North Wing Bay 0 West: X = -46.0..-20.0, Z = -74.0..-42.0, Y = 8.50m — 32m Wide x 26m Deep!)
	# --------------------------------------------------------------------------
	_build_store_sign_header(Vector3(-19.65, 12.65, -58.0), Vector3(0, 90, 0), "BRICKWORKZ MODULAR TOY STORE [West_F2_01 — 32m Wide x 26m Deep]", Color(0.96, 0.90, 0.68))
	_create_static_box(self, Vector3(-20.0, 8.508, -58.0), Vector3(0.9, 0.015, 5.6), Color(0.86, 0.74, 0.32), 0.3)

	# Center Feature Podium & Plastic Brick Noise Clusters in West_F2_01
	_create_static_box(self, Vector3(-25.5, 8.95, -58.0), Vector3(1.4, 0.90, 1.4), Color(0.76, 0.32, 0.28), 0.35)
	_create_static_box(self, Vector3(-23.2, 8.53, -55.2), Vector3(0.85, 0.06, 0.85), Color(0.84, 0.72, 0.28), 0.35)
	_create_static_box(self, Vector3(-31.2, 8.53, -61.0), Vector3(0.85, 0.06, 0.85), Color(0.32, 0.52, 0.78), 0.35)
	_spawn_model_or_box("res://ExternalAssets/Mall/PolyHaven/ImportReady/SM_RetailShelves01.fbx", Vector3(-39.5, 8.50, -64.8), Vector3(1.1, 2.1, 0.55), Color(0.48, 0.52, 0.56), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/PolyHaven/ImportReady/SM_RetailShelves01.fbx", Vector3(-39.5, 8.50, -51.2), Vector3(1.1, 2.1, 0.55), Color(0.48, 0.52, 0.56), true)

	var wf201_lights := [
		Vector3(-24.5, 14.2, -66.0), Vector3(-24.5, 14.2, -50.0),
		Vector3(-33.0, 14.2, -66.0), Vector3(-33.0, 14.2, -50.0),
		Vector3(-41.5, 14.2, -66.0), Vector3(-41.5, 14.2, -50.0),
	]
	for lpos in wf201_lights:
		var fix := MallLightFixture.new()
		fix.store_id = "West_F2_01"
		fix.light_color = Color(0.98, 0.95, 0.88)
		fix.base_energy = 1.35
		fix.position = lpos
		add_child(fix)

	store_ghost_west_f2_01 = StoreGhost.new()
	store_ghost_west_f2_01.store_id = "West_F2_01"
	store_ghost_west_f2_01.floor_level = 2
	store_ghost_west_f2_01.can_briefly_pursue_outside_store = true
	store_ghost_west_f2_01.doorway_threshold_x = -999.0
	store_ghost_west_f2_01.position = Vector3(-30.0, 8.50, -58.0)
	add_child(store_ghost_west_f2_01)

	_create_floating_label(
		Vector3(-25.5, 10.65, -58.0),
		"[F] BRICKWORKZ Center Modular Feature Podium [West_F2_01 — The Night-Shift Builder]\n" +
		"• Peaceful Ritual: Place Missing Modular Display Piece [F] | Upper-Floor Ghost (6s Outside Pursuit)",
		Color(0.96, 0.90, 0.68)
	)

	# Add Checkout Service Bell Label in Store #5 (East_F2_01 — The Lost Shopper)
	_create_floating_label(
		Vector3(25.5, 10.45, -62.0),
		"[F] Arcadia Evening Wear Checkout Service Bell [East_F2_01 — The Lost Shopper]\n" +
		"• Peaceful Ritual: Speak \"Where are you?\" on 98.3 FM & Ring Service Bell [F] (+Ledger #3)",
		Color(0.84, 0.94, 0.98)
	)

	# --------------------------------------------------------------------------
	# Store #6: Center_F3_01 — "Mall City Civic Facility & Upper Ark Control" (SurvivorCivicFacility, Floor 3 North Wing Bay 0 East: X = 20.0..46.0, Z = -74.0..-42.0, Y = 17.0m — 32m Wide x 26m Deep!)
	# --------------------------------------------------------------------------
	_create_static_box(self, Vector3(20.0, 17.008, -58.0), Vector3(0.9, 0.015, 5.6), Color(0.68, 0.85, 0.92), 0.3)
	_build_store_sign_header(
		Vector3(19.65, 21.15, -58.0),
		Vector3(0, -90, 0),
		"MALL CITY CIVIC FACILITY & UPPER ARK CONTROL [Center_F3_01 — 32m Wide x 26m Deep]",
		Color(0.84, 0.95, 0.98)
	)

	# Furnish Center_F3_01 with Ark Blueprint Drafting Table, Pharmacy Clinic Shelves & DIY Workshop across its 32m x 26m bay!
	_create_static_box(self, Vector3(25.5, 17.48, -58.0), Vector3(1.8, 0.96, 1.2), Color(0.46, 0.54, 0.60), 0.35)
	_spawn_model_or_box("res://ExternalAssets/Mall/PolyHaven/decorative_book_set_01/SM_BookHardcover02.fbx", Vector3(25.5, 17.98, -58.0), Vector3(0.48, 0.12, 0.36), Color(0.32, 0.68, 0.82), false)
	_spawn_model_or_box("res://ExternalAssets/Mall/PolyHaven/ImportReady/SM_RetailShelves01.fbx", Vector3(32.8, 17.0, -64.2), Vector3(1.1, 2.1, 0.55), Color(0.48, 0.52, 0.56), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/PolyHaven/ImportReady/SM_RetailShelves01.fbx", Vector3(41.8, 17.0, -64.2), Vector3(1.1, 2.1, 0.55), Color(0.48, 0.52, 0.56), true)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25339_GlazedDisplayCounter.fbx", Vector3(23.5, 17.0, -64.2), Vector3(1.25, 1.02, 0.65), Color(0.58, 0.72, 0.80), true)

	var cf301_lights := [
		Vector3(24.5, 22.6, -66.0), Vector3(24.5, 22.6, -50.0),
		Vector3(33.0, 22.6, -66.0), Vector3(33.0, 22.6, -50.0),
		Vector3(41.5, 22.6, -66.0), Vector3(41.5, 22.6, -50.0),
	]
	for lpos in cf301_lights:
		var fix := MallLightFixture.new()
		fix.store_id = "Center_F3_01"
		fix.light_color = Color(0.96, 0.95, 0.92)
		fix.base_energy = 1.35
		fix.position = lpos
		add_child(fix)

	store_ghost_center_f3_01 = StoreGhost.new()
	store_ghost_center_f3_01.store_id = "Center_F3_01"
	store_ghost_center_f3_01.floor_level = 3
	store_ghost_center_f3_01.can_briefly_pursue_outside_store = true
	store_ghost_center_f3_01.position = Vector3(30.0, 17.0, -58.0)
	add_child(store_ghost_center_f3_01)

	_create_floating_label(
		Vector3(25.5, 19.25, -58.0),
		"[F] Geodesic Ark Blueprint Drafting Table [Center_F3_01 — The Chief Architect]\n" +
		"• Peaceful Ritual: Tune Spirit Box to 104.7 FM & Inspect Blueprints [F] (+Ledger #4 & Ark Evidence!)",
		Color(0.45, 0.98, 1.0)
	)

	# Spawn Floating Ghost Orbs in Upper-Floor Stores (West_F2_01, East_F2_01, Center_F3_01)
	var orb_configs := [
		{"store_id": "West_F2_01", "pos": Vector3(-24.5, 9.85, -56.2), "title": "BRICKWORKZ IR Ghost Orb #1"},
		{"store_id": "West_F2_01", "pos": Vector3(-29.0, 10.10, -59.8), "title": "BRICKWORKZ Camera-Shy Entity Orb #2"},
		{"store_id": "East_F2_01", "pos": Vector3(24.5, 9.85, -56.5), "title": "Arcadia Evening Wear IR Ghost Orb #1"},
		{"store_id": "East_F2_01", "pos": Vector3(29.5, 10.15, -59.5), "title": "Arcadia Evening Wear IR Ghost Orb #2"},
		{"store_id": "Center_F3_01", "pos": Vector3(24.5, 18.4, -56.5), "title": "Ark Control Telemetry Ghost Orb"},
	]
	for cfg in orb_configs:
		var orb := VideoAndAudioSurveillance.GhostOrbActor.new()
		orb.store_id = str(cfg["store_id"])
		orb.base_position = cfg["pos"] as Vector3
		orb.position = cfg["pos"] as Vector3
		add_child(orb)
		orb.set_ir_revealed(true)

## Builds all 5 AMallHidingSpot types in group "hiding_spots" (Docs/GameDesignBible.md Section 5)
func _build_all_five_hiding_spot_types() -> void:
	# 1. FittingRoomBooth (in Victoria's Phantoms West_F1_02)
	var spot_fitting := MallHidingSpotActor.new()
	spot_fitting.spot_type = "FittingRoomBooth"
	spot_fitting.spot_display_name = "Enclosed Fitting Room Cubicle Booth"
	spot_fitting.position = Vector3(-31.5, 0.0, 64.2)
	add_child(spot_fitting)
	_spawn_model_or_box("res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25284_FittingCubicleClosed.fbx", spot_fitting.position, Vector3(1.11, 2.25, 1.26), Color(0.42, 0.30, 0.48), true)

	# 2. CircularClothingRack (in Arcadia Evening Wear East_F2_01)
	var spot_rack := MallHidingSpotActor.new()
	spot_rack.spot_type = "CircularClothingRack"
	spot_rack.spot_display_name = "Dense Circular Clothing Rack"
	spot_rack.position = Vector3(30.5, 8.50, -51.5)
	add_child(spot_rack)

	# 3. SaunaWoodenBooth (in Whole-West-Side Floor 1 Himalayan Pink Salt & Tangquan Sauna Liminal Biome!)
	var spot_sauna := MallHidingSpotActor.new()
	spot_sauna.spot_type = "SaunaWoodenBooth"
	spot_sauna.spot_display_name = "Steamy Cedar Sauna Privacy Booth"
	spot_sauna.position = Vector3(-64.0, 0.12, 18.0)
	add_child(spot_sauna)

	# 4. SleepPodBunk (Floor 2 Mattress & Sleep-Pod Showroom at Vector3(-42.5, 8.50, -5.5))
	var spot_pod := MallHidingSpotActor.new()
	spot_pod.spot_type = "SleepPodBunk"
	spot_pod.spot_display_name = "Curtained Mattress & Sleep-Pod Bunk"
	spot_pod.position = Vector3(-42.5, 8.50, -5.5)
	add_child(spot_pod)
	_create_static_box(self, Vector3(-42.5, 9.15, -5.5), Vector3(2.1, 1.3, 1.25), Color(0.26, 0.36, 0.52), 0.4)

	# 5. ServiceUtilityCloset (Back-of-House Janitor & Utility Closet with Lock/Key support at Vector3(14.5, 8.50, -35.5))
	var spot_closet := MallHidingSpotActor.new()
	spot_closet.spot_type = "ServiceUtilityCloset"
	spot_closet.spot_display_name = "Back-of-House Janitor & Service Utility Closet"
	spot_closet.is_locked = true
	spot_closet.requires_master_key = true
	spot_closet.position = Vector3(14.5, 8.50, -35.5)
	add_child(spot_closet)
	_create_static_box(self, Vector3(14.5, 9.65, -35.5), Vector3(1.35, 2.3, 1.25), Color(0.28, 0.32, 0.36), 0.5)

## Builds the Automated Delivery Airlock Bay (EMallStoreArchetype::DeliveryAirlockBay) at Vector3(-6.0, 0.0, -34.0)
func _build_automated_delivery_airlock_bay() -> void:
	_create_static_box(self, delivery_airlock_pos + Vector3(0.0, 1.6, -0.6), Vector3(3.4, 3.2, 1.4), Color(0.22, 0.26, 0.30), 0.4)
	_create_static_box(self, delivery_airlock_pos + Vector3(0.0, 0.45, 0.25), Vector3(2.2, 0.90, 0.95), Color(0.32, 0.38, 0.44), 0.35)

	var chute_light := OmniLight3D.new()
	chute_light.position = delivery_airlock_pos + Vector3(0.0, 2.4, 0.4)
	chute_light.light_color = Color(0.35, 0.95, 0.68)
	chute_light.light_energy = 2.2
	chute_light.omni_range = 6.5
	add_child(chute_light)

	airlock_label_3d = _create_floating_label(
		delivery_airlock_pos + Vector3(0.0, 2.85, 0.0),
		"[F] Automated Delivery Airlock Bay (EMallStoreArchetype::DeliveryAirlockBay)\n" +
		"• Dispenses Free Emergency Rations (+35 HP / +50 Stamina) & Relay Supplies\n" +
		"• Tune Spirit Box to 104.7 FM or Press [F] to Inspect Airlock Serial Plate (LORE_RADIO_AIRLOCK_02)",
		Color(0.45, 1.0, 0.78)
	)

func _build_food_court_four_meal_selector_sign() -> void:
	var meal_info: Dictionary = FOOD_COURT_MEALS[selected_food_meal_index]
	food_court_menu_label_3d = _create_floating_label(
		Vector3(-18.0, 2.55, -32.0),
		"NORTH NIGHT-MARKET 4-MEAL MENU [Press E at Table to Cycle Meal | Press F to Sit & Eat]:\n" +
		"Active Selection [%d/4]: %s\n(%s)" % [
			selected_food_meal_index + 1,
			str(meal_info["title"]),
			str(meal_info["description"]),
		],
		Color(1.0, 0.90, 0.42)
	)

func _spawn_12_hidden_lore_triggers_and_ciphers() -> void:
	# Spawn the 4 UV Hidden Wall Ciphers (illuminated by UV Blacklight or inspected with [F])
	var cipher_configs := [
		{
			"lore_id": "LORE_UV_CIPHER_01",
			"title": "UV Wall Cipher #1 [East_F0_02 Shutter]: Read Tells Before Peace!",
			"store_id": "East_F0_02",
			"pos": Vector3(19.85, 1.35, -55.4),
		},
		{
			"lore_id": "LORE_UV_CIPHER_02",
			"title": "UV Wall Cipher #2 [West_F1_02 Backroom]: 45% Gray-Space Dim Calms Display Coordinator!",
			"store_id": "West_F1_02",
			"pos": Vector3(-31.8, 1.35, 58.0),
		},
		{
			"lore_id": "LORE_UV_CIPHER_03",
			"title": "UV Wall Cipher #3 [Floor 2 Promenade]: Upper Ghosts Pursue 6s Outside Store!",
			"store_id": "West_F2_01",
			"pos": Vector3(-19.85, 9.85, -58.0),
		},
		{
			"lore_id": "LORE_UV_CIPHER_04",
			"title": "UV Wall Cipher #4 [Floor 3 Ark Core]: Tune 104.7 FM at Drafting Table!",
			"store_id": "Center_F3_01",
			"pos": Vector3(19.85, 18.35, -58.0),
		},
	]
	for cfg in cipher_configs:
		var decal := PlaceableEvidenceActors.UVEvidencePrint.new()
		decal.print_title = str(cfg["title"])
		decal.store_id = str(cfg["store_id"])
		decal.position = cfg["pos"] as Vector3
		add_child(decal)
		_uv_cipher_entries.append({
			"node": decal,
			"lore_id": str(cfg["lore_id"]),
		})

	# Additional UV Evidence Prints inside West_F1_02, West_F1_03, West_F2_01, and Center_F3_01
	var extra_uv_prints := [
		{"store_id": "West_F1_02", "pos": Vector3(-24.2, 0.02, 56.2), "title": "UV Handprint [Display Coordinator Silk Rack]"},
		{"store_id": "West_F1_03", "pos": Vector3(-24.2, 0.02, -57.2), "title": "UV Footprint [Static Evelyn Transformer Isle]"},
		{"store_id": "West_F2_01", "pos": Vector3(-23.8, 8.52, -58.0), "title": "UV Blueprint Arrow [Night-Shift Builder Podium]"},
		{"store_id": "Center_F3_01", "pos": Vector3(23.8, 17.02, -58.0), "title": "UV Handprint [Chief Architect Drafting Table]"},
	]
	for ucfg in extra_uv_prints:
		var uvp := PlaceableEvidenceActors.UVEvidencePrint.new()
		uvp.store_id = str(ucfg["store_id"])
		uvp.print_title = str(ucfg["title"])
		uvp.position = ucfg["pos"] as Vector3
		add_child(uvp)


# ------------------------------------------------------------------------------
# 3. 4-Meal North Night-Market Food Court Selector (`cycle_food_court_meal()`)
# ------------------------------------------------------------------------------
func cycle_food_court_meal() -> String:
	selected_food_meal_index = (selected_food_meal_index + 1) % FOOD_COURT_MEALS.size()
	var meal_info: Dictionary = FOOD_COURT_MEALS[selected_food_meal_index]
	var title_str: String = str(meal_info["title"])
	if food_court_menu_label_3d:
		food_court_menu_label_3d.text = (
			"NORTH NIGHT-MARKET 4-MEAL MENU [Press E at Table to Cycle Meal | Press F to Sit & Eat]:\n" +
			"Active Selection [%d/4]: %s\n(%s)" % [
				selected_food_meal_index + 1,
				title_str,
				str(meal_info["description"]),
			]
		)
	food_court_meal_cycled.emit(selected_food_meal_index, title_str)
	GameState.status_message.emit(
		"Food Court Menu [%d/4]: Selected %s — Press [F] at the table to sit and eat!" % [
			selected_food_meal_index + 1, title_str
		],
		Color(1.0, 0.88, 0.42)
	)
	return title_str

func apply_selected_meal_buff(p: PlayerController, lc: MallCartSystem = null) -> void:
	if not is_instance_valid(p):
		return
	var meal_info: Dictionary = FOOD_COURT_MEALS[selected_food_meal_index]
	p.food_buff_name = str(meal_info["title"])
	p.food_buff_timer = 90.0
	if bool(meal_info.get("grants_chill_shield", false)):
		p.pink_salt_ward_timer = maxf(p.pink_salt_ward_timer, 90.0)
	var boost_amt: float = float(meal_info.get("cart_battery_boost", 45.0))
	if is_instance_valid(lc):
		lc.boost_battery(boost_amt)
	else:
		for node in get_tree().get_nodes_in_group("light_carts"):
			var cart := node as MallCartSystem
			if cart:
				cart.boost_battery(boost_amt)
	GameState.status_message.emit(
		"FINISHED MEAL: %s! (+40 HP, +65 Stamina, +%.0f%% Light Cart Charge, 90s Buff Active!)" % [
			str(meal_info["title"]), boost_amt
		],
		Color(0.42, 1.0, 0.68)
	)


# ------------------------------------------------------------------------------
# 4. Intro Contract Briefing Card & Endgame Extraction / Endless Escalation Modal
# ------------------------------------------------------------------------------
func build_briefing_and_endgame_ui(root_ctrl: Control) -> void:
	if root_ctrl == null or briefing_card_panel != null:
		return

	# Ensure F10 action exists for quick Endgame / Endless Escalation Summary toggle
	if not InputMap.has_action("toggle_endgame_summary"):
		InputMap.add_action("toggle_endgame_summary")
		var ev_f10 := InputEventKey.new()
		ev_f10.physical_keycode = KEY_F10
		InputMap.action_add_event("toggle_endgame_summary", ev_f10)

	# A. Top-Center Intro Contract Briefing Card (#89-AV: Missing Auditor Elena Vance)
	briefing_card_panel = PanelContainer.new()
	briefing_card_panel.set_anchors_preset(Control.PRESET_CENTER_TOP)
	briefing_card_panel.offset_left = -380
	briefing_card_panel.offset_right = 380
	briefing_card_panel.offset_top = 18
	briefing_card_panel.offset_bottom = 205

	var card_style := StyleBoxFlat.new()
	card_style.bg_color = Color(0.05, 0.09, 0.14, 0.94)
	card_style.border_width_left = 3
	card_style.border_width_top = 3
	card_style.border_width_right = 3
	card_style.border_width_bottom = 3
	card_style.border_color = Color(0.38, 0.95, 1.0, 1.0)
	card_style.corner_radius_top_left = 8
	card_style.corner_radius_top_right = 8
	card_style.corner_radius_bottom_left = 8
	card_style.corner_radius_bottom_right = 8
	card_style.content_margin_left = 16
	card_style.content_margin_right = 16
	card_style.content_margin_top = 10
	card_style.content_margin_bottom = 10
	briefing_card_panel.add_theme_stylebox_override("panel", card_style)
	root_ctrl.add_child(briefing_card_panel)

	var b_vbox := VBoxContainer.new()
	b_vbox.add_theme_constant_override("separation", 6)
	briefing_card_panel.add_child(b_vbox)

	var b_title := Label.new()
	b_title.text = "DISPATCH CONTRACT #89-AV — MISSING AUDITOR ELENA VANCE (03:17 AM)"
	b_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	b_title.add_theme_font_size_override("font_size", 16)
	b_title.modulate = Color(0.45, 0.98, 1.0)
	b_vbox.add_child(b_title)

	var b_body := RichTextLabel.new()
	b_body.bbcode_enabled = true
	b_body.fit_content = true
	b_body.scroll_active = false
	b_body.custom_minimum_size = Vector2(720, 98)
	b_body.text = (
		"[color=#d8f0ff]• [b]Premise:[/b] You entered the condemned [b]Arcadia Oval Megamall[/b] to locate missing urban architectural auditor [b]Elena Vance[/b]. Behind you, the street entrance vanished into solid concrete and all clocks froze at [b]03:17 AM[/b].\n" +
		"• [b]Investigation & Dual Resolution:[/b] Sweep 6 Haunted Stores across Floors 1–3, read Disposition Tells (Peaceful Ritual vs. Proton Ray Gun + Canister Trap), and collect [b]12 Hidden Lore Clues[/b] (4 Ledgers, 4 104.7 FM Beacons, 4 UV Ciphers).\n" +
		"• [b]Vertical Escape:[/b] Ride Escalators & the Service Freight Elevator to [b]Floor 3 (Y = 17.0m)[/b] to reunite with Elena Vance at [b][0, 17, -18][/b] (or press [b][F10][/b] for Endgame Summary & Endless Escalation Mode).[/color]"
	)
	b_vbox.add_child(b_body)

	var dismiss_btn := Button.new()
	dismiss_btn.text = "[Enter / F] Acknowledge Contract #89-AV & Dismiss Briefing Card"
	dismiss_btn.custom_minimum_size = Vector2(720, 30)
	dismiss_btn.modulate = Color(0.55, 1.0, 0.78)
	dismiss_btn.pressed.connect(func() -> void: briefing_card_panel.visible = false)
	b_vbox.add_child(dismiss_btn)

	# B. Endgame Summary & Endless Escalation Modal (`endgame_modal_panel`)
	endgame_modal_panel = PanelContainer.new()
	endgame_modal_panel.set_anchors_preset(Control.PRESET_CENTER)
	endgame_modal_panel.offset_left = -410
	endgame_modal_panel.offset_right = 410
	endgame_modal_panel.offset_top = -275
	endgame_modal_panel.offset_bottom = 275
	endgame_modal_panel.visible = false
	endgame_modal_panel.add_theme_stylebox_override("panel", card_style)
	root_ctrl.add_child(endgame_modal_panel)

	var eg_vbox := VBoxContainer.new()
	eg_vbox.add_theme_constant_override("separation", 10)
	endgame_modal_panel.add_child(eg_vbox)

	var eg_title := Label.new()
	eg_title.text = "CONTRACT #89-AV — ENDGAME EXTRACTION & ENDLESS ESCALATION SUMMARY"
	eg_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	eg_title.add_theme_font_size_override("font_size", 18)
	eg_title.modulate = Color(0.45, 1.0, 0.85)
	eg_vbox.add_child(eg_title)

	endgame_summary_rtl = RichTextLabel.new()
	endgame_summary_rtl.bbcode_enabled = true
	endgame_summary_rtl.custom_minimum_size = Vector2(770, 375)
	eg_vbox.add_child(endgame_summary_rtl)

	endgame_escalate_btn = Button.new()
	endgame_escalate_btn.text = "[1] Launch Next Endless Escalation Floor (Floor 4 — Remixed Ghosts, Gremlins & +$150 Bounty!)"
	endgame_escalate_btn.custom_minimum_size = Vector2(770, 42)
	endgame_escalate_btn.modulate = Color(1.0, 0.90, 0.38)
	endgame_escalate_btn.pressed.connect(func() -> void: launch_next_endless_escalation_floor())
	eg_vbox.add_child(endgame_escalate_btn)

	var close_eg_btn := Button.new()
	close_eg_btn.text = "[F10 / Esc] Close Summary & Continue Exploring Arcadia Megamall"
	close_eg_btn.custom_minimum_size = Vector2(770, 36)
	close_eg_btn.pressed.connect(func() -> void: show_endgame_summary(false))
	eg_vbox.add_child(close_eg_btn)

func show_endgame_summary(force_state: Variant = true) -> void:
	if endgame_modal_panel == null:
		_ensure_ui_built()
	if endgame_modal_panel == null:
		return
	var next_vis: bool = (not endgame_modal_panel.visible) if force_state == null else bool(force_state)
	endgame_modal_panel.visible = next_vis
	if next_vis:
		if briefing_card_panel:
			briefing_card_panel.visible = false
		_refresh_endgame_summary_text()
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
		endgame_summary_opened.emit(GameState.highest_floor_reached, GameState.resolved_stores.size(), GameState.discovered_lore.size())
	else:
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

func _refresh_endgame_summary_text() -> void:
	if endgame_summary_rtl == null:
		return

	var peaceful_count := 0
	var forceful_count := 0
	var store_lines: Array[String] = []
	var store_order := ["East_F0_02", "West_F1_02", "West_F1_03", "West_F2_01", "East_F2_01", "Center_F3_01"]

	for sid in store_order:
		var prof: Dictionary = GameState.ghost_profiles.get(sid, {})
		var g_name: String = str(prof.get("name", sid))
		var s_name: String = str(prof.get("store_name", sid))
		var res_method: String = str(GameState.resolved_stores.get(sid, "Active Haunting"))
		if "Peaceful" in res_method:
			peaceful_count += 1
		elif GameState.resolved_stores.has(sid):
			forceful_count += 1
		var ev_list: Array = GameState.store_evidence_ledger.get(sid, [])
		var ev_str := ", ".join(ev_list) if not ev_list.is_empty() else "Unconfirmed"
		store_lines.append("• [b]%s (%s — %s):[/b] [color=#ffe680]%s[/color] | Evidence: %s" % [
			sid, g_name, s_name, res_method, ev_str
		])

	# Count the 12 Hidden Lore Collectibles
	var collectible_lore_count := 0
	for k in GameState.discovered_lore.keys():
		if str(k) != "LORE_BRIEFING_00":
			collectible_lore_count += 1

	var campaign_tier_label := (
		"FLOORS 7+ ENDLESS ESCALATION MODE (Floor %d)" % GameState.highest_floor_reached
		if GameState.highest_floor_reached >= 7
		else "6-FLOOR STORY CAMPAIGN (Current Floor %d / Highest Floor %d)" % [GameState.current_floor, GameState.highest_floor_reached]
	)
	var next_floor := maxi(GameState.highest_floor_reached + 1, 4)
	if endgame_escalate_btn:
		var next_mode_tag := "ENDLESS ESCALATION WAVE" if next_floor >= 7 else "CAMPAIGN ESCALATION FLOOR"
		endgame_escalate_btn.text = "[1] Launch Next %s (Floor %d — Remixed Ghosts, Gremlins & +$150 Bounty!)" % [
			next_mode_tag, next_floor
		]

	endgame_summary_rtl.text = (
		"[b][color=#55ff99]CONTRACT STATUS:[/color][/b] %s\n" % (
			"ELENA VANCE LOCATED & GEODESIC ARK CONTAINMENT STABILIZED"
			if ark_containment_stabilized
			else "INVESTIGATION IN PROGRESS (Reach Floor 3 [0, 17, -18] to Stabilize Geodesic Ark)"
		) +
		"[b][color=#66f2ff]CAMPAIGN / ESCALATION TIER:[/color][/b] %s\n" % campaign_tier_label +
		"[b]Total Team Cash Balance:[/b] [color=#55ff88]$%d[/color] | [b]Persistent Total Death Tally:[/b] [color=#ff6666]%d Death(s)[/color]\n" % [
			GameState.team_cash, GameState.total_death_tally
		] +
		"[b]Stores Resolved:[/b] [color=#ffe680]%d / 6[/color] (%d Peaceful Lore Resolutions | %d Forceful Canister Extractions)\n" % [
			GameState.resolved_stores.size(), peaceful_count, forceful_count
		] +
		"[b]Hidden Lore Collectibles Discovered:[/b] [color=#66f2ff]%d / 12[/color] (Ledgers, 104.7 FM Broadcasts & UV Wall Ciphers)\n" % collectible_lore_count +
		"----------------------------------------------------------------------------------\n" +
		"\n".join(store_lines)
	)

func launch_next_endless_escalation_floor() -> void:
	endless_escalation_wave += 1
	var next_floor := maxi(GameState.highest_floor_reached + 1, 4)
	GameState.current_floor = next_floor
	GameState.highest_floor_reached = maxi(GameState.highest_floor_reached, next_floor)
	GameState.notify_floor_reached(next_floor)
	GameState.add_cash(150, "Floor %d Escalation Clearance Bounty" % next_floor)

	# If escalating past Floor 7 (Floor 8, 9, 10...), dynamically spawn the procedural upper floor in MallCurvedAtriumRing and extend the Freight Elevator!
	var p := get_parent()
	if p and "curved_atrium" in p and is_instance_valid(p.get("curved_atrium")):
		var ca := p.get("curved_atrium") as MallCurvedAtriumRing
		if ca and next_floor > 7:
			ca.spawn_procedural_upper_floor(next_floor)
	for fe_node in get_tree().get_nodes_in_group("freight_elevators"):
		var fe := fe_node as MallFreightElevator
		if fe and next_floor > fe.max_floors:
			fe.extend_to_floor(next_floor)

	# Procedurally remix all store ghosts (higher HP, faster hunt speed, fresh dispositions)
	GameState.resolved_stores.clear()
	for g_node in get_tree().get_nodes_in_group("store_ghosts"):
		var ghost := g_node as StoreGhost
		if ghost:
			ghost.is_resolved = false
			ghost.is_enraged = false
			if ghost.store_id != "East_F0_02":
				ghost.is_corrupted_hostile = (randf() < clampf(0.45 + float(endless_escalation_wave) * 0.08, 0.45, 0.85))
			ghost.max_ghost_hp += 120.0
			ghost.ghost_hp = ghost.max_ghost_hp
			ghost.weakened_percent = 0.0
			ghost.hunt_speed = minf(2.45, ghost.hunt_speed + 0.08)
			ghost._enter_phase(StoreGhost.GhostPhase.INITIAL_ACTIVITY)

	var tier_desc := (
		"FLOORS 7+ ENDLESS ESCALATION MODE (Floor %d)" % next_floor
		if next_floor >= 7
		else "Floor %d Campaign Escalation" % next_floor
	)
	GameState.status_message.emit(
		"LAUNCHED %s! (+$150 Bounty | All 6 Store Ghosts Remixed with Higher HP & New Dispositions!)" % tier_desc,
		Color(1.0, 0.88, 0.35)
	)
	_refresh_endgame_summary_text()
	endless_escalation_launched.emit(next_floor)


# ------------------------------------------------------------------------------
# Contextual Interaction & Input Hooks (Works Automatically or Called by World Builder)
# ------------------------------------------------------------------------------
func _deferred_wire_world_and_ui() -> void:
	_ensure_ui_built()
	var p := _find_local_player()
	if is_instance_valid(p) and not p.meal_finished.is_connected(_on_player_meal_finished):
		p.meal_finished.connect(_on_player_meal_finished)

func _ensure_ui_built() -> void:
	if briefing_card_panel != null and is_instance_valid(briefing_card_panel):
		return
	var parent_node := get_parent()
	if parent_node:
		for child in parent_node.get_children():
			if child is CanvasLayer:
				for c2 in child.get_children():
					if c2 is Control:
						build_briefing_and_endgame_ui(c2 as Control)
						return
	_ui_canvas_layer = CanvasLayer.new()
	add_child(_ui_canvas_layer)
	var root_ctrl := Control.new()
	root_ctrl.set_anchors_preset(Control.PRESET_FULL_RECT)
	root_ctrl.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_ui_canvas_layer.add_child(root_ctrl)
	build_briefing_and_endgame_ui(root_ctrl)

func _on_player_meal_finished() -> void:
	var p := _find_local_player()
	if is_instance_valid(p):
		apply_selected_meal_buff(p)

func _find_local_player() -> PlayerController:
	var parent_node := get_parent()
	if parent_node and "player" in parent_node and is_instance_valid(parent_node.get("player")):
		return parent_node.get("player") as PlayerController
	for node in get_tree().get_nodes_in_group("players"):
		if node is PlayerController:
			return node as PlayerController
	return null

func _process(delta: float) -> void:
	if Engine.is_editor_hint():
		return
	if _airlock_cooldown > 0.0:
		_airlock_cooldown = maxf(0.0, _airlock_cooldown - delta)

	var p := _find_local_player()
	if is_instance_valid(p):
		# Auto-update current_floor when player ascends to any upper floor (Floor 2..7+)
		var detected_floor := int(floor((p.global_position.y + 2.0) / 8.5)) + 1
		if detected_floor > GameState.current_floor:
			GameState.notify_floor_reached(detected_floor)

		# Check if West_F1_02 lights were dimmed to 45% Gray-Space Dim via Light Remote Gun -> calm Display Coordinator!
		_check_west_f1_02_dimmer_ritual()

		# Check UV Blacklight cone against our UVCipherWallDecals & GhostOrbs
		_tick_player_uv_and_video_evidence_checks(p)

	if Input.is_action_just_pressed("toggle_endgame_summary"):
		show_endgame_summary(null)

	if Input.is_action_just_pressed("interact") and is_instance_valid(p):
		try_handle_campaign_interact(p)

	if Input.is_action_just_pressed("secondary_action") and is_instance_valid(p):
		try_handle_campaign_secondary_action(p)

func _unhandled_key_input(event: InputEvent) -> void:
	if Engine.is_editor_hint():
		return
	if event is InputEventKey and event.pressed and not event.echo:
		var kc: int = event.physical_keycode if event.physical_keycode != 0 else event.keycode
		if briefing_card_panel != null and briefing_card_panel.visible and kc == KEY_ENTER:
			briefing_card_panel.visible = false
			get_viewport().set_input_as_handled()
			return
		if endgame_modal_panel != null and endgame_modal_panel.visible:
			if kc == KEY_1:
				launch_next_endless_escalation_floor()
				get_viewport().set_input_as_handled()
			elif kc == KEY_ESCAPE:
				show_endgame_summary(false)
				get_viewport().set_input_as_handled()

func _check_west_f1_02_dimmer_ritual() -> void:
	if not is_instance_valid(store_ghost_west_f1_02) or store_ghost_west_f1_02.is_resolved:
		return
	var dimmed_count := 0
	for node in get_tree().get_nodes_in_group("mall_light_fixtures"):
		var fix := node as MallLightFixture
		if fix and fix.store_id == "West_F1_02" and fix.current_mode == MallLightFixture.LightMode.DIM_45:
			dimmed_count += 1
	if dimmed_count >= 2 and not store_ghost_west_f1_02.is_corrupted_hostile:
		GameState.discover_lore("LORE_UV_CIPHER_02", true)
		store_ghost_west_f1_02.try_peaceful_resolution()

func _tick_player_uv_and_video_evidence_checks(p: PlayerController) -> void:
	for entry in _uv_cipher_entries:
		var node := entry.get("node") as PlaceableEvidenceActors.UVEvidencePrint
		var lid := str(entry.get("lore_id", ""))
		if is_instance_valid(node) and node.ever_discovered and not lid.is_empty():
			GameState.discover_lore(lid, true)

	var cur_item := p.get_current_active_item()
	var item_id := str(cur_item.get("id", ""))
	if item_id == "photo_camera" or item_id == "uv_blacklight" or item_id == "video_camera" or item_id == "assembled_video_rig":
		for orb_node in get_tree().get_nodes_in_group("ghost_orbs"):
			if orb_node is VideoAndAudioSurveillance.GhostOrbActor:
				var vorb := orb_node as VideoAndAudioSurveillance.GhostOrbActor
				if p.global_position.distance_to(vorb.global_position) <= 12.0:
					vorb.set_ir_revealed(true)
					GameState.record_evidence(vorb.store_id, "Ghost Orbs / Shy Video Entity", 45)
			elif orb_node is GhostOrbActor:
				var orb := orb_node as GhostOrbActor
				if p.global_position.distance_to(orb.global_position) <= 9.5:
					orb.inspect_or_capture_orb()

func _on_voice_command_for_lore_and_rituals(phrase: String, speaker_pos: Vector3, spirit_box_freq: float) -> void:
	var lower := phrase.to_lower()
	if "where are you" in lower and (speaker_pos.distance_to(Vector3(25.5, 8.50, -62.0)) <= 16.0 or speaker_pos.distance_to(Vector3(20.5, 8.50, -15.5)) <= 14.0):
		_east_f2_spoke_where_are_you = true

	# Check 104.7 FM Secret Broadcast Landmarks (4 Spirit Box Secret Broadcasts)
	if absf(spirit_box_freq - 104.7) <= 0.25:
		if speaker_pos.distance_to(Vector3(0.0, 0.0, 1.5)) <= 14.0:
			GameState.discover_lore("LORE_RADIO_104_7", true)
		if speaker_pos.distance_to(delivery_airlock_pos) <= 14.0:
			GameState.discover_lore("LORE_RADIO_AIRLOCK_02", true)
		if speaker_pos.distance_to(Vector3(-22.5, 0.48, 22.2)) <= 14.0 or speaker_pos.distance_to(Vector3(-11.2, 0.48, 18.6)) <= 14.0 or speaker_pos.distance_to(Vector3(5.2, 0.48, 4.2)) <= 14.0:
			GameState.discover_lore("LORE_RADIO_BREAKER_03", true)
		if speaker_pos.distance_to(elena_camp_f3_pos) <= 16.0 or speaker_pos.y >= 15.5:
			GameState.discover_lore("LORE_RADIO_OCULUS_04", true)

func try_handle_campaign_secondary_action(p: PlayerController) -> bool:
	if not is_instance_valid(p):
		return false
	var ppos := p.global_position

	# 1. Cycle 4-Meal Food Court Menu when pressing [E] near North Night-Market Food Court (-18.0, 0.0, -32.0)
	if ppos.distance_to(Vector3(-18.0, 0.0, -32.0)) < 4.5 or ppos.distance_to(Vector3(-11.0, 0.0, -5.5)) < 4.2:
		cycle_food_court_meal()
		return true

	return false

func try_handle_campaign_interact(p: PlayerController) -> bool:
	if not is_instance_valid(p):
		return false
	var ppos := p.global_position

	if briefing_card_panel != null and briefing_card_panel.visible and ppos.distance_to(Vector3(2.0, 0.1, 0.0)) < 1.8:
		briefing_card_panel.visible = false

	# 1. Elena Vance's Final Camp & Geodesic Ark Extraction Console on Floor 3 (Vector3(0.0, 17.0, -34.0))
	if ppos.distance_to(elena_camp_f3_pos) < 4.5 or ppos.distance_to(Vector3(0.0, 17.0, -18.0)) < 4.2:
		ark_containment_stabilized = true
		GameState.discover_lore("LORE_RADIO_OCULUS_04", true)
		GameState.discover_lore("LORE_UV_CIPHER_04", true)
		GameState.status_message.emit(
			"CONTRACT #89-AV MILESTONE: Reunited with Auditor Elena Vance & Stabilized the Geodesic Ark Containment Core!",
			Color(0.42, 1.0, 0.88)
		)
		show_endgame_summary(true)
		return true

	# 3. Store #2: West_F1_02 ("Victoria's Phantoms") Spotlight Dimmer Console (-25.5, 0.0, 58.0)
	if ppos.distance_to(Vector3(-25.5, 0.0, 58.0)) < 4.5 or ppos.distance_to(Vector3(-19.5, 0.0, 8.5)) < 3.6:
		for node in get_tree().get_nodes_in_group("mall_light_fixtures"):
			var fix := node as MallLightFixture
			if fix and fix.store_id == "West_F1_02":
				fix.apply_light_mode(MallLightFixture.LightMode.DIM_45, false)
		GameState.record_evidence("West_F1_02", "Spirit Box (91.5 FM)", 40)
		GameState.discover_lore("LORE_UV_CIPHER_02", true)
		if is_instance_valid(store_ghost_west_f1_02):
			store_ghost_west_f1_02.try_peaceful_resolution()
		return true

	# 4. Store #3: West_F1_03 ("Sheer Terror Hosiery") Rear Transformer Log (-25.5, 0.0, -58.0)
	if ppos.distance_to(Vector3(-25.5, 0.0, -58.0)) < 4.5 or ppos.distance_to(Vector3(-30.0, 0.0, -58.0)) < 4.5 or ppos.distance_to(Vector3(-19.5, 0.0, -8.5)) < 3.8:
		GameState.discover_lore("LORE_LEDGER_WEST_F1_03", true)
		GameState.record_evidence("West_F1_03", "Ghost Writing", 40)
		return false # Let megamall_world_builder also run its West_F1_03 ritual handler

	# 5. Store #4: West_F2_01 ("BRICKWORKZ") Center Modular Feature Podium (-25.5, 8.50, -58.0)
	if ppos.distance_to(Vector3(-25.5, 8.50, -58.0)) < 4.5 or ppos.distance_to(Vector3(-20.5, 8.50, -15.5)) < 3.8:
		GameState.record_evidence("West_F2_01", "UV Prints", 35)
		GameState.record_evidence("West_F2_01", "Ghost Orbs / Shy Video Entity", 45)
		GameState.discover_lore("LORE_UV_CIPHER_03", true)
		if is_instance_valid(store_ghost_west_f2_01):
			store_ghost_west_f2_01.try_peaceful_resolution()
		return true

	# 6. Store #5: East_F2_01 ("Arcadia Evening Wear") Checkout Service Bell (25.5, 8.50, -62.0)
	if ppos.distance_to(Vector3(25.5, 8.50, -62.0)) < 4.5 or ppos.distance_to(Vector3(20.5, 8.50, -19.8)) < 3.8:
		GameState.discover_lore("LORE_LEDGER_EAST_F2_01", true)
		GameState.record_evidence("East_F2_01", "Ghost Writing", 40)
		GameState.record_evidence("East_F2_01", "Spirit Box (98.3 FM)", 45)
		var parent_node := get_parent()
		if parent_node and "store_ghost_floor2" in parent_node and is_instance_valid(parent_node.get("store_ghost_floor2")):
			var g_f2 := parent_node.get("store_ghost_floor2") as StoreGhost
			g_f2.try_peaceful_resolution()
		return true

	# 7. Store #6: Center_F3_01 ("Upper Ark Control & Civic Facility") Blueprint Drafting Table (25.5, 17.0, -58.0)
	if ppos.distance_to(Vector3(25.5, 17.0, -58.0)) < 4.5 or ppos.distance_to(Vector3(20.5, 17.0, -15.5)) < 3.8:
		GameState.discover_lore("LORE_LEDGER_ARK_F3_01", true)
		GameState.discover_lore("LORE_UV_CIPHER_04", true)
		GameState.record_evidence("Center_F3_01", "Ghost Writing", 40)
		GameState.record_evidence("Center_F3_01", "Spirit Box (104.7 FM)", 45)
		GameState.record_evidence("Center_F3_01", "Recorded EVP Audio", 50)
		GameState.record_evidence("Center_F3_01", "SLS / Laser Silhouette", 50)
		GameState.record_evidence("Center_F3_01", "Ectoplasmic Residue / SKU Anomaly", 55)
		if is_instance_valid(store_ghost_center_f3_01):
			store_ghost_center_f3_01.try_peaceful_resolution()
		return true

	# 8. Automated Delivery Airlock Bay (-6.0, 0.0, -34.0)
	if ppos.distance_to(delivery_airlock_pos) < 4.2 or ppos.distance_to(Vector3(-5.5, 0.0, -18.5)) < 3.8:
		GameState.discover_lore("LORE_RADIO_AIRLOCK_02", true)
		if _airlock_cooldown <= 0.0:
			_airlock_cooldown = 12.0
			_airlock_dispense_count += 1
			p.health = minf(100.0, p.health + 35.0)
			p.stamina = minf(100.0, p.stamina + 50.0)
			if _airlock_dispense_count == 1 and not GameState.powered_circuits.get("EscalatorCircuit_F1_To_F2", false):
				var fuse_drop := PlaceableEvidenceActors.WorldDroppedItem.new()
				fuse_drop.item_data = PlayerController.make_item_data("escalator_fuse")
				fuse_drop.position = delivery_airlock_pos + Vector3(0.0, 0.0, 1.45)
				get_parent().add_child(fuse_drop)
				GameState.status_message.emit(
					"AUTOMATED DELIVERY AIRLOCK: Dispensed Emergency Rations (+35 HP / +50 Stamina) & dropped a free Escalator Relay Fuse Crate!",
					Color(0.42, 1.0, 0.75)
				)
			else:
				GameState.status_message.emit(
					"AUTOMATED DELIVERY AIRLOCK: Dispensed Emergency Thermal Ration (+35 HP / +50 Stamina) & logged LORE_RADIO_AIRLOCK_02!",
					Color(0.42, 1.0, 0.75)
				)
		else:
			GameState.status_message.emit(
				"Automated Delivery Airlock cycling next supply drop (%.1fs remaining)..." % _airlock_cooldown,
				Color(0.78, 0.88, 0.96)
			)
		return true

	# 9. All 5 Hiding Spot Types in group "hiding_spots"
	for hs_node in get_tree().get_nodes_in_group("hiding_spots"):
		var hs := hs_node as MallHidingSpotActor
		if hs and ppos.distance_to(hs.global_position) < 2.85:
			return hs.try_toggle_hide(p)

	# 10. Escalator Breaker Panel (-22.5, 0.48, 22.2) -> Discover LORE_RADIO_BREAKER_03
	if ppos.distance_to(Vector3(-22.5, 0.48, 22.2)) < 4.2 or ppos.distance_to(Vector3(-11.2, 0.48, 18.6)) < 3.8 or ppos.distance_to(Vector3(5.2, 0.48, 4.2)) < 3.8:
		GameState.discover_lore("LORE_RADIO_BREAKER_03", true)

	return false

func _move_freight_elevator_to_floor_3(_p: PlayerController) -> void:
	for fe_node in get_tree().get_nodes_in_group("freight_elevators"):
		var fe := fe_node as MallFreightElevator
		if fe:
			fe.send_to_floor(3, "Service Freight Elevator -> Floor 3 Sky Arboretum & Ark Control Deck (Y = 17.0m)")
			GameState.notify_floor_reached(3)
			return

func spawn_item_in_delivery_airlock(item_data: Dictionary) -> PlaceableEvidenceActors.WorldDroppedItem:
	var drop := PlaceableEvidenceActors.WorldDroppedItem.new()
	drop.item_data = item_data.duplicate(true)
	drop.position = delivery_airlock_pos + Vector3(randf_range(-0.6, 0.6), 0.0, 1.45)
	var parent_node := get_parent()
	if parent_node:
		parent_node.add_child(drop)
	else:
		add_child(drop)
	return drop

func get_campaign_journal_summary() -> String:
	var lines: Array[String] = []
	var order := ["East_F0_02", "West_F1_02", "West_F1_03", "West_F2_01", "East_F2_01", "Center_F3_01"]
	for sid in order:
		var prof: Dictionary = GameState.ghost_profiles.get(sid, {})
		var status_str: String = str(GameState.resolved_stores.get(sid, "Active Haunting"))
		var ev_arr: Array = GameState.store_evidence_ledger.get(sid, [])
		lines.append("• [b]%s — %s (%s, Floor %d):[/b] %s | Evidence: %s" % [
			sid,
			str(prof.get("name", sid)),
			str(prof.get("store_name", "")),
			int(prof.get("floor_level", 1)),
			status_str,
			", ".join(ev_arr) if not ev_arr.is_empty() else "Unconfirmed",
		])
	return "\n".join(lines)


# ------------------------------------------------------------------------------
# Geometry & Prop Construction Helpers
# ------------------------------------------------------------------------------
func _build_store_sign_header(pos: Vector3, rot_deg: Vector3, title_text: String, glow_col: Color) -> void:
	var sign_root := _spawn_model_or_box(
		"res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25346_FasciaSignBoard.fbx",
		pos,
		Vector3(4.2, 0.65, 0.18),
		Color(0.24, 0.26, 0.30),
		false
	)
	sign_root.rotation_degrees = rot_deg
	var lbl := Label3D.new()
	lbl.position = pos + Vector3(0.0, 0.32, 0.0)
	lbl.text = title_text
	lbl.modulate = glow_col.lerp(Color(0.96, 0.94, 0.88), 0.35)
	lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	lbl.font_size = 22
	lbl.outline_size = 5
	lbl.visibility_range_end = 26.0
	lbl.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
	add_child(lbl)

func _create_static_box(parent: Node3D, pos: Vector3, size: Vector3, color: Color, roughness: float) -> StaticBody3D:
	var sb := StaticBody3D.new()
	sb.position = pos
	var col := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = size
	col.shape = shape
	sb.add_child(col)

	var mi := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = size
	mi.mesh = box
	mi.material_override = MallPbrMaterialLibrary.get_smart_box_material(size, color, roughness)
	sb.add_child(mi)
	parent.add_child(sb)
	return sb

func _spawn_model_or_box(res_path: String, pos: Vector3, fallback_size: Vector3, fallback_color: Color, add_box_collision: bool = false) -> Node3D:
	var root := Node3D.new()
	root.position = pos
	add_child(root)

	if add_box_collision:
		var sb := StaticBody3D.new()
		var col := CollisionShape3D.new()
		var shape := BoxShape3D.new()
		shape.size = fallback_size
		col.shape = shape
		col.position = Vector3(0.0, fallback_size.y * 0.5, 0.0)
		sb.add_child(col)
		root.add_child(sb)

	if ResourceLoader.exists(res_path):
		var packed := load(res_path) as PackedScene
		if packed:
			var inst := packed.instantiate()
			MallPbrMaterialLibrary.beautify_model_materials(inst, res_path, fallback_color)
			root.add_child(inst)
			return root

	var mi := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = fallback_size
	mi.mesh = box
	mi.position = Vector3(0.0, fallback_size.y * 0.5, 0.0)
	mi.material_override = MallPbrMaterialLibrary.get_smart_box_material(fallback_size, fallback_color, 0.45)
	root.add_child(mi)
	return root

func _create_floating_label(pos: Vector3, text: String, color: Color) -> Label3D:
	var lbl := Label3D.new()
	lbl.position = pos
	lbl.text = text
	lbl.modulate = color.lerp(Color(0.96, 0.94, 0.88), 0.35)
	lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	lbl.font_size = 20
	lbl.outline_size = 5
	lbl.visibility_range_end = 13.5
	lbl.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
	add_child(lbl)
	return lbl
