"""Convert the CC0 escalator into reusable Unreal shell and tread assets."""
import bpy
import json
import sys
import math
import hashlib
from pathlib import Path
from mathutils import Vector

OUTPUT = Path(r"C:\unreal\uprj\MyProject2\ExternalAssets\Escalator")
SOURCE = OUTPUT / "concourse_escalator.glb"
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(SOURCE))

# Source elevations reveal the ascending direction is Blender -Y.
# Bake centimeters and shift the lower leading landing edge/walking plane to 0.
LANDING = 130.0
RUN = 623.5382907247958
RISE = 360.0
WIDTH = 106.0
LENGTH = RUN + 2.0 * LANDING
SOURCE_HALF_LENGTH = 441.7691230773926
SOURCE_LOWER_WALK_Z = 22.025
for obj in list(bpy.context.scene.objects):
    if obj.type == "MESH":
        matrix = obj.matrix_world.copy()
        for vertex in obj.data.vertices:
            p = matrix @ vertex.co
            vertex.co = (-p.y * 100.0 + SOURCE_HALF_LENGTH, p.x * 100.0, p.z * 100.0 - SOURCE_LOWER_WALK_Z)
        obj.parent = None
        obj.matrix_world.identity()

components = []
for obj in list(bpy.context.scene.objects):
    if obj.type != "MESH":
        continue
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    # Weld the GLB's per-face vertices before connectivity separation.
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.remove_doubles(threshold=0.01)
    bpy.ops.mesh.separate(type="LOOSE")
    bpy.ops.object.mode_set(mode="OBJECT")

for obj in list(bpy.context.scene.objects):
    if obj.type != "MESH":
        continue
    points = [obj.matrix_world @ v.co for v in obj.data.vertices]
    bounds = [[min(p[i] for p in points) for i in range(3)], [max(p[i] for p in points) for i in range(3)]]
    components.append({"name": obj.name, "material": [m.name for m in obj.data.materials], "vertices": len(points), "faces": len(obj.data.polygons), "bounds": bounds, "dimensions": [bounds[1][i]-bounds[0][i] for i in range(3)]})

(OUTPUT / "mesh_analysis.json").write_text(json.dumps(components, indent=2), encoding="utf-8")

def bounds(obj):
    points = [obj.matrix_world @ v.co for v in obj.data.vertices]
    return Vector([min(p[i] for p in points) for i in range(3)]), Vector([max(p[i] for p in points) for i in range(3)])

# All source moving components are separate thin plates, cleats or riser faces.
removed = []
shell_parts = []
for obj in list(bpy.context.scene.objects):
    if obj.type != "MESH":
        continue
    low, high = bounds(obj)
    dim = high - low
    mat = obj.data.materials[0].name
    tread = mat == "metal" and 37.0 < dim.x < 41.0 and dim.y > 100.0 and dim.z < 4.0
    cleat = mat == "dark" and 34.0 < dim.x < 36.0 and dim.y < 1.1 and dim.z < 1.1
    riser = mat == "dark" and dim.x < 2.5 and dim.y > 100.0 and 22.0 < dim.z < 24.0
    landing_insert = mat == "dark" and 27.0 < dim.x < 29.0 and dim.y > 100.0 and dim.z < 2.0
    original_rail = mat == "dark" and dim.x > 500.0 and dim.y < 15.0
    original_newel = mat == "dark" and 15.0 < dim.x < 20.0 and 11.0 < dim.y < 13.0 and 14.0 < dim.z < 17.0
    if tread or cleat or riser or landing_insert or original_rail or original_newel:
        removed.append(obj.name)
        bpy.data.objects.remove(obj, do_unlink=True)
    else:
        # The source AI mesh's long boxes had the incline rotation sign reversed.
        # Reflect these around the center only; landings and turned newels were
        # correctly positioned already. Reverse face winding after reflection.
        if dim.x > 400.0:
            for vertex in obj.data.vertices:
                vertex.co.x = LENGTH - vertex.co.x
                # Source wide truss roof protruded 17cm above the tread path.
                # Move its cladding under the cast steps, clear of their risers.
                if mat == "dark" and dim.y > 120.0:
                    vertex.co.z -= 42.0
            obj.data.flip_normals()
        # Leave only 45cm fixed plates. The remaining flat approach/departure
        # area belongs to the runtime's moving tread loop.
        if mat == "metal" and 128.0 < dim.x < 132.0 and dim.y > 130.0:
            lower = (low.x + high.x) * 0.5 < LENGTH * 0.5
            for vertex in obj.data.vertices:
                t = (vertex.co.x - low.x) / dim.x
                vertex.co.x = (0.0 if lower else LENGTH - 45.0) + t * 45.0
        if mat == "metal" and 4.0 < dim.x < 5.0 and dim.y < 2.0 and dim.z < 1.5:
            shift = -85.0 if (low.x + high.x)*0.5 < LENGTH*0.5 else 85.0
            for vertex in obj.data.vertices:
                vertex.co.x += shift
        shell_parts.append(obj)

def material(name, color, metallic, roughness):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    mat.diffuse_color = (*color, 1.0)
    return mat

steel = material("metal", (0.56, 0.59, 0.64), 0.85, 0.28)
rubber = material("dark", (0.027, 0.032, 0.038), 0.02, 0.61)
glass = material("glass", (0.40, 0.62, 0.65), 0.0, 0.11)
glass_bsdf = glass.node_tree.nodes.get("Principled BSDF")
glass_bsdf.inputs["Transmission Weight"].default_value = 0.85
glass_bsdf.inputs["IOR"].default_value = 1.48
glass_bsdf.inputs["Alpha"].default_value = 0.32
glass.diffuse_color = (0.40, 0.62, 0.65, 0.32)
yellow = material("safety_yellow", (0.95, 0.56, 0.025), 0.20, 0.39)
tread_dark = material("tread_dark", (0.12, 0.14, 0.17), 0.62, 0.45)
tread_metal = material("tread_metal", (0.46, 0.49, 0.54), 0.88, 0.34)

def box(name, center, dims, mat, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dims
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    if bevel:
        mod = obj.modifiers.new("Manufactured edge radius", "BEVEL")
        mod.width = bevel
        mod.segments = 2
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj

def join(parts, name):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in parts:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = name
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    return obj

def curve(p0, p1, p2, p3, count=10):
    points = []
    for i in range(1, count+1):
        t = i / count
        points.append(tuple((1-t)**3*p0[k] + 3*(1-t)**2*t*p1[k] + 3*(1-t)*t*t*p2[k] + t**3*p3[k] for k in range(2)))
    return points

def newel_glass(name, outline, y):
    verts = [(x,y-0.7,z) for x,z in outline] + [(x,y+0.7,z) for x,z in outline]
    n = len(outline)
    faces = [tuple(reversed(range(n))), tuple(range(n,2*n))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], faces)
    mesh.materials.append(glass)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj

def swept_handrail(name, profile, y):
    points = [Vector((x,y,z)) for x,z in profile]
    verts, faces = [], []
    sides = 16
    for i,p in enumerate(points):
        tangent = points[min(i+1,len(points)-1)] - points[max(i-1,0)]
        tangent.normalize()
        vertical = Vector((-tangent.z, 0, tangent.x))
        for k in range(sides):
            a = math.tau*k/sides
            offset = Vector((0,5.0*math.cos(a),0)) + vertical*(3.2*math.sin(a))
            verts.append(tuple(p+offset))
    for i in range(len(points)-1):
        for k in range(sides):
            nk = (k+1)%sides
            faces.append((i*sides+k,i*sides+nk,(i+1)*sides+nk,(i+1)*sides+k))
    faces += [tuple(reversed(range(sides))),tuple(range((len(points)-1)*sides,len(points)*sides))]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts,[],faces)
    mesh.materials.append(rubber)
    for p in mesh.polygons:
        p.use_smooth = True
    obj = bpy.data.objects.new(name,mesh)
    bpy.context.collection.objects.link(obj)
    return obj

# A real mall silhouette: level newel approach, soft incline transition,
# flattened exit and rounded rubber-belt returns. The short return tails sit
# inside the clear newel housings; runtime geometry and walk profile stay intact.
rail_height = 111.0
newel_radius = 30.0
upper_center = LANDING + RUN + 60.0
rail_profile = [(112.0, rail_height-60.0), (70.0, rail_height-60.0)]
rail_profile += [(70+newel_radius*math.cos(-math.pi/2-math.pi*i/24), rail_height-newel_radius+newel_radius*math.sin(-math.pi/2-math.pi*i/24)) for i in range(1,25)]
rail_profile.append((100.0,rail_height))
rail_profile += curve((100,rail_height),(120,rail_height),(140,rail_height+10/math.sqrt(3)),(160,rail_height+30/math.sqrt(3)))
rail_profile.append((LANDING+RUN-30,RISE+rail_height-30/math.sqrt(3)))
rail_profile += curve((LANDING+RUN-30,RISE+rail_height-30/math.sqrt(3)),(LANDING+RUN-10,RISE+rail_height-10/math.sqrt(3)),(LANDING+RUN+10,RISE+rail_height),(LANDING+RUN+30,RISE+rail_height))
rail_profile.append((upper_center,RISE+rail_height))
rail_profile += [(upper_center+newel_radius*math.cos(math.pi/2-math.pi*i/24),RISE+rail_height-newel_radius+newel_radius*math.sin(math.pi/2-math.pi*i/24)) for i in range(1,25)]
rail_profile.append((upper_center-35,RISE+rail_height-60))

lower_outline = [(122.3,105.0),(70,107.5)]
lower_outline += [(70+26.5*math.cos(math.pi/2+math.pi*i/24),81+26.5*math.sin(math.pi/2+math.pi*i/24)) for i in range(1,25)]
lower_outline += [(112,8),(164.3,32.3)]
upper_outline = [(719.3,449.0),(upper_center,467.5)]
upper_outline += [(upper_center+26.5*math.cos(math.pi/2-math.pi*i/24),441+26.5*math.sin(math.pi/2-math.pi*i/24)) for i in range(1,25)]
upper_outline += [(790,360),(761.3,376.9)]
for side in (-1,1):
    shell_parts.append(newel_glass("Lower rounded glass newel",lower_outline,side*59))
    shell_parts.append(newel_glass("Upper rounded glass newel",upper_outline,side*59))
    shell_parts.append(swept_handrail("Continuous mall handrail",rail_profile,side*59))

# Yellow safety comb boundaries make the entry/exit and tread edges readable.
for x, z in ((44.0, 0.25), (LENGTH-44.0, RISE+0.25)):
    shell_parts.append(box("Comb yellow threshold", (x, 0, z), (2.0, WIDTH, 0.4), yellow, 0.12))
shell = join(shell_parts, "SM_MallEscalatorShell")

# Retain a source tread as the template, with the source riser height. Add many
# close ribs rather than the five wide decorative cleats from the source GLB.
step_parts = [box("Cast aluminum tread", (0, 0, -11.9), (40.0, WIDTH, 22.2), tread_dark, 0.18)]
step_parts.append(box("Tread deck", (0, 0, -1.0), (39.8, WIDTH, 1.0), tread_metal, 0.10))
for n in range(66):
    y = -50.6 + n * 101.2 / 65.0
    step_parts.append(box("Parallel tread rib", (0, y, -0.2), (39.2, 0.55, 0.4), tread_metal))
    step_parts.append(box("Riser flute", (-19.92, y, -11.5), (0.25, 0.55, 20.3), tread_metal))
for side in (-1, 1):
    step_parts.append(box("Yellow side demarcation", (0, side*51.95, 0.03), (40.0, 1.9, 0.26), yellow))
step_parts.append(box("Yellow nosing", (-19.25, 0, 0.04), (1.5, WIDTH, 0.28), yellow, 0.08))
step = join(step_parts, "SM_MallEscalatorStep")

# Coordinates are centimeters. Metric scale 0.01 creates FBX unit metadata in cm.
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = 0.01

def export(obj, filename):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(filepath=str(OUTPUT / filename), use_selection=True,
        object_types={"MESH"}, global_scale=1.0, apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_ALL", axis_forward="-Y", axis_up="Z",
        bake_space_transform=False, use_mesh_modifiers=True, mesh_smooth_type="FACE",
        use_tspace=False, add_leaf_bones=False, bake_anim=False)

export(shell, "SM_MallEscalatorShell.fbx")
export(step, "SM_MallEscalatorStep.fbx")

metadata = {
    "units": "centimeters", "local_axes": {"X": "ascending travel", "Y": "cross tread", "Z": "up"},
    "origin": "lower outer landing leading edge, centered across width, walking surface Z=0",
    "lower_landing": {"start": [0,0,0], "end": [LANDING,0,0]},
    "slope": {"start": [LANDING,0,0], "end": [LANDING+RUN,0,RISE]},
    "upper_landing": {"start": [LANDING+RUN,0,RISE], "end": [LENGTH,0,RISE]},
    "run_cm": RUN, "rise_cm": RISE, "landing_length_cm": LANDING,
    "step_width_cm": WIDTH, "shell_width_cm": 136.0,
    "fixed_end_plate_length_cm": 45.0,
    "handrail": {"level_approach": True, "rounded_newel_radius_cm": 30.0, "rubber_section_cm": [10.0,6.4], "height_above_walk_profile_cm": 111.0},
    "step_pitch_cm": 40.0, "step_surface_pivot": [0,0,0],
    "step_bounds_cm": [list(bounds(step)[0]), list(bounds(step)[1])],
    "shell_bounds_cm": [list(bounds(shell)[0]), list(bounds(shell)[1])],
    "shell_material_slots": [m.name for m in shell.data.materials],
    "step_material_slots": [m.name for m in step.data.materials],
    "removed_static_moving_parts": len(removed),
    "fbx_import_settings": {"import_uniform_scale": 1.0, "convert_scene": True, "force_front_x_axis": False},
    "source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
}
(OUTPUT / "mesh_metadata.json").write_text(json.dumps(metadata, indent=2), encoding="utf-8")
(OUTPUT / "LICENSE_AND_SOURCE.txt").write_text(
    "Concourse Escalator Module\n"
    "Source: https://3dassets.dev/assets/retail-store-fixtures-and-mall-concourse-escalator-mod-e0d69a9c\n"
    "Direct original model: https://cdn.3dassets.dev/assets/25360/v1/model.glb\n"
    "Source page license: CC0 1.0 Universal (public domain dedication).\n"
    "License: https://creativecommons.org/publicdomain/zero/1.0/\n"
    "Downloaded: 2026-10-10\n"
    "Source page reports an AI-authored static model. No animation, rig or collision was supplied.\n"
    "Original SHA256: " + metadata["source_sha256"] + "\n"
    "Adaptations: corrected opposite incline orientation of source long truss/glass/rail components,\n"
    "separated static frame, shortened fixed landings to expose moving approaches, centimeters/pivot normalization, replacement cast steps,\n"
    "parallel tread/riser ribs, yellow demarcation and landing threshold strips,\n"
    "rounded glass newels and continuous flattened rubber handrail sections.\n"
    "Preparation script: ../../Automation/Escalator/prepare_mesh.py\n", encoding="utf-8")

# Assembly preview only. Native runtime owns all moving tread transforms.
step.hide_render = True
step.hide_set(True)
preview_parts = []
for i in range(20):
    x = 65.0 + i * 40.0
    z = max(0.0, min(RISE, (x-LANDING) / RUN * RISE))
    obj = bpy.data.objects.new("Preview moving tread %02d" % i, step.data)
    bpy.context.collection.objects.link(obj)
    obj.location = (x, 0, z)
    preview_parts.append(obj)

ground = box("Preview ground", (440, 0, -30), (2000, 1800, 20), material("Preview mall floor", (0.16,0.18,0.2), 0.1, 0.35))

def area(name, position, power, size):
    bpy.ops.object.light_add(type="AREA", location=position)
    obj = bpy.context.object
    obj.name = name
    obj.data.energy = power
    obj.data.shape = "DISK"
    obj.data.size = size
    obj.rotation_euler = (Vector((440,0,180))-obj.location).to_track_quat("-Z", "Y").to_euler()

# Scene distances are numeric centimeters; energies compensate for inverse square.
area("Soft showroom key", (250,-400,950), 16000000, 650)
area("Showroom fill", (700,500,700), 13000000, 600)
area("Rim light", (0,250,700), 7000000, 450)
bpy.ops.object.camera_add(location=(-650,-1000,820))
camera = bpy.context.object
camera.name = "Escalator preview camera"
camera.rotation_euler = (Vector((460,0,205))-camera.location).to_track_quat("-Z", "Y").to_euler()
camera.data.type = "ORTHO"
camera.data.ortho_scale = 1180
camera.data.clip_start = 1.0
camera.data.clip_end = 10000.0
bpy.context.scene.camera = camera
scene = bpy.context.scene
scene.world.color = (0.25, 0.25, 0.25)
scene.render.engine = "CYCLES"
scene.cycles.samples = 32
scene.cycles.use_denoising = True
scene.render.resolution_x = 1440
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
scene.view_settings.view_transform = "AgX"
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = str(OUTPUT / "escalator_preview.png")
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT / "MallEscalator.blend"))
bpy.ops.render.render(write_still=True)
print("ESCALATOR_ASSETS_READY", json.dumps(metadata))
