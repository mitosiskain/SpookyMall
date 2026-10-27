import bpy
import json
from pathlib import Path
from mathutils import Vector

root=Path(r"C:\unreal\uprj\MyProject2\ExternalAssets\Mall\PolyHaven\ImportReady")
plan=json.loads((root/"import_plan.json").read_text())
results=[]
for spec in plan["meshes"]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.system="METRIC"
    bpy.context.scene.unit_settings.scale_length=1
    bpy.ops.import_scene.fbx(filepath=spec["fbx_path"], use_custom_normals=True)
    meshes=[obj for obj in bpy.context.scene.objects if obj.type=="MESH"]
    assert len(meshes)==1, (spec["mesh_name"],len(meshes))
    obj=meshes[0]
    points=[obj.matrix_world @ vertex.co * 100 for vertex in obj.data.vertices]
    actual=[[min(p[i] for p in points) for i in range(3)],[max(p[i] for p in points) for i in range(3)]]
    dimensions=[actual[1][i]-actual[0][i] for i in range(3)]
    assert all(abs(dimensions[i]-spec["dimensions_cm"][i])<.01 for i in range(3)), (spec["mesh_name"], dimensions,spec["dimensions_cm"])
    assert abs(actual[0][2])<.01, (spec["mesh_name"],actual[0][2])
    slots=[mat.name for mat in obj.data.materials]
    assert slots==spec["material_slots"], (spec["mesh_name"],slots,spec["material_slots"])
    assert len(obj.data.uv_layers)==len(spec["uv_channels"]), (spec["mesh_name"],"UV layer mismatch")
    results.append({"mesh_name":spec["mesh_name"],"dimension_match":True,"grounded":True,"material_slots_match":True,"uv_layers_preserved":True,"dimensions_cm":dimensions})
    print("PASS",spec["mesh_name"],[round(x,2) for x in dimensions],slots,flush=True)
(root/"fbx_roundtrip_verification.json").write_text(json.dumps(results,indent=2))
print("All",len(results),"FBX meshes roundtrip verified",flush=True)
