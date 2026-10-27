from pathlib import Path
import json
root=Path(r'C:\unreal\uprj\MyProject2')
report=json.loads((root/'Saved/MallBuild/import_report.json').read_text())
retail=json.loads((root/'ExternalAssets/Mall/RetailKit/ImportReady/import_plan.json').read_text())
calls=[]
def call(**args): calls.append({'tool':'create_edit_material','arguments':args})
def pairs(d): return [{'name':k,'value':v} for k,v in d.items()]
leaf='/Game/Generated_Materials/M_Mall_pachira_aquatica_01_leaves'
call(operation='edit_expressions',material_path=leaf,expression_edits=[{'expression_name':'MaterialExpressionSubstrateShadingModels_0','properties':pairs({'ShadingModelOverride':'MSM_TwoSidedFoliage'})}])
call(operation='verify_material',material_path=leaf,require_outputs=['BaseColor','Normal','Roughness','OpacityMask'])
call(operation='verify_material',material_path='/Game/Generated_Materials/M_MallFloorMarble',require_outputs=['BaseColor','Normal','Roughness','Metallic'])
for key,r in report['materials'].items():
    name=r['material_name']; path='/Game/Generated_Materials/'+name; tex=r['unreal_textures']
    if key=='pachira_aquatica_01_leaves':continue
    if key=='potted_plant_01_leaves':
        call(operation='get_save_path',asset_path=name)
        call(operation='create_instance',asset_path=path,parent_material_path=leaf,parameter_overrides={'textures':{'FoliageTexture':tex['base_color'],'LeafNormal':tex['normal'],'LeafAlphaMask':tex['opacity_mask']}})
        continue
    if key!='pachira_aquatica_01_bark':
        call(operation='get_save_path',asset_path=name)
        p={'texture_path':tex['base_color'],'normal_path':tex['normal'],'metallic':r.get('metallic_constant',0),'roughness':0.65}
        call(operation='create_from_template',asset_path=path,template_name='pbr_standard',parameters=pairs(p))
    call(operation='verify_material',material_path=path,require_outputs=['BaseColor','Normal','Roughness','Metallic'])
for r in retail['materials']:
    name='M_MallRetail_'+r['name'];path='/Game/Generated_Materials/'+name
    call(operation='get_save_path',asset_path=name)
    if r['name']=='glass':
        p={'tint_color':[0.91,0.97,1,1],'opacity':0.15,'roughness':0.07,'metallic':0,'refraction':1.02}
        call(operation='create_from_template',asset_path=path,template_name='glass',parameters=pairs(p))
        call(operation='verify_material',material_path=path,require_outputs=['BaseColor','Opacity','Roughness'])
    else:
        p={'base_color':r['base_color_linear_rgba'],'roughness':r['roughness'],'metallic':r['metallic'],'emissive_color':r['emissive_linear_rgb']+[1],'emissive_strength':1}
        call(operation='create_from_template',asset_path=path,template_name='pbr_standard',parameters=pairs(p))
        call(operation='verify_material',material_path=path,require_outputs=['BaseColor','Roughness','Metallic'])
(root/'Automation/Mall/material_batch_request.json').write_text(json.dumps({'calls':calls}))
print('Prepared',len(calls),'material operations')
