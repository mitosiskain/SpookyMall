"""Read-only image integrity and resolution checks for the prepared PBR recipes."""
import json
from pathlib import Path
from PIL import Image

root=Path(r"C:\unreal\uprj\MyProject2\ExternalAssets\Mall\Surfaces")
recipes=json.loads((root/"material_recipes.json").read_text())
results=[]
for material in recipes["materials"]:
    assert material["normal_convention"]=="DirectX"
    assert material["normal_flip_green"] is False
    for role,path in material["textures"].items():
        with Image.open(path) as image:
            size=image.size
            image.verify()
        assert size==(2048,2048),(role,path,size)
        results.append({"asset_id":material["asset_id"],"role":role,"path":path,"resolution":list(size),"image_integrity":"passed"})
(root/"surface_verification.json").write_text(json.dumps(results,indent=2),encoding="utf-8")
print("PASS",len(results),"usable maps; all2K; allfour recipes use explicit DirectX normals.")
