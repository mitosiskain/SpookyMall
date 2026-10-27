import bpy
import json
from pathlib import Path
ROOT = Path(r"C:\unreal\uprj\MyProject2\ExternalAssets\Mall\UserImports\Prepared")
bpy.ops.wm.open_mainfile(filepath=str(ROOT / "MallBalconyModule_400.blend"), load_ui=False, use_scripts=False)
o=next(o for o in bpy.context.scene.objects if o.type=="MESH")
for slot,material in enumerate(o.data.materials):
    polys=[p for p in o.data.polygons if p.material_index==slot]
    nodes={}
    for p in polys:
        coords=[tuple(round(o.data.vertices[v].co[i],3) for i in range(3)) for v in p.vertices]
        for k in coords:
            nodes.setdefault(k,set()).update(coords)
    seen=set();components=[]
    for k in nodes:
        if k in seen:continue
        stack=[k]; points=[]
        while stack:
            x=stack.pop()
            if x in seen:continue
            seen.add(x);points.append(x);stack.extend(nodes[x]-seen)
        components.append({'n':len(points),'lo':[min(p[i] for p in points) for i in range(3)],'hi':[max(p[i] for p in points) for i in range(3)]})
    print(material.name,json.dumps(sorted(components,key=lambda c:-c['n'])),flush=True)
