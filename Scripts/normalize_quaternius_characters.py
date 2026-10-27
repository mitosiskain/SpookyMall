import bpy
import os

os.makedirs("SourceAssets/Characters/NormalizedLowPoly", exist_ok=True)

def normalize_and_apply_armature(src_glb, dst_glb, target_height_m):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)

    bpy.ops.import_scene.gltf(filepath=src_glb)
    bpy.context.view_layer.update()

    # Remove any bone custom_shape helper meshes like Icosphere
    for o in list(bpy.data.objects):
        if o.type == "MESH" and o.name.startswith("Icosphere") and o.parent is None:
            bpy.data.objects.remove(o, do_unlink=True)

    # First apply all transforms on Armatures and Meshes so scale is (1.0, 1.0, 1.0) everywhere!
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.context.view_layer.update()

    # Measure current world-space vertex Z bounds
    min_z = 1e9
    max_z = -1e9
    for obj in bpy.data.objects:
        if obj.type == "MESH":
            for v in obj.data.vertices:
                wv = obj.matrix_world @ v.co
                min_z = min(min_z, wv.z)
                max_z = max(max_z, wv.z)

    cur_h = max(0.001, max_z - min_z)
    scale_factor = target_height_m / cur_h

    roots = [o for o in bpy.data.objects if o.parent is None]
    for r in roots:
        r.scale = (scale_factor, scale_factor, scale_factor)
    bpy.context.view_layer.update()

    # Apply the target height scale as well so exported Armature & Mesh scale is strictly (1, 1, 1)!
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.context.view_layer.update()

    # Ground feet at Z = 0
    min_z2 = 1e9
    max_z2 = -1e9
    for obj in bpy.data.objects:
        if obj.type == "MESH":
            for v in obj.data.vertices:
                wv = obj.matrix_world @ v.co
                min_z2 = min(min_z2, wv.z)
                max_z2 = max(max_z2, wv.z)

    for r in roots:
        r.location.z -= min_z2
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.context.view_layer.update()

    bpy.ops.export_scene.gltf(filepath=dst_glb, export_format='GLB', export_apply=True)
    print(f"Applied & Normalized {os.path.basename(dst_glb)} -> {max_z2 - min_z2:.3f}m (scale 1,1,1)")

normalize_and_apply_armature("SourceAssets/Characters/QuaterniusCC0/Investigator_Suit_M.glb", "SourceAssets/Characters/NormalizedLowPoly/Investigator_LowPoly.glb", 1.78)
normalize_and_apply_armature("SourceAssets/Characters/QuaterniusCC0/Clerk_Worker_M.glb", "SourceAssets/Characters/NormalizedLowPoly/Ghost_Clerk_LowPoly.glb", 1.80)
normalize_and_apply_armature("SourceAssets/Characters/QuaterniusCC0/Ghost_Formal_F.glb", "SourceAssets/Characters/NormalizedLowPoly/Ghost_Evelyn_LowPoly.glb", 1.76)
normalize_and_apply_armature("SourceAssets/Characters/QuaterniusCC0/Quartermaster_Casual_M.glb", "SourceAssets/Characters/NormalizedLowPoly/Quartermaster_Chen_LowPoly.glb", 1.78)
normalize_and_apply_armature("SourceAssets/Characters/QuaterniusCC0/Auditor_Suit_F.glb", "SourceAssets/Characters/NormalizedLowPoly/Elena_Vance_LowPoly.glb", 1.74)
normalize_and_apply_armature("SourceAssets/Characters/QuaterniusCC0/Cyberpunk_Gremlin2Legs.glb", "SourceAssets/Characters/NormalizedLowPoly/Hallway_Gremlin_LowPoly.glb", 0.95)
normalize_and_apply_armature("SourceAssets/Characters/QuaterniusCC0/Ectoplasm_Ghost.glb", "SourceAssets/Characters/NormalizedLowPoly/Player_Ectoplasm_Form.glb", 1.55)
