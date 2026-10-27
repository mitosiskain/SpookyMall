"""Stage a curated CC0 retail kit and supplemental surface maps for the mall."""
import json, hashlib, urllib.request, time
from pathlib import Path

ROOT = Path(r'C:\unreal\uprj\MyProject2\ExternalAssets\Mall')
ROOT.mkdir(parents=True, exist_ok=True)

def fetch(url, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists():
        request = urllib.request.Request(url, headers={'User-Agent':'MallAssetIntake/1.0'})
        with urllib.request.urlopen(request, timeout=120) as response:
            path.write_bytes(response.read())
    return {'url':url,'path':str(path),'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}

url = 'https://3dassets.dev/api/v1/packs/retail-store-fixtures-and-mall'
entry = fetch(url, ROOT / 'RetailKit' / 'pack_manifest.json')
payload = json.loads((ROOT/'RetailKit'/'pack_manifest.json').read_text(encoding='utf-8'))
print(json.dumps({'manifest':entry,'type':type(payload).__name__,'keys':list(payload) if isinstance(payload,dict) else None}),flush=True)
if isinstance(payload,dict):
    for key,value in payload.items():
        if isinstance(value,list):
            print(json.dumps({'array':key,'count':len(value),'example':value[:1]}),flush=True)

# Stage every small CC0 model for future stores; import a curated subset for the test atrium.
assets = payload['data']['assets']
downloads=[]
for asset in assets:
    record=fetch(asset['cdnUrl'], ROOT/'RetailKit'/'GLB'/(asset['id']+'.glb'))
    record.update({'id':asset['id'],'name':asset['title'],'page':asset['url'],'license':asset['license'],'ai_generated':asset['aiGenerated']})
    downloads.append(record)
    if len(downloads)%30==0:
        print(json.dumps({'retail_models_downloaded':len(downloads)}),flush=True)
(ROOT/'RetailKit'/'downloads.json').write_text(json.dumps(downloads,indent=2),encoding='utf-8')
(ROOT/'RetailKit'/'LICENSE_AND_SOURCE.txt').write_text('Retail Store Fixtures and Mall\nSource: https://3dassets.dev/packs/retail-store-fixtures-and-mall\nCC0 1.0 Universal: https://creativecommons.org/publicdomain/zero/1.0/\nAI-authored; geometry needs visual review.\nSource units metres, +Y up; no collision or gameplay supplied.\nDownloaded October 10, 2026. Individual URLs and SHA256 values: downloads.json.\n',encoding='utf-8')
print(json.dumps({'retail_complete':len(downloads),'bytes':sum(x['bytes'] for x in downloads)}),flush=True)
