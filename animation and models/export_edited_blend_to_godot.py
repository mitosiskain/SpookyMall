import bpy
import os
import subprocess
from mathutils import Vector, Quaternion, Matrix

PROJECT_DIR = "/run/media/mitosiskain/junk drive but it_s fast/ai"
OUT_GLB = os.path.join(PROJECT_DIR, "SourceAssets", "Characters", "MikuDoomer_Chen.glb")

arp_arm = bpy.data.objects.get("Mikudoomer_ARP")
if not arp_arm:
    raise RuntimeError("Mikudoomer_ARP not found in MikuDoomer_ReadyToEdit.blend!")

# 1. Define the clean 53-bone Godot Humanoid hierarchy mapped from Mikudoomer_ARP's internal/deform bones
CLEAN_HIERARCHY = {
    "root.x": None,
    "spine_01.x": "root.x",
    "spine_02.x": "spine_01.x",
    "spine_03.x": "spine_02.x",
    "neck.x": "spine_03.x",
    "head.x": "neck.x",
    "shoulder.l": "spine_03.x",
    "arm.l": "shoulder.l",
    "forearm.l": "arm.l",
    "hand.l": "forearm.l",
    "shoulder.r": "spine_03.x",
    "arm.r": "shoulder.r",
    "forearm.r": "arm.r",
    "hand.r": "forearm.r",
    "thigh.l": "root.x",
    "leg.l": "thigh.l",
    "foot.l": "leg.l",
    "toes_01.l": "foot.l",
    "thigh.r": "root.x",
    "leg.r": "thigh.r",
    "foot.r": "leg.r",
    "toes_01.r": "foot.r",
}
for side in [".l", ".r"]:
    for f_name in ["thumb", "index", "middle", "ring", "pinky"]:
        CLEAN_HIERARCHY[f"{f_name}1{side}"] = f"hand{side}"
        CLEAN_HIERARCHY[f"c_{f_name}2{side}"] = f"{f_name}1{side}"
        CLEAN_HIERARCHY[f"c_{f_name}3{side}"] = f"c_{f_name}2{side}"

# Complete mapping of all ARP stretch/twist/base vertex groups onto the 53 clean bones
WEIGHT_REMAP = {
    "c_head.x": "head.x",
    "thigh_stretch.l": "thigh.l",
    "thigh_stretch.r": "thigh.r",
    "thigh_twist.l": "thigh.l",
    "thigh_twist.r": "thigh.r",
    "c_thigh_b.l": "thigh.l",
    "c_thigh_b.r": "thigh.r",
    "leg_stretch.l": "leg.l",
    "leg_stretch.r": "leg.r",
    "leg_twist.l": "leg.l",
    "leg_twist.r": "leg.r",
    "arm_stretch.l": "arm.l",
    "arm_stretch.r": "arm.r",
    "arm_twist.l": "arm.l",
    "arm_twist.r": "arm.r",
    "c_arm_twist_offset.l": "arm.l",
    "c_arm_twist_offset.r": "arm.r",
    "forearm_stretch.l": "forearm.l",
    "forearm_stretch.r": "forearm.r",
    "forearm_twist.l": "forearm.l",
    "forearm_twist.r": "forearm.r",
    "c_index1_base.l": "hand.l",
    "c_index1_base.r": "hand.r",
    "c_middle1_base.l": "hand.l",
    "c_middle1_base.r": "hand.r",
    "c_ring1_base.l": "hand.l",
    "c_ring1_base.r": "hand.r",
    "c_pinky1_base.l": "hand.l",
    "c_pinky1_base.r": "hand.r",
    "c_thumb1_base.l": "hand.l",
    "c_thumb1_base.r": "hand.r",
    "c_index1.l": "index1.l",
    "c_index1.r": "index1.r",
    "c_middle1.l": "middle1.l",
    "c_middle1.r": "middle1.r",
    "c_ring1.l": "ring1.l",
    "c_ring1.r": "ring1.r",
    "c_pinky1.l": "pinky1.l",
    "c_pinky1.r": "pinky1.r",
    "c_thumb1.l": "thumb1.l",
    "c_thumb1.r": "thumb1.r",
}

# 2. Sample all Actions on Mikudoomer_ARP (any action with c_* controller fcurves, including ARP _remap actions!)
def is_arp_action(act):
    if hasattr(act, "fcurves"):
        return any('pose.bones["c_' in fc.data_path for fc in act.fcurves)
    if hasattr(act, "layers"):
        for layer in act.layers:
            for strip in layer.strips:
                if hasattr(strip, "channelbags"):
                    for bag in strip.channelbags:
                        if any('pose.bones["c_' in fc.data_path for fc in bag.fcurves):
                            return True
    return False

arp_actions = [a for a in bpy.data.actions if len(a.slots) > 0 and is_arp_action(a)]
sampled_actions = {}

if not arp_arm.animation_data:
    arp_arm.animation_data_create()

# Record rest matrices of the 53 bones from Mikudoomer_ARP
rest_data = {}
for bname in CLEAN_HIERARCHY.keys():
    b = arp_arm.data.bones.get(bname)
    if b:
        rest_data[bname] = {
            "head": (arp_arm.matrix_world @ b.matrix_local).to_translation(),
            "tail": arp_arm.matrix_world @ b.tail_local,
            "roll": b.AxisRollFromMatrix((arp_arm.matrix_world @ b.matrix_local).to_3x3())[1],
            "mat_world": arp_arm.matrix_world @ b.matrix_local,
        }

for act in arp_actions:
    arp_arm.animation_data.action = act
    if hasattr(arp_arm.animation_data, "action_slot") and len(act.slots) > 0:
        arp_arm.animation_data.action_slot = act.slots[0]

    f_start = int(round(act.frame_range[0]))
    f_end = int(round(act.frame_range[1]))
    frames_dict = {}
    for f in range(f_start, f_end + 1):
        bpy.context.scene.frame_set(f)
        bpy.context.view_layer.update()
        frame_poses = {}
        for bname in CLEAN_HIERARCHY.keys():
            pb = arp_arm.pose.bones.get(bname)
            if pb:
                frame_poses[bname] = (arp_arm.matrix_world @ pb.matrix).copy()
        frames_dict[f] = frame_poses
    sampled_actions[act.name] = (f_start, f_end, frames_dict)
    print(f"Sampled ARP action '{act.name}' ({f_start}..{f_end})")

# Reset ARP armature to REST pose before transferring meshes so no posed deformation is ever baked
arp_arm.data.pose_position = 'REST'
bpy.context.view_layer.update()

# 3. Build the clean 53-bone Godot Armature in memory (without saving over MikuDoomer_ReadyToEdit.blend!)
clean_arm_data = bpy.data.armatures.new("ArmatureData")
clean_arm_obj = bpy.data.objects.new("Armature", clean_arm_data)
bpy.context.scene.collection.objects.link(clean_arm_obj)

if bpy.context.mode != 'OBJECT':
    bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.object.select_all(action='DESELECT')
clean_arm_obj.select_set(True)
bpy.context.view_layer.objects.active = clean_arm_obj
bpy.ops.object.mode_set(mode='EDIT')

for bname, info in rest_data.items():
    eb = clean_arm_data.edit_bones.new(bname)
    eb.head = info["head"]
    eb.tail = info["tail"]
    eb.roll = info["roll"]
    eb.use_deform = True

for bname, parent_name in CLEAN_HIERARCHY.items():
    if parent_name and bname in clean_arm_data.edit_bones and parent_name in clean_arm_data.edit_bones:
        clean_arm_data.edit_bones[bname].parent = clean_arm_data.edit_bones[parent_name]
        clean_arm_data.edit_bones[bname].use_connect = False

bpy.ops.object.mode_set(mode='OBJECT')

# 4. Transfer character meshes to clean_arm_obj and merge stretch/twist vertex groups
char_meshes = [obj for obj in bpy.data.objects if obj.type == 'MESH' and obj.parent == arp_arm]
for mesh_obj in char_meshes:
    vgroups = {vg.name: vg.index for vg in mesh_obj.vertex_groups}
    for old_vg, new_vg in WEIGHT_REMAP.items():
        if old_vg in vgroups:
            old_idx = vgroups[old_vg]
            if new_vg not in vgroups:
                vg_new = mesh_obj.vertex_groups.new(name=new_vg)
                vgroups[new_vg] = vg_new.index
            else:
                vg_new = mesh_obj.vertex_groups[new_vg]
            for v in mesh_obj.data.vertices:
                for g in v.groups:
                    if g.group == old_idx and g.weight > 0.0:
                        vg_new.add([v.index], g.weight, 'ADD')
            mesh_obj.vertex_groups.remove(mesh_obj.vertex_groups[old_vg])
            vgroups = {vg.name: vg.index for vg in mesh_obj.vertex_groups}

    # Remove any empty vertex groups not in CLEAN_HIERARCHY
    for vg in list(mesh_obj.vertex_groups):
        if vg.name not in CLEAN_HIERARCHY:
            mesh_obj.vertex_groups.remove(vg)

    world_mat = mesh_obj.matrix_world.copy()
    mesh_obj.parent = clean_arm_obj
    mesh_obj.matrix_parent_inverse = clean_arm_obj.matrix_world.inverted()
    mesh_obj.matrix_world = world_mat
    for mod in mesh_obj.modifiers:
        if mod.type == 'ARMATURE':
            mod.object = clean_arm_obj

# Remove ARP rig & custom shapes from memory before exporting GLB
for obj in list(bpy.data.objects):
    if obj != clean_arm_obj and obj not in char_meshes:
        bpy.data.objects.remove(obj, do_unlink=True)
for act in list(bpy.data.actions):
    bpy.data.actions.remove(act)

clean_arm_obj.name = "Armature"

# 5. Bake sampled world poses onto clean_arm_obj Actions
clean_arm_obj.animation_data_create()
clean_rest_R = {b.name: b.matrix_local.to_3x3() for b in clean_arm_obj.data.bones}
clean_rest_pos = {b.name: b.matrix_local.to_translation() for b in clean_arm_obj.data.bones}

for pb in clean_arm_obj.pose.bones:
    pb.rotation_mode = 'QUATERNION'

for act_name, (f_start, f_end, frames_dict) in sampled_actions.items():
    dst_act = bpy.data.actions.new(name=act_name)
    dst_act.use_fake_user = True
    clean_arm_obj.animation_data.action = dst_act

    for f in range(f_start, f_end + 1):
        frame_poses = frames_dict[f]
        dR_world = {}
        for bname, mat_w in frame_poses.items():
            dR_world[bname] = mat_w.to_3x3() @ clean_rest_R[bname].inverted()

        for bname, parent_name in CLEAN_HIERARCHY.items():
            if bname not in dR_world:
                continue
            pb = clean_arm_obj.pose.bones[bname]
            R_rest = clean_rest_R[bname]
            dR_self = dR_world[bname]
            dR_par = dR_world.get(parent_name, Matrix.Identity(3)) if parent_name else Matrix.Identity(3)
            pb.rotation_quaternion = (R_rest.inverted() @ dR_par.inverted() @ dR_self @ R_rest).to_quaternion()
            pb.keyframe_insert(data_path="rotation_quaternion", frame=f)

            if bname == "root.x":
                delta_pos = frame_poses["root.x"].to_translation() - clean_rest_pos["root.x"]
                pb.location = R_rest.inverted() @ delta_pos
                pb.keyframe_insert(data_path="location", frame=f)

    # Push to NLA track so glTF exporter includes every action
    track = clean_arm_obj.animation_data.nla_tracks.new()
    track.name = act_name
    strip = track.strips.new(act_name, f_start, dst_act)
    strip.name = act_name
    track.mute = False

if "Custom_Chen_Idle" in bpy.data.actions:
    act_idle = bpy.data.actions["Custom_Chen_Idle"]
    clean_arm_obj.animation_data.action = act_idle
    if hasattr(clean_arm_obj.animation_data, "action_slot") and len(act_idle.slots) > 0:
        clean_arm_obj.animation_data.action_slot = act_idle.slots[0]

# 6. Export clean game GLB (with export_apply=False so rest mesh is never baked in pose!) and reimport into Godot
os.makedirs(os.path.dirname(OUT_GLB), exist_ok=True)
bpy.ops.export_scene.gltf(
    filepath=OUT_GLB,
    export_format='GLB',
    use_selection=False,
    export_apply=False,
    export_animations=True,
    export_animation_mode='ACTIONS',
    export_nla_strips=True,
    export_def_bones=True,
    export_skins=True,
    export_materials='EXPORT',
)
print("Successfully exported edited MikuDoomer_Chen.glb to:", OUT_GLB)

subprocess.run(["godot", "--headless", "--path", PROJECT_DIR, "--import"], check=False)
print("Godot reimport complete!")
