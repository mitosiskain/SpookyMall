import bpy,re
from pathlib import Path
from mathutils import Vector
root=Path(r'C:\unreal\uprj\MyProject2\ExternalAssets\Mall\RetailKit\ImportReady')
bpy.ops.wm.open_mainfile(filepath=str(root/'RetailGallery.blend'))
order=['25369','25366','25310','25345','25356','25358','25340','25343']
for obj in bpy.context.scene.objects:
    if obj.type!='MESH': continue
    match=re.search(r'SM_Retail_(\d+)_',obj.name)
    if match:
        i=order.index(match.group(1))
        obj.location=((i%4)*430,(i//4)*650-300,0)
target=Vector((630,200,145))
cam=bpy.context.scene.camera
cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
cam.data.ortho_scale=2150
bpy.ops.wm.save_as_mainfile(filepath=str(root/'RetailGallery.blend'))
bpy.ops.render.render(write_still=True)
