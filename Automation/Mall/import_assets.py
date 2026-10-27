"""Import prepared mall geometry and maps; preserve existing project assets.

Called through Aura's Python tool. Material graphs are built separately through
its material tools. Geometry/settings were checked offline before this import.
"""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / 'ExternalAssets' / 'Mall'
DEST = '/Game/Environment/Mall'

def main():
    ph = json.loads((SOURCE/'PolyHaven'/'ImportReady'/'import_plan.json').read_text())
    retail = json.loads((SOURCE/'RetailKit'/'ImportReady'/'import_plan.json').read_text())
    surfaces = json.loads((SOURCE/'Surfaces'/'material_recipes.json').read_text())
    geometry = []
    for mesh in ph['meshes']:
        geometry.append(dict(name=mesh['mesh_name'], file=mesh['fbx_path'], folder=DEST+'/Models/PolyHaven', bounds=mesh['bounds_cm'], slots=mesh['material_slots'], foliage='Plant' in mesh['mesh_name'] or 'Pachira' in mesh['mesh_name']))
    for mesh in retail['assets']:
        geometry.append(dict(name=mesh['ue_name'], file=mesh['fbx_path'], folder=DEST+'/Models/Retail', bounds=mesh['bounds_cm'], slots=mesh['original_material_slots'], foliage=False))
    escalator = json.loads((SOURCE/'Escalator450'/'mesh_metadata.json').read_text())
    geometry.append(dict(name='SM_MallEscalatorShell_450',file=str(SOURCE/'Escalator450'/'SM_MallEscalatorShell_450.fbx'),folder=DEST+'/Models/Architecture',bounds=escalator['shell_bounds_cm'],slots=['metal','glass','dark','safety_yellow'],foliage=False))
    for name in ['SM_MallBalconyModule_400','SM_MallBalconyStraight_400']:
        path=SOURCE/'UserImports'/'Prepared'/(name+'.fbx')
        if path.exists():
            geometry.append(dict(name=name,file=str(path),folder=DEST+'/Models/Architecture',bounds=None,slots=None,foliage=False))
    # All sources must exist before an import can mutate the project.
    absent=[g['file'] for g in geometry if not Path(g['file']).is_file()]
    if absent:
        raise FileNotFoundError(str(absent))
    editor=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    imports=[]
    warnings=[]
    for g in geometry:
        path=g['folder']+'/'+g['name']
        mesh=unreal.load_asset(path) if editor.does_asset_exist(path) else None
        if not mesh:
            options=unreal.FbxImportUI()
            for key,value in {'automated_import_should_detect_type':False,'mesh_type_to_import':unreal.FBXImportType.FBXIT_STATIC_MESH,'import_as_skeletal':False,'import_mesh':True,'import_animations':False,'import_materials':False,'import_textures':False,'override_full_name':True}.items():
                options.set_editor_property(key,value)
            data=options.get_editor_property('static_mesh_import_data')
            for key,value in {'import_uniform_scale':1.0,'convert_scene':True,'convert_scene_unit':True,'force_front_x_axis':False,'transform_vertex_to_absolute':True,'bake_pivot_in_vertex':False,'combine_meshes':True,'auto_generate_collision':False,'normal_import_method':unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,'reorder_material_to_fbx_order':True,'build_nanite':False}.items():
                data.set_editor_property(key,value)
            task=unreal.AssetImportTask()
            for key,value in {'filename':g['file'],'destination_path':g['folder'],'destination_name':g['name'],'automated':True,'replace_existing':False,'save':False,'async_':False,'factory':unreal.FbxFactory(),'options':options}.items():
                task.set_editor_property(key,value)
            tools.import_asset_tasks([task])
            mesh=unreal.load_asset(path)
        if not mesh:
            warnings.append('No mesh produced: '+path)
            continue
        box=mesh.get_bounding_box()
        actual=[[box.min.x,box.min.y,box.min.z],[box.max.x,box.max.y,box.max.z]]
        if g['bounds'] and any(abs(actual[i][j]-g['bounds'][i][j])>1.0 for i in range(2) for j in range(3)):
            warnings.append('Bounds require review: '+path)
        slots=[str(s.get_editor_property('imported_material_slot_name')) for s in mesh.get_editor_property('static_materials')]
        if g['slots'] and slots!=g['slots']:
            warnings.append('Material order requires review: '+path)
        if not g['foliage'] and 'Escalator' not in g['name']:
            body=mesh.get_editor_property('body_setup')
            if body:
                body.modify()
                body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
                # Refresh cooked triangle collision; the body flag alone can leave
                # an imported mesh without physics support until its next rebuild.
                mesh_editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
                for section in range(mesh.get_num_sections(0)):
                    mesh_editor.enable_section_collision(mesh,True,0,section)
        if not editor.save_loaded_asset(mesh,False):
            warnings.append('Mesh save returned false: '+path)
        imports.append(dict(path=path,bounds_cm=actual,slots=slots,foliage=g['foliage']))
    recipes={name:dict(recipe,material_name='M_Mall_'+name) for name,recipe in ph['materials'].items()}
    for recipe in surfaces['materials']:
        recipes[recipe['material_name']]=recipe
    texture_records={}
    for name,recipe in recipes.items():
        maps=dict(recipe['textures'])
        for key in ['cover_texture','page_mask_texture']:
            if recipe.get(key): maps[key]=recipe[key]
        recipe['unreal_textures']={}
        for usage,file in maps.items():
            filename=Path(file)
            path=DEST+'/Textures/'+filename.stem
            texture=unreal.load_asset(path) if editor.does_asset_exist(path) else None
            if not texture:
                task=unreal.AssetImportTask()
                for key,value in {'filename':str(filename),'destination_path':DEST+'/Textures','destination_name':filename.stem,'automated':True,'replace_existing':False,'save':False,'factory':unreal.TextureFactory()}.items(): task.set_editor_property(key,value)
                tools.import_asset_tasks([task])
                texture=unreal.load_asset(path)
            if not texture:
                warnings.append('No texture produced: '+path)
                continue
            texture.modify()
            texture.set_editor_property('srgb',usage in ['base_color','cover_texture'])
            if usage=='normal':
                texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
                texture.set_editor_property('flip_green_channel',recipe.get('normal_flip_green',False))
            elif usage not in ['base_color','cover_texture']:
                texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
            if not editor.save_loaded_asset(texture,False): warnings.append('Texture save returned false: '+path)
            recipe['unreal_textures'][usage]=path
            texture_records[path]=usage
    report=dict(meshes=imports,materials=recipes,texture_count=len(texture_records),warnings=warnings)
    output=ROOT/'Saved'/'MallBuild'/'import_report.json'
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log(json.dumps({'meshes':len(imports),'textures':len(texture_records),'warnings':warnings,'report':str(output)}))

if __name__=='__main__':
    main()
