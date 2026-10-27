"""Project-original utility cart model; no external geometry is included.

Blender --background --factory-startup --python create_utility_cart.py
Centimeters, +X forward, Z up, origin at the ground under the deck center.
The shelf/handle measurements are edit points for later art refinement.
"""
from pathlib import Path
import bpy, math, json, hashlib
from mathutils import Vector

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'Prepared'
OUT.mkdir(exist_ok=True)
for o in list(bpy.data.objects): bpy.data.objects.remove(o,do_unlink=True)
bpy.context.scene.unit_settings.system='METRIC'
bpy.context.scene.unit_settings.scale_length=0.01

def material(name, color, metallic, roughness):
    m=bpy.data.materials.new(name)
    m.diffuse_color=(*color,1)
    m.use_nodes=True
    shader=m.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value=(*color,1)
    shader.inputs['Metallic'].default_value=metallic
    shader.inputs['Roughness'].default_value=roughness
    return m

steel=material('Cart_BrushedSteel',(0.36,0.40,0.44),0.85,0.32)
paint=material('Cart_BluePaint',(0.035,0.12,0.17),0.15,0.44)
rubber=material('Cart_Rubber',(0.018,0.023,0.028),0.0,0.76)
amber=material('Cart_AmberSafety',(0.9,0.35,0.035),0.0,0.5)
parts=[]

def cube(name, location, size, mat, bevel=0.5):
    bpy.ops.mesh.primitive_cube_add(size=1,location=location)
    o=bpy.context.object; o.name=name; o.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        mod=o.modifiers.new('Rounded metal edges','BEVEL'); mod.width=bevel;mod.segments=2
        bpy.ops.object.modifier_apply(modifier=mod.name)
    o.data.materials.append(mat);parts.append(o)
    return o

def tube(name, start, end, radius, mat, vertices=16):
    a,b=Vector(start),Vector(end);delta=b-a
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=delta.length,
        end_fill_type='NGON',location=(a+b)/2)
    o=bpy.context.object;o.name=name;o.rotation_euler=delta.to_track_quat('Z','Y').to_euler()
    o.data.materials.append(mat);parts.append(o)
    return o

# Three open decks, top at 93 cm, with pressed retaining lips. 100 x 64 cm.
for z in (24,57,91):
    cube('Pressed shelf',(0,0,z),(104,64,2),paint,0.65)
    for y in (-32,32):cube('Shelf long lip',(0,y,z+2.2),(104,1.4,4.4),steel,0.35)
    for x in (-52,52):cube('Shelf end lip',(x,0,z+2.2),(1.4,64,4.4),steel,0.35)
for x in (-48,48):
    for y in (-28,28):
        tube('Corner upright',(x,y,17),(x,y,98),1.7,steel)
        cube('Rubber corner bumper',(x,y,93),(8,8,5),rubber,1.2)
        # Castor top, stem, yoke and wheel, modeled at rest.
        cube('Castor mounting plate',(x,y,18),(10,10,1.8),steel,0.35)
        tube('Castor swivel',(x,y,13),(x,y,17),3.3,steel)
        for dy in (-3.3,3.3):cube('Castor fork',(x+1,y+dy,9.5),(7,1.2,8),steel,0.5)
        tube('Rubber wheel',(x+2,y-2.6,7),(x+2,y+2.6,7),7,rubber,24)
        tube('Wheel hub',(x+2,y-3.6,7),(x+2,y+3.6,7),2.6,steel,16)
        if x<0:cube('Brake pedal',(x+5,y,13),(8,5,1.5),amber,0.4)
# A rear U handle extends beyond the shelves; rear is -X.
for y in (-25,25):
    tube('Handle lower',(-48,y,65),(-62,y,99),1.7,steel)
    tube('Handle upper',(-62,y,99),(-62,y,108),1.7,steel)
tube('Push handle',(-62,-25,108),(-62,25,108),2.1,rubber,24)
# Lower longitudinal braces provide a stable, visible wheel frame.
for y in (-28,28):tube('Lower frame',(-48,y,20),(48,y,20),1.6,steel)

bpy.ops.object.select_all(action='DESELECT')
for o in parts:o.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.object.convert(target='MESH')
bpy.ops.object.join()
o=bpy.context.object;o.name='SM_EquipmentUtilityCart'
bpy.context.scene.cursor.location=(0,0,0)
bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
points=[o.matrix_world@v.co for v in o.data.vertices]
lo=[min(v[i] for v in points) for i in range(3)]
hi=[max(v[i] for v in points) for i in range(3)]
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'EquipmentUtilityCart.blend'))
destination=OUT/(o.name+'.fbx')
bpy.ops.export_scene.fbx(filepath=str(destination),use_selection=True,object_types={'MESH'},
    global_scale=1.0,apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL',
    axis_forward='-Y',axis_up='Z',bake_space_transform=False,use_mesh_modifiers=True,
    mesh_smooth_type='FACE',use_tspace=False,add_leaf_bones=False,bake_anim=False)
metadata=dict(file=destination.name,sha256=hashlib.sha256(destination.read_bytes()).hexdigest(),
    origin='Project-original geometry created for MyProject2; no external geometry or textures',
    axes='centimeters, +X forward, Z up',pivot='ground beneath center of decks',
    bounds_cm=[lo,hi],dimensions_cm=[hi[i]-lo[i] for i in range(3)],
    shelf_top_cm=[25,58,92],top_lip_cm=95.2,handle_height_cm=108,
    triangles=sum(len(p.vertices)-2 for p in o.data.polygons),
    uv_channels=len(o.data.uv_layers),
    materials=[dict(name=m.name,base_color=list(m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value),
        metallic=m.node_tree.nodes['Principled BSDF'].inputs['Metallic'].default_value,
        roughness=m.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value) for m in o.data.materials],
    notes='Static display mesh. Gameplay movement/collision supplied by native cart. Wheel animation is a later art refinement.')
(OUT/'utility_cart_preparation.json').write_text(json.dumps(metadata,indent=2)+'\n')
print('UTILITY_CART_PREPARED '+json.dumps(metadata))
