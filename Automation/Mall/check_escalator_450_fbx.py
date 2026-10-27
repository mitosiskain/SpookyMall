"""Check serialized shell units, pivot, dimensions and material slots."""
import bpy, json
from pathlib import Path
from mathutils import Vector
root=Path(r'C:\unreal\uprj\MyProject2\ExternalAssets\Mall\Escalator450')
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(root/'SM_MallEscalatorShell_450.fbx'))
meshes=[obj for obj in bpy.context.scene.objects if obj.type=='MESH']
assert len(meshes)==1
obj=meshes[0]
# Blender's default FBX import normalizes centimeters to scene meters.
points=[(obj.matrix_world@v.co)*100.0 for v in obj.data.vertices]
low=Vector([min(p[i] for p in points) for i in range(3)])
high=Vector([max(p[i] for p in points) for i in range(3)])
expected_low=Vector((0,-68.00388336,-60.17764282))
expected_high=Vector((1039.4228634,68.00388336,564.2))
assert (low-expected_low).length<0.02, (list(low),list(expected_low))
assert (high-expected_high).length<0.02, (list(high),list(expected_high))
slots=[m.name for m in obj.data.materials]
assert slots==['metal','glass','dark','safety_yellow'], slots
report={'fbx_roundtrip_passed':True,'imported_meshes':len(meshes),'bounds_cm':[list(low),list(high)],'slots':slots}
(root/'fbx_verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report))
