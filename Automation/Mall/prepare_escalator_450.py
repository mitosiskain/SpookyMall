"""Offline 4.5 m floor-to-floor derivative; never edits existing UE assets.

Blender invocation: blender --background --factory-startup --python <this file>
The 30 degree incline is lengthened along its own axis. The original level
landings, end plates, glass newels and rubber rail section retain their size.
"""
import bpy
import json
import math
import hashlib
from pathlib import Path
from mathutils import Vector

PROJECT = Path(r"C:\unreal\uprj\MyProject2")
SOURCE_DIR = PROJECT / "ExternalAssets/Escalator"
OUTPUT = PROJECT / "ExternalAssets/Mall/Escalator450"
OUTPUT.mkdir(parents=True, exist_ok=True)
SOURCE_BLEND = SOURCE_DIR / "MallEscalator.blend"
OLD_RUN = 623.5382907247958
OLD_RISE = 360.0
RISE = 450.0
RUN = RISE / math.tan(math.radians(30.0))
LANDING = 130.0
WIDTH = 106.0
LENGTH = RUN + 2.0 * LANDING
OLD_LENGTH = OLD_RUN + 2.0 * LANDING
DELTA = Vector((RUN - OLD_RUN, 0.0, RISE - OLD_RISE))
TANGENT = Vector((math.cos(math.radians(30)), 0, math.sin(math.radians(30))))
SOURCE_PAGE = "https://3dassets.dev/assets/retail-store-fixtures-and-mall-concourse-escalator-mod-e0d69a9c"
source_hash = hashlib.sha256(SOURCE_BLEND.read_bytes()).hexdigest()

bpy.ops.wm.open_mainfile(filepath=str(SOURCE_BLEND))
shell = bpy.data.objects["SM_MallEscalatorShell"]
step = bpy.data.objects["SM_MallEscalatorStep"]
step.hide_set(False)
step.hide_render = False
for obj in list(bpy.data.objects):
    if obj not in (shell, step):
        bpy.data.objects.remove(obj, do_unlink=True)
bpy.ops.object.select_all(action="DESELECT")
shell.select_set(True)
bpy.context.view_layer.objects.active = shell
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.separate(type="LOOSE")
bpy.ops.object.mode_set(mode="OBJECT")

def bounds(obj):
    points = [obj.matrix_world @ v.co for v in obj.data.vertices]
    return (Vector([min(p[i] for p in points) for i in range(3)]),
            Vector([max(p[i] for p in points) for i in range(3)]))

parts = []
changes = []
removed_rails = 0
for obj in list(bpy.context.scene.objects):
    if obj.type != "MESH" or obj == step:
        continue
    low, high = bounds(obj)
    dims = high - low
    material_names = {obj.data.materials[p.material_index].name for p in obj.data.polygons}
    if material_names == {"dark"} and dims.x > 800.0 and dims.y < 12.0:
        bpy.data.objects.remove(obj, do_unlink=True)
        removed_rails += 1
        continue
    if dims.x > 500.0:
        # Endpoint-preserving extension parallel to the incline, with an
        # unchanged 20 cm zone at either end. This preserves bevels and beam
        # normal section dimensions rather than scaling the shell in Z.
        projected = [v.co.dot(TANGENT) for v in obj.data.vertices]
        pmin, pmax = min(projected), max(projected)
        for v, position in zip(obj.data.vertices, projected):
            t = max(0.0, min(1.0, (position-pmin-20.0)/(pmax-pmin-40.0)))
            v.co += DELTA * t
        change = "extended straight incline span along its own axis"
    elif (low.x + high.x) * 0.5 > OLD_LENGTH * 0.5:
        for v in obj.data.vertices:
            v.co += DELTA
        change = "translated upper landing or newel without scaling"
    else:
        change = "unchanged lower landing or newel"
    obj.data.update()
    changed_low, changed_high = bounds(obj)
    changes.append({"name": obj.name, "operation": change,
        "old_bounds_cm": [list(low), list(high)],
        "new_bounds_cm": [list(changed_low), list(changed_high)]})
    parts.append(obj)
assert removed_rails == 2, removed_rails

def curve(p0, p1, p2, p3, count=10):
    return [tuple((1-t)**3*p0[k]+3*(1-t)**2*t*p1[k]+3*(1-t)*t*t*p2[k]+t**3*p3[k]
        for k in range(2)) for t in [i/count for i in range(1,count+1)]]

rail_height, radius = 111.0, 30.0
upper_center = LANDING + RUN + 60.0
profile = [(112.0, rail_height-60.0), (70.0, rail_height-60.0)]
profile += [(70+radius*math.cos(-math.pi/2-math.pi*i/24),
             rail_height-radius+radius*math.sin(-math.pi/2-math.pi*i/24)) for i in range(1,25)]
profile.append((100.0, rail_height))
profile += curve((100,rail_height),(120,rail_height),(140,rail_height+10/math.sqrt(3)),
                 (160,rail_height+30/math.sqrt(3)))
profile.append((LANDING+RUN-30,RISE+rail_height-30/math.sqrt(3)))
profile += curve((LANDING+RUN-30,RISE+rail_height-30/math.sqrt(3)),
    (LANDING+RUN-10,RISE+rail_height-10/math.sqrt(3)),
    (LANDING+RUN+10,RISE+rail_height),(LANDING+RUN+30,RISE+rail_height))
profile.append((upper_center, RISE+rail_height))
profile += [(upper_center+radius*math.cos(math.pi/2-math.pi*i/24),
    RISE+rail_height-radius+radius*math.sin(math.pi/2-math.pi*i/24)) for i in range(1,25)]
profile.append((upper_center-35,RISE+rail_height-60))

def swept_handrail(y):
    points = [Vector((x,y,z)) for x,z in profile]
    verts, faces, sides = [], [], 16
    for i, p in enumerate(points):
        tangent = (points[min(i+1,len(points)-1)]-points[max(i-1,0)]).normalized()
        vertical = Vector((-tangent.z, 0, tangent.x))
        for k in range(sides):
            a = math.tau*k/sides
            verts.append(tuple(p+Vector((0,5*math.cos(a),0))+vertical*(3.2*math.sin(a))))
    for i in range(len(points)-1):
        for k in range(sides):
            nk = (k+1)%sides
            faces.append((i*sides+k,i*sides+nk,(i+1)*sides+nk,(i+1)*sides+k))
    faces += [tuple(reversed(range(sides))), tuple(range((len(points)-1)*sides,len(points)*sides))]
    mesh = bpy.data.meshes.new("450 cm constant-section rubber rail")
    mesh.from_pydata(verts, [], faces)
    mesh.materials.append(bpy.data.materials["dark"])
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    obj = bpy.data.objects.new("Regenerated constant-section handrail", mesh)
    bpy.context.collection.objects.link(obj)
    return obj

for side in (-1,1):
    parts.append(swept_handrail(side*59.0))
bpy.ops.object.select_all(action="DESELECT")
for obj in parts:
    obj.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
shell = bpy.context.object
shell.name = "SM_MallEscalatorShell_450"
bpy.context.scene.cursor.location = (0,0,0)
bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
material_order = ["metal", "glass", "dark", "safety_yellow"]
face_materials = [shell.data.materials[p.material_index].name for p in shell.data.polygons]
shell.data.materials.clear()
for name in material_order:
    shell.data.materials.append(bpy.data.materials[name])
for p, name in zip(shell.data.polygons, face_materials):
    p.material_index = material_order.index(name)
low, high = bounds(shell)
assert abs(low.x) < 0.01
assert abs(high.x-LENGTH) < 0.01
assert abs((high.y-low.y)-136.0077667236328) < 0.01
assert abs(high.z-564.2) < 0.01
assert abs(low.z+60.177642822265625) < 0.01
assert abs(math.degrees(math.atan2(RISE, RUN))-30.0) < 0.0001

bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = 0.01
bpy.ops.object.select_all(action="DESELECT")
shell.select_set(True)
bpy.context.view_layer.objects.active = shell
bpy.ops.export_scene.fbx(filepath=str(OUTPUT / "SM_MallEscalatorShell_450.fbx"),
    use_selection=True, object_types={"MESH"}, global_scale=1.0, apply_unit_scale=True,
    apply_scale_options="FBX_SCALE_ALL", axis_forward="-Y", axis_up="Z",
    bake_space_transform=False, use_mesh_modifiers=True, mesh_smooth_type="FACE",
    use_tspace=False, add_leaf_bones=False, bake_anim=False)

metadata = {"units": "centimeters", "local_axes": {"X": "ascending travel", "Y": "cross tread", "Z": "up"},
    "origin": "lower outer landing leading edge, centered across width, walking surface Z=0",
    "lower_landing": {"start": [0,0,0], "end": [LANDING,0,0]},
    "slope": {"start": [LANDING,0,0], "end": [LANDING+RUN,0,RISE]},
    "upper_landing": {"start": [LANDING+RUN,0,RISE], "end": [LENGTH,0,RISE]},
    "run_cm": RUN, "rise_cm": RISE, "landing_length_cm": LANDING,
    "length_cm": LENGTH, "slope_degrees": 30.0, "step_width_cm": WIDTH,
    "fixed_end_plate_length_cm": 45.0, "shell_bounds_cm": [list(low),list(high)],
    "shell_material_slots": material_order,
    "handrail": {"height_above_walk_profile_cm": 111.0, "rubber_section_cm": [10.0,6.4],
        "rounded_newel_radius_cm": 30.0, "constant_section_regenerated": True},
    "existing_step_mesh": "/Game/Environment/Escalator/Meshes/SM_MallEscalatorStep",
    "recommended_runtime_properties": {"Rise": 450.0, "Run": RUN, "LandingLength": 130.0,
        "BeltWidth":106.0, "ShellMeshAsset":"SM_MallEscalatorShell_450", "ShowEndPlates":False},
    "fbx_import_settings": {"import_uniform_scale":1.0, "convert_scene":True, "convert_scene_unit":True,
        "force_front_x_axis":False, "combine_meshes":True, "auto_generate_collision":False,
        "normal_import_method":"import normals", "import_materials":False, "import_textures":False},
    "source_page": SOURCE_PAGE, "source_license":"CC0 1.0 Universal",
    "source_original_sha256":"b9bc39fa4516480abaf3d0a55c6713c5ba6896c2ab5798493756a894b7d71889",
    "source_blend_sha256": source_hash, "upper_landing_translation_cm":list(DELTA),
    "derivative_method":"extend incline parallel to its own axis; translate upper components; regenerate constant-section handrails",
    "component_changes":changes}
(OUTPUT / "mesh_metadata.json").write_text(json.dumps(metadata,indent=2),encoding="utf-8")
(OUTPUT / "LICENSE_AND_SOURCE.txt").write_text(
    "Mall escalator 450 cm rise derivative\nSource: "+SOURCE_PAGE+"\n"
    "Original download: https://cdn.3dassets.dev/assets/25360/v1/model.glb\n"
    "License: CC0 1.0 Universal; https://creativecommons.org/publicdomain/zero/1.0/\n"
    "Original model downloaded 2026-10-10; derivative prepared 2026-10-10.\n"
    "AI-authored source; static shell only. Existing custom moving tread mesh reused.\n"
    "Derived from ../../Escalator/MallEscalator.blend; existing source and Unreal assets remain unchanged.\n"
    "Adaptation: rise360->450 cm, same30-degree incline, slope run779.422863 cm.\n"
    "Lower landings/newels unchanged; upper components shift155.884573 cm X and90 cm Z.\n"
    "Inclined steel/glass spans extended along incline axis; rubber handrails regenerated with\n"
    "original10x6.4 cm section and30 cm rounded newels. Fixed endplates remain45 cm.\n"
    "Preparation: ../../../Automation/Mall/prepare_escalator_450.py\n",encoding="utf-8")

# Keep the original step as a hidden source in the editable Blender derivative.
step.hide_render = True
step.hide_set(True)
pitch = 39.2
count = round((RUN/math.cos(math.radians(30)) + 2*(LANDING-45))/pitch)
travel = RUN/math.cos(math.radians(30)) + 2*(LANDING-45)
pitch = travel/count
for i in range(count+1):
    distance = min(travel,i*pitch)
    if distance < LANDING-45:
        x,z = 45+distance,0
    elif distance < LANDING-45+RUN/math.cos(math.radians(30)):
        s = distance-(LANDING-45)
        x,z = LANDING+s*math.cos(math.radians(30)),s*math.sin(math.radians(30))
    else:
        x,z = LANDING+RUN+distance-(LANDING-45+RUN/math.cos(math.radians(30))),RISE
    obj = bpy.data.objects.new("Preview tread %02d"%i,step.data)
    bpy.context.collection.objects.link(obj)
    obj.location = (x,0,z)
    obj.scale.x = (pitch-0.15)/40.0
    obj.hide_render=False

def box(name,center,dims,mat):
    bpy.ops.mesh.primitive_cube_add(size=1,location=center)
    obj=bpy.context.object
    obj.name=name
    obj.dimensions=dims
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    obj.data.materials.append(mat)
    return obj

floor_mat=bpy.data.materials.get("Preview mall floor")
box("Preview ground",(520,0,-32),(2100,1800,20),floor_mat)
box("Preview upper level",(LENGTH+90,0,RISE-16),(180,500,32),floor_mat)
target = Vector((520,0,245))
for name,position,power,size in [("Key",(260,-440,1060),22000000,750),
        ("Fill",(900,500,800),17000000,700),("Rim",(80,250,850),9000000,450)]:
    bpy.ops.object.light_add(type="AREA",location=position)
    obj=bpy.context.object
    obj.name=name
    obj.data.energy=power
    obj.data.shape="DISK"
    obj.data.size=size
    obj.rotation_euler=(target-obj.location).to_track_quat("-Z","Y").to_euler()
bpy.ops.object.camera_add(location=(-720,-1180,940))
camera=bpy.context.object
camera.rotation_euler=(target-camera.location).to_track_quat("-Z","Y").to_euler()
camera.data.type="ORTHO"
camera.data.ortho_scale=1390
camera.data.clip_start=1.0
camera.data.clip_end=10000.0
scene=bpy.context.scene
scene.camera=camera
scene.render.engine="CYCLES"
scene.cycles.samples=20
scene.cycles.use_denoising=True
scene.render.resolution_x=1280
scene.render.resolution_y=800
scene.render.resolution_percentage=100
scene.render.image_settings.file_format="PNG"
scene.render.filepath=str(OUTPUT/"escalator_450_preview.png")
scene.view_settings.view_transform="AgX"
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT/"MallEscalator450.blend"))
bpy.ops.render.render(write_still=True)
assert hashlib.sha256(SOURCE_BLEND.read_bytes()).hexdigest()==source_hash
print("ESCALATOR_450_READY",json.dumps({k:v for k,v in metadata.items() if k!="component_changes"}))
