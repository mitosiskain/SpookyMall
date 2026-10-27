"""Initial offline import of the prepared CC0 physical ghost sounds.

Run with Unreal's PythonScript commandlet while the interactive editor is closed.
Existing assets are preserved: later replacements should use Reimport or new names.
"""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
names = ["SW_Ghost_InvisibleBreath", "SW_Ghost_InvisibleMoan", "SW_Ghost_HuntGrowl"]
tasks = []
for name in names:
    asset_path = "/Game/Audio/Ghost/Physical/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        task = unreal.AssetImportTask()
        task.set_editor_property("Filename", str(root / "SourceAssets/GhostSounds" / (name + ".wav")))
        task.set_editor_property("DestinationPath", "/Game/Audio/Ghost/Physical")
        task.set_editor_property("DestinationName", name)
        task.set_editor_property("bReplaceExisting", False)
        task.set_editor_property("bAutomated", True)
        task.set_editor_property("bSave", True)
        task.set_editor_property("Factory", unreal.SoundFactory())
        tasks.append(task)
if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
rows = []
for name in names:
    sound = unreal.load_asset("/Game/Audio/Ghost/Physical/" + name)
    if not isinstance(sound, unreal.SoundWave) or sound.get_editor_property("NumChannels") != 1:
        raise RuntimeError("Expected mono SoundWave: " + name)
    duration = sound.get_editor_property("Duration")
    if not 0.0 < duration <= 15.0:
        raise RuntimeError("Expected finite short ghost cue: " + name)
    sound.set_editor_property("bLooping", False)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(sound)
    rows.append({"asset": sound.get_path_name(), "duration": duration,
                 "channels": sound.get_editor_property("NumChannels"),
                 "looping": sound.get_editor_property("bLooping"), "saved": saved})
(root / "Automation/GhostSounds/import_validation.json").write_text(json.dumps(rows, indent=2) + "\n", encoding="utf-8")
print(json.dumps(rows))
