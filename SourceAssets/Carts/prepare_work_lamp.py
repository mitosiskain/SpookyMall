"""Offline CC0 Work Lamp preparation. Run with Blender --background --python.

Produces one full lamp and one isolated lamp head in centimeter space, +X front,
Z up. Source files stay immutable. No Unreal editor or project assets are touched.
"""
from pathlib import Path
import bpy, json, hashlib, math
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'Prepared'
OUT.mkdir(exist_ok=True)

def clear():
    for o in list(bpy.data.objects): bpy.data.objects.remove(o, do_unlink=True)
    for mesh in list(bpy.data.meshes):
        if mesh.users == 0: bpy.data.meshes.remove(mesh)
    for material in list(bpy.data.materials):
        if material.users == 0: bpy.data.materials.remove(material)
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.scale_length = 1.0

def export(head_only):
    clear()
    bpy.ops.import_scene.gltf(filepath=str(ROOT / 'WorkLamp.glb'))
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    head = next(o for o in meshes if o.name.startswith('work-lamp_head'))
    pivot = head.matrix_world.translation.copy() if head_only else Vector((0, 0, 0))
    selected = [o for o in meshes if 'body' not in o.name] if head_only else meshes
    original_world = {o.name: o.matrix_world.copy() for o in selected}
    # The source documentation defines the front as Blender -Y.
    rotate = Matrix.Rotation(math.pi / 2.0, 4, 'Z')
    for o in selected:
        world = original_world[o.name]
        for v in o.data.vertices: v.co = (rotate @ (world @ v.co - pivot)) * 100.0
        o.parent = None
        o.matrix_world.identity()
        o.animation_data_clear()
    for o in list(bpy.context.scene.objects):
        if o not in selected: bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.select_all(action='DESELECT')
    for o in selected: o.select_set(True)
    bpy.context.view_layer.objects.active = selected[0]
    bpy.ops.object.join()
    o = bpy.context.object
    o.name = 'SM_CartWorkLampHead' if head_only else 'SM_CartWorkLampFull'
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.context.scene.unit_settings.scale_length = 0.01
    destination = OUT / (o.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=str(destination), use_selection=True,
        object_types={'MESH'}, global_scale=1.0, apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_ALL', axis_forward='-Y', axis_up='Z',
        bake_space_transform=False, use_mesh_modifiers=True, mesh_smooth_type='FACE',
        use_tspace=False, add_leaf_bones=False, bake_anim=False, path_mode='STRIP')
    points = [o.matrix_world @ v.co for v in o.data.vertices]
    lo = [min(p[i] for p in points) for i in range(3)]
    hi = [max(p[i] for p in points) for i in range(3)]
    return dict(file=destination.name, sha256=hashlib.sha256(destination.read_bytes()).hexdigest(),
        bounds_cm=[lo,hi], dimensions_cm=[hi[i]-lo[i] for i in range(3)],
        pivot='source head tilt-axis origin' if head_only else 'source ground origin',
        triangles=sum(len(p.vertices)-2 for p in o.data.polygons),
        uv_channels=len(o.data.uv_layers), materials=[m.name for m in o.data.materials],
        axes='centimeters, +X lamp front, Z up',
        notes='Default source head tilt retained (10 degrees down). Set component pitch to aim the spotlight downward.')

rows=[export(False), export(True)]
(OUT / 'work_lamp_preparation.json').write_text(json.dumps(rows,indent=2)+'\n')
print('CART_LAMP_PREPARED '+json.dumps(rows))
