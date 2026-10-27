"""Curated CC0 retail GLB models to centimeter-space UE FBX assets, offline only."""
import bpy, json, struct, hashlib, re, math
from pathlib import Path
from mathutils import Vector

PROJECT=Path(r'C:\unreal\uprj\MyProject2')
ROOT=PROJECT/'ExternalAssets/Mall/RetailKit'
OUT=ROOT/'ImportReady'
OUT.mkdir(parents=True,exist_ok=True)
manifest=json.loads((ROOT/'pack_manifest.json').read_text(encoding='utf-8-sig'))['data']
catalog={str(a['id']):a for a in manifest['assets']}
selected={
    '25356':'MallUnitShellModule','25357':'MallUnitShellPier','25358':'MallUnitShellCorner',
    '25340':'ShopfrontGlazedModule','25341':'ShopfrontPier','25343':'ShopfrontOpenEntrance',
    '25355':'ShopfrontCornerPost','25345':'RollerShutterOpen','25344':'RollerShutterClosed',
    '25369':'LargeAtriumPlanter','25370':'AtriumPlanterTrough',
    '25366':'ConcourseBench','25368':'CurvedConcourseBench','25378':'ConcoursePlanterBench',
    '25310':'CashDesk120','25317':'CashDeskBackFixture',
    '25162':'Slatwall120x240','25166':'SlatwallShelfBracket',
    '25183':'PerimeterWallBay240','25181':'PerimeterWallBayHangingRails',
    '25184':'PerimeterWallBayShelfAndRail','25193':'PerimeterWallBayShoeShelves',
    '25197':'PerimeterWallBayCorner','25242':'MerchandiseGondola',
    '25244':'GondolaEndcap','25208':'StraightHangingRail',
    '25210':'RoundHangingRail','25354':'CeilingTrackLightingBar',
}

def read_glb(path):
    raw=path.read_bytes()
    magic,version,total=struct.unpack_from('<4sII',raw,0)
    assert magic==b'glTF' and version==2 and total==len(raw)
    size,kind=struct.unpack_from('<I4s',raw,12)
    assert kind==b'JSON'
    return json.loads(raw[20:20+size].decode('utf-8'))

def reset():
    for obj in list(bpy.data.objects): bpy.data.objects.remove(obj,do_unlink=True)
    for mesh in list(bpy.data.meshes):
        if mesh.users==0: bpy.data.meshes.remove(mesh)
    for mat in list(bpy.data.materials):
        if mat.users==0: bpy.data.materials.remove(mat)
    bpy.context.scene.unit_settings.system='METRIC'
    bpy.context.scene.unit_settings.scale_length=1.0

def bounds(obj):
    points=[obj.matrix_world@v.co for v in obj.data.vertices]
    return Vector([min(p[i] for p in points) for i in range(3)]),Vector([max(p[i] for p in points) for i in range(3)])

rows=[]
gallery={}
all_materials={}
for source_id,suffix in selected.items():
    reset()
    asset=catalog[source_id]
    src=ROOT/'GLB'/f'{source_id}.glb'
    gltf=read_glb(src)
    materials=[]
    for m in gltf.get('materials',[]):
        p=m.get('pbrMetallicRoughness',{})
        spec=m.get('extensions',{}).get('KHR_materials_transmission',{})
        row={'name':m.get('name','Material'),
            'base_color_linear_rgba':p.get('baseColorFactor',[1,1,1,1]),
            'metallic':p.get('metallicFactor',1),'roughness':p.get('roughnessFactor',1),
            'alpha_mode':m.get('alphaMode','OPAQUE'),'alpha_cutoff':m.get('alphaCutoff',0.5),
            'two_sided':m.get('doubleSided',False),'emissive_linear_rgb':m.get('emissiveFactor',[0,0,0]),
            'transmission':spec.get('transmissionFactor',0),'has_textures':any('Texture' in k for k in p)}
        materials.append(row)
        all_materials.setdefault(row['name'],row)
    assert len(gltf.get('textures',[]))==0,source_id
    bpy.ops.import_scene.gltf(filepath=str(src))
    bpy.context.scene.frame_set(0)
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and len(o.data.vertices)>0]
    assert meshes,source_id
    points=[(o.matrix_world@v.co)*100.0 for o in meshes for v in o.data.vertices]
    low=Vector([min(p[i] for p in points) for i in range(3)])
    high=Vector([max(p[i] for p in points) for i in range(3)])
    pivot=Vector(((low.x+high.x)*0.5,(low.y+high.y)*0.5,low.z))
    for obj in meshes:
        matrix=obj.matrix_world.copy()
        obj.animation_data_clear()
        for v in obj.data.vertices: v.co=(matrix@v.co)*100.0-pivot
        obj.parent=None
        obj.matrix_world.identity()
    for obj in list(bpy.context.scene.objects):
        if obj not in meshes: bpy.data.objects.remove(obj,do_unlink=True)
    bpy.ops.object.select_all(action='DESELECT')
    for obj in meshes:obj.select_set(True)
    bpy.context.view_layer.objects.active=meshes[0]
    bpy.ops.object.join()
    obj=bpy.context.object
    obj.name=f'SM_Retail_{source_id}_{suffix}'
    bpy.context.scene.cursor.location=(0,0,0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.context.scene.unit_settings.system='METRIC'
    bpy.context.scene.unit_settings.scale_length=0.01
    dst=OUT/(obj.name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(dst),use_selection=True,object_types={'MESH'},
        global_scale=1.0,apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL',
        axis_forward='-Y',axis_up='Z',bake_space_transform=False,use_mesh_modifiers=True,
        mesh_smooth_type='FACE',use_tspace=False,add_leaf_bones=False,bake_anim=False)
    baked_low,baked_high=bounds(obj)
    expected=asset['stats']['sizeMeters']
    expected_cm=Vector((expected[0]*100,expected[2]*100,expected[1]*100))
    difference=(baked_high-baked_low)-expected_cm
    assert max(abs(v) for v in difference)<1.0,(source_id,list(difference))
    slots=[m.name for m in obj.data.materials]
    row={'source_id':source_id,'title':asset['title'],'ue_name':obj.name,
        'ue_folder':'/Game/Mall/Assets/Retail','fbx_path':str(dst),
        'dimensions_cm':list(baked_high-baked_low),'bounds_cm':[list(baked_low),list(baked_high)],
        'pivot':'horizontal bounding-box center at lowest mesh point',
        'source_origin_to_baked_cm':list(-pivot),'source_module_front_plane_local_y_cm':-pivot.y,
        'local_axes':{'X':'module width','Y':'depth; customer front toward -Y','Z':'up'},
        'source_glTF_axes':{'X':'width','Y':'up','Z':'customer front'},
        'original_material_slots':slots,'materials':materials,
        'source_url':asset['url'],'download_url':asset['cdnUrl'],'license':asset['license'],
        'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),
        'source_animation_clips':[a.get('name','Unnamed') for a in gltf.get('animations',[])],
        'animation_note':'static frame0 baked; source GLB preserves original clips for later interactive conversion',
        'source_description':asset['description'],'triangles':sum(len(p.vertices)-2 for p in obj.data.polygons),
        'vertices':len(obj.data.vertices)}
    rows.append(row)
    if source_id in ['25356','25358','25340','25343','25369','25366','25310','25345']:
        gallery[source_id]=str(dst)
    print('RETAIL_READY',source_id,obj.name,list(baked_high-baked_low))

plan={'asset_count':len(rows),'source_pack':manifest['url'],'license':manifest['licenseLabel'],
    'intended_use':'central atrium structure, public seating, planters and enterable placeholder retail interiors',
    'units':'centimeters','fbx_import_settings':{'import_uniform_scale':1.0,'convert_scene':True,
        'convert_scene_unit':True,'force_front_x_axis':False,'combine_meshes':True,
        'normal_import_method':'import normals','auto_generate_collision':False,
        'import_materials':False,'import_textures':False},
    'collision_note':'Use box collisions or per-poly static shell collision after checking entrance clearance. No baked collision in source.',
    'materials':list(all_materials.values()),'assets':rows}
(OUT/'import_plan.json').write_text(json.dumps(plan,indent=2),encoding='utf-8')
(OUT/'LICENSE_AND_SOURCE.txt').write_text('Retail Store Fixtures and Mall\n'+manifest['url']+'\n'
    +'All selected assets: CC0 1.0 Universal; https://creativecommons.org/publicdomain/zero/1.0/\n'
    +'Downloaded/prepared: 2026-10-10. AI-authored source meshes; human geometry review recommended.\n'
    +'Original GLBs and full pack manifest retained one directory above.\n'
    +'Each asset source URL, license, hash, dimensions and PBR material factors are recorded in import_plan.json.\n'
    +'Derivatives bake original rigid transforms into centimeters and combine mesh sections; UVs/material slots preserved.\n'
    +'Original animations remain in GLBs. FBXs are static meshes at source frame0.\n',encoding='utf-8')

reset()
gallery_order=['25369','25366','25310','25345','25356','25358','25340','25343']
for i,source_id in enumerate(gallery_order):
    fbx_path=gallery[source_id]
    previous=set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=fbx_path)
    imported=[obj for obj in bpy.data.objects if obj not in previous and obj.type=='MESH']
    assert len(imported)==1
    obj=imported[0]
    matrix=obj.matrix_world.copy()
    for vertex in obj.data.vertices: vertex.co=(matrix@vertex.co)*100.0
    obj.matrix_world.identity()
    obj.location=((i%4)*430,(i//4)*650-300,0)
target=Vector((630,200,145))
for position,power,size in [((300,-400,1100),25000000,900),((1100,850,700),19000000,750)]:
    bpy.ops.object.light_add(type='AREA',location=position)
    light=bpy.context.object
    light.data.energy=power
    light.data.size=size
    light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(-900,-1800,1250))
cam=bpy.context.object
cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
cam.data.type='ORTHO'
cam.data.ortho_scale=1970
cam.data.clip_start=1
cam.data.clip_end=10000
scene=bpy.context.scene
scene.camera=cam
scene.render.engine='CYCLES'
scene.cycles.samples=16
scene.cycles.use_denoising=True
scene.render.resolution_x=1600
scene.render.resolution_y=1100
scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.render.filepath=str(OUT/'retail_gallery.png')
scene.world.color=(0.35,0.35,0.35)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'RetailGallery.blend'))
bpy.ops.render.render(write_still=True)
print('RETAIL_IMPORT_PLAN_READY',str(OUT/'import_plan.json'),len(rows))
