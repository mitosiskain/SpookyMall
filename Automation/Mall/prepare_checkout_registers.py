"""Prepare two already-downloaded CC0 checkout props. No Unreal mutations.

The original 28-model intake and original GLBs are read-only. Outputs live in
the separate ImportReady/Checkout directory and use the established FBX units.
"""
import bpy
import hashlib
import json
import math
import struct
from datetime import date
from pathlib import Path
from mathutils import Matrix, Vector

PROJECT = Path(r"C:\unreal\uprj\MyProject2")
ROOT = PROJECT / "ExternalAssets/Mall/RetailKit"
OUT = ROOT / "ImportReady/Checkout"
OUT.mkdir(parents=True, exist_ok=True)
SELECTED = {"25324": "POSTerminal", "25326": "CardTerminalStand"}
catalog = {str(a["id"]): a for a in json.loads((ROOT / "pack_manifest.json").read_text(encoding="utf-8-sig"))["data"]["assets"]}
downloads = {str(a["id"]): a for a in json.loads((ROOT / "downloads.json").read_text(encoding="utf-8-sig"))}
protected = list((ROOT / "ImportReady").glob("*.fbx")) + [ROOT / "GLB" / (i + ".glb") for i in SELECTED]
protected_hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}

def reset():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for mesh in list(bpy.data.meshes):
        if mesh.users == 0:
            bpy.data.meshes.remove(mesh)
    for mat in list(bpy.data.materials):
        if mat.users == 0:
            bpy.data.materials.remove(mat)
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 1.0

def read_glb(path):
    raw = path.read_bytes()
    magic, version, total = struct.unpack_from("<4sII", raw, 0)
    assert magic == b"glTF" and version == 2 and total == len(raw)
    size, kind = struct.unpack_from("<I4s", raw, 12)
    assert kind == b"JSON"
    return json.loads(raw[20:20+size].decode("utf-8"))

def bounds(points):
    return Vector([min(p[i] for p in points) for i in range(3)]), Vector([max(p[i] for p in points) for i in range(3)])

assets = []
for source_id, suffix in SELECTED.items():
    reset()
    source = ROOT / "GLB" / (source_id + ".glb")
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    assert digest == downloads[source_id]["sha256"], (source_id, "source hash mismatch")
    gltf = read_glb(source)
    assert not gltf.get("textures"), (source_id, "unexpected texture references")
    bpy.ops.import_scene.gltf(filepath=str(source))
    bpy.context.scene.frame_set(0)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH" and o.data.vertices]
    matrices = {o.name: o.matrix_world.copy() for o in meshes}
    low, high = bounds([(matrices[o.name] @ v.co) * 100.0 for o in meshes for v in o.data.vertices])
    pivot = Vector(((low.x+high.x)/2, (low.y+high.y)/2, low.z))
    for obj in meshes:
        matrix = matrices[obj.name]
        obj.animation_data_clear()
        for vertex in obj.data.vertices:
            vertex.co = (matrix @ vertex.co) * 100.0 - pivot
        obj.parent = None
        obj.matrix_world = Matrix.Identity(4)
    for obj in list(bpy.context.scene.objects):
        if obj not in meshes:
            bpy.data.objects.remove(obj, do_unlink=True)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in meshes:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    name = "SM_Retail_" + source_id + "_" + suffix
    obj.name = name
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    low, high = bounds([obj.matrix_world @ v.co for v in obj.data.vertices])
    asset = catalog[source_id]
    source_dims = asset["stats"]["sizeMeters"]
    expected = Vector((source_dims[0]*100, source_dims[2]*100, source_dims[1]*100))
    assert max(abs(d) for d in ((high-low)-expected)) < 1.0
    slots = [m.name for m in obj.data.materials]
    materials = []
    for mat in gltf.get("materials", []):
        pbr = mat.get("pbrMetallicRoughness", {})
        materials.append({"name": mat.get("name", "Material"),
            "base_color_linear_rgba": pbr.get("baseColorFactor", [1,1,1,1]),
            "metallic": pbr.get("metallicFactor", 1), "roughness": pbr.get("roughnessFactor", 1),
            "alpha_mode": mat.get("alphaMode", "OPAQUE"), "two_sided": mat.get("doubleSided", False),
            "has_textures": False})
    row = {"source_id": source_id, "title": asset["title"], "ue_name": name,
        "ue_folder": "/Game/Environment/Mall/Models/Retail", "fbx_path": str(OUT/(name+".fbx")),
        "source_path": str(source), "source_url": asset["url"], "download_url": asset["cdnUrl"],
        "license": asset["license"], "source_sha256": digest, "dimensions_cm": list(high-low),
        "bounds_cm": [list(low),list(high)], "pivot": "horizontal bounding-box center at lowest mesh point",
        "source_origin_to_baked_cm": list(-pivot),
        "local_axes": {"X": "width", "Y": "depth; Blender customer side toward -Y", "Z": "up"},
        "unreal_local_axes": {"X": "width", "Y": "depth; Unreal customer side toward +Y after native FBX reflection", "Z": "up"},
        "native_orientation_evidence": "Saved/MallBuild/import_orientation_notes.json, same established FBX settings",
        "original_material_slots": slots, "materials": materials, "uv_channels": [uv.name for uv in obj.data.uv_layers],
        "triangles": sum(len(p.vertices)-2 for p in obj.data.polygons), "vertices": len(obj.data.vertices),
        "source_animation_clips": [a.get("name", "Unnamed") for a in gltf.get("animations", [])],
        "source_description": asset["description"], "collision_supplied": False,
        "note": "Static display prop; separate material slots retained. No checkout gameplay or animation added."}
    bpy.context.scene.unit_settings.scale_length = 0.01
    bpy.ops.export_scene.fbx(filepath=row["fbx_path"], use_selection=True, object_types={"MESH"},
        global_scale=1.0, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL", axis_forward="-Y", axis_up="Z",
        bake_space_transform=False, use_mesh_modifiers=True, mesh_smooth_type="FACE", use_tspace=False,
        add_leaf_bones=False, bake_anim=False)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(name+".blend")))
    row["prepared_fbx_sha256"] = hashlib.sha256(Path(row["fbx_path"]).read_bytes()).hexdigest()
    assets.append(row)

checks = []
for row in assets:
    reset()
    bpy.ops.import_scene.fbx(filepath=row["fbx_path"])
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    assert len(meshes) == 1, row["source_id"]
    obj = meshes[0]
    low, high = bounds([(obj.matrix_world @ v.co)*100.0 for v in obj.data.vertices])
    expected_low, expected_high = map(Vector,row["bounds_cm"])
    bound_error = max(abs(low[i]-expected_low[i]) for i in range(3))
    bound_error = max(bound_error, max(abs(high[i]-expected_high[i]) for i in range(3)))
    assert bound_error < 0.02, (row["source_id"],bound_error)
    assert abs(low.z) < 0.01
    slots = [m.name for m in obj.data.materials]
    assert slots == row["original_material_slots"], (row["source_id"],slots)
    assert len(obj.data.uv_layers) == len(row["uv_channels"])
    checks.append({"source_id":row["source_id"],"full_bounds_error_cm":bound_error,"floor_pivot_passed":True,
        "dimensions_and_orientation_passed":True,"material_slots_passed":True,"uv_channel_count_passed":True,
        "roundtrip_bounds_cm":[list(low),list(high)]})

assert all(hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest for path,digest in protected_hashes.items())
plan = {"asset_count":len(assets),"prepared_date":date.today().isoformat(),"source_pack":"https://3dassets.dev/packs/retail-store-fixtures-and-mall",
    "license":"CC0 1.0 Universal","license_url":"https://creativecommons.org/publicdomain/zero/1.0/","units":"centimeters",
    "fbx_import_settings":{"import_uniform_scale":1.0,"convert_scene":True,"convert_scene_unit":True,
        "force_front_x_axis":False,"combine_meshes":True,"normal_import_method":"import normals",
        "auto_generate_collision":False,"import_materials":False,"import_textures":False},
    "existing_material_binding_prefix":"/Game/Generated_Materials/M_MallRetail_",
    "collision_note":"No source collision. For static decoration use small simple collision or complex-as-simple; validate placement before gameplay.",
    "assets":assets,"protected_inputs_hash_check_passed":True,"status":"prepared_roundtrip_verified_not_imported"}
(OUT/"import_plan.json").write_text(json.dumps(plan,indent=2),encoding="utf-8")
(OUT/"fbx_verification.json").write_text(json.dumps({"all_passed":True,"checked_assets":2,"checks":checks},indent=2),encoding="utf-8")
(OUT/"LICENSE_AND_SOURCE.txt").write_text(
    "Checkout display props, already-downloaded Retail Store Fixtures and Mall sources\n"
    "Pack: https://3dassets.dev/packs/retail-store-fixtures-and-mall\n"
    "Both models: CC0 1.0 Universal; https://creativecommons.org/publicdomain/zero/1.0/\n"
    "POS: " + assets[0]["source_url"] + "\nCard terminal: " + assets[1]["source_url"] + "\n"
    "Original GLB/source URLs and hashes retained in import_plan.json and ../../downloads.json.\n"
    "AI-authored mesh sources. Prepared ground-centered centimeter static meshes; source sections/materials retained.\n"
    "No UV channels existed in these textureless procedural sources; no texture or UV authoring was added.\n"
    "No collision, animation, checkout functionality, download or Unreal import was performed.\n"
    "Existing prepared28 FBXs and both original GLBs passed unchanged-hash checks.\n",encoding="utf-8")
print("CHECKOUT_PREPARATION_VERIFIED",json.dumps({"assets":assets,"checks":checks,"existing_sources_unchanged":True}))
