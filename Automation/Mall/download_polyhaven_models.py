"""Download selected CC0 mall furnishings through the official Poly Haven API."""
import concurrent.futures
import hashlib
import json
import re
import time
import urllib.request
from pathlib import Path

ROOT = Path(r"C:\unreal\uprj\MyProject2\ExternalAssets\Mall\PolyHaven")
ASSETS = ["pachira_aquatica_01", "potted_plant_01", "modular_street_seating", "steel_frame_shelves_01", "decorative_book_set_01"]
HEADERS = {"User-Agent": "Codex-MyProject2-Mall-Asset-Preparation/1.0"}
ROOT.mkdir(parents=True, exist_ok=True)

def request_bytes(url):
    for attempt in range(3):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS), timeout=90) as response:
                return response.read()
        except Exception:
            if attempt == 2:
                raise
            time.sleep(1 + attempt)

tasks = []
asset_entries = []
for asset in ASSETS:
    files = json.loads(request_bytes(f"https://api.polyhaven.com/files/{asset}"))
    info = json.loads(request_bytes(f"https://api.polyhaven.com/info/{asset}"))
    (ROOT / f"{asset}_files.json").write_text(json.dumps(files, indent=2), encoding="utf-8")
    (ROOT / f"{asset}_info.json").write_text(json.dumps(info, indent=2), encoding="utf-8")
    dest = ROOT / asset
    dest.mkdir(exist_ok=True)
    blend = files["blend"]["2k"]["blend"]
    tasks.append((asset, "source_blend", blend, dest / Path(blend["url"]).name))
    maps = []
    for key, resolutions in files.items():
        if key in ("blend", "fbx", "usd", "gltf") or "2k" not in resolutions:
            continue
        lowered = key.lower()
        is_cover = re.search(r"_cover\d+$", lowered) is not None
        if is_cover and "book_softcover" not in lowered and "book_hardcover" not in lowered:
            # Catalogue/magazine alternate covers aren't needed in this first store.
            continue
        is_extra_cover = is_cover and not lowered.endswith(("cover02", "cover04"))
        skip_gl = "nor_gl" in lowered and lowered.replace("nor_gl", "nor_dx") in files
        keep_mask = lowered in ("book_hardcover_01_mask02", "book_softcover_01_mask02")
        if is_extra_cover or ("mask" in lowered and not keep_mask) or "disp" in lowered or skip_gl or lowered.endswith("arm"):
            continue
        # Prefer Unreal's DirectX normal convention. Leaves are only offered in GL for one asset.
        if keep_mask or lowered.endswith(("nor_dx", "nor_gl")) or any(lowered.endswith(suffix) for suffix in ("diff", "diffuse", "rough", "roughness", "alpha", "ao", "metal", "metallic")):
            entry = resolutions["2k"].get("png")
            if not entry:
                entry = resolutions["2k"].get("jpg")
            if not entry:
                continue
            maps.append(key)
            tasks.append((asset, key, entry, dest / "textures" / Path(entry["url"]).name))
        elif lowered.endswith(("cover02", "cover04")):
            entry = resolutions["2k"].get("png") or resolutions["2k"].get("jpg")
            maps.append(key)
            tasks.append((asset, key, entry, dest / "textures" / Path(entry["url"]).name))
    asset_entries.append({"asset_id": asset, "asset_page": f"https://polyhaven.com/a/{asset}", "license": "CC0 1.0 Universal", "license_url": "https://polyhaven.com/license", "status": "download_pending", "texture_resolution": "2k", "downloaded_maps": maps})

def download(task):
    asset, role, remote, path = task
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and hashlib.md5(path.read_bytes()).hexdigest() == remote["md5"]:
        data = path.read_bytes()
    else:
        data = request_bytes(remote["url"])
        assert len(data) == remote["size"], (path, "size mismatch")
        assert hashlib.md5(data).hexdigest() == remote["md5"], (path, "MD5 mismatch")
        path.write_bytes(data)
    return {"asset_id": asset, "role": role, "url": remote["url"], "local_path": str(path), "size_bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(), "source_md5": remote["md5"], "status": "downloaded_verified"}

results = []
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
    for result in executor.map(download, tasks):
        results.append(result)
        print(result["asset_id"], result["role"], result["size_bytes"], flush=True)
for asset in asset_entries:
    asset["status"] = "downloaded_verified"
manifest = {"date": "2026-10-10", "source": "Poly Haven", "api_user_agent": HEADERS["User-Agent"], "credit": "Downloaded from Poly Haven (https://polyhaven.com). Assets CC0; attribution not required.", "assets": asset_entries, "files": results}
(ROOT / "download_manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
(ROOT / "LICENSE_AND_SOURCE.txt").write_text("Poly Haven mall asset intake\nAll source models and textures: CC0 1.0 Universal\nLicense: https://polyhaven.com/license\nLegal dedication: https://creativecommons.org/publicdomain/zero/1.0/\nAPI queried with identifying User-Agent. Each original source URL and verified SHA256 is in download_manifest.json.\nNo assets are redistributed separately by this intake script.\n", encoding="utf-8")
print("DONE", len(results), "files", sum(r["size_bytes"] for r in results), "bytes", flush=True)
