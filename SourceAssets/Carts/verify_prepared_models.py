"""Round-trip FBX validation and offline cart preview; no Unreal mutations."""
from pathlib import Path
import bpy,json,math
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
OUT=ROOT/'Prepared'
rows=[]
for file in ('SM_EquipmentUtilityCart.fbx','SM_CartWorkLampFull.fbx','SM_CartWorkLampHead.fbx'):
    for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
    bpy.context.scene.unit_settings.system='METRIC'
    bpy.context.scene.unit_settings.scale_length=0.01
    bpy.ops.import_scene.fbx(filepath=str(OUT/file),use_anim=False)
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    points=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
    lo=[min(p[i] for p in points) for i in range(3)]
    hi=[max(p[i] for p in points) for i in range(3)]
    expected=json.loads((OUT/('utility_cart_preparation.json' if 'UtilityCart' in file else 'work_lamp_preparation.json')).read_text())
    if isinstance(expected,list):expected=next(r for r in expected if r['file']==file)
    delta=max(abs(hi[i]-lo[i]-expected['dimensions_cm'][i]) for i in range(3))
    assert delta<0.01,(file,delta)
    triangles=sum(len(p.vertices)-2 for o in meshes for p in o.data.polygons)
    assert triangles==expected['triangles'],(file,triangles,expected['triangles'])
    assert all(len(o.data.uv_layers)>0 for o in meshes)
    rows.append(dict(file=file,passed=True,dimensions_cm=[hi[i]-lo[i] for i in range(3)],
        maximum_dimension_error_cm=delta,triangles=triangles))
(OUT/'fbx_verification.json').write_text(json.dumps(rows,indent=2)+'\n')
print('CART_FBX_ROUNDTRIP '+json.dumps(rows))

bpy.ops.wm.open_mainfile(filepath=str(OUT/'EquipmentUtilityCart.blend'))
scene=bpy.context.scene
scene.render.engine='CYCLES'
scene.cycles.device='CPU'
scene.cycles.samples=24
scene.render.resolution_x=720;scene.render.resolution_y=620;scene.render.resolution_percentage=100
scene.world.color=(0.45,0.45,0.45)
bpy.ops.object.light_add(type='SUN',location=(150,-130,240))
bpy.context.object.rotation_euler=(math.radians(25),math.radians(-20),math.radians(-30))
bpy.context.object.data.energy=3
bpy.ops.object.camera_add(location=(-190,-230,170))
camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,56))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=210;camera.data.clip_end=3000
scene.camera=camera
scene.render.filepath=str(OUT/'utility_cart_preview.png')
bpy.ops.render.render(write_still=True)
