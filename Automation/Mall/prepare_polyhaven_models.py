"""Prepare audited, grounded centimeter FBXs without altering the CC0 originals."""
import bpy
import hashlib
import json
from pathlib import Path
from mathutils import Vector

ROOT = Path(r"C:\unreal\uprj\MyProject2\ExternalAssets\Mall\PolyHaven")
OUT = ROOT / "ImportReady"
OUT.mkdir(exist_ok=True)
download = json.loads((ROOT / "download_manifest.json").read_text())
texture_lookup = {(item["asset_id"], item["role"].lower()): item["local_path"] for item in download["files"] if item["role"] != "source_blend"}
material_plan = {}
mesh_plan = []

def prepare_material(asset, material):
    name = material.name
    prefix = name.removeprefix(asset + "_")
    if asset == "steel_frame_shelves_01":
        prefix = ""
    if asset == "decorative_book_set_01":
        prefix = name.rsplit("_cover", 1)[0]
    roles = {role: path for (asset_id, role), path in texture_lookup.items() if asset_id == asset}
    def find(*suffixes):
        for suffix in suffixes:
            key = prefix + ("_" if prefix else "") + suffix
            if key in roles:
                return roles[key]
        return None
    maps = {"base_color": find("diff", "diffuse"), "roughness": find("rough", "roughness"), "normal": find("nor_dx", "nor_gl"), "metallic": find("metal", "metallic"), "ambient_occlusion": find("ao"), "opacity_mask": find("alpha")}
    spec = {"asset_id": asset, "material_name": name, "textures": {key: value for key, value in maps.items() if value}, "base_color_srgb": True, "data_maps_srgb": False, "normal_flip_green": bool(maps["normal"] and "_nor_gl_" in maps["normal"]), "two_sided": bool(maps["opacity_mask"]), "blend_mode": "Masked" if maps["opacity_mask"] else "Opaque", "shading_model": "TwoSidedFoliage" if maps["opacity_mask"] else "DefaultLit", "metallic_constant": 0.0}
    if asset == "decorative_book_set_01":
        spec["cover_texture"] = roles.get(name)
        spec["page_mask_texture"] = roles.get(prefix + "_mask02")
        spec["base_color_expression"] = "lerp(Cover(TexCoord1), BaseColor(TexCoord0), PageMask(TexCoord0).r)"
        spec["uv_channels"] = {"0": "standard_uv", "1": "cover"}
    material_plan[name] = spec
    material.use_nodes = True
    nodes = material.node_tree.nodes
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    surface = nodes.new("ShaderNodeBsdfPrincipled")
    material.node_tree.links.new(surface.outputs["BSDF"], output.inputs["Surface"])
    surface.inputs["Roughness"].default_value = .6
    if maps["base_color"]:
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = bpy.data.images.load(maps["base_color"], check_existing=True)
        material.node_tree.links.new(tex.outputs["Color"], surface.inputs["Base Color"])
    if maps["opacity_mask"]:
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = bpy.data.images.load(maps["opacity_mask"], check_existing=True)
        material.node_tree.links.new(tex.outputs["Color"], surface.inputs["Alpha"])

def open_asset(asset):
    bpy.ops.wm.open_mainfile(filepath=str(ROOT / asset / (asset + "_2k.blend")), load_ui=False, use_scripts=False)
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = .01
    for material in bpy.data.materials:
        if material.name.startswith(asset) or asset == "decorative_book_set_01":
            prepare_material(asset, material)
    # Remove procedural subdivision/displacement while retaining authored UVs and mesh shapes.
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH":
            for mod in list(obj.modifiers):
                if mod.type in {"NODES", "DISPLACE", "SUBSURF", "DATA_TRANSFER"}:
                    obj.modifiers.remove(mod)

def export_group(asset, objects, name, root_xy=None, z_base=None):
    copies = []
    points = [obj.matrix_world @ vertex.co for obj in objects for vertex in obj.data.vertices]
    minimum = Vector([min(p[i] for p in points) for i in range(3)])
    maximum = Vector([max(p[i] for p in points) for i in range(3)])
    pivot = Vector((root_xy[0], root_xy[1], minimum.z if z_base is None else z_base)) if root_xy is not None else Vector(((minimum.x + maximum.x)/2, (minimum.y + maximum.y)/2, minimum.z if z_base is None else z_base))
    for obj in objects:
        clone = obj.copy()
        clone.data = obj.data.copy()
        bpy.context.collection.objects.link(clone)
        clone.name = name + "_part"
        bpy.ops.object.select_all(action="DESELECT")
        clone.select_set(True)
        bpy.context.view_layer.objects.active = clone
        if "pebbles" in obj.name:
            decimate = clone.modifiers.new("Simplify soil pebbles", "DECIMATE")
            decimate.ratio = .15
        for mod in list(clone.modifiers):
            bpy.ops.object.modifier_apply(modifier=mod.name)
        world = clone.matrix_world.copy()
        for vertex in clone.data.vertices:
            vertex.co = (world @ vertex.co - pivot) * 100
        clone.parent = None
        clone.matrix_world.identity()
        copies.append(clone)
    bpy.ops.object.select_all(action="DESELECT")
    for clone in copies:
        clone.select_set(True)
    bpy.context.view_layer.objects.active = copies[0]
    bpy.ops.object.join()
    combined = bpy.context.view_layer.objects.active
    combined.name = name
    combined.data.name = name
    path = OUT / (name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"}, global_scale=1, apply_unit_scale=True, apply_scale_options="FBX_SCALE_ALL", axis_forward="-Y", axis_up="Z", bake_space_transform=False, use_mesh_modifiers=True, mesh_smooth_type="FACE", use_tspace=False, add_leaf_bones=False, bake_anim=False, path_mode="ABSOLUTE")
    combined.data.calc_loop_triangles()
    coordinates = [v.co for v in combined.data.vertices]
    lo = [min(p[i] for p in coordinates) for i in range(3)]
    hi = [max(p[i] for p in coordinates) for i in range(3)]
    entry = {"asset_id": asset, "mesh_name": name, "fbx_path": str(path), "units": "centimeters", "pivot": "ground at local Z0", "bounds_cm": [lo, hi], "dimensions_cm": [hi[i]-lo[i] for i in range(3)], "triangles": len(combined.data.loop_triangles), "material_slots": [mat.name for mat in combined.data.materials], "uv_channels": [layer.name for layer in combined.data.uv_layers], "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    mesh_plan.append(entry)
    print(name, entry["triangles"], [round(value,2) for value in entry["dimensions_cm"]], entry["material_slots"], flush=True)
    bpy.data.objects.remove(combined, do_unlink=True)

asset = "pachira_aquatica_01"
open_asset(asset)
for suffix in "abcd":
    bark = bpy.data.objects[f"{asset}_bark_{suffix}"]
    leaves = bpy.data.objects[f"{asset}_leaves_{suffix}"]
    export_group(asset, [bark, leaves], "SM_Pachira" + suffix.upper(), root_xy=(bark.location.x, bark.location.y))

asset = "potted_plant_01"
open_asset(asset)
export_group(asset, [obj for obj in bpy.context.scene.objects if obj.type == "MESH"], "SM_PottedPlant01", root_xy=(0,0))

asset = "modular_street_seating"
open_asset(asset)
bench_names = {"crossbar", "legs_double", "legs_single", "suspended_support_01", "back_support_r", "back_support_l", "arm_rest_01", "arm_rest_02", "seat", "seat_back"}
export_group(asset, [obj for obj in bpy.context.scene.objects if obj.name in bench_names], "SM_MallBench01")

asset = "steel_frame_shelves_01"
open_asset(asset)
export_group(asset, [obj for obj in bpy.context.scene.objects if obj.type == "MESH"], "SM_RetailShelves01")

asset = "decorative_book_set_01"
open_asset(asset)
export_group(asset, [bpy.data.objects["book_hardcover_01_cover02"]], "SM_BookHardcover02")
export_group(asset, [bpy.data.objects["book_softcover_01_cover04"]], "SM_BookSoftcover04")

used_materials = {name for mesh in mesh_plan for name in mesh["material_slots"]}
material_plan = {name: spec for name, spec in material_plan.items() if name in used_materials}
plan = {"source": "Poly Haven", "license": "CC0 1.0 Universal", "license_url": "https://polyhaven.com/license", "units": "centimeters", "fbx_import": {"ImportUniformScale": 1, "ConvertScene": True, "ConvertSceneUnit": True, "ForceFrontXAxis": False, "CombineMeshes": True, "ImportMaterials": False, "ImportTextures": False, "AutoGenerateCollision": False, "NormalImportMethod": "ImportNormals"}, "meshes": mesh_plan, "materials": material_plan, "notes": ["Each FBX contains a single combined rooted mesh; assign material slots using this plan, not automatic FBX materials.", "Leaf materials must be masked and two-sided. PottedPlant leaves use a GL normal; set FlipGreenChannel=true for that texture only.", "Books retain two UV channels. A clean cover uses Cover UV1 and page/base maps UV0, blended with Mask02 (white is pages).", "Plants use no blocking collision; furniture needs authored simple collision or separate blocking volumes.", "Original BLEND files are unmodified. Procedural subdivisions/displacement removed; dense potted soil pebbles decimated to15%."]}
(OUT / "import_plan.json").write_text(json.dumps(plan, indent=2), encoding="utf-8")
print("DONE", len(mesh_plan), "FBX meshes", flush=True)
