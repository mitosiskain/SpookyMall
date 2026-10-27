"""Audit and normalize the user-owned balcony GLB without touching its source."""
import bpy
import bmesh
import hashlib
import json
import struct
from pathlib import Path
from mathutils import Vector

SOURCE = Path(r"C:\unreal\uprj\MyProject2\ExternalAssets\Mall\UserImports\railing.glb")
OUT = SOURCE.parent / "Prepared"
OUT.mkdir(parents=True, exist_ok=True)
TEXTURES = OUT / "Textures"
TEXTURES.mkdir(exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(SOURCE))
objects = [o for o in bpy.context.scene.objects if o.type == "MESH"]
assert len(objects) == 1
obj = objects[0]
world = obj.matrix_world.copy()
source_points = [world @ v.co for v in obj.data.vertices]

def bounds(points):
    return [[min(p[i] for p in points) for i in range(3)],
            [max(p[i] for p in points) for i in range(3)]]

# Blender imported GLTF Y-up as Z-up; UE import reports original X,Z,Y axes.
source_ue = [Vector((p.x * 100, -p.y * 100, p.z * 100)) for p in source_points]
source_materials = []
for index, material in enumerate(obj.data.materials):
    used = {v for poly in obj.data.polygons if poly.material_index == index for v in poly.vertices}
    source_materials.append({"slot": index, "name": material.name,
        "bounds_original_ue_cm": bounds([source_ue[i] for i in used]),
        "faces": sum(p.material_index == index for p in obj.data.polygons)})

stone = next(m for m in source_materials if m["name"] == "Stone")
floor_z = stone["bounds_original_ue_cm"][1][2]
original_bounds = bounds(source_ue)
origin_ue = Vector(((stone["bounds_original_ue_cm"][0][0] + stone["bounds_original_ue_cm"][1][0]) / 2,
                    (stone["bounds_original_ue_cm"][0][1] + stone["bounds_original_ue_cm"][1][1]) / 2,
                    floor_z))

# Preserve the rail height; normalize the original slab to 400 x 200 x 50 cm.
source_length = stone["bounds_original_ue_cm"][1][1] - stone["bounds_original_ue_cm"][0][1]
source_depth = stone["bounds_original_ue_cm"][1][0] - stone["bounds_original_ue_cm"][0][0]
source_thickness = floor_z - stone["bounds_original_ue_cm"][0][2]
for vertex, p in zip(obj.data.vertices, source_ue):
    q = p - origin_ue
    # New +X runs along the original 4m length; +Y is original +X (depth).
    vertex.co = Vector((-q.y * 400 / source_length, q.x * 200 / source_depth,
                        q.z if q.z >= 0 else q.z * 50 / source_thickness))
obj.matrix_world.identity()
obj.name = "SM_MallBalconyModule_400"
obj.data.name = obj.name
original_material_slots = list(obj.data.materials)
bm = bmesh.new()
bm.from_mesh(obj.data)
# Axis normalization reflects Blender XY; reverse winding for outward culling.
bmesh.ops.reverse_faces(bm, faces=list(bm.faces))
corrected_mesh = bpy.data.meshes.new(obj.name + "_Corrected")
bm.to_mesh(corrected_mesh)
bm.free()
for material in original_material_slots:
    corrected_mesh.materials.append(material)
obj.data = corrected_mesh
obj.data.update()
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = .01

texture_refs = []
# Extract encoded embedded images byte-for-byte, avoiding any resampling.
source_bytes = SOURCE.read_bytes()
json_size, _ = struct.unpack_from("<II", source_bytes, 12)
gltf = json.loads(source_bytes[20:20+json_size])
binary_chunk_header = 20 + json_size
binary_size, binary_kind = struct.unpack_from("<II", source_bytes, binary_chunk_header)
assert binary_kind == 0x004E4942
binary_start = binary_chunk_header + 8
image_files = {}
for image_info in gltf.get("images", []):
    name = image_info["name"]
    extension = ".jpg" if image_info["mimeType"] == "image/jpeg" else ".png"
    filename = "".join(c if c.isalnum() or c in "_-" else "_" for c in name) + extension
    target = TEXTURES / filename
    view = gltf["bufferViews"][image_info["bufferView"]]
    start = binary_start + view.get("byteOffset", 0)
    target.write_bytes(source_bytes[start:start+view["byteLength"]])
    image_files[name] = target
for image in bpy.data.images:
    if image.name not in image_files:
        continue
    image.filepath_raw = str(image_files[image.name])
    texture_refs.append({"image_name": image.name, "path": image.filepath_raw,
                         "colorspace": image.colorspace_settings.name})
assert len(texture_refs) == 5

obj.select_set(True)
bpy.context.view_layer.objects.active = obj
fbx_path = OUT / (obj.name + ".fbx")
bpy.ops.export_scene.fbx(filepath=str(fbx_path), use_selection=True,
    object_types={"MESH"}, global_scale=1, apply_unit_scale=True,
    apply_scale_options="FBX_SCALE_ALL", axis_forward="-Y", axis_up="Z",
    bake_space_transform=False, use_mesh_modifiers=True, mesh_smooth_type="FACE",
    use_tspace=False, add_leaf_bones=False, bake_anim=False, path_mode="ABSOLUTE")
obj.data.calc_loop_triangles()
variant_materials = []
for index, material in enumerate(obj.data.materials):
    used = {v for poly in obj.data.polygons if poly.material_index == index for v in poly.vertices}
    variant_materials.append({"slot": index, "name": material.name,
        "bounds_cm": bounds([obj.data.vertices[i].co for i in used])})

audit = {"source": str(SOURCE), "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
    "original_bounds_ue_cm": original_bounds, "original_materials": source_materials,
    "original_floor_surface_z_cm": floor_z, "original_floor_center_ue_cm": list(origin_ue),
    "original_slab_thickness_cm": source_thickness,
    "original_rail_height_above_floor_cm": original_bounds[1][2] - floor_z,
    "variant_fbx": str(fbx_path), "variant_fbx_sha256": hashlib.sha256(fbx_path.read_bytes()).hexdigest(),
    "variant_units": "centimeters", "variant_pivot": "slab center XY, walking surface Z0",
    "variant_bounds_cm": bounds([v.co for v in obj.data.vertices]),
    "variant_floor_bounds_cm": next(m["bounds_cm"] for m in variant_materials if m["name"] == "Stone"),
    "variant_materials": variant_materials, "variant_triangles": len(obj.data.loop_triangles),
    "textures": texture_refs, "original_source_unchanged": True,
    "fbx_import": {"ImportUniformScale": 1, "ConvertScene": True, "ConvertSceneUnit": True,
        "ForceFrontXAxis": False, "CombineMeshes": True, "ImportMaterials": False,
        "ImportTextures": False, "AutoGenerateCollision": False},
    "source_url": "https://www.fab.com/listings/b82906d1-1b98-4266-83da-c89a389bb62a",
    "license_note": "User-reported acquired Fab Standard Personal tier; acquisition receipt governs. No standalone redistribution.",
    "rail_orientation": "L shape: long edge +Y, return +X; open edges -Y and -X.",
    "notes": ["Reuse already recovered UE materials by matching the three original material slots.",
        "FBX exported texture references are absolute paths to extracted original embedded images.",
        "The original image normal map is OpenGL; preserve its recovered UE normal-map handling.",
        "Only the slab vertical depth is changed; rail height above the walking surface is preserved.",
        "Face winding corrected for XY reflection; outward normals recomputed."]}
(OUT / "balcony_module_audit.json").write_text(json.dumps(audit, indent=2), encoding="utf-8")
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "MallBalconyModule_400.blend"))

# Cut only railing below local Y80, preserving the main posts at Y~90.
# All short return glass is below Y65.77, so it is removed completely.
straight = obj.copy()
straight.data = obj.data.copy()
straight.name = "SM_MallBalconyStraight_400"
straight.data.name = straight.name
bpy.context.collection.objects.link(straight)
bm = bmesh.new()
bm.from_mesh(straight.data)
nonstone = [f for f in bm.faces if f.material_index != 1]
rail_vertices = list({v for f in nonstone for v in f.verts})
bmesh.ops.remove_doubles(bm, verts=rail_vertices, dist=.0001)
nonstone = [f for f in bm.faces if f.material_index != 1]
rail_edges = list({e for f in nonstone for e in f.edges})
rail_vertices = list({v for f in nonstone for v in f.verts})
cut = bmesh.ops.bisect_plane(bm, geom=nonstone + rail_edges + rail_vertices,
    dist=.0001, plane_co=(0,80,0), plane_no=(0,1,0), clear_inner=True,
    clear_outer=False)
cut_edges = [e for e in cut["geom_cut"] if isinstance(e,bmesh.types.BMEdge) and e.is_boundary]
if cut_edges:
    caps = bmesh.ops.holes_fill(bm, edges=cut_edges, sides=0)
    for face in caps.get("faces", []):
        face.material_index = 2
        face.smooth = False
        face.normal_update()
        if face.normal.y > 0:
            face.normal_flip()
bm.to_mesh(straight.data)
bm.free()
straight.data.update()
if hasattr(straight.data,"set_sharp_from_angle"):
    straight.data.set_sharp_from_angle(angle=.5235987756)
bpy.ops.object.select_all(action="DESELECT")
straight.select_set(True)
bpy.context.view_layer.objects.active = straight
straight_path = OUT / (straight.name + ".fbx")
bpy.ops.export_scene.fbx(filepath=str(straight_path), use_selection=True,
    object_types={"MESH"}, global_scale=1, apply_unit_scale=True,
    apply_scale_options="FBX_SCALE_ALL", axis_forward="-Y", axis_up="Z",
    bake_space_transform=False, use_mesh_modifiers=True, mesh_smooth_type="FACE",
    use_tspace=False, add_leaf_bones=False, bake_anim=False, path_mode="ABSOLUTE")
straight.data.calc_loop_triangles()
straight_audit = dict(audit)
straight_audit.update({"variant_fbx":str(straight_path),
    "variant_fbx_sha256":hashlib.sha256(straight_path.read_bytes()).hexdigest(),
    "variant_bounds_cm":bounds([v.co for v in straight.data.vertices]),
    "variant_triangles":len(straight.data.loop_triangles),
    "rail_orientation":"Straight guard along +Y, ends open along +/-X; steel connectors project 10cm toward deck from the main guard.",
    "variant_materials":[]})
for index, material in enumerate(straight.data.materials):
    used = {v for poly in straight.data.polygons if poly.material_index==index for v in poly.vertices}
    straight_audit["variant_materials"].append({"slot":index,"name":material.name,
        "bounds_cm":bounds([straight.data.vertices[i].co for i in used])})
stone_vertices = {v for poly in straight.data.polygons if poly.material_index==1 for v in poly.vertices}
assert bounds([straight.data.vertices[i].co for i in stone_vertices]) == audit["variant_floor_bounds_cm"]
glass_vertices = {v for poly in straight.data.polygons if poly.material_index==0 for v in poly.vertices}
assert min(straight.data.vertices[i].co.y for i in glass_vertices) > 89
straight_audit["variant_floor_bounds_cm"] = audit["variant_floor_bounds_cm"]
(OUT / "balcony_straight_audit.json").write_text(json.dumps(straight_audit,indent=2),encoding="utf-8")
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "MallBalconyStraight_400.blend"))

# An orthographic render makes the material-separated shape easy to review.
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = 16
scene.render.resolution_x = 1000
scene.render.resolution_y = 750
scene.render.resolution_percentage = 100
if scene.world is None:
    scene.world = bpy.data.worlds.new("BalconyAuditWorld")
scene.world.color = (.3, .3, .3)
camera_data = bpy.data.cameras.new("BalconyAuditCamera")
camera = bpy.data.objects.new("BalconyAuditCamera", camera_data)
bpy.context.collection.objects.link(camera)
camera.location = (520, -500, 380)
camera.rotation_euler = (Vector((0,0,35)) - camera.location).to_track_quat("-Z", "Y").to_euler()
camera_data.type = "ORTHO"
camera_data.ortho_scale = 650
camera_data.clip_end = 5000
scene.camera = camera
for name, location, energy, size in [("AuditKey", (50,-200,650), 1800000, 450),
                                     ("AuditFill",(-400,300,350), 1400000,400)]:
    data = bpy.data.lights.new(name, "AREA")
    light = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(light)
    light.location = location
    light.rotation_euler = (-light.location).to_track_quat("-Z", "Y").to_euler()
    data.energy = energy
    data.shape = "DISK"
    data.size = size
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = str(OUT / "balcony_geometry_preview.png")
straight.hide_render = True
bpy.ops.render.render(write_still=True)
straight.hide_render = False
obj.hide_render = True
scene.render.filepath = str(OUT / "balcony_straight_preview.png")
bpy.ops.render.render(write_still=True)
print(json.dumps(audit, indent=2), flush=True)
print(json.dumps(straight_audit,indent=2),flush=True)
