"""Import the prepared centimeter FBXs through Unreal's native FBX factory.

Run in the Unreal Python environment. Existing target meshes are verified and
reused, never replaced. This script does not create actors or modify a level.
"""

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_DIR = PROJECT_ROOT / "ExternalAssets" / "Escalator"
DESTINATION = "/Game/Environment/Escalator/Meshes"
BOUNDS_TOLERANCE_CM = 1.0


def vector_list(vector):
    return [float(vector.x), float(vector.y), float(vector.z)]


def normalize_asset_path(path):
    # AssetImportTask returns object paths; registry existence accepts packages.
    return str(path).split(".", 1)[0]


def verify_mesh(mesh, bounds_key, slots_key, metadata):
    if not isinstance(mesh, unreal.StaticMesh):
        raise TypeError("Expected StaticMesh at target, found %s" % type(mesh).__name__)
    box = mesh.get_bounding_box()
    actual_bounds = [vector_list(box.min), vector_list(box.max)]
    expected_bounds = metadata[bounds_key]
    for side in range(2):
        for axis in range(3):
            error = abs(actual_bounds[side][axis] - expected_bounds[side][axis])
            if error > BOUNDS_TOLERANCE_CM:
                raise RuntimeError(
                    "%s bounds differ from centimeter source on %s/%s by %.3f cm: "
                    "actual=%s expected=%s. Check scene axes and import scale."
                    % (mesh.get_path_name(), ("min", "max")[side], "XYZ"[axis],
                       error, actual_bounds, expected_bounds)
                )
    slots = []
    for slot in mesh.get_editor_property("static_materials"):
        name = str(slot.get_editor_property("material_slot_name"))
        imported_name = str(slot.get_editor_property("imported_material_slot_name"))
        material = slot.get_editor_property("material_interface")
        slots.append({
            "name": name,
            "imported_name": imported_name,
            "material": material.get_path_name() if material else None,
        })
    expected_slots = metadata[slots_key]
    actual_slots = [slot["imported_name"] for slot in slots]
    if actual_slots != expected_slots:
        raise RuntimeError(
            "%s imported material slot order changed: actual=%s expected=%s"
            % (mesh.get_path_name(), actual_slots, expected_slots)
        )
    missing = [slot["name"] for slot in slots if not slot["material"]]
    if missing:
        raise RuntimeError("%s has unassigned material slots: %s" % (mesh.get_path_name(), missing))
    return {
        "path": mesh.get_path_name(),
        "bounds_cm": actual_bounds,
        "dimensions_cm": [actual_bounds[1][i] - actual_bounds[0][i] for i in range(3)],
        "material_slots": slots,
    }


def make_task(name):
    source = SOURCE_DIR / (name + ".fbx")
    if not source.is_file():
        raise FileNotFoundError("Prepared escalator FBX missing: %s" % source)

    options = unreal.FbxImportUI()
    for key, value in {
        "automated_import_should_detect_type": False,
        "mesh_type_to_import": unreal.FBXImportType.FBXIT_STATIC_MESH,
        "import_as_skeletal": False,
        "import_mesh": True,
        "import_animations": False,
        "import_materials": True,
        "import_textures": False,
        "override_full_name": True,
    }.items():
        options.set_editor_property(key, value)

    mesh_options = options.get_editor_property("static_mesh_import_data")
    for key, value in {
        "import_uniform_scale": 1.0,
        "convert_scene": True,
        "convert_scene_unit": True,
        "force_front_x_axis": False,
        "transform_vertex_to_absolute": True,
        "bake_pivot_in_vertex": False,
        "combine_meshes": True,
        "auto_generate_collision": False,
        "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,
        "reorder_material_to_fbx_order": True,
        "build_nanite": False,
    }.items():
        mesh_options.set_editor_property(key, value)

    task = unreal.AssetImportTask()
    for key, value in {
        "filename": str(source),
        "destination_path": DESTINATION,
        "destination_name": name,
        "automated": True,
        "replace_existing": False,
        "replace_existing_settings": False,
        "save": False,
        "async_": False,
        "factory": unreal.FbxFactory(),
        "options": options,
    }.items():
        task.set_editor_property(key, value)
    return task


def import_or_verify(name, bounds_key, slots_key, metadata, assets, tools):
    path = DESTINATION + "/" + name
    reused = assets.does_asset_exist(path)
    new_outputs = []
    if not reused:
        before = (
            {normalize_asset_path(p) for p in assets.list_assets(DESTINATION, True, False)}
            if assets.does_directory_exist(DESTINATION) else set()
        )
        task = make_task(name)
        tools.import_asset_tasks([task])
        outputs = list(task.get_objects())
        # Include only newly created outputs within our isolated destination.
        for obj in outputs:
            package = normalize_asset_path(obj.get_path_name())
            if not package.startswith(DESTINATION + "/"):
                raise RuntimeError("FBX import returned an unexpected output outside destination: %s" % package)
            if package not in before:
                new_outputs.append(obj)
        if not assets.does_asset_exist(path):
            raise RuntimeError("FBX task produced no expected mesh: %s; outputs=%s" % (
                path, [obj.get_path_name() for obj in outputs]))

    mesh = assets.load_asset(path)
    result = verify_mesh(mesh, bounds_key, slots_key, metadata)
    result["status"] = "reused" if reused else "imported"

    if not reused:
        for key, value in {
            "EscalatorUnits": "centimeters",
            "EscalatorLocalAxes": "X=ascending travel; Y=cross tread; Z=up",
            "EscalatorSourceSHA256": metadata["source_sha256"],
            "EscalatorSourceLicense": "CC0-1.0",
            "EscalatorGeometryMetadata": str(SOURCE_DIR / "mesh_metadata.json"),
        }.items():
            assets.set_metadata_tag(mesh, key, value)

        # Some factory versions return only the mesh; collect its newly created
        # material dependencies explicitly so imported colors survive a restart.
        before_packages = before
        for slot in mesh.get_editor_property("static_materials"):
            material = slot.get_editor_property("material_interface")
            if material:
                package = normalize_asset_path(material.get_path_name())
                if package.startswith(DESTINATION + "/") and package not in before_packages:
                    new_outputs.append(material)
        new_outputs.append(mesh)
        unique_outputs = {obj.get_path_name(): obj for obj in new_outputs}
        # Save new materials first, then their referencing mesh, never SaveAll.
        ordered = sorted(unique_outputs.values(), key=lambda obj: isinstance(obj, unreal.StaticMesh))
        for obj in ordered:
            if not assets.save_loaded_asset(obj, only_if_is_dirty=True):
                raise RuntimeError("Unable to save escalator import output: %s" % obj.get_path_name())
        result["saved_outputs"] = [obj.get_path_name() for obj in ordered]
    print("ESCALATOR_MESH " + json.dumps(result, separators=(",", ":")))
    return result


def main():
    metadata_path = SOURCE_DIR / "mesh_metadata.json"
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    if metadata.get("units") != "centimeters":
        raise RuntimeError("Prepared source metadata is not in centimeters")
    assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    results = []
    for spec in (
        ("SM_MallEscalatorShell", "shell_bounds_cm", "shell_material_slots"),
        ("SM_MallEscalatorStep", "step_bounds_cm", "step_material_slots"),
    ):
        results.append(import_or_verify(*spec, metadata, assets, tools))
    print("ESCALATOR_IMPORT_COMPLETE " + json.dumps({"meshes": [r["path"] for r in results]}))


try:
    main()
except Exception as error:
    print("ESCALATOR_IMPORT_ERROR " + json.dumps({"type": type(error).__name__, "message": str(error)}))
    raise
