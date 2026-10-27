"""Import prepared ghost-hunting equipment FBX models, author/compile Blueprints, and place test instances in Lvl_MallOval_Test."""
from pathlib import Path
import json
import unreal

PROJECT_DIR = Path(unreal.Paths.project_dir()).resolve()
PREPARED_DIR = PROJECT_DIR / "SourceAssets" / "Equipment" / "Prepared"
CC0_DIR = PROJECT_DIR / "SourceAssets" / "Equipment" / "PolyHavenCC0"

ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EDITOR_ASSET_LIB = unreal.EditorAssetLibrary


def import_static_mesh(fbx_path: Path, dest_folder: str, asset_name: str):
    if not EDITOR_ASSET_LIB.does_directory_exist(dest_folder):
        EDITOR_ASSET_LIB.make_directory(dest_folder)

    task = unreal.AssetImportTask()
    task.filename = str(fbx_path)
    task.destination_path = dest_folder
    task.destination_name = asset_name
    task.replace_existing = True
    task.automated = True
    task.save = True

    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = True
    options.import_textures = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.generate_lightmap_u_vs = True
    options.static_mesh_import_data.auto_generate_collision = True
    task.options = options

    ASSET_TOOLS.import_asset_tasks([task])
    full_path = f"{dest_folder}/{asset_name}"
    mesh = EDITOR_ASSET_LIB.load_asset(full_path)
    if mesh:
        EDITOR_ASSET_LIB.save_loaded_asset(mesh)
    return mesh


def import_texture(tex_path: Path, dest_folder: str, asset_name: str):
    if not tex_path.exists():
        return None
    if not EDITOR_ASSET_LIB.does_directory_exist(dest_folder):
        EDITOR_ASSET_LIB.make_directory(dest_folder)
    task = unreal.AssetImportTask()
    task.filename = str(tex_path)
    task.destination_path = dest_folder
    task.destination_name = asset_name
    task.replace_existing = True
    task.automated = True
    task.save = True
    ASSET_TOOLS.import_asset_tasks([task])
    tex = EDITOR_ASSET_LIB.load_asset(f"{dest_folder}/{asset_name}")
    if tex:
        EDITOR_ASSET_LIB.save_loaded_asset(tex)
    return tex


def create_or_update_equipment_bp(
    bp_folder: str,
    bp_name: str,
    parent_class_path: str,
    mesh_asset,
    mesh_comp_property: str = "device_mesh",
    icon_asset=None,
):
    if not EDITOR_ASSET_LIB.does_directory_exist(bp_folder):
        EDITOR_ASSET_LIB.make_directory(bp_folder)

    bp_path = f"{bp_folder}/{bp_name}"
    parent_class = unreal.load_class(None, parent_class_path)
    if not parent_class:
        raise RuntimeError(f"Could not load parent class: {parent_class_path}")

    bp = EDITOR_ASSET_LIB.load_asset(bp_path)
    if not bp:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        bp = ASSET_TOOLS.create_asset(bp_name, bp_folder, unreal.Blueprint, factory)

    gen_class = bp.generated_class()
    cdo = unreal.get_default_object(gen_class)

    if mesh_asset:
        mesh_comp = cdo.get_editor_property(mesh_comp_property)
        if mesh_comp:
            mesh_comp.set_static_mesh(mesh_asset)
            mesh_comp.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))
            mesh_comp.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))

    if icon_asset:
        try:
            cdo.set_editor_property("icon", icon_asset)
        except Exception:
            pass

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    EDITOR_ASSET_LIB.save_loaded_asset(bp)
    return bp


def place_in_test_map(bp_map: dict):
    map_path = "/Game/Environment/Mall/Maps/Lvl_MallOval_Test"
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if not world:
        unreal.log_warning(f"Could not load {map_path}")
        return []

    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing_actors = actor_sub.get_all_level_actors()

    placements = [
        {
            "label": "MallEMFReader_East_F0_02",
            "bp": bp_map["BP_MallEMFReader"],
            "loc": unreal.Vector(4090.0, -250.0, 1.0),
            "rot": unreal.Rotator(0.0, 180.0, 0.0),
        },
        {
            "label": "MallUVLight_East_F0_02",
            "bp": bp_map["BP_MallUVLight"],
            "loc": unreal.Vector(4090.0, -205.0, 1.0),
            "rot": unreal.Rotator(0.0, 180.0, 0.0),
        },
        {
            "label": "MallThermometer_East_F0_02",
            "bp": bp_map["BP_MallThermometer"],
            "loc": unreal.Vector(4090.0, -160.0, 1.0),
            "rot": unreal.Rotator(0.0, 180.0, 0.0),
        },
        {
            "label": "MallMotionSensor_East_F0_02",
            "bp": bp_map["BP_MallMotionSensor"],
            "loc": unreal.Vector(4090.0, 165.0, 1.0),
            "rot": unreal.Rotator(0.0, 0.0, 0.0),
        },
        {
            "label": "MallPhotoCamera_East_F0_02",
            "bp": bp_map["BP_MallPhotoCamera"],
            "loc": unreal.Vector(4090.0, 215.0, 1.0),
            "rot": unreal.Rotator(0.0, 180.0, 0.0),
        },
        {
            "label": "MallUVEvidence_Footprint_01",
            "bp": bp_map["BP_MallUVEvidence"],
            "loc": unreal.Vector(4520.0, -110.0, 0.5),
            "rot": unreal.Rotator(0.0, 25.0, 0.0),
        },
        {
            "label": "MallUVEvidence_Footprint_02",
            "bp": bp_map["BP_MallUVEvidence"],
            "loc": unreal.Vector(4760.0, 95.0, 0.5),
            "rot": unreal.Rotator(0.0, -15.0, 0.0),
        },
    ]

    placed_summary = []
    for item in placements:
        # Remove any existing instance with the same actor label to keep idempotency
        for act in list(existing_actors):
            if act and act.get_actor_label() == item["label"]:
                actor_sub.destroy_actor(act)

        spawned = actor_sub.spawn_actor_from_object(item["bp"], item["loc"], item["rot"])
        if spawned:
            spawned.set_actor_label(item["label"])
            placed_summary.append({
                "label": item["label"],
                "blueprint": item["bp"].get_path_name(),
                "location": [item["loc"].x, item["loc"].y, item["loc"].z],
            })

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    return placed_summary


def main():
    # 1. Import prepared FBX Static Meshes
    sm_emf = import_static_mesh(PREPARED_DIR / "SM_EMFReader.fbx", "/Game/Equipment/EMFReader/Meshes", "SM_EMFReader")
    sm_uv = import_static_mesh(PREPARED_DIR / "SM_UVLight.fbx", "/Game/Equipment/UVLight/Meshes", "SM_UVLight")
    sm_uv_print = import_static_mesh(PREPARED_DIR / "SM_UVFootprint.fbx", "/Game/Equipment/UVLight/Meshes", "SM_UVFootprint")
    sm_thermo = import_static_mesh(PREPARED_DIR / "SM_Thermometer.fbx", "/Game/Equipment/Thermometer/Meshes", "SM_Thermometer")
    sm_motion = import_static_mesh(PREPARED_DIR / "SM_MotionSensor.fbx", "/Game/Equipment/MotionSensor/Meshes", "SM_MotionSensor")
    sm_camera = import_static_mesh(PREPARED_DIR / "SM_PhotoCamera.fbx", "/Game/Equipment/PhotoCamera/Meshes", "SM_PhotoCamera")
    sm_spirit_cc0 = import_static_mesh(PREPARED_DIR / "SM_SpiritBox_CC0.fbx", "/Game/Equipment/SpiritBox/Meshes", "SM_SpiritBox_CC0")

    # 2. Import CC0 diffuse textures as hotbar icons & surface textures
    icon_emf = import_texture(
        CC0_DIR / "retro_multimeter" / "textures" / "retro_multimeter_diff_1k.jpg",
        "/Game/Equipment/EMFReader/Textures",
        "T_EMFReader_Diff",
    )
    icon_uv = import_texture(
        CC0_DIR / "signal_flashlight" / "textures" / "signal_flashlight_diff_1k.jpg",
        "/Game/Equipment/UVLight/Textures",
        "T_UVLight_Diff",
    )
    icon_cam = import_texture(
        CC0_DIR / "Camera_01" / "textures" / "Camera_01_body_diff_1k.jpg",
        "/Game/Equipment/PhotoCamera/Textures",
        "T_PhotoCamera_Diff",
    )
    icon_spirit = import_texture(
        CC0_DIR / "portable_cassette_player" / "textures" / "portable_cassette_player_diff_1k.jpg",
        "/Game/Equipment/SpiritBox/Textures",
        "T_SpiritBox_CC0_Diff",
    )

    # 3. Create & compile Blueprints
    bp_emf = create_or_update_equipment_bp(
        "/Game/Equipment/EMFReader",
        "BP_MallEMFReader",
        "/Script/MyProject2.MallEMFReader",
        sm_emf,
        "device_mesh",
        icon_emf,
    )
    bp_uv = create_or_update_equipment_bp(
        "/Game/Equipment/UVLight",
        "BP_MallUVLight",
        "/Script/MyProject2.MallUVLight",
        sm_uv,
        "device_mesh",
        icon_uv,
    )
    bp_uv_ev = create_or_update_equipment_bp(
        "/Game/Equipment/UVLight",
        "BP_MallUVEvidence",
        "/Script/MyProject2.MallUVEvidence",
        sm_uv_print,
        "evidence_mesh",
        None,
    )
    bp_thermo = create_or_update_equipment_bp(
        "/Game/Equipment/Thermometer",
        "BP_MallThermometer",
        "/Script/MyProject2.MallThermometer",
        sm_thermo,
        "device_mesh",
        icon_emf,
    )
    bp_motion = create_or_update_equipment_bp(
        "/Game/Equipment/MotionSensor",
        "BP_MallMotionSensor",
        "/Script/MyProject2.MallMotionSensor",
        sm_motion,
        "device_mesh",
        icon_uv,
    )
    bp_camera = create_or_update_equipment_bp(
        "/Game/Equipment/PhotoCamera",
        "BP_MallPhotoCamera",
        "/Script/MyProject2.MallPhotoCamera",
        sm_camera,
        "device_mesh",
        icon_cam,
    )

    bp_map = {
        "BP_MallEMFReader": bp_emf,
        "BP_MallUVLight": bp_uv,
        "BP_MallUVEvidence": bp_uv_ev,
        "BP_MallThermometer": bp_thermo,
        "BP_MallMotionSensor": bp_motion,
        "BP_MallPhotoCamera": bp_camera,
    }

    placed = place_in_test_map(bp_map)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    report = {
        "blueprints": [bp.get_path_name() for bp in bp_map.values() if bp],
        "meshes": [
            m.get_path_name()
            for m in (sm_emf, sm_uv, sm_uv_print, sm_thermo, sm_motion, sm_camera, sm_spirit_cc0)
            if m
        ],
        "placed_in_test_map": placed,
    }
    report_path = PROJECT_DIR / "Saved" / "Automation" / "ghost_equipment_build_report.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log(f"Ghost equipment Blueprints built & saved: {json.dumps(report)}")


if __name__ == "__main__":
    main()
