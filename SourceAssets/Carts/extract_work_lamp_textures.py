"""Extract embedded source GLB PBR textures for normal Unreal material import.

Use Python with Pillow. Keeps original GLB bytes unchanged. Basecolor is sRGB;
roughness, metallic and the green-flipped DirectX normal are linear textures.
"""
from pathlib import Path
from io import BytesIO
import json, struct, hashlib
from PIL import Image, ImageOps

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'Prepared'/'Textures'
OUT.mkdir(parents=True, exist_ok=True)
raw=(ROOT/'WorkLamp.glb').read_bytes()
length,kind=struct.unpack_from('<II',raw,12)
document=json.loads(raw[20:20+length])
bin_header=20+length
bin_length,bin_type=struct.unpack_from('<II',raw,bin_header)
blob=raw[bin_header+8:bin_header+8+bin_length]
rows=[]
for image in document['images']:
    view=document['bufferViews'][image['bufferView']]
    start=view.get('byteOffset',0)
    data=blob[start:start+view['byteLength']]
    picture=Image.open(BytesIO(data)).convert('RGB')
    name=image['name'].replace('work-lamp_', 'WorkLamp_')
    outputs=[]
    if name.endswith('_normal'):
        r,g,b=picture.split()
        picture=Image.merge('RGB',(r,ImageOps.invert(g),b))
        outputs=[(name+'_DX',picture,'linear, DirectX normal, green inverted from glTF OpenGL normal')]
    elif '_metallic-' in name:
        prefix=name.split('_metallic-')[0]
        r,g,b=picture.split()
        outputs=[(prefix+'_roughness',g,'linear, glTF packed G roughness'),
                 (prefix+'_metallic',b,'linear, glTF packed B metallic')]
    else:
        outputs=[(name,picture,'sRGB base color')]
    for filename,output,usage in outputs:
        destination=OUT/(filename+'.png')
        output.save(destination)
        rows.append(dict(file=destination.name,source_embedded_image=image['name'],
            dimensions=list(output.size),usage=usage,
            sha256=hashlib.sha256(destination.read_bytes()).hexdigest()))
(OUT/'textures.json').write_text(json.dumps(rows,indent=2)+'\n')
print(json.dumps(rows,indent=2))
