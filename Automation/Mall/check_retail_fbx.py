import bpy,json
from pathlib import Path
from mathutils import Vector
root=Path(r'C:\unreal\uprj\MyProject2\ExternalAssets\Mall\RetailKit\ImportReady')
plan=json.loads((root/'import_plan.json').read_text(encoding='utf-8-sig'))
checks=[]
for asset in plan['assets']:
    for obj in list(bpy.data.objects): bpy.data.objects.remove(obj,do_unlink=True)
    for mesh in list(bpy.data.meshes):
        if mesh.users==0:bpy.data.meshes.remove(mesh)
    for mat in list(bpy.data.materials):
        if mat.users==0:bpy.data.materials.remove(mat)
    bpy.context.scene.unit_settings.scale_length=1.0
    bpy.ops.import_scene.fbx(filepath=asset['fbx_path'])
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    assert len(meshes)==1,asset['source_id']
    obj=meshes[0]
    points=[(obj.matrix_world@v.co)*100.0 for v in obj.data.vertices]
    low=Vector([min(p[i] for p in points) for i in range(3)])
    high=Vector([max(p[i] for p in points) for i in range(3)])
    error=((high-low)-Vector(asset['dimensions_cm'])).length
    assert error<0.02,(asset['source_id'],error)
    assert abs(low.z)<0.01,(asset['source_id'],low.z)
    slots=[m.name for m in obj.data.materials]
    assert slots==asset['original_material_slots'],(asset['source_id'],slots)
    checks.append({'source_id':asset['source_id'],'dimensions_error_cm':error,'material_slots_passed':True,'floor_pivot_passed':True})
(root/'fbx_verification.json').write_text(json.dumps({'all_passed':True,'checked_assets':len(checks),'checks':checks},indent=2),encoding='utf-8')
print('ALL_RETAIL_FBX_CHECKS_PASSED',len(checks))
