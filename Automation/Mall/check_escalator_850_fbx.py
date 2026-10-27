"""Round-trip the 850 cm shell to verify serialized units, pivot and slots."""
import bpy
import json
import math
from pathlib import Path
from mathutils import Vector

root = Path(r"C:\unreal\uprj\MyProject2\ExternalAssets\Mall\Escalator850")
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(root / "SM_MallEscalatorShell_850.fbx"))
meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
assert len(meshes) == 1, len(meshes)
obj = meshes[0]
# Default Blender FBX import normalizes the exported centimeters to meters.
points = [(obj.matrix_world @ v.co) * 100.0 for v in obj.data.vertices]
low = Vector([min(p[i] for p in points) for i in range(3)])
high = Vector([max(p[i] for p in points) for i in range(3)])
run = 850.0 / math.tan(math.radians(30.0))
expected_low = Vector((0.0, -68.00388336, -60.17764282))
expected_high = Vector((run + 260.0, 68.00388336, 964.2))
assert (low - expected_low).length < 0.02, (list(low), list(expected_low))
assert (high - expected_high).length < 0.02, (list(high), list(expected_high))
slots = [m.name for m in obj.data.materials]
assert slots == ["metal", "glass", "dark", "safety_yellow"], slots
report = {
    "fbx_roundtrip_passed": True,
    "imported_meshes": len(meshes),
    "bounds_cm": [list(low), list(high)],
    "expected_bounds_cm": [list(expected_low), list(expected_high)],
    "maximum_bound_error_cm": max((low - expected_low).length, (high - expected_high).length),
    "slots": slots,
    "units": "centimeters",
}
(root / "fbx_verification.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print("ESCALATOR_850_FBX_VERIFIED", json.dumps(report))
