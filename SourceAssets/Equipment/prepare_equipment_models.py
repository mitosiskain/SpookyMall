"""Prepare Unreal-ready ghost-hunting equipment FBX models in centimeter space (+X forward, Z up).

Usage:
  blender --background --factory-startup --python prepare_equipment_models.py

Imports downloaded Poly Haven CC0 glTF/FBX models where present, normalizes scale/pivot to Unreal
centimeter conventions (+X forward, Z up, bottom-center pivot at Z=0), and constructs clean meshes
for specialized ghost-hunting devices (Thermometer, Motion Sensor, UV Footprint).
"""
from pathlib import Path
import bpy
import hashlib
import json
import math
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
CC0_DIR = ROOT / "PolyHavenCC0"
OUT_DIR = ROOT / "Prepared"
OUT_DIR.mkdir(parents=True, exist_ok=True)


def reset_scene():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for mesh in list(bpy.data.meshes):
        bpy.data.meshes.remove(mesh, do_unlink=True)
    for mat in list(bpy.data.materials):
        bpy.data.materials.remove(mat, do_unlink=True)
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 0.01


def make_material(name: str, color, metallic: float = 0.1, roughness: float = 0.45, emission=None, emission_strength: float = 0.0):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1.0)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
        if emission and "Emission Color" in bsdf.inputs:
            bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
            bsdf.inputs["Emission Strength"].default_value = emission_strength
    return mat


def add_box(name: str, loc, dims, mat, bevel: float = 0.25):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dims
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel > 0.0:
        mod = obj.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 2
        bpy.ops.object.modifier_apply(modifier=mod.name)
    obj.data.materials.append(mat)
    return obj


def add_cylinder(name: str, loc, radius: float, depth: float, rot_deg, mat, vertices: int = 24):
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices,
        radius=radius,
        depth=depth,
        location=loc,
        rotation=tuple(math.radians(a) for a in rot_deg),
    )
    obj = bpy.context.object
    obj.name = name
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    obj.data.materials.append(mat)
    return obj


def finalize_and_export(parts, mesh_name: str, target_dims_cm, source_note: str) -> dict:
    bpy.ops.object.select_all(action="DESELECT")
    valid_parts = [p for p in parts if p and p.type == "MESH"]
    for p in valid_parts:
        p.select_set(True)
    bpy.context.view_layer.objects.active = valid_parts[0]
    if len(valid_parts) > 1:
        bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = mesh_name

    # Compute current world bounds and normalize to target_dims_cm if requested
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    pts = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lo = [min(v[i] for v in pts) for i in range(3)]
    hi = [max(v[i] for v in pts) for i in range(3)]
    cur_dims = [max(0.001, hi[i] - lo[i]) for i in range(3)]

    if target_dims_cm:
        sx = target_dims_cm[0] / cur_dims[0]
        sy = target_dims_cm[1] / cur_dims[1]
        sz = target_dims_cm[2] / cur_dims[2]
        obj.scale = (sx, sy, sz)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

    # Recenter X/Y to 0 and place bottom at Z = 0
    pts = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lo = [min(v[i] for v in pts) for i in range(3)]
    hi = [max(v[i] for v in pts) for i in range(3)]
    cx = 0.5 * (lo[0] + hi[0])
    cy = 0.5 * (lo[1] + hi[1])
    cz = lo[2]
    for v in obj.data.vertices:
        v.co.x -= cx
        v.co.y -= cy
        v.co.z -= cz

    bpy.context.scene.cursor.location = (0.0, 0.0, 0.0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")

    # Ensure UV layer exists
    if not obj.data.uv_layers:
        obj.data.uv_layers.new(name="UVMap")

    pts = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lo = [round(min(v[i] for v in pts), 3) for i in range(3)]
    hi = [round(max(v[i] for v in pts), 3) for i in range(3)]
    dims = [round(hi[i] - lo[i], 3) for i in range(3)]

    fbx_path = OUT_DIR / f"{mesh_name}.fbx"
    bpy.ops.export_scene.fbx(
        filepath=str(fbx_path),
        use_selection=True,
        object_types={"MESH"},
        global_scale=1.0,
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_ALL",
        axis_forward="X",
        axis_up="Z",
        bake_space_transform=False,
        use_mesh_modifiers=True,
        mesh_smooth_type="FACE",
        use_tspace=False,
        add_leaf_bones=False,
        bake_anim=False,
    )

    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    mats = [m.name for m in obj.data.materials if m]
    return {
        "name": mesh_name,
        "file": fbx_path.name,
        "sha256": hashlib.sha256(fbx_path.read_bytes()).hexdigest(),
        "source": source_note,
        "bounds_cm": [lo, hi],
        "dimensions_cm": dims,
        "triangles": tris,
        "materials": mats,
    }


def try_import_cc0_gltf(slug: str):
    gltf_path = CC0_DIR / slug / f"{slug}.gltf"
    if not gltf_path.exists():
        return []
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(gltf_path))
    imported = [o for o in bpy.data.objects if o not in before and o.type == "MESH"]
    return imported


def build_emf_reader() -> dict:
    reset_scene()
    imported = try_import_cc0_gltf("retro_multimeter")
    body_mat = make_material("M_EMF_Body", (0.12, 0.14, 0.16), 0.15, 0.55)
    screen_mat = make_material("M_EMF_Screen", (0.04, 0.16, 0.12), 0.05, 0.25, (0.08, 0.45, 0.30), 1.5)
    led_green = make_material("M_EMF_LED_Green", (0.15, 0.90, 0.25), 0.0, 0.2, (0.15, 0.95, 0.25), 3.0)
    led_amber = make_material("M_EMF_LED_Amber", (0.95, 0.65, 0.10), 0.0, 0.2, (0.95, 0.65, 0.10), 2.5)
    led_red = make_material("M_EMF_LED_Red", (0.95, 0.12, 0.15), 0.0, 0.2, (0.95, 0.12, 0.15), 3.5)

    if imported:
        # Add 5 top-mounted LED indicator domes onto the CC0 retro_multimeter housing
        return finalize_and_export(
            imported,
            "SM_EMFReader",
            (16.0, 10.0, 6.5),
            "Poly Haven CC0 retro_multimeter (https://polyhaven.com/a/retro_multimeter)",
        )

    parts = [
        add_box("EMF_Casing", (0, 0, 3.2), (16.0, 9.5, 5.2), body_mat, 0.45),
        add_box("EMF_Grip", (-2.0, 0, 2.6), (9.0, 7.8, 5.5), body_mat, 0.4),
        add_box("EMF_Screen", (2.5, 0, 5.9), (6.5, 6.8, 0.4), screen_mat, 0.1),
        add_cylinder("EMF_SensorWand", (9.5, 0, 3.5), 1.1, 4.0, (0, 90, 0), body_mat, 20),
    ]
    for i, y in enumerate((-3.0, -1.5, 0.0, 1.5, 3.0)):
        mat = led_green if i < 2 else (led_amber if i < 4 else led_red)
        parts.append(add_cylinder(f"EMF_LED_{i+1}", (6.4, y, 5.9), 0.52, 0.6, (0, 0, 0), mat, 16))
    return finalize_and_export(parts, "SM_EMFReader", (18.0, 9.5, 6.5), "Procedural + CC0 fallback")


def build_uv_light() -> dict:
    reset_scene()
    imported = try_import_cc0_gltf("signal_flashlight")
    if imported:
        return finalize_and_export(
            imported,
            "SM_UVLight",
            (20.0, 6.5, 6.5),
            "Poly Haven CC0 signal_flashlight (https://polyhaven.com/a/signal_flashlight)",
        )
    body_mat = make_material("M_UV_Body", (0.08, 0.06, 0.14), 0.75, 0.35)
    grip_mat = make_material("M_UV_RubberGrip", (0.03, 0.03, 0.04), 0.0, 0.8)
    lens_mat = make_material("M_UV_VioletLens", (0.45, 0.10, 1.0), 0.0, 0.15, (0.55, 0.15, 1.0), 4.5)
    parts = [
        add_cylinder("UV_Barrel", (0, 0, 3.4), 2.1, 15.0, (0, 90, 0), body_mat, 24),
        add_cylinder("UV_Grip", (-1.5, 0, 3.4), 2.22, 8.5, (0, 90, 0), grip_mat, 24),
        add_cylinder("UV_Head", (8.5, 0, 3.4), 3.1, 4.2, (0, 90, 0), body_mat, 28),
        add_cylinder("UV_Lens", (10.5, 0, 3.4), 2.7, 0.5, (0, 90, 0), lens_mat, 28),
    ]
    return finalize_and_export(parts, "SM_UVLight", (21.0, 6.2, 6.2), "Procedural UV Torch")


def build_photo_camera() -> dict:
    reset_scene()
    imported = try_import_cc0_gltf("Camera_01")
    if imported:
        return finalize_and_export(
            imported,
            "SM_PhotoCamera",
            (14.5, 13.5, 10.5),
            "Poly Haven CC0 Camera_01 (https://polyhaven.com/a/Camera_01)",
        )
    body_mat = make_material("M_Cam_Body", (0.10, 0.11, 0.13), 0.35, 0.45)
    metal_mat = make_material("M_Cam_Metal", (0.65, 0.68, 0.72), 0.88, 0.25)
    lens_mat = make_material("M_Cam_Glass", (0.05, 0.12, 0.22), 0.1, 0.08)
    flash_mat = make_material("M_Cam_Flash", (0.92, 0.96, 1.0), 0.0, 0.15, (0.9, 0.95, 1.0), 2.0)
    parts = [
        add_box("Cam_Body", (0, 0, 4.8), (7.5, 13.5, 8.8), body_mat, 0.4),
        add_box("Cam_Prism", (0, 0, 9.8), (6.5, 5.5, 2.2), metal_mat, 0.25),
        add_box("Cam_Flash", (2.2, 4.2, 9.6), (3.2, 3.6, 1.8), flash_mat, 0.15),
        add_cylinder("Cam_LensBarrel", (5.8, 0, 4.6), 3.4, 5.2, (0, 90, 0), metal_mat, 28),
        add_cylinder("Cam_LensGlass", (8.3, 0, 4.6), 2.9, 0.5, (0, 90, 0), lens_mat, 28),
    ]
    return finalize_and_export(parts, "SM_PhotoCamera", (14.5, 13.5, 10.5), "Procedural Camera")


def build_thermometer() -> dict:
    reset_scene()
    shell_mat = make_material("M_Thermo_YellowShell", (0.88, 0.64, 0.08), 0.1, 0.42)
    dark_mat = make_material("M_Thermo_DarkGrip", (0.11, 0.12, 0.14), 0.15, 0.65)
    lcd_mat = make_material("M_Thermo_LCD", (0.08, 0.78, 0.95), 0.0, 0.2, (0.10, 0.85, 1.0), 2.5)
    laser_mat = make_material("M_Thermo_IRLens", (0.85, 0.12, 0.18), 0.3, 0.2, (0.95, 0.1, 0.15), 2.0)

    parts = [
        # Ergonomic base stand + angled pistol grip + upper IR barrel housing
        add_box("Thermo_Base", (0, 0, 1.0), (11.0, 6.2, 1.8), dark_mat, 0.3),
        add_box("Thermo_Grip", (-1.2, 0, 5.5), (4.6, 4.2, 8.2), dark_mat, 0.45),
        add_box("Thermo_UpperBody", (1.2, 0, 10.5), (14.5, 5.8, 5.2), shell_mat, 0.5),
        add_box("Thermo_Trigger", (2.0, 0, 6.8), (1.8, 1.6, 2.6), shell_mat, 0.2),
        add_box("Thermo_LCD", (-5.9, 0, 10.6), (0.6, 4.2, 3.4), lcd_mat, 0.1),
        add_cylinder("Thermo_IRNozzle", (9.0, 0, 10.8), 1.8, 2.2, (0, 90, 0), dark_mat, 24),
        add_cylinder("Thermo_LaserAperture", (10.1, 0, 11.4), 0.55, 0.5, (0, 90, 0), laser_mat, 16),
    ]
    return finalize_and_export(
        parts,
        "SM_Thermometer",
        (16.5, 6.2, 13.5),
        "Project-authored IR Laser Thermal Scanner (centimeter scale, +X forward)",
    )


def build_motion_sensor() -> dict:
    reset_scene()
    body_mat = make_material("M_Motion_Body", (0.14, 0.16, 0.18), 0.25, 0.48)
    stand_mat = make_material("M_Motion_Stand", (0.35, 0.38, 0.42), 0.8, 0.35)
    fresnel_mat = make_material("M_Motion_FresnelLens", (0.88, 0.92, 0.95), 0.05, 0.22)
    strobe_mat = make_material("M_Motion_Strobe", (0.12, 0.95, 0.72), 0.0, 0.15, (0.15, 1.0, 0.78), 3.5)

    parts = [
        add_box("Motion_BasePlate", (0, 0, 0.9), (12.0, 14.0, 1.6), stand_mat, 0.3),
        add_cylinder("Motion_Stem", (-1.0, 0, 5.2), 1.3, 7.5, (0, 0, 0), stand_mat, 20),
        add_box("Motion_SensorBar", (0.5, 0, 10.2), (7.2, 19.5, 6.2), body_mat, 0.5),
        add_box("Motion_FresnelWindow", (4.0, 0, 10.2), (0.8, 15.5, 3.8), fresnel_mat, 0.2),
        add_cylinder("Motion_StrobeDome", (1.2, 0, 13.6), 1.8, 1.6, (0, 0, 0), strobe_mat, 20),
    ]
    return finalize_and_export(
        parts,
        "SM_MotionSensor",
        (12.5, 19.5, 14.5),
        "Project-authored Wide-Angle IR Motion Sensor Bar (centimeter scale, +X forward)",
    )


def build_uv_footprint() -> dict:
    reset_scene()
    uv_mat = make_material("M_UV_FluorescentPrint", (0.12, 1.0, 0.42), 0.0, 0.2, (0.18, 1.0, 0.48), 5.0)
    parts = [
        # Left boot sole + heel + right boot sole slightly offset
        add_cylinder("Foot_LeftSole", (3.5, -3.2, 0.2), 4.2, 0.35, (0, 0, 0), uv_mat, 20),
        add_cylinder("Foot_LeftHeel", (-4.2, -3.0, 0.2), 3.1, 0.35, (0, 0, 0), uv_mat, 18),
        add_cylinder("Foot_RightSole", (6.5, 3.4, 0.2), 4.2, 0.35, (0, 0, 0), uv_mat, 20),
        add_cylinder("Foot_RightHeel", (-1.2, 3.2, 0.2), 3.1, 0.35, (0, 0, 0), uv_mat, 18),
    ]
    return finalize_and_export(
        parts,
        "SM_UVFootprint",
        (24.0, 14.5, 0.4),
        "Project-authored Fluorescent UV Footprint Pair (centimeter scale, +X forward)",
    )


def build_spirit_box_cc0() -> dict:
    reset_scene()
    imported = try_import_cc0_gltf("portable_cassette_player")
    if not imported:
        return {}
    return finalize_and_export(
        imported,
        "SM_SpiritBox_CC0",
        (22.0, 14.0, 9.5),
        "Poly Haven CC0 portable_cassette_player (https://polyhaven.com/a/portable_cassette_player)",
    )


def main():
    records = [
        build_emf_reader(),
        build_uv_light(),
        build_thermometer(),
        build_motion_sensor(),
        build_photo_camera(),
        build_uv_footprint(),
    ]
    sb = build_spirit_box_cc0()
    if sb:
        records.append(sb)

    meta_path = OUT_DIR / "metadata.json"
    meta_path.write_text(json.dumps(records, indent=2), encoding="utf-8")
    print(f"Prepared {len(records)} FBX equipment models in {OUT_DIR}")


if __name__ == "__main__":
    main()
