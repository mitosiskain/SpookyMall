"""Read back the exported FBX and record its metric dimensions and railing planes."""
import bpy
import json
import sys
from pathlib import Path
from mathutils import Vector

ROOT = Path(r"C:\unreal\uprj\MyProject2\ExternalAssets\Mall\UserImports\Prepared")
bpy.ops.wm.read_factory_settings(use_empty=True)
name = sys.argv[sys.argv.index("--")+1] if "--" in sys.argv else "SM_MallBalconyModule_400"
bpy.ops.import_scene.fbx(filepath=str(ROOT / (name + ".fbx")))
obj = next(o for o in bpy.context.scene.objects if o.type == "MESH")
points = [obj.matrix_world @ v.co * 100 for v in obj.data.vertices]
lo = [min(p[i] for p in points) for i in range(3)]
hi = [max(p[i] for p in points) for i in range(3)]
material_names = [m.name for m in obj.data.materials]
glass_index = next(i for i,n in enumerate(material_names) if "Glass" in n)
groups = {"normal_X": [], "normal_Y": [], "normal_Z": []}
for p in obj.data.polygons:
    if p.material_index != glass_index:
        continue
    center = sum((points[i] for i in p.vertices), Vector()) / len(p.vertices)
    normal = obj.matrix_world.to_3x3() @ p.normal
    axis = max(range(3), key=lambda i: abs(normal[i]))
    groups["normal_" + "XYZ"[axis]].append((p.area, list(center)))
planes = {}
for key, faces in groups.items():
    if not faces:
        continue
    area = sum(weight for weight,center in faces)
    planes[key] = {"faces": len(faces), "area": area,
        "mean_center_cm": [sum(weight * center[i] for weight,center in faces) / area for i in range(3)]}
obj.data.calc_loop_triangles()
stone_index = next(i for i,n in enumerate(material_names) if n=="Stone")
stone_signed_volume = 0
top_area = 0
top_normal_z = 0
for triangle in obj.data.loop_triangles:
    if obj.data.polygons[triangle.polygon_index].material_index == stone_index:
        a,b,c = (points[i] for i in triangle.vertices)
        stone_signed_volume += a.dot(b.cross(c))/6
        if min(a.z,b.z,c.z)>-.01:
            cross = (b-a).cross(c-a)
            area = cross.length*.5
            top_area += area
            top_normal_z += cross.normalized().z*area
result = {"mesh_name":name,"fbx_bounds_cm": [lo,hi], "material_slots": material_names,
          "stone_signed_volume_cm3":stone_signed_volume,
          "walking_plane_area_cm2":top_area,"walking_plane_normal_z":top_normal_z/top_area if top_area else None,
          "uv_channels": [uv.name for uv in obj.data.uv_layers], "glass_planes": planes}
minimum_y = -100 if "Straight" in name else -100.12883
assert max(abs(lo[i] - [-200,minimum_y,-50][i]) for i in range(3)) < .02
assert max(abs(hi[i] - [200,100,132.36688][i]) for i in range(3)) < .02
assert len(material_names) == 3
assert len(obj.data.uv_layers) >= 1
print(json.dumps(result,indent=2),flush=True)
assert stone_signed_volume>0, "Slab normals must face outward"
assert top_area>60000 and top_normal_z/top_area>.99, "Broad flat walking plane must face up"
if "Straight" in name:
    glass_points = [points[i] for p in obj.data.polygons if p.material_index==glass_index for i in p.vertices]
    assert min(p.y for p in glass_points)>89
(ROOT / (name + "_verification.json")).write_text(json.dumps(result,indent=2),encoding="utf-8")
print(json.dumps(result,indent=2),flush=True)
