"""Download free CC0 3D equipment models from Poly Haven API with full provenance and SHA256 tracking."""
import hashlib
import json
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CC0_DIR = ROOT / "PolyHavenCC0"
CC0_DIR.mkdir(parents=True, exist_ok=True)

ASSETS = [
    {
        "slug": "retro_multimeter",
        "role": "EMF Reader / Handheld Meter CC0 Base",
        "page_url": "https://polyhaven.com/a/retro_multimeter",
    },
    {
        "slug": "Camera_01",
        "role": "Photo Camera CC0 Base",
        "page_url": "https://polyhaven.com/a/Camera_01",
    },
    {
        "slug": "signal_flashlight",
        "role": "UV Blacklight Torch CC0 Base",
        "page_url": "https://polyhaven.com/a/signal_flashlight",
    },
    {
        "slug": "portable_cassette_player",
        "role": "Spirit Box / Audio Recorder CC0 Base",
        "page_url": "https://polyhaven.com/a/portable_cassette_player",
    },
]

def fetch_json(url: str) -> dict:
    req = urllib.request.Request(url, headers={"User-Agent": "MyProject2-AssetPipeline/1.0"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.loads(resp.read().decode("utf-8"))

def download_file(url: str, dest: Path) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": "MyProject2-AssetPipeline/1.0"})
    with urllib.request.urlopen(req, timeout=60) as resp:
        data = resp.read()
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(data)
    return hashlib.sha256(data).hexdigest()

def main():
    provenance_records = []
    for item in ASSETS:
        slug = item["slug"]
        asset_dir = CC0_DIR / slug
        asset_dir.mkdir(parents=True, exist_ok=True)
        info = fetch_json(f"https://api.polyhaven.com/info/{slug}")
        files = fetch_json(f"https://api.polyhaven.com/files/{slug}")

        downloaded_files = []
        # Download the primary FBX or GLTF/Blend geometry + 1k textures
        fbx_group = files.get("fbx", {}).get("1k", {}).get("fbx", {})
        if fbx_group and "url" in fbx_group:
            fbx_dest = asset_dir / f"{slug}.fbx"
            sha = download_file(fbx_group["url"], fbx_dest)
            downloaded_files.append({"file": fbx_dest.name, "url": fbx_group["url"], "sha256": sha})

        gltf_group = files.get("gltf", {}).get("1k", {}).get("gltf", {})
        if gltf_group and "url" in gltf_group:
            gltf_dest = asset_dir / f"{slug}.gltf"
            sha = download_file(gltf_group["url"], gltf_dest)
            downloaded_files.append({"file": gltf_dest.name, "url": gltf_group["url"], "sha256": sha})
            for include_rel, inc_meta in gltf_group.get("include", {}).items():
                inc_dest = asset_dir / include_rel
                inc_sha = download_file(inc_meta["url"], inc_dest)
                downloaded_files.append({"file": include_rel, "url": inc_meta["url"], "sha256": inc_sha})

        provenance_records.append({
            "slug": slug,
            "name": info.get("name", slug),
            "role": item["role"],
            "source_url": item["page_url"],
            "license": "CC0 1.0 Universal (Public Domain Dedication)",
            "license_url": "https://creativecommons.org/publicdomain/zero/1.0/",
            "authors": info.get("authors", {}),
            "polycount": info.get("polycount", 0),
            "dimensions_mm": info.get("dimensions", []),
            "files": downloaded_files,
        })
        print(f"Downloaded CC0 asset: {slug} ({len(downloaded_files)} files)")

    prov_path = CC0_DIR / "provenance.json"
    prov_path.write_text(json.dumps(provenance_records, indent=2), encoding="utf-8")
    print(f"Saved provenance to {prov_path}")

if __name__ == "__main__":
    main()
