"""Restore only missing user Fab packages from the preserved autosave snapshot.

Unreal autosaves keep the original exported object names; the _Auto suffix is
the recovery filename. Existing Content files are never overwritten. Each copy
is confined to the same relative package directory and recorded with a hash.
"""
from pathlib import Path
import hashlib,json,re,shutil

root=Path(r'C:\unreal\uprj\MyProject2')
source=(root/'Saved'/'MallRecoverySnapshot'/'AutosavesFab').resolve()
dest=(root/'Content'/'Fab').resolve()
selected={}
for path in source.rglob('*.uasset'):
    match=re.fullmatch(r'(.+)_Auto\d*',path.stem)
    if not match:
        continue
    relative=path.relative_to(source).with_name(match.group(1)+'.uasset')
    target=(dest/relative).resolve()
    if not target.is_relative_to(dest):
        raise RuntimeError('Recovery target outside Fab content')
    prior=selected.get(target)
    if not prior or path.stat().st_mtime>prior.stat().st_mtime:
        selected[target]=path
records=[]
warnings=[]
for target,path in selected.items():
    if target.exists():
        continue
    data=path.read_bytes()
    name=target.stem
    if name.encode('utf-8') not in data and name.encode('utf-16-le') not in data:
        warnings.append('Exported object name not found; not restored: '+str(path))
        continue
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(path,target)
    records.append({'source':str(path),'target':str(target),'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)})
report={'restored':records,'warnings':warnings,'existing_files_overwritten':0}
output=root/'Saved'/'MallBuild'/'fab_recovery_manifest.json'
output.parent.mkdir(parents=True,exist_ok=True)
output.write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({'restored_packages':len(records),'warnings':warnings,'manifest':str(output)}))
