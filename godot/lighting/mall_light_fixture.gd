@tool
class_name MallLightFixture
extends Node3D
## Dynamic Mall Ceiling & Storefront Light Fixture.
## Supports Player Light Remote Gun states (100% ON, 45% GRAY-SPACE DIM, 0% OFF)
## and AI Ghost Flicker / Hunt Blackout overrides (VisibleWarning 10% flicker, VisibleHunt 0%).

enum LightMode {
	ON_100 = 0,
	DIM_45 = 1,
	OFF_0 = 2,
}

@export var store_id: String = "Concourse"
@export var base_energy: float = 2.6
@export var light_color: Color = Color(1.0, 0.94, 0.84)
@export var current_mode: LightMode = LightMode.ON_100

var omni_light: OmniLight3D = null
var bulb_mesh: MeshInstance3D = null
var bulb_material: StandardMaterial3D = null

var _ghost_override_active: bool = false
var _ghost_multiplier: float = 1.0
var _flicker_intensity: float = 0.0
var _flicker_phase: float = 0.0

func _ready() -> void:
	add_to_group("mall_light_fixtures")
	_build_fixture_visuals()
	apply_light_mode(current_mode, false)

func _build_fixture_visuals() -> void:
	# Low-poly recessed ceiling troffer housing + emissive diffuser panel
	var housing := MeshInstance3D.new()
	var housing_box := BoxMesh.new()
	housing_box.size = Vector3(1.2, 0.08, 0.6)
	housing.mesh = housing_box
	var housing_mat := StandardMaterial3D.new()
	housing_mat.albedo_color = Color(0.16, 0.17, 0.2)
	housing_mat.roughness = 0.4
	housing.material_override = housing_mat
	add_child(housing)

	bulb_mesh = MeshInstance3D.new()
	var panel_box := BoxMesh.new()
	panel_box.size = Vector3(1.08, 0.03, 0.48)
	bulb_mesh.mesh = panel_box
	bulb_mesh.position = Vector3(0.0, -0.04, 0.0)
	bulb_material = StandardMaterial3D.new()
	bulb_material.albedo_color = light_color
	bulb_material.emission_enabled = true
	bulb_material.emission = light_color
	bulb_material.emission_energy_multiplier = 2.5
	bulb_mesh.material_override = bulb_material
	add_child(bulb_mesh)

	omni_light = OmniLight3D.new()
	omni_light.position = Vector3(0.0, -0.22, 0.0)
	omni_light.light_color = light_color
	omni_light.light_energy = base_energy
	omni_light.light_volumetric_fog_energy = 1.35
	omni_light.omni_range = 11.5
	omni_light.omni_attenuation = 1.25
	omni_light.shadow_enabled = false
	add_child(omni_light)

	# Collision body so the Light Remote Gun raycast (up to 60m) can hit this fixture directly
	var static_body := StaticBody3D.new()
	static_body.collision_layer = 2
	static_body.collision_mask = 0
	var col := CollisionShape3D.new()
	var box_shape := BoxShape3D.new()
	box_shape.size = Vector3(1.3, 0.25, 0.7)
	col.shape = box_shape
	static_body.add_child(col)
	add_child(static_body)

func cycle_remote_mode() -> String:
	match current_mode:
		LightMode.ON_100:
			apply_light_mode(LightMode.DIM_45, true)
			return "45% Gray-Space Dim"
		LightMode.DIM_45:
			apply_light_mode(LightMode.OFF_0, true)
			return "OFF (0%)"
		_:
			apply_light_mode(LightMode.ON_100, true)
			return "100% Full Output"

func apply_light_mode(mode: LightMode, notify: bool = true) -> void:
	current_mode = mode
	if notify and mode == LightMode.DIM_45 and store_id == "East_F0_02":
		GameState.status_message.emit("Store [%s] set to 45%% Gray-Space Dim — Photophobic Ghost calmed!" % store_id, Color(0.65, 0.85, 1.0))
	_refresh_output()

func set_ghost_flicker_override(active: bool, multiplier: float = 1.0, flicker_amount: float = 0.0) -> void:
	_ghost_override_active = active
	_ghost_multiplier = multiplier
	_flicker_intensity = flicker_amount
	_refresh_output()

func _process(delta: float) -> void:
	if _ghost_override_active and _flicker_intensity > 0.01:
		_flicker_phase += delta * 28.0
		_refresh_output()

func _refresh_output() -> void:
	var mode_factor: float = 1.0
	match current_mode:
		LightMode.ON_100:
			mode_factor = 1.0
		LightMode.DIM_45:
			mode_factor = 0.45
		LightMode.OFF_0:
			mode_factor = 0.0

	var final_factor: float = mode_factor
	if _ghost_override_active:
		var jitter: float = 0.0
		if _flicker_intensity > 0.01:
			jitter = (sin(_flicker_phase) * cos(_flicker_phase * 2.37) * 0.5 + 0.5) * _flicker_intensity
		final_factor = clampf(mode_factor * _ghost_multiplier + jitter, 0.0, 1.2)

	if omni_light:
		omni_light.light_energy = base_energy * final_factor
		omni_light.visible = final_factor > 0.01
	if bulb_material:
		bulb_material.emission_energy_multiplier = 2.8 * final_factor
		bulb_material.albedo_color = light_color.darkened(1.0 - clampf(final_factor, 0.15, 1.0))
