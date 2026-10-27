import bpy, json
from pathlib import Path
from mathutils import Vector
bpy.ops.wm.open_mainfile(filepath=r"C:\unreal\uprj\MyProject2\ExternalAssets\Escalator\MallEscalator.blend")
shell=bpy.data.objects['SM_MallEscalatorShell']
for obj in list(bpy.data.objects):
    if obj != shell:
        bpy.data.objects.remove(obj, do_unlink=True)
bpy.ops.object.select_all(action='DESELECT')
shell.select_set(True)
bpy.context.view_layer.objects.active=shell
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.separate(type='LOOSE')
bpy.ops.object.mode_set(mode='OBJECT')
rows=[]
for obj in bpy.context.scene.objects:
    if obj.type!='MESH': continue
    points=[obj.matrix_world@v.co for v in obj.data.vertices]
    low=Vector([min(p[i] for p in points) for i in range(3)])
    high=Vector([max(p[i] for p in points) for i in range(3)])
    rows.append({'name':obj.name,'low':list(low),'high':list(high),'dims':list(high-low),'verts':len(points),'materials':list(set(obj.data.materials[p.material_index].name for p in obj.data.polygons))})
out=Path(r'C:\unreal\uprj\MyProject2\Automation\Mall\escalator_source_components.json')
out.write_text(json.dumps(rows,indent=2))
print(json.dumps(rows))
