@tool
class_name MallCurvedAtriumRing
extends Node3D
## Megamall Anomaly — Double-Radius Multi-Floor Parametric Central Oval Shopping Island,
## Void-Spanning Escalator Network, and Vaulted Geodesic Skylight Atrium (Floors 1..7 + Procedural 8+!).
##
## Architectural Guarantee:
## - Double-Radius Oval Island (oval_island_radius_x = 16.0m, oval_island_radius_z = 20.0m -> 32m x 40m).
## - Double-Radius Open Elliptical Atrium Void (atrium_radius_x = 28.5m, atrium_radius_z = 30.0m -> 57m x 60m).
## - Continuous Curved-to-Rectangular Outer Concourse Gallery Slab on EVERY upper floor (Floors 2..7+)
##   projecting from the Elliptical Atrium Rim out to the Rectangular Hub Perimeter (X = +-46.0m, Z = +-42.0m)
##   with 100% solid trimesh + box collision and ZERO holes!
## - Continuous 1.20m-tall Curved Glass & Brushed-Steel Balustrades around 100% of the Inner Oval Island
##   and 100% of the Outer Elliptical Atrium Rim, opening ONLY at the 4 Guarded Cardinal Bridges,
##   the Sealed Void-Spanning Escalator Landings (Z = +22.2m), and the Sealed Freight Elevator Landing (X = 10.0m, Z = -28.0m).

@export var curve_segments: int = 96
@export var num_upper_floors: int = 6 ## 6 upper floors = 7 Total Built-In Floors (Floor 1 at 0m .. Floor 7 at 51.0m), expandable infinitely!
@export var floor_to_floor_height: float = 8.50
@export var oval_island_radius_x: float = 16.00
@export var oval_island_radius_z: float = 20.00
@export var atrium_radius_x: float = 28.50
@export var atrium_radius_z: float = 30.00
@export var concourse_ring_radius_x: float = 36.00
@export var concourse_ring_radius_z: float = 37.50
@export var outer_extent_x: float = 46.00
@export var outer_extent_z: float = 42.00
@export var slab_thickness: float = 0.50
@export var balustrade_height: float = 1.20
@export var bridge_half_width: float = 3.20

## Void-Spanning Escalator Z coordinate (in the South Atrium Void where Z = +22.2m is south of the Oval Island rz = 20.0m!)
const ESCALATOR_VOID_Z: float = 22.20
const ESCALATOR_WEST_OUTER_X: float = -19.82
const ESCALATOR_EAST_OUTER_X: float = 19.82

var escalators_up: Dictionary = {}   # floor_num (1..N) -> MallEscalator (climbs from floor_num to floor_num + 1)
var escalators_down: Dictionary = {} # floor_num (1..N) -> MallEscalator (descends from floor_num + 1 to floor_num)
var _built_upper_floors: Dictionary = {}
var _dome_root: Node3D = null
var _trees_root: Node3D = null

var _mat_marble: Material = null
var _mat_terrazzo: Material = null
var _mat_plaster: Material = null
var _mat_glass: Material = null
var _mat_steel: Material = null
var _mat_cove: Material = null
var _mat_ceiling: Material = null

func _ready() -> void:
	name = "MallCurvedAtriumRing"
	add_to_group("mall_curved_atrium")
	rebuild_curved_architecture()

func evaluate_ellipse_point(rx: float, rz: float, angle_rad: float, y_elev: float) -> Vector3:
	return Vector3(rx * cos(angle_rad), y_elev, rz * sin(angle_rad))

func project_direction_to_rectangle(angle_rad: float, half_x: float, half_z: float, y_elev: float) -> Vector3:
	var ca := cos(angle_rad)
	var sa := sin(angle_rad)
	var tx := (half_x / absf(ca)) if absf(ca) > 0.0001 else 1e6
	var tz := (half_z / absf(sa)) if absf(sa) > 0.0001 else 1e6
	var t := minf(tx, tz)
	return Vector3(ca * t, y_elev, sa * t)

func is_point_in_cardinal_bridge_opening(pt_x: float, pt_z: float, extra_margin: float = 0.08) -> bool:
	var hw := bridge_half_width - extra_margin
	# East / West Bridges (along X axis, |Z| <= hw)
	if absf(pt_z) <= hw and absf(pt_x) >= oval_island_radius_x - 1.2:
		return true
	# North / South Bridges (along Z axis, |X| <= hw)
	if absf(pt_x) <= hw and absf(pt_z) >= oval_island_radius_z - 1.2:
		return true
	return false

func is_point_in_outer_escalator_or_elevator_portal(pt_x: float, pt_z: float) -> bool:
	# 1. South Void-Spanning Escalator Portals on the West & East Outer Elliptical Atrium Rim at Z = +22.2m
	if absf(pt_z - ESCALATOR_VOID_Z) <= 0.72 and absf(pt_x) >= 16.0:
		return true
	# 2. North Service Freight Elevator Landing Portal at X = [7.7 .. 12.3], Z < -25.0m
	if pt_x >= 7.65 and pt_x <= 12.35 and pt_z <= -25.0:
		return true
	return false

func rebuild_curved_architecture() -> void:
	for c in get_children():
		c.queue_free()
	escalators_up.clear()
	escalators_down.clear()
	_built_upper_floors.clear()

	_mat_marble = MallPbrMaterialLibrary.get_marble_floor_material()
	_mat_terrazzo = MallPbrMaterialLibrary.get_terrazzo_material(Color(0.91, 0.89, 0.85), 0.40)
	_mat_plaster = MallPbrMaterialLibrary.get_plaster_material(Color(0.93, 0.91, 0.87), 0.36)
	_mat_glass = MallPbrMaterialLibrary.get_architectural_glass_material(Color(0.76, 0.88, 0.94, 0.28))
	_mat_steel = MallPbrMaterialLibrary.get_brushed_steel_material(Color(0.78, 0.80, 0.84))
	_mat_cove = MallPbrMaterialLibrary.get_cove_accent_material(Color(0.98, 0.91, 0.78), 0.80)
	_mat_ceiling = MallPbrMaterialLibrary.get_ceiling_tile_material()

	_build_ground_hebe_fountain_and_hot_springs()
	for floor_idx in range(1, num_upper_floors + 1):
		_build_single_upper_floor_oval_and_bridges(floor_idx)
	# Floor 2 Central Oval Island kept open to match Floor 1's exact layout during prototyping!
	_build_floor3_oval_sky_arboretum_conservatory()
	_build_upper_floors_4_to_7_oval_landmarks()
	_build_soul_trees_and_floating_forest_pods()
	_build_vaulted_geodesic_skylight_dome()

## Dynamically generates a new procedural upper floor (e.g. Floor 8, 9, 10...) at runtime and raises the Skylight Dome!
func spawn_procedural_upper_floor(target_floor_number: int) -> Node3D:
	var target_upper_idx := target_floor_number - 1
	if target_upper_idx <= 0:
		return null
	if target_upper_idx > num_upper_floors:
		for idx in range(num_upper_floors + 1, target_upper_idx + 1):
			_build_single_upper_floor_oval_and_bridges(idx)
			_build_procedural_oval_outpost(idx + 1)
		num_upper_floors = target_upper_idx
		_build_soul_trees_and_floating_forest_pods()
		_build_vaulted_geodesic_skylight_dome()
	return _built_upper_floors.get(target_upper_idx, null)

# ------------------------------------------------------------------------------
# 1. Ground Floor (Y = 0.0m): Double-Radius Hebe Fountain Plaza & Biotope Basins
# ------------------------------------------------------------------------------
func _build_ground_hebe_fountain_and_hot_springs() -> void:
	var ground_root := Node3D.new()
	ground_root.name = "GroundOvalFountainPlaza"
	add_child(ground_root)

	var mat_water := MallPbrMaterialLibrary.get_animated_water_material()

	# A. Decorative Double-Radius Elliptical Terrazzo Medallion Ring on Ground Floor (16.0m x 20.0m)
	var st_med := SurfaceTool.new()
	st_med.begin(Mesh.PRIMITIVE_TRIANGLES)
	var med_segs := 72
	var med_step := TAU / float(med_segs)
	for i in range(med_segs):
		var a0 := float(i) * med_step
		var a1 := float(i + 1) * med_step
		var mp0 := evaluate_ellipse_point(oval_island_radius_x, oval_island_radius_z, a0, 0.012)
		var mp1 := evaluate_ellipse_point(oval_island_radius_x, oval_island_radius_z, a1, 0.012)
		_add_tri(st_med, Vector3(0.0, 0.012, 0.0), mp1, mp0, Vector3.UP)
	var medallion := MeshInstance3D.new()
	medallion.mesh = st_med.commit()
	medallion.material_override = _mat_terrazzo
	ground_root.add_child(medallion)

	# B. Central Hebe Fountain at (0, 0, 0) (Outer radius 1.42m so Player Spawn at (2.0, 0.1, 0.0) stands on dry marble!)
	var fountain_sb := StaticBody3D.new()
	fountain_sb.name = "HebeFountain"
	ground_root.add_child(fountain_sb)

	var f_col := CollisionShape3D.new()
	var f_cyl_shape := CylinderShape3D.new()
	f_cyl_shape.radius = 1.42
	f_cyl_shape.height = 0.46
	f_col.shape = f_cyl_shape
	f_col.position = Vector3(0.0, 0.23, 0.0)
	fountain_sb.add_child(f_col)

	var rim_mi := MeshInstance3D.new()
	var rim_torus := TorusMesh.new()
	rim_torus.inner_radius = 1.18
	rim_torus.outer_radius = 1.44
	rim_torus.rings = 48
	rim_torus.ring_segments = 24
	rim_mi.mesh = rim_torus
	rim_mi.position = Vector3(0.0, 0.24, 0.0)
	rim_mi.material_override = _mat_marble
	fountain_sb.add_child(rim_mi)

	var water_mi := MeshInstance3D.new()
	var water_cyl := CylinderMesh.new()
	water_cyl.top_radius = 1.28
	water_cyl.bottom_radius = 1.28
	water_cyl.height = 0.04
	water_cyl.radial_segments = 48
	water_mi.mesh = water_cyl
	water_mi.position = Vector3(0.0, 0.30, 0.0)
	water_mi.material_override = mat_water
	fountain_sb.add_child(water_mi)

	var ped_mi := MeshInstance3D.new()
	var ped_cyl := CylinderMesh.new()
	ped_cyl.top_radius = 0.28
	ped_cyl.bottom_radius = 0.42
	ped_cyl.height = 1.45
	ped_mi.mesh = ped_cyl
	ped_mi.position = Vector3(0.0, 0.725, 0.0)
	ped_mi.material_override = _mat_plaster
	fountain_sb.add_child(ped_mi)

	var bowl_mi := MeshInstance3D.new()
	var bowl_cyl := CylinderMesh.new()
	bowl_cyl.top_radius = 0.68
	bowl_cyl.bottom_radius = 0.26
	bowl_cyl.height = 0.22
	bowl_mi.mesh = bowl_cyl
	bowl_mi.position = Vector3(0.0, 1.38, 0.0)
	bowl_mi.material_override = _mat_marble
	fountain_sb.add_child(bowl_mi)

	var spire_mi := MeshInstance3D.new()
	var spire_cyl := CylinderMesh.new()
	spire_cyl.top_radius = 0.10
	spire_cyl.bottom_radius = 0.20
	spire_cyl.height = 0.85
	spire_mi.mesh = spire_cyl
	spire_mi.position = Vector3(0.0, 1.85, 0.0)
	spire_mi.material_override = _mat_steel
	fountain_sb.add_child(spire_mi)

	var fountain_light := OmniLight3D.new()
	fountain_light.position = Vector3(0.0, 1.15, 0.0)
	fountain_light.light_color = Color(0.96, 0.93, 0.86)
	fountain_light.light_energy = 1.15
	fountain_light.omni_range = 12.0
	fountain_sb.add_child(fountain_light)

	var f_lbl := Label3D.new()
	f_lbl.position = Vector3(0.0, 2.55, 0.0)
	f_lbl.text = "Hebe Fountain — Double-Radius Central Oval Atrium [0, 0, 0]\n(7 Built-In Floors + Procedural Endless Mode | Tune Spirit Box to 104.7 FM)"
	f_lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	f_lbl.font_size = 19
	f_lbl.outline_size = 5
	f_lbl.modulate = Color(0.88, 0.94, 0.98)
	f_lbl.visibility_range_end = 14.0
	f_lbl.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
	fountain_sb.add_child(f_lbl)

	# C. 4-Quadrant Geothermal Hot-Spring Water Channels (Expanded for Double-Radius Oval!)
	var quad_centers := [
		Vector3(-7.8, 0.0, -8.5),
		Vector3(7.8, 0.0, -8.5),
		Vector3(-7.8, 0.0, 8.5),
		Vector3(7.8, 0.0, 8.5),
	]
	for qpos in quad_centers:
		var basin := StaticBody3D.new()
		basin.position = qpos
		ground_root.add_child(basin)

		var b_col := CollisionShape3D.new()
		var b_shape := CylinderShape3D.new()
		b_shape.radius = 2.40
		b_shape.height = 0.34
		b_col.shape = b_shape
		b_col.position = Vector3(0.0, 0.17, 0.0)
		basin.add_child(b_col)

		var curb_mi := MeshInstance3D.new()
		var curb_tor := TorusMesh.new()
		curb_tor.inner_radius = 2.10
		curb_tor.outer_radius = 2.42
		curb_tor.rings = 36
		curb_tor.ring_segments = 16
		curb_mi.mesh = curb_tor
		curb_mi.position = Vector3(0.0, 0.18, 0.0)
		curb_mi.material_override = _mat_marble
		basin.add_child(curb_mi)

		var b_water := MeshInstance3D.new()
		var bw_cyl := CylinderMesh.new()
		bw_cyl.top_radius = 2.22
		bw_cyl.bottom_radius = 2.22
		bw_cyl.height = 0.04
		b_water.mesh = bw_cyl
		b_water.position = Vector3(0.0, 0.22, 0.0)
		b_water.material_override = mat_water
		basin.add_child(b_water)

# ------------------------------------------------------------------------------
# 2. Single Upper Floor Builder (Watertight Double-Radius Oval Island +
#    Continuous Outer Elliptical-to-Rectangular Gallery Slab + 4 Bridges + Void Escalators!)
# ------------------------------------------------------------------------------
func _build_single_upper_floor_oval_and_bridges(floor_idx: int) -> void:
	if _built_upper_floors.has(floor_idx):
		return
	var floor_num := floor_idx + 1 # 2, 3, 4, 5, 6, 7...
	var floor_y := float(floor_idx) * floor_to_floor_height
	var bot_y := floor_y - slab_thickness

	var floor_root := Node3D.new()
	floor_root.name = "OvalAtriumFloor_%d" % floor_num
	add_child(floor_root)
	_built_upper_floors[floor_idx] = floor_root

	var oval_sb := StaticBody3D.new()
	oval_sb.name = "CentralOvalIslandDeck_F%d" % floor_num
	floor_root.add_child(oval_sb)

	var st_island_top := SurfaceTool.new()
	st_island_top.begin(Mesh.PRIMITIVE_TRIANGLES)
	var st_outer_top := SurfaceTool.new()
	st_outer_top.begin(Mesh.PRIMITIVE_TRIANGLES)
	var st_fascia := SurfaceTool.new()
	st_fascia.begin(Mesh.PRIMITIVE_TRIANGLES)
	var st_cove := SurfaceTool.new()
	st_cove.begin(Mesh.PRIMITIVE_TRIANGLES)
	var st_glass := SurfaceTool.new()
	st_glass.begin(Mesh.PRIMITIVE_TRIANGLES)
	var st_metal := SurfaceTool.new()
	st_metal.begin(Mesh.PRIMITIVE_TRIANGLES)

	# Build sorted list of angles (96 uniform steps + exact 4 rectangle corner angles so outer slab hits (+-46, +-42) with ZERO corner clipping!)
	var angles: Array[float] = []
	var segs := maxi(curve_segments, 72)
	for i in range(segs):
		angles.append(float(i) * TAU / float(segs))
	var corner_angles := [
		atan2(outer_extent_z, outer_extent_x),
		atan2(outer_extent_z, -outer_extent_x),
		TAU + atan2(-outer_extent_z, -outer_extent_x),
		TAU + atan2(-outer_extent_z, outer_extent_x),
	]
	for ca in corner_angles:
		var exists := false
		for a in angles:
			if absf(a - ca) < 0.002:
				exists = true
				break
		if not exists:
			angles.append(ca)
	angles.sort()
	angles.append(TAU)

	for i in range(angles.size() - 1):
		var a0: float = angles[i]
		var a1: float = angles[i + 1]
		var mid_a := (a0 + a1) * 0.5

		# 1. CENTRAL OVAL ISLAND (rx = 16.0m, rz = 20.0m)
		var p0_top := evaluate_ellipse_point(oval_island_radius_x, oval_island_radius_z, a0, floor_y)
		var p1_top := evaluate_ellipse_point(oval_island_radius_x, oval_island_radius_z, a1, floor_y)
		var p0_bot := evaluate_ellipse_point(oval_island_radius_x, oval_island_radius_z, a0, bot_y)
		var p1_bot := evaluate_ellipse_point(oval_island_radius_x, oval_island_radius_z, a1, bot_y)
		var center_top := Vector3(0.0, floor_y, 0.0)
		var center_bot := Vector3(0.0, bot_y, 0.0)

		_add_tri(st_island_top, center_top, p1_top, p0_top, Vector3.UP)
		_add_tri(st_fascia, center_bot, p0_bot, p1_bot, Vector3.DOWN)

		var out_n := Vector3(cos(mid_a) / oval_island_radius_x, 0.0, sin(mid_a) / oval_island_radius_z).normalized()
		_add_quad(st_fascia, p0_bot, p1_bot, p1_top, p0_top, out_n)

		var c0_bot := p0_bot + out_n * 0.015 + Vector3(0, 0.04, 0)
		var c1_bot := p1_bot + out_n * 0.015 + Vector3(0, 0.04, 0)
		_add_quad(st_cove, c0_bot, c1_bot, c1_bot + Vector3(0, 0.09, 0), c0_bot + Vector3(0, 0.09, 0), out_n)

		# 2. CONTINUOUS OUTER CONCOURSE GALLERY SLAB (From AtriumRadius (28.5m, 30.0m) out to OuterRectangle (+-46.0m, +-42.0m)!)
		var a_p0_top := evaluate_ellipse_point(atrium_radius_x, atrium_radius_z, a0, floor_y)
		var a_p1_top := evaluate_ellipse_point(atrium_radius_x, atrium_radius_z, a1, floor_y)
		var a_p0_bot := evaluate_ellipse_point(atrium_radius_x, atrium_radius_z, a0, bot_y)
		var a_p1_bot := evaluate_ellipse_point(atrium_radius_x, atrium_radius_z, a1, bot_y)

		var r_p0_top := evaluate_ellipse_point(concourse_ring_radius_x, concourse_ring_radius_z, a0, floor_y)
		var r_p1_top := evaluate_ellipse_point(concourse_ring_radius_x, concourse_ring_radius_z, a1, floor_y)
		var r_p0_bot := evaluate_ellipse_point(concourse_ring_radius_x, concourse_ring_radius_z, a0, bot_y)
		var r_p1_bot := evaluate_ellipse_point(concourse_ring_radius_x, concourse_ring_radius_z, a1, bot_y)

		var o_p0_top := project_direction_to_rectangle(a0, outer_extent_x, outer_extent_z, floor_y)
		var o_p1_top := project_direction_to_rectangle(a1, outer_extent_x, outer_extent_z, floor_y)
		var o_p0_bot := project_direction_to_rectangle(a0, outer_extent_x, outer_extent_z, bot_y)
		var o_p1_bot := project_direction_to_rectangle(a1, outer_extent_x, outer_extent_z, bot_y)

		# Top walkable marble quads (AtriumRim -> ConcourseRing -> OuterRect)
		_add_quad(st_outer_top, a_p0_top, r_p0_top, r_p1_top, a_p1_top, Vector3.UP)
		_add_quad(st_outer_top, r_p0_top, o_p0_top, o_p1_top, r_p1_top, Vector3.UP)

		# Bottom plaster soffit quads
		_add_quad(st_fascia, a_p0_bot, a_p1_bot, r_p1_bot, r_p0_bot, Vector3.DOWN)
		_add_quad(st_fascia, r_p0_bot, r_p1_bot, o_p1_bot, o_p0_bot, Vector3.DOWN)

		var in_n := -Vector3(cos(mid_a) / atrium_radius_x, 0.0, sin(mid_a) / atrium_radius_z).normalized()
		_add_quad(st_fascia, a_p1_bot, a_p0_bot, a_p0_top, a_p1_top, in_n)

		var ac0_bot := a_p0_bot + in_n * 0.015 + Vector3(0, 0.04, 0)
		var ac1_bot := a_p1_bot + in_n * 0.015 + Vector3(0, 0.04, 0)
		_add_quad(st_cove, ac1_bot, ac0_bot, ac0_bot + Vector3(0, 0.09, 0), ac1_bot + Vector3(0, 0.09, 0), in_n)

		# 3. CENTRAL OVAL OUTER BALUSTRADE (100% continuous except at the 4 Cardinal Bridge openings!)
		var mid_oval_pt := (p0_top + p1_top) * 0.5
		if not is_point_in_cardinal_bridge_opening(mid_oval_pt.x, mid_oval_pt.z, 0.06):
			_add_balustrade_segment(st_glass, st_metal, oval_sb, p0_top, p1_top, out_n, balustrade_height, (i % 2 == 0))

		# 4. OUTER ATRIUM INNER BALUSTRADE (100% continuous except at the 4 Cardinal Bridges, Escalator Portals, and Freight Elevator Portal!)
		var mid_atrium_pt := (a_p0_top + a_p1_top) * 0.5
		if (
			not is_point_in_cardinal_bridge_opening(mid_atrium_pt.x, mid_atrium_pt.z, 0.06)
			and not is_point_in_outer_escalator_or_elevator_portal(mid_atrium_pt.x, mid_atrium_pt.z)
		):
			_add_balustrade_segment(st_glass, st_metal, oval_sb, a_p1_top, a_p0_top, in_n, balustrade_height, (i % 2 == 0))

	# Commit Island Top Mesh + Trimesh Collision + Solid BoxShape3D Backup Slices
	var mi_island_top := MeshInstance3D.new()
	mi_island_top.mesh = st_island_top.commit()
	mi_island_top.material_override = _mat_terrazzo
	oval_sb.add_child(mi_island_top)
	var island_col := CollisionShape3D.new()
	island_col.shape = mi_island_top.mesh.create_trimesh_shape()
	oval_sb.add_child(island_col)

	for slice_i in range(12):
		var frac := (float(slice_i) + 0.5) / 12.0
		var z_half := oval_island_radius_z * sin(acos(clampf((frac - 0.5) * 1.92, -0.97, 0.97)))
		var x_pos := lerpf(-oval_island_radius_x * 0.92, oval_island_radius_x * 0.92, frac)
		var slice_col := CollisionShape3D.new()
		var slice_box := BoxShape3D.new()
		slice_box.size = Vector3((oval_island_radius_x * 1.95) / 12.0, slab_thickness, z_half * 2.0)
		slice_col.shape = slice_box
		slice_col.position = Vector3(x_pos, floor_y - slab_thickness * 0.5, 0.0)
		oval_sb.add_child(slice_col)

	# Commit Outer Concourse Gallery Top Mesh + Trimesh Collision + 4 Outer Rectangular BoxShape3D Safety Slabs
	var mi_outer_top := MeshInstance3D.new()
	mi_outer_top.mesh = st_outer_top.commit()
	mi_outer_top.material_override = _mat_marble
	oval_sb.add_child(mi_outer_top)
	var outer_col := CollisionShape3D.new()
	outer_col.shape = mi_outer_top.mesh.create_trimesh_shape()
	oval_sb.add_child(outer_col)

	# Solid BoxShape3D backup slabs for the outer rectangular rings (North, South, West, East outside the Atrium Ellipse)
	var backup_boxes := [
		{"pos": Vector3(0.0, floor_y - slab_thickness * 0.5, -36.0), "size": Vector3(outer_extent_x * 2.0, slab_thickness, 12.0)},
		{"pos": Vector3(0.0, floor_y - slab_thickness * 0.5, 36.0), "size": Vector3(outer_extent_x * 2.0, slab_thickness, 12.0)},
		{"pos": Vector3(-37.25, floor_y - slab_thickness * 0.5, 0.0), "size": Vector3(17.5, slab_thickness, outer_extent_z * 2.0)},
		{"pos": Vector3(37.25, floor_y - slab_thickness * 0.5, 0.0), "size": Vector3(17.5, slab_thickness, outer_extent_z * 2.0)},
	]
	for bb in backup_boxes:
		var bcol := CollisionShape3D.new()
		var bshape := BoxShape3D.new()
		bshape.size = bb["size"]
		bcol.shape = bshape
		bcol.position = bb["pos"]
		oval_sb.add_child(bcol)

	var mi_fascia := MeshInstance3D.new()
	mi_fascia.mesh = st_fascia.commit()
	mi_fascia.material_override = _mat_plaster
	floor_root.add_child(mi_fascia)

	var mi_cove := MeshInstance3D.new()
	mi_cove.mesh = st_cove.commit()
	mi_cove.material_override = _mat_cove
	floor_root.add_child(mi_cove)

	var mi_glass := MeshInstance3D.new()
	mi_glass.mesh = st_glass.commit()
	mi_glass.material_override = _mat_glass
	floor_root.add_child(mi_glass)

	var mi_metal := MeshInstance3D.new()
	mi_metal.mesh = st_metal.commit()
	mi_metal.material_override = _mat_steel
	floor_root.add_child(mi_metal)

	# B. 4 Wide (6.40m) Cardinal Clear-Span Bridges (North, South, West, East)
	#    1. North Bridge: X = [-3.20 .. +3.20], Z = [-31.0 .. -18.5] (12.5m total deck overlapping both landings!)
	_build_cardinal_bridge(
		floor_root,
		Vector3(0.0, floor_y, -24.75),
		Vector3(bridge_half_width * 2.0, slab_thickness, 12.5),
		true,
		false
	)
	#    2. South Bridge: X = [-3.20 .. +3.20], Z = [+18.5 .. +31.0] (with sealed West/East portals at Z = +22.2m for the Void Escalators!)
	_build_cardinal_bridge(
		floor_root,
		Vector3(0.0, floor_y, 24.75),
		Vector3(bridge_half_width * 2.0, slab_thickness, 12.5),
		true,
		true
	)
	#    3. West Bridge: X = [-30.0 .. -14.5], Z = [-3.20 .. +3.20]
	_build_cardinal_bridge(
		floor_root,
		Vector3(-22.25, floor_y, 0.0),
		Vector3(15.5, slab_thickness, bridge_half_width * 2.0),
		false,
		false
	)
	#    4. East Bridge: X = [+14.5 .. +30.0], Z = [-3.20 .. +3.20]
	_build_cardinal_bridge(
		floor_root,
		Vector3(22.25, floor_y, 0.0),
		Vector3(15.5, slab_thickness, bridge_half_width * 2.0),
		false,
		false
	)

	# C. Sealed Freight Elevator Landing Apron at X = [7.6 .. 12.4], Z = [-29.5 .. -27.6]
	#    Connects the Outer Atrium Rim directly to the Freight Elevator's North Shaft Safety Gate at Z = -27.78m!
	_create_pbr_static_box(floor_root, Vector3(10.0, floor_y - slab_thickness * 0.5 - 0.006, -28.55), Vector3(4.8, slab_thickness, 1.9), _mat_marble)
	_create_pbr_static_box(floor_root, Vector3(7.65, floor_y + balustrade_height * 0.5, -28.45), Vector3(0.12, balustrade_height, 1.7), _mat_glass)
	_create_pbr_static_box(floor_root, Vector3(12.35, floor_y + balustrade_height * 0.5, -28.45), Vector3(0.12, balustrade_height, 1.7), _mat_glass)

	# D. Void-Spanning Escalator Pair connecting Floor (floor_num - 1) <-> Floor (floor_num)!
	#    Spans across the South Elliptical Atrium Void at Z = +22.20m with ZERO floor cutouts!
	_build_void_escalator_pair_for_floor_transition(floor_root, floor_num - 1, floor_num)

func _build_void_escalator_pair_for_floor_transition(parent: Node3D, lower_floor_num: int, upper_floor_num: int) -> void:
	var lower_y := float(lower_floor_num - 1) * floor_to_floor_height
	var upper_y := float(upper_floor_num - 1) * floor_to_floor_height

	# 1. Sealed Lower & Upper Landing Apron Pads + Side Guardrails so there is ZERO gap between the curved rim/bridge and the escalator!
	if lower_floor_num >= 2:
		# Lower West & East Outer Rim Aprons at Y = lower_y, Z = +22.20m
		for sx in [-1.0, 1.0]:
			_create_pbr_static_box(parent, Vector3(sx * 19.3, lower_y - slab_thickness * 0.5 - 0.006, ESCALATOR_VOID_Z), Vector3(2.8, slab_thickness, 1.9), _mat_marble)
			_create_pbr_static_box(parent, Vector3(sx * 18.8, lower_y + balustrade_height * 0.5, ESCALATOR_VOID_Z - 0.78), Vector3(1.8, balustrade_height, 0.10), _mat_glass)
			_create_pbr_static_box(parent, Vector3(sx * 18.8, lower_y + balustrade_height * 0.5, ESCALATOR_VOID_Z + 0.78), Vector3(1.8, balustrade_height, 0.10), _mat_glass)

	# Upper Bridge Landing Aprons at Y = upper_y, Z = +22.20m (extending from X = +-2.4m to +-4.1m on the South Cardinal Bridge!)
	for sx in [-1.0, 1.0]:
		_create_pbr_static_box(parent, Vector3(sx * 3.25, upper_y - slab_thickness * 0.5 - 0.006, ESCALATOR_VOID_Z), Vector3(1.9, slab_thickness, 1.9), _mat_marble)
		_create_pbr_static_box(parent, Vector3(sx * 3.55, upper_y + balustrade_height * 0.5, ESCALATOR_VOID_Z - 0.78), Vector3(1.3, balustrade_height, 0.10), _mat_glass)
		_create_pbr_static_box(parent, Vector3(sx * 3.55, upper_y + balustrade_height * 0.5, ESCALATOR_VOID_Z + 0.78), Vector3(1.3, balustrade_height, 0.10), _mat_glass)

	# 2. Upward Void Escalator (West Void: X = -19.82m -> -2.50m, Y = lower_y -> upper_y, Z = +22.20m)
	var esc_up := MallEscalator.new()
	esc_up.name = "Escalator_F%d_To_F%d_Up" % [lower_floor_num, upper_floor_num]
	esc_up.escalator_title = "Atrium Void Escalator (F%d -> F%d)" % [lower_floor_num, upper_floor_num]
	esc_up.circuit_tag = "EscalatorCircuit_F%d_To_F%d" % [lower_floor_num, upper_floor_num]
	esc_up.position = Vector3(ESCALATOR_WEST_OUTER_X, lower_y, ESCALATOR_VOID_Z)
	esc_up.rotation_degrees = Vector3.ZERO
	esc_up.reverse_direction = false
	esc_up.set_operational_mode(MallEscalator.MaintenanceState.OPERATIONAL, true)
	parent.add_child(esc_up)
	escalators_up[lower_floor_num] = esc_up

	# 3. Downward Void Escalator (East Void: X = +2.50m -> +19.82m, Y = upper_y -> lower_y, Z = +22.20m)
	var esc_down := MallEscalator.new()
	esc_down.name = "Escalator_F%d_To_F%d_Down" % [upper_floor_num, lower_floor_num]
	esc_down.escalator_title = "Atrium Void Escalator (F%d -> F%d)" % [upper_floor_num, lower_floor_num]
	esc_down.circuit_tag = "EscalatorCircuit_F%d_To_F%d_Down" % [upper_floor_num, lower_floor_num]
	esc_down.position = Vector3(ESCALATOR_EAST_OUTER_X, lower_y, ESCALATOR_VOID_Z)
	esc_down.rotation_degrees = Vector3(0.0, 180.0, 0.0) # Local +X points West (-X), so upper end is at X = +2.50m and lower end is at X = +19.82m!
	esc_down.reverse_direction = true
	esc_down.set_operational_mode(MallEscalator.MaintenanceState.OPERATIONAL, true)
	parent.add_child(esc_down)
	escalators_down[lower_floor_num] = esc_down

func _build_cardinal_bridge(
	parent: Node3D,
	top_center: Vector3,
	deck_size: Vector3,
	runs_north_south: bool,
	has_escalator_portal: bool
) -> void:
	var sb := StaticBody3D.new()
	sb.position = top_center
	parent.add_child(sb)

	var col := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = deck_size
	col.shape = box
	col.position = Vector3(0.0, -deck_size.y * 0.5, 0.0)
	sb.add_child(col)

	var deck_mi := MeshInstance3D.new()
	var deck_bm := BoxMesh.new()
	deck_bm.size = deck_size - Vector3(0.0, 0.008, 0.0)
	deck_mi.mesh = deck_bm
	deck_mi.position = Vector3(0.0, -deck_size.y * 0.5 - 0.004, 0.0)
	deck_mi.material_override = _mat_marble
	sb.add_child(deck_mi)

	var soffit_mi := MeshInstance3D.new()
	var soffit_bm := BoxMesh.new()
	soffit_bm.size = Vector3(deck_size.x + 0.08, deck_size.y - 0.04, deck_size.z - 0.08)
	soffit_mi.mesh = soffit_bm
	soffit_mi.position = Vector3(0.0, -deck_size.y * 0.5 - 0.01, 0.0)
	soffit_mi.material_override = _mat_plaster
	sb.add_child(soffit_mi)

	if runs_north_south:
		var z_min := top_center.z - deck_size.z * 0.5
		var z_max := top_center.z + deck_size.z * 0.5
		for side_sign in [-1.0, 1.0]:
			var rail_x: float = side_sign * (deck_size.x * 0.5 - 0.06)
			if has_escalator_portal:
				# Split rail around the Void Escalator Landing Portal at World Z = ESCALATOR_VOID_Z (+22.2m, half-gap = 0.72m)!
				var gap_z0 := ESCALATOR_VOID_Z - 0.72
				var gap_z1 := ESCALATOR_VOID_Z + 0.72
				if gap_z0 > z_min + 0.2:
					var len_a := gap_z0 - z_min
					var mid_za := (z_min + gap_z0) * 0.5 - top_center.z
					_add_straight_bridge_rail(sb, Vector3(rail_x, balustrade_height * 0.5, mid_za), Vector3(0.12, balustrade_height, len_a))
				if z_max > gap_z1 + 0.2:
					var len_b := z_max - gap_z1
					var mid_zb := (gap_z1 + z_max) * 0.5 - top_center.z
					_add_straight_bridge_rail(sb, Vector3(rail_x, balustrade_height * 0.5, mid_zb), Vector3(0.12, balustrade_height, len_b))
			else:
				_add_straight_bridge_rail(sb, Vector3(rail_x, balustrade_height * 0.5, 0.0), Vector3(0.12, balustrade_height, deck_size.z))
	else:
		for side_sign in [-1.0, 1.0]:
			var rail_z: float = side_sign * (deck_size.z * 0.5 - 0.06)
			_add_straight_bridge_rail(sb, Vector3(0.0, balustrade_height * 0.5, rail_z), Vector3(deck_size.x, balustrade_height, 0.12))

func _add_straight_bridge_rail(sb: StaticBody3D, rail_pos: Vector3, rail_size: Vector3) -> void:
	var r_col := CollisionShape3D.new()
	var r_shape := BoxShape3D.new()
	r_shape.size = rail_size
	r_col.shape = r_shape
	r_col.position = rail_pos
	sb.add_child(r_col)

	var glass_mi := MeshInstance3D.new()
	var glass_bm := BoxMesh.new()
	glass_bm.size = rail_size - Vector3(0.02, 0.18, 0.02)
	glass_mi.mesh = glass_bm
	glass_mi.position = rail_pos
	glass_mi.material_override = _mat_glass
	sb.add_child(glass_mi)

	var handrail_mi := MeshInstance3D.new()
	var handrail_bm := BoxMesh.new()
	handrail_bm.size = Vector3(rail_size.x + 0.04, 0.08, rail_size.z + 0.04)
	handrail_mi.mesh = handrail_bm
	handrail_mi.position = Vector3(rail_pos.x, balustrade_height - 0.04, rail_pos.z)
	handrail_mi.material_override = _mat_steel
	sb.add_child(handrail_mi)

	var shoe_mi := MeshInstance3D.new()
	var shoe_bm := BoxMesh.new()
	shoe_bm.size = Vector3(rail_size.x + 0.04, 0.10, rail_size.z + 0.04)
	shoe_mi.mesh = shoe_bm
	shoe_mi.position = Vector3(rail_pos.x, 0.05, rail_pos.z)
	shoe_mi.material_override = _mat_steel
	sb.add_child(shoe_mi)

# ------------------------------------------------------------------------------
# 3. Floor 2 (Y = 8.50m) Double-Radius Connected Oval Island Stores (4 Large Boutiques!)
# ------------------------------------------------------------------------------
func _build_floor2_connected_oval_stores() -> void:
	var f2_y := floor_to_floor_height
	var oval_stores_root := Node3D.new()
	oval_stores_root.name = "Floor2ConnectedOvalStores"
	add_child(oval_stores_root)

	# Expanded for the 32m x 40m Double-Radius Oval Island:
	# - Central Cross-Arcade: X = -2.8..+2.8m and Z = -2.8..+2.8m (5.6m wide grand cross-arcade!)
	# - 4 Large Connected Oval Store Pavilions (6.4m x 7.8m each) at (+-6.2, f2_y, +-6.8)
	# - Surrounded by a wide 4.5m 360-degree Inner Oval Ring Walkway!
	var wall_h := 3.60
	var pav_specs := [
		{
			"id": "Oval_F2_NW",
			"title": "LUMEN OPTICS & CAMERAS [Oval NW]",
			"center": Vector3(-6.2, f2_y, -6.8),
			"sign_pos": Vector3(-6.2, f2_y + 3.75, -2.9),
			"prop_path": "res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25339_GlazedDisplayCounter.fbx",
			"pickup_id": "video_camera",
		},
		{
			"id": "Oval_F2_NE",
			"title": "IMPERIAL JADE & SILK [Oval NE]",
			"center": Vector3(6.2, f2_y, -6.8),
			"sign_pos": Vector3(6.2, f2_y + 3.75, -2.9),
			"prop_path": "res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25225_TieredRoundTable.fbx",
			"pickup_id": "hinoki_censer",
		},
		{
			"id": "Oval_F2_SW",
			"title": "SOLARIS HI-FI & VINYL [Oval SW]",
			"center": Vector3(-6.2, f2_y, 6.8),
			"sign_pos": Vector3(-6.2, f2_y + 3.75, 2.9),
			"prop_path": "res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25310_CashDesk120.fbx",
			"pickup_id": "audio_recorder",
		},
		{
			"id": "Oval_F2_SE",
			"title": "ARCADIA BOTANICALS [Oval SE]",
			"center": Vector3(6.2, f2_y, 6.8),
			"sign_pos": Vector3(6.2, f2_y + 3.75, 2.9),
			"prop_path": "res://ExternalAssets/Mall/PolyHaven/ImportReady/SM_RetailShelves01.fbx",
			"pickup_id": "pink_salt_pouch",
		},
	]

	for spec in pav_specs:
		var c: Vector3 = spec["center"]
		var sx := -1.0 if c.x < 0.0 else 1.0
		var sz := -1.0 if c.z < 0.0 else 1.0

		_create_pbr_static_box(oval_stores_root, Vector3(c.x, f2_y + wall_h, c.z), Vector3(6.2, 0.24, 7.6), _mat_ceiling)
		_create_pbr_static_box(oval_stores_root, Vector3(c.x + sx * 2.85, f2_y + wall_h * 0.5, c.z + sz * 3.55), Vector3(0.40, wall_h, 0.40), _mat_plaster)
		_create_pbr_static_box(oval_stores_root, Vector3(c.x + sx * 2.95, f2_y + wall_h * 0.5, c.z + sz * 1.80), Vector3(0.12, wall_h - 0.2, 2.60), _mat_glass)
		_create_pbr_static_box(oval_stores_root, Vector3(c.x + sx * 1.40, f2_y + wall_h * 0.5, c.z + sz * 3.65), Vector3(2.40, wall_h - 0.2, 0.12), _mat_glass)
		_create_pbr_static_box(oval_stores_root, Vector3(c.x, f2_y + wall_h - 0.18, c.z - sz * 3.65), Vector3(6.10, 0.30, 0.18), _mat_steel)

		var sign_lbl := Label3D.new()
		sign_lbl.position = spec["sign_pos"]
		sign_lbl.text = str(spec["title"])
		sign_lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		sign_lbl.font_size = 21
		sign_lbl.outline_size = 6
		sign_lbl.modulate = Color(0.98, 0.93, 0.80)
		sign_lbl.visibility_range_end = 24.0
		sign_lbl.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
		oval_stores_root.add_child(sign_lbl)

		var prop_path: String = str(spec["prop_path"])
		if ResourceLoader.exists(prop_path):
			var packed := load(prop_path) as PackedScene
			if packed:
				var inst := packed.instantiate() as Node3D
				if inst:
					inst.position = Vector3(c.x + sx * 0.9, f2_y, c.z + sz * 0.9)
					MallPbrMaterialLibrary.beautify_model_materials(inst, prop_path, Color(0.76, 0.72, 0.66))
					oval_stores_root.add_child(inst)
					var sb := StaticBody3D.new()
					sb.position = inst.position
					oval_stores_root.add_child(sb)
					var col := CollisionShape3D.new()
					var box := BoxShape3D.new()
					box.size = Vector3(1.1, 1.0, 0.8)
					col.shape = box
					col.position = Vector3(0.0, 0.5, 0.0)
					sb.add_child(col)

		var p_light := OmniLight3D.new()
		p_light.position = Vector3(c.x, f2_y + wall_h - 0.35, c.z)
		p_light.light_color = Color(0.98, 0.94, 0.86)
		p_light.light_energy = 1.15
		p_light.omni_range = 10.5
		p_light.distance_fade_enabled = true
		p_light.distance_fade_begin = 38.0
		p_light.distance_fade_length = 14.0
		oval_stores_root.add_child(p_light)

		if not Engine.is_editor_hint():
			var drop := PlaceableEvidenceActors.WorldDroppedItem.new()
			drop.item_data = PlayerController.make_item_data(str(spec["pickup_id"]))
			drop.position = Vector3(c.x - sx * 0.65, f2_y + 0.05, c.z - sz * 0.65)
			oval_stores_root.add_child(drop)

# ------------------------------------------------------------------------------
# 4. Floor 3 (Y = 17.00m) Central Oval Sky-Arboretum Conservatory & Upper Floors 4..7
# ------------------------------------------------------------------------------
func _build_floor3_oval_sky_arboretum_conservatory() -> void:
	var f3_y := floor_to_floor_height * 2.0 # 17.00m
	var f3_root := Node3D.new()
	f3_root.name = "Floor3OvalSkyArboretum"
	add_child(f3_root)

	var center_planter := StaticBody3D.new()
	center_planter.position = Vector3(0.0, f3_y, 0.0)
	f3_root.add_child(center_planter)

	var p_col := CollisionShape3D.new()
	var p_cyl := CylinderShape3D.new()
	p_cyl.radius = 1.25
	p_cyl.height = 0.52
	p_col.shape = p_cyl
	p_col.position = Vector3(0.0, 0.26, 0.0)
	center_planter.add_child(p_col)

	var p_mi := MeshInstance3D.new()
	var p_tor := TorusMesh.new()
	p_tor.inner_radius = 1.02
	p_tor.outer_radius = 1.26
	p_tor.rings = 32
	p_tor.ring_segments = 16
	p_mi.mesh = p_tor
	p_mi.position = Vector3(0.0, 0.28, 0.0)
	p_mi.material_override = _mat_marble
	center_planter.add_child(p_mi)

	var tree_path := "res://ExternalAssets/Mall/PolyHaven/ImportReady/SM_PachiraD.fbx"
	if ResourceLoader.exists(tree_path):
		var packed := load(tree_path) as PackedScene
		if packed:
			var t_inst := packed.instantiate() as Node3D
			if t_inst:
				t_inst.position = Vector3(0.0, 0.35, 0.0)
				t_inst.scale = Vector3(1.25, 1.35, 1.25)
				MallPbrMaterialLibrary.beautify_model_materials(t_inst, tree_path, Color(0.24, 0.58, 0.32))
				center_planter.add_child(t_inst)

	var lbl := Label3D.new()
	lbl.position = Vector3(0.0, f3_y + 2.65, 0.0)
	lbl.text = "Floor 3 Central Oval Sky-Arboretum Conservatory [0, 17.0, 0]\n(Cross the 4 Cardinal Skybridges or Continue Ascending to Floors 4, 5, 6 & 7!)"
	lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	lbl.font_size = 20
	lbl.outline_size = 6
	lbl.modulate = Color(0.84, 0.96, 0.90)
	lbl.visibility_range_end = 18.0
	lbl.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
	f3_root.add_child(lbl)

func _build_upper_floors_4_to_7_oval_landmarks() -> void:
	var upper_themes := {
		4: {"title": "FLOOR 4: THE CIVIC & DEPARTMENT WING [Y = 25.5m]", "desc": "Mall City Civic Sanctuary & Department Store Concourse", "col": Color(0.82, 0.92, 0.98)},
		5: {"title": "FLOOR 5: THE BLACKOUT CONCOURSE & GREMLIN WARRENS [Y = 34.0m]", "desc": "High-Voltage Capacitor Ring & Emergency Revival Outpost", "col": Color(0.98, 0.86, 0.68)},
		6: {"title": "FLOOR 6: THE AUDIT & STABILIZATION RING [Y = 42.5m]", "desc": "104.7 FM Number-Station Beacon & Ring Coil Telemetry", "col": Color(0.76, 0.96, 0.92)},
		7: {"title": "FLOOR 7: THE GEODESIC ARK CANOPY & ENDLESS GATEWAY [Y = 51.0m]", "desc": "Elena Vance's Final Extraction Camp | Ride Elevator [E] to Generate Procedural Floor 8+!", "col": Color(0.68, 0.98, 1.0)},
	}
	for fl_num in upper_themes.keys():
		if fl_num - 1 > num_upper_floors:
			continue
		var info: Dictionary = upper_themes[fl_num]
		var fy := float(fl_num - 1) * floor_to_floor_height
		var fl_node: Node3D = _built_upper_floors.get(fl_num - 1, self)

		# Central Oval Sanctuary Rest Bench & Telemetry Beacon on Floors 4..7
		var bench_path := "res://ExternalAssets/Mall/RetailKit/ImportReady/SM_Retail_25366_ConcourseBench.fbx"
		if ResourceLoader.exists(bench_path):
			var packed := load(bench_path) as PackedScene
			if packed:
				for bx in [-4.5, 4.5]:
					var b_inst := packed.instantiate() as Node3D
					if b_inst:
						b_inst.position = Vector3(bx, fy, 0.0)
						MallPbrMaterialLibrary.beautify_model_materials(b_inst, bench_path, Color(0.54, 0.40, 0.28))
						fl_node.add_child(b_inst)

		var lbl := Label3D.new()
		lbl.position = Vector3(0.0, fy + 2.8, 0.0)
		lbl.text = "%s\n%s" % [info["title"], info["desc"]]
		lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		lbl.font_size = 22
		lbl.outline_size = 6
		lbl.modulate = info["col"]
		lbl.visibility_range_end = 26.0
		lbl.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
		fl_node.add_child(lbl)

func _build_procedural_oval_outpost(floor_num: int) -> void:
	var fy := float(floor_num - 1) * floor_to_floor_height
	var fl_node: Node3D = _built_upper_floors.get(floor_num - 1, self)
	var lbl := Label3D.new()
	lbl.position = Vector3(0.0, fy + 2.8, 0.0)
	lbl.text = "PROCEDURAL FLOOR %d: ENDLESS ESCALATION OUTPOST [Y = %.1fm]\n(Generated Dynamically — Ride Freight Elevator [E] to Ascend to Floor %d!)" % [
		floor_num, fy, floor_num + 1
	]
	lbl.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	lbl.font_size = 22
	lbl.outline_size = 6
	lbl.modulate = Color(0.75, 1.0, 0.88)
	lbl.visibility_range_end = 28.0
	lbl.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
	fl_node.add_child(lbl)

# ------------------------------------------------------------------------------
# 5. Monumental Multi-Story "Soul Tree" Columns & Suspended "Floating Forest" Pods
# ------------------------------------------------------------------------------
func _build_soul_trees_and_floating_forest_pods() -> void:
	if is_instance_valid(_trees_root):
		_trees_root.queue_free()
	_trees_root = Node3D.new()
	_trees_root.name = "MonumentalArboretumAndPods"
	add_child(_trees_root)

	var mat_bark := StandardMaterial3D.new()
	mat_bark.albedo_color = Color(0.46, 0.34, 0.24)
	mat_bark.roughness = 0.72
	var mat_foliage := StandardMaterial3D.new()
	mat_foliage.albedo_color = Color(0.24, 0.56, 0.30)
	mat_foliage.roughness = 0.65

	# Positioned in the open North-East & South-West Biotope Chasm between the 16x20m Oval Island and 28.5x30m Outer Rim!
	var tree_positions := [
		Vector3(-15.5, 0.0, -15.5),
		Vector3(15.5, 0.0, 15.5),
	]
	var trunk_h := float(num_upper_floors + 1) * floor_to_floor_height - 2.5
	for tpos in tree_positions:
		var tree_sb := StaticBody3D.new()
		tree_sb.position = tpos
		_trees_root.add_child(tree_sb)

		var col := CollisionShape3D.new()
		var cyl_shape := CylinderShape3D.new()
		cyl_shape.radius = 0.85
		cyl_shape.height = trunk_h
		col.shape = cyl_shape
		col.position = Vector3(0.0, trunk_h * 0.5, 0.0)
		tree_sb.add_child(col)

		var trunk_mi := MeshInstance3D.new()
		var trunk_cyl := CylinderMesh.new()
		trunk_cyl.top_radius = 0.65
		trunk_cyl.bottom_radius = 1.05
		trunk_cyl.height = trunk_h
		trunk_mi.mesh = trunk_cyl
		trunk_mi.position = Vector3(0.0, trunk_h * 0.5, 0.0)
		trunk_mi.material_override = mat_bark
		tree_sb.add_child(trunk_mi)

		for tier_i in range(num_upper_floors + 1):
			var canopy_y := 6.5 + float(tier_i) * floor_to_floor_height
			var canopy_mi := MeshInstance3D.new()
			var sph := SphereMesh.new()
			sph.radius = 2.6 - float(tier_i % 3) * 0.25
			sph.height = 1.85
			canopy_mi.mesh = sph
			canopy_mi.position = Vector3(0.0, canopy_y, 0.0)
			canopy_mi.material_override = mat_foliage
			tree_sb.add_child(canopy_mi)

# ------------------------------------------------------------------------------
# 6. Vaulted Geodesic Skylight Dome & Radial Spaceship Hull Arch Ribs
# ------------------------------------------------------------------------------
func _build_vaulted_geodesic_skylight_dome() -> void:
	if is_instance_valid(_dome_root):
		_dome_root.queue_free()
	_dome_root = Node3D.new()
	_dome_root.name = "VaultedGeodesicSkylightDome"
	add_child(_dome_root)

	var mat_cove_cyan := MallPbrMaterialLibrary.get_cove_accent_material(Color(0.58, 0.90, 0.98), 1.35)

	var roof_base_y := float(num_upper_floors + 1) * floor_to_floor_height # 59.5m for 7 floors!
	var dome_mid_y := roof_base_y + 3.2
	var dome_apex_y := roof_base_y + 6.0

	var segs := 48
	var step_rad := TAU / float(segs)

	var st_soffit := SurfaceTool.new()
	st_soffit.begin(Mesh.PRIMITIVE_TRIANGLES)
	var st_skylight := SurfaceTool.new()
	st_skylight.begin(Mesh.PRIMITIVE_TRIANGLES)
	var st_ribs := SurfaceTool.new()
	st_ribs.begin(Mesh.PRIMITIVE_TRIANGLES)
	var st_oculus := SurfaceTool.new()
	st_oculus.begin(Mesh.PRIMITIVE_TRIANGLES)

	for i in range(segs):
		var a0 := float(i) * step_rad
		var a1 := float(i + 1) * step_rad

		var r_outer0 := project_direction_to_rectangle(a0, outer_extent_x, outer_extent_z, roof_base_y)
		var r_outer1 := project_direction_to_rectangle(a1, outer_extent_x, outer_extent_z, roof_base_y)
		var r_ring0 := evaluate_ellipse_point(atrium_radius_x + 2.5, atrium_radius_z + 2.5, a0, roof_base_y)
		var r_ring1 := evaluate_ellipse_point(atrium_radius_x + 2.5, atrium_radius_z + 2.5, a1, roof_base_y)
		var r_mid0 := evaluate_ellipse_point(atrium_radius_x * 0.68, atrium_radius_z * 0.68, a0, dome_mid_y)
		var r_mid1 := evaluate_ellipse_point(atrium_radius_x * 0.68, atrium_radius_z * 0.68, a1, dome_mid_y)
		var r_oc0 := evaluate_ellipse_point(oval_island_radius_x * 0.42, oval_island_radius_z * 0.42, a0, dome_apex_y)
		var r_oc1 := evaluate_ellipse_point(oval_island_radius_x * 0.42, oval_island_radius_z * 0.42, a1, dome_apex_y)

		_add_quad(st_soffit, r_ring0, r_ring1, r_outer1, r_outer0, Vector3.DOWN)
		_add_quad(st_skylight, r_mid0, r_mid1, r_ring1, r_ring0, Vector3.DOWN)
		_add_quad(st_skylight, r_oc0, r_oc1, r_mid1, r_mid0, Vector3.DOWN)
		_add_tri(st_oculus, Vector3(0.0, dome_apex_y + 0.08, 0.0), r_oc0, r_oc1, Vector3.DOWN)

		if i % 3 == 0:
			_add_beam_segment(st_ribs, r_ring0, r_mid0, 0.22, 0.28)
			_add_beam_segment(st_ribs, r_mid0, r_oc0, 0.16, 0.22)

	var mi_soffit := MeshInstance3D.new()
	mi_soffit.mesh = st_soffit.commit()
	mi_soffit.material_override = _mat_plaster
	_dome_root.add_child(mi_soffit)

	var mi_sky := MeshInstance3D.new()
	mi_sky.mesh = st_skylight.commit()
	mi_sky.material_override = _mat_glass
	_dome_root.add_child(mi_sky)

	var mi_ribs := MeshInstance3D.new()
	mi_ribs.mesh = st_ribs.commit()
	mi_ribs.material_override = _mat_steel
	_dome_root.add_child(mi_ribs)

	var mi_oc := MeshInstance3D.new()
	mi_oc.mesh = st_oculus.commit()
	mi_oc.material_override = mat_cove_cyan
	_dome_root.add_child(mi_oc)

# ------------------------------------------------------------------------------
# 7. Procedural Mesh & Balustrade Helpers
# ------------------------------------------------------------------------------
func _add_tri(st: SurfaceTool, v0: Vector3, v1: Vector3, v2: Vector3, n: Vector3) -> void:
	var face_cross := (v1 - v0).cross(v2 - v0)
	var b := v1
	var c := v2
	if face_cross.dot(n) >= 0.0:
		b = v2
		c = v1
	st.set_normal(n)
	st.set_tangent(Plane(1.0, 0.0, 0.0, 1.0))
	st.set_uv(Vector2(v0.x * 0.25, v0.z * 0.25))
	st.add_vertex(v0)
	st.set_normal(n)
	st.set_tangent(Plane(1.0, 0.0, 0.0, 1.0))
	st.set_uv(Vector2(b.x * 0.25, b.z * 0.25))
	st.add_vertex(b)
	st.set_normal(n)
	st.set_tangent(Plane(1.0, 0.0, 0.0, 1.0))
	st.set_uv(Vector2(c.x * 0.25, c.z * 0.25))
	st.add_vertex(c)

func _add_quad(st: SurfaceTool, v0: Vector3, v1: Vector3, v2: Vector3, v3: Vector3, n: Vector3) -> void:
	_add_tri(st, v0, v1, v2, n)
	_add_tri(st, v0, v2, v3, n)

func _add_balustrade_segment(
	st_glass: SurfaceTool,
	st_metal: SurfaceTool,
	sb: StaticBody3D,
	p0: Vector3,
	p1: Vector3,
	face_n: Vector3,
	rail_h: float,
	add_post: bool
) -> void:
	var seg_vec := p1 - p0
	var seg_len := seg_vec.length()
	if seg_len < 0.02:
		return

	var curb_top := p0.y + 0.12
	var glass_top := p0.y + rail_h - 0.08
	var rail_top := p0.y + rail_h

	var g0_bot := Vector3(p0.x, curb_top, p0.z)
	var g1_bot := Vector3(p1.x, curb_top, p1.z)
	var g0_top := Vector3(p0.x, glass_top, p0.z)
	var g1_top := Vector3(p1.x, glass_top, p1.z)
	_add_quad(st_glass, g0_bot, g1_bot, g1_top, g0_top, face_n)
	_add_quad(st_glass, g1_bot, g0_bot, g0_top, g1_top, -face_n)

	_add_beam_segment(st_metal, Vector3(p0.x, p0.y + 0.06, p0.z), Vector3(p1.x, p1.y + 0.06, p1.z), 0.05, 0.06)
	_add_beam_segment(st_metal, Vector3(p0.x, rail_top - 0.04, p0.z), Vector3(p1.x, rail_top - 0.04, p1.z), 0.06, 0.04)

	if add_post:
		_add_beam_segment(st_metal, p0, Vector3(p0.x, rail_top, p0.z), 0.04, 0.04)

	# Solid BoxShape3D safety collider along the balustrade segment (with slight 0.08m overlap so zero micro-gaps between segments!)
	var mid := (p0 + p1) * 0.5 + Vector3(0.0, rail_h * 0.5, 0.0)
	var col := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = Vector3(seg_len + 0.08, rail_h, 0.16)
	col.shape = box
	col.position = mid
	col.rotation = Vector3(0.0, -atan2(seg_vec.z, seg_vec.x), 0.0)
	sb.add_child(col)

func _add_beam_segment(st: SurfaceTool, p0: Vector3, p1: Vector3, half_w: float, half_h: float) -> void:
	var dir := (p1 - p0).normalized()
	if dir.length_squared() < 0.001:
		return
	var right := Vector3.UP.cross(dir)
	if right.length_squared() < 0.001:
		right = Vector3.RIGHT
	else:
		right = right.normalized()
	var up := dir.cross(right).normalized()

	var a0 := p0 - right * half_w - up * half_h
	var a1 := p0 + right * half_w - up * half_h
	var a2 := p0 + right * half_w + up * half_h
	var a3 := p0 - right * half_w + up * half_h

	var b0 := p1 - right * half_w - up * half_h
	var b1 := p1 + right * half_w - up * half_h
	var b2 := p1 + right * half_w + up * half_h
	var b3 := p1 - right * half_w + up * half_h

	_add_quad(st, a3, a2, b2, b3, up)
	_add_quad(st, a0, b0, b1, a1, -up)
	_add_quad(st, a1, b1, b2, a2, right)
	_add_quad(st, a0, a3, b3, b0, -right)

func _create_pbr_static_box(parent: Node3D, center_pos: Vector3, size: Vector3, mat: Material) -> StaticBody3D:
	var sb := StaticBody3D.new()
	sb.position = center_pos
	parent.add_child(sb)

	var col := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = size
	col.shape = shape
	sb.add_child(col)

	var mi := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = size
	mi.mesh = bm
	mi.material_override = mat
	sb.add_child(mi)
	return sb
