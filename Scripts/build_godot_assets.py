import bpy
import math
import os

def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)

def make_mat(name, color, metallic=0.2, roughness=0.5, emission=None, emission_strength=0.0):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
        if emission and "Emission Color" in bsdf.inputs:
            bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
            bsdf.inputs["Emission Strength"].default_value = emission_strength
    return mat

def add_box(name, size, loc, mat, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc, rotation=rot)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)
    return obj

def add_cyl(name, radius, depth, loc, mat, rot=(0, 0, 0), vertices=16):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc, rotation=rot)
    obj = bpy.context.active_object
    obj.name = name
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    obj.data.materials.append(mat)
    return obj

def export_glb(filepath):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=filepath, export_format='GLB', export_apply=True)
    print("Exported:", filepath)

# 1. SM_ProtonRayGun_T1.glb (Parody Proton Ray Gun Wand)
clear_scene()
mat_gun_body = make_mat("M_ProtonBody", (0.22, 0.24, 0.27), metallic=0.7, roughness=0.35)
mat_copper = make_mat("M_ProtonCopper", (0.85, 0.48, 0.20), metallic=0.85, roughness=0.25)
mat_glow = make_mat("M_ProtonGlow", (0.2, 0.9, 1.0), metallic=0.1, roughness=0.2, emission=(0.2, 0.9, 1.0), emission_strength=4.0)
mat_grip = make_mat("M_ProtonGrip", (0.12, 0.12, 0.14), metallic=0.1, roughness=0.8)

add_box("Receiver", (0.12, 0.36, 0.10), (0, 0, 0), mat_gun_body)
add_cyl("Barrel", 0.032, 0.34, (0, -0.32, 0.01), mat_copper, rot=(math.radians(90), 0, 0))
add_cyl("MuzzleRing", 0.045, 0.04, (0, -0.48, 0.01), mat_glow, rot=(math.radians(90), 0, 0))
add_box("RearGrip", (0.045, 0.06, 0.14), (0, 0.12, -0.09), mat_grip, rot=(math.radians(18), 0, 0))
add_box("VentCore", (0.13, 0.14, 0.05), (0, -0.04, 0.04), mat_glow)
export_glb("SourceAssets/Equipment/Prepared/SM_ProtonRayGun_T1.glb")

# 2. SM_EctoplasmCanister.glb (Deployable Ghost Trap Canister)
clear_scene()
mat_hazard = make_mat("M_CanisterBody", (0.88, 0.72, 0.12), metallic=0.6, roughness=0.4)
mat_dark = make_mat("M_CanisterDark", (0.15, 0.16, 0.18), metallic=0.75, roughness=0.35)
mat_ecto = make_mat("M_EctoTube", (0.25, 1.0, 0.55), metallic=0.1, roughness=0.15, emission=(0.25, 1.0, 0.55), emission_strength=3.5)

add_box("TrapBase", (0.26, 0.38, 0.14), (0, 0, 0.07), mat_dark)
add_box("HazardTrim", (0.27, 0.34, 0.04), (0, 0, 0.12), mat_hazard)
add_box("DoorLeft", (0.11, 0.30, 0.015), (-0.06, 0, 0.145), mat_dark)
add_box("DoorRight", (0.11, 0.30, 0.015), (0.06, 0, 0.145), mat_dark)
add_cyl("EctoVial", 0.035, 0.26, (0.14, 0, 0.09), mat_ecto, rot=(math.radians(90), 0, 0))
export_glb("SourceAssets/Equipment/Prepared/SM_EctoplasmCanister.glb")

# 3. SM_HeavyHydraulicAssembly.glb (Floor 3 Two-Handed Heavy Carry Part)
clear_scene()
mat_ind = make_mat("M_HydraulicYellow", (0.92, 0.58, 0.10), metallic=0.65, roughness=0.4)
mat_steel = make_mat("M_HydraulicSteel", (0.45, 0.48, 0.52), metallic=0.85, roughness=0.25)
add_box("Frame", (0.48, 0.36, 0.32), (0, 0, 0.16), mat_ind)
add_cyl("PistonA", 0.07, 0.44, (-0.12, 0, 0.26), mat_steel, rot=(0, math.radians(90), 0))
add_cyl("PistonB", 0.07, 0.44, (0.12, 0, 0.26), mat_steel, rot=(0, math.radians(90), 0))
export_glb("SourceAssets/Equipment/Prepared/SM_HeavyHydraulicAssembly.glb")
