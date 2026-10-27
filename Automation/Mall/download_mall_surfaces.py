"""Download the approved free CC0 supplementary mall PBR materials."""
import concurrent.futures
import hashlib
import json
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(r"C:\unreal\uprj\MyProject2\ExternalAssets\Mall\Surfaces")
HEADERS = {"User-Agent": "Codex-MyProject2-Mall-Asset-Preparation/1.0"}
TEXTURECAN_URL = "https://www.texturecan.com/downloads/tiles_0013/tiles_0013_2k_y3eHkx.zip"
tasks=[]
assets=[]
for asset_id in ("white_plaster_02", "terrazzo_tiles", "painted_metal_shutter"):
    files=json.loads((ROOT/(asset_id+"_files.json")).read_text())
    maps={}
    for role, key in (("base_color","Diffuse"),("normal","nor_dx"),("roughness","Rough"),("ambient_occlusion","AO")):
        remote=files[key]["2k"].get("png") or files[key]["2k"].get("jpg")
        path=ROOT/asset_id/Path(remote["url"]).name
        tasks.append((asset_id,role,remote,path))
        maps[role]=str(path)
    assets.append({"material_name": "M_Mall_"+asset_id, "asset_id": asset_id, "provider":"Poly Haven", "source_url":"https://polyhaven.com/a/"+asset_id, "license":"CC0 1.0 Universal", "license_url":"https://polyhaven.com/license", "resolution":"2K", "textures":maps, "normal_convention":"DirectX", "normal_flip_green":False, "base_color_srgb":True, "data_maps_srgb":False, "metallic_constant":0.0, "shading_model":"DefaultLit", "blend_mode":"Opaque", "status":"download_pending"})

def get_bytes(url):
    with urllib.request.urlopen(urllib.request.Request(url,headers=HEADERS),timeout=90) as response:
        return response.read()

def download(task):
    asset_id,role,remote,path=task
    path.parent.mkdir(parents=True,exist_ok=True)
    data=path.read_bytes() if path.exists() else get_bytes(remote["url"])
    assert len(data)==remote["size"]
    assert hashlib.md5(data).hexdigest()==remote["md5"]
    path.write_bytes(data)
    return {"asset_id":asset_id,"role":role,"source_url":remote["url"],"local_path":str(path),"size_bytes":len(data),"sha256":hashlib.sha256(data).hexdigest(),"source_md5":remote["md5"],"status":"downloaded_verified"}
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
    results=list(executor.map(download,tasks))
for result in results:
    print(result["asset_id"],result["role"],result["size_bytes"],flush=True)
for asset in assets:
    asset["status"]="downloaded_verified"

archive=ROOT/"texturecan_tiles_0013"/"tiles_0013_2k.zip"
archive.parent.mkdir(parents=True,exist_ok=True)
data=archive.read_bytes() if archive.exists() else get_bytes(TEXTURECAN_URL)
archive.write_bytes(data)
results.append({"asset_id":"texturecan_tiles_0013","role":"source_archive","source_url":TEXTURECAN_URL,"local_path":str(archive),"size_bytes":len(data),"sha256":hashlib.sha256(data).hexdigest(),"status":"downloaded_verified_zip"})
with zipfile.ZipFile(archive) as package:
    for member in package.infolist():
        target=(archive.parent/member.filename).resolve()
        assert target.is_relative_to(archive.parent.resolve()),member.filename
        if member.is_dir():
            target.mkdir(parents=True,exist_ok=True)
            continue
        target.parent.mkdir(parents=True,exist_ok=True)
        member_data=package.read(member)
        target.write_bytes(member_data)
        results.append({"asset_id":"texturecan_tiles_0013","role":"archive_member","source_url":TEXTURECAN_URL,"archive_member":member.filename,"local_path":str(target),"size_bytes":len(member_data),"sha256":hashlib.sha256(member_data).hexdigest(),"status":"extracted_verified_zip_crc"})
        print("TextureCan",member.filename,len(member_data),flush=True)
tc={"material_name":"M_Mall_CeilingTiles","asset_id":"texturecan_tiles_0013","provider":"TextureCan","source_url":"https://www.texturecan.com/details/131/","license":"CC0 1.0 Universal","license_url":"https://www.texturecan.com/terms/","resolution":"2K","status":"downloaded_verified","textures":{},"normal_convention":"DirectX","normal_convention_source":"https://www.texturecan.com/post/3/DirectX-vs-OpenGL-Normal-Map/","normal_flip_green":False,"base_color_srgb":True,"data_maps_srgb":False,"metallic_constant":0.0,"shading_model":"DefaultLit","blend_mode":"Opaque"}
for file in archive.parent.rglob("*"):
    if not file.is_file() or "__MACOSX" in file.parts or file.name.startswith("._") or file.suffix.lower() not in (".png",".jpg",".jpeg",".tga"):
        continue
    stem=file.stem.lower()
    if "normal_opengl" in stem:
        continue
    for role,terms in (("base_color",("color","colour","diffuse")),("normal",("normal",)),("roughness",("roughness","rough")),("ambient_occlusion",("ambient","ao")),("height",("height","displacement")),("metallic",("metallic","metalness"))):
        if any(term in stem for term in terms):
            tc["textures"][role]=str(file)
            break
assets.append(tc)
manifest={"date":"2026-10-10","assets":assets,"files":results,"notes":["Poly Haven API queried with identifying Codex project User-Agent; original metadata retained.","CC0 commercial game use; credits retained for provenance although not required.","TextureCan archive CRC checked by zipfile extraction; use the explicitly named DirectX normal. Ignore the alternate OpenGL normal and __MACOSX metadata.","Painted metal shutter source is coated paint; metallic constant0.0 is appropriate until a separate exposed-metal mask is authored."]}
(ROOT/"surface_manifest.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
(ROOT/"material_recipes.json").write_text(json.dumps({"materials":assets,"import_notes":{"base_color":{"sRGB":True,"CompressionSettings":"Default"},"normal":{"sRGB":False,"CompressionSettings":"Normalmap"},"data":{"sRGB":False,"CompressionSettings":"Masks"},"uv":"Use consistent world scale or suitable TexCoord tiling. Source surface dimensions must be reviewed for each authored mesh."}},indent=2),encoding="utf-8")
(ROOT/"LICENSE_AND_SOURCE.txt").write_text("Supplemental mall surfaces\nPoly Haven sources and TextureCan ceiling tile archive are CC0 1.0 Universal.\nPoly Haven license: https://polyhaven.com/license\nTextureCan terms: https://www.texturecan.com/terms/\nCC0 dedication: https://creativecommons.org/publicdomain/zero/1.0/\nExact source pages, download URLs and SHA256 values are in surface_manifest.json.\n",encoding="utf-8")
print("DONE",len(results),"files",sum(item["size_bytes"] for item in results),"bytes",flush=True)
