"""Scoped mesh asset setup. No material graph or level geometry edits."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
report=json.loads((root/'Saved/MallBuild/import_report.json').read_text())
bindings={k:'/Game/Generated_Materials/'+v['material_name'] for k,v in report['materials'].items()}
bindings.update({k:'/Game/Generated_Materials/M_MallRetail_'+k for k in ['floor','carcass','dark','metal','glass','surface','accent']})
bindings.update({'floor':'/Game/Generated_Materials/M_MallFloorMarble','Stone':'/Game/Generated_Materials/M_MallFloorMarble','Stainless steel':'/Game/Generated_Materials/M_MallRetail_metal','Advanced Procedural Glass for Cycles':'/Game/Generated_Materials/M_MallRetail_glass'})
results=[]
for row in report['meshes']:
    mesh=unreal.load_asset(row['path']);applied=[];missing=[]
    if not mesh:
        results.append({'path':row['path'],'error':'mesh_missing'});continue
    for i,slot in enumerate(row['slots']):
        path=('/Game/Environment/Escalator/Meshes/'+slot) if 'SM_MallEscalatorShell_450' in row['path'] else bindings.get(slot)
        mat=unreal.load_asset(path) if path else None
        if mat:
            mesh.set_material(i,mat);applied.append({'slot':slot,'material':mat.get_path_name()})
        else:missing.append({'slot':slot,'path':path})
    saved=unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=True)
    results.append({'path':row['path'],'applied':applied,'missing':missing,'saved':saved})
(root/'Saved/MallBuild/material_bindings.json').write_text(json.dumps(results,indent=2))
unreal.log(json.dumps({'mesh_count':len(results),'bound_slots':sum(len(x.get('applied',[])) for x in results),'missing':sum(len(x.get('missing',[])) for x in results),'save_failures':[x['path'] for x in results if not x.get('saved',False)]}))
