import bpy
import json
from pathlib import Path

root = Path(r"C:\unreal\uprj\MyProject2\ExternalAssets\Mall\PolyHaven")
result = {}
for path in root.glob("*/*_2k.blend"):
    bpy.ops.wm.open_mainfile(filepath=str(path), load_ui=False, use_scripts=False)
    meshes = []
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        points = [obj.matrix_world @ vertex.co for vertex in obj.data.vertices]
        meshes.append({"name": obj.name, "parent": obj.parent.name if obj.parent else None, "vertices": len(points), "faces": len(obj.data.polygons), "dimensions": list(obj.dimensions), "location": list(obj.location), "bounds": [[min(p[i] for p in points) for i in range(3)], [max(p[i] for p in points) for i in range(3)]], "materials": [mat.name if mat else "" for mat in obj.data.materials], "collections": [col.name for col in obj.users_collection], "modifiers": [mod.type for mod in obj.modifiers]})
    result[path.parent.name] = meshes
    print(path.parent.name, json.dumps(meshes), flush=True)
(root / "source_mesh_inventory.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
