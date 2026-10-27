"""Finish the newly placed escalator demonstration and save its authoring assets."""
import json
import unreal

subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
labels = [
    "MallEscalator_Up", "MallEscalator_Down", "Escalator_LowerLanding",
    "Escalator_UpperLanding", "Escalator_UpperBackGuard",
    "Escalator_UpperLeftGuard", "Escalator_UpperRightGuard",
    "Escalator_SupportA", "Escalator_SupportB", "Escalator_SupportC", "Escalator_SupportD",
]
actors = {actor.get_actor_label(): actor for actor in subsystem.get_all_level_actors()
          if actor.get_actor_label() in labels}
missing = set(labels) - set(actors)
if missing:
    raise RuntimeError("Escalator demonstration actors missing: " + str(sorted(missing)))

with unreal.ScopedEditorTransaction("Configure mall escalator demonstration"):
    for actor in actors.values():
        actor.set_folder_path("Mall Escalator Demo")
    actors["MallEscalator_Down"].set_editor_property("ReverseDirection", True)

assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
for path in ["/Game/Environment/Escalator/BP_MallEscalator",
             "/Game/Environment/Escalator/BP_EscalatorLanding"]:
    if not assets.save_asset(path, only_if_is_dirty=False):
        raise RuntimeError("Could not save " + path)
if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level():
    raise RuntimeError("Could not save the current level")

subsystem.set_selected_level_actors([actors["MallEscalator_Up"], actors["MallEscalator_Down"]])
print("ESCALATOR_SCENE_READY " + json.dumps({
    "actors": labels,
    "up_tread_count": actors["MallEscalator_Up"].get_editor_property("Steps").get_instance_count(),
    "down_reversed": actors["MallEscalator_Down"].get_editor_property("ReverseDirection"),
    "origin": [1100, 800, 6], "upper_floor_height": 366,
}))
