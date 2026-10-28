@tool
class_name MallLightFixture
extends Node3D
## Dynamic Mall Ceiling & Storefront Light Fixture.
## Supports Player Light Remote Gun states (100% ON, 45% GRAY-SPACE DIM, 0% OFF)
## and AI Ghost Flicker / Hunt Blackout overrides (VisibleWarning 10% flicker, VisibleHunt 0%).
## Uses desaturated architectural warm-white Kelvin color temperatures and brushed steel housing.

enum LightMode {
	ON_100 = 0,
	DIM_45 = 1,
	OFF_0 = 2,
}

@export var store_id: String = "Concourse"
@export var base_energy: float = 1.45
@export var light_color: Color = Color(1.0, 0.95, 0.88)
@export var current_mode: LightMode = LightMode.ON_100

var omni_light: OmniLight3D = null
var bulb_mesh: MeshInstance3D = null
var bulb_material: StandardMaterial3D = null

var _ghost_override_active: bool = false
var _ghost_multiplier: float = 1.0
var _flicker_intensity: float = 0.0
var _flicker_phase: float = 0.0
var _natural_color: Color = Color(1.0, 0.95, 0.88)

func _ready() -> void:
	add_to_group("mall_light_fixtures")
	# Desaturate overly saturated colors toward warm architectural halogen/daylight
	_natural_color = light_color.lerp(Color(1.0, 0.96, 0.90), 0.68)
	base_energy = clampf(base_energy * 0.58, 1.15, 1.65)
	_build_fixture_visuals()
	apply_light_mode(current_mode, false)

func _build_fixture_visuals() -> void:
	# Brushed steel architectural ceiling troffer housing + warm frosted diffuser panel
	var housing := MeshInstance3D.new()
	var housing_box := BoxMesh.new()
	housing_box.size = Vector3(1.28, 0.09, 0.64)
	housing.mesh = housing_box
	housing.material_override = MallPbrMaterialLibrary.get_brushed_steel_material(Color(0.74, 0.76, 0.80))
	add_child(housing)

	bulb_mesh = MeshInstance3D.new()
	var panel_box := BoxMesh.new()
	panel_box.size = Vector3(1.14, 0.03, 0.50)
	bulb_mesh.mesh = panel_box
	bulb_mesh.position = Vector3(0.0, -0.045, 0.0)
	bulb_material = StandardMaterial3D.new()
	bulb_material.albedo_color = _natural_color
	bulb_material.roughness = 0.35
	bulb_material.emission_enabled = true
	bulb_material.emission = _natural_color
	bulb_material.emission_energy_multiplier = 1.15
	bulb_mesh.material_override = bulb_material
	add_child(bulb_mesh)

	omni_light = OmniLight3D.new()
	omni_light.position = Vector3(0.0, -0.25, 0.0)
	omni_light.light_color = _natural_color
	omni_light.light_energy = base_energy
	omni_light.light_volumetric_fog_energy = 0.38
	omni_light.omni_range = 14.5
	omni_light.omni_attenuation = 1.12
	omni_light.shadow_enabled = false
	add_child(omni_light)

	# Collision body so the Light Remote Gun raycast (up to 60m) can hit this fixture directly
	var static_body := StaticBody3D.new()
	static_body.collision_layer = 2
	static_body.collision_mask = 0
	var col := CollisionShape3D.new()
	var box_shape := BoxShape3D.new()
	box_shape.size = Vector3(1.35, 0.28, 0.72)
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
		bulb_material.emission_energy_multiplier = 1.15 * final_factor
		bulb_material.albedo_color = _natural_color.darkened(1.0 - clampf(final_factor, 0.15, 1.0))
