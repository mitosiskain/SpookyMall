"""Import the replacement under a new name; never overwrite a loaded native default.

Run with Unreal's PythonScript commandlet while the interactive editor is closed.
For later recording changes, use the editor's Reimport operation or a new asset.
"""
from pathlib import Path
import json
import unreal

project = Path(unreal.Paths.project_dir()).resolve()
asset_path = "/Game/Audio/Ghost/SpiritBox/SW_SpiritBox_Static"
sound = unreal.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None
if sound is None:
    task = unreal.AssetImportTask()
    task.set_editor_property("Filename", str(project / "SourceAssets/SpiritBox/SW_SpiritBox_Scan.wav"))
    task.set_editor_property("DestinationPath", "/Game/Audio/Ghost/SpiritBox")
    task.set_editor_property("DestinationName", "SW_SpiritBox_Static")
    task.set_editor_property("bReplaceExisting", False)
    task.set_editor_property("bAutomated", True)
    task.set_editor_property("bSave", True)
    task.set_editor_property("Factory", unreal.SoundFactory())
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound = unreal.load_asset(asset_path)
if not isinstance(sound, unreal.SoundWave):
    raise RuntimeError("Replacement was not imported as a SoundWave")
if abs(sound.get_editor_property("Duration") - 12.0) > 0.01 or sound.get_editor_property("NumChannels") != 1:
    raise RuntimeError("Replacement must be twelve-second mono audio; refusing to overwrite an existing asset")
sound.set_editor_property("bLooping", True)
saved = unreal.EditorAssetLibrary.save_loaded_asset(sound)
result = {"asset": sound.get_path_name(), "duration": sound.get_editor_property("Duration"),
          "channels": sound.get_editor_property("NumChannels"), "looping": sound.get_editor_property("bLooping"),
          "saved": saved, "original_asset_preserved": True}
(project / "Automation/SpiritBox/static_import_validation.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps(result))
