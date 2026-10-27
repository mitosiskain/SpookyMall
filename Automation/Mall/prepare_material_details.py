from pathlib import Path
import json
root=Path(r'C:\unreal\uprj\MyProject2');r=json.loads((root/'Saved/MallBuild/import_report.json').read_text());calls=[]
def c(**a):calls.append({'tool':'create_edit_material','arguments':a})
def pairs(d):return [{'name':k,'value':v} for k,v in d.items()]
for key,m in r['materials'].items():
    if 'leaves' in key:continue
    path='/Game/Generated_Materials/'+m['material_name'];t=m['unreal_textures']
    c(operation='read_graph',material_path=path)
    edits=[{'expression_name':'MaterialExpressionScalarParameter_3','properties':pairs({'DefaultValue':1.0})}]
    if 'metallic' in t:edits.append({'expression_name':'MaterialExpressionScalarParameter_1','properties':pairs({'DefaultValue':1.0})})
    c(operation='edit_expressions',material_path=path,expression_edits=edits)
    for index,(kind,scalar) in enumerate([('roughness',3)]+([('metallic',1)] if 'metallic' in t else []),2):
        node='MaterialExpressionTextureSampleParameter2D_'+str(index);target=kind.title()
        c(operation='add_texture_input',material_path=path,asset_path=t[kind],target_input=target,texture_parameter_name=target+'Map')
        c(operation='connect_expressions',material_path=path,connections=[{'from_expression':'MaterialExpressionMultiply_0','from_output':0,'to_expression':node,'to_input':'Coordinates'}])
        c(operation='add_custom_expression',material_path=path,code='return saturate(MapValue.r * Scale);',output_type='float',inputs=[{'name':'MapValue','type':'float3','auto_connect':{'expression':node}},{'name':'Scale','auto_connect':{'expression':'MaterialExpressionScalarParameter_'+str(scalar)}}],target_output=target)
    if 'cover_texture' in t:
        index=4 if 'metallic' in t else 3
        c(operation='add_expression',material_path=path,class_name='TextureSampleParameter2D',properties=pairs({'ParameterName':'CoverTexture','Texture':t['cover_texture']}))
        c(operation='add_expression',material_path=path,class_name='TextureSampleParameter2D',properties=pairs({'ParameterName':'PageMask','Texture':t['page_mask_texture'],'SamplerType':'SAMPLERTYPE_Masks'}))
        c(operation='add_expression',material_path=path,class_name='TextureCoordinate',properties=pairs({'CoordinateIndex':1}))
        c(operation='connect_expressions',material_path=path,connections=[{'from_expression':'MaterialExpressionTextureCoordinate_1','from_output':0,'to_expression':'MaterialExpressionTextureSampleParameter2D_'+str(index),'to_input':'Coordinates'}])
        c(operation='add_custom_expression',material_path=path,code='return lerp(Cover.rgb, Pages.rgb, Mask.r);',output_type='float3',inputs=[{'name':'Cover','type':'float3','auto_connect':{'expression':'MaterialExpressionTextureSampleParameter2D_'+str(index)}},{'name':'Pages','type':'float3','auto_connect':{'expression':'MaterialExpressionTextureSampleParameter2D_0'}},{'name':'Mask','type':'float3','auto_connect':{'expression':'MaterialExpressionTextureSampleParameter2D_'+str(index+1)}}],target_output='BaseColor')
    c(operation='verify_material',material_path=path,require_outputs=['BaseColor','Normal','Roughness','Metallic'])
(root/'Automation/Mall/material_details_request.json').write_text(json.dumps({'calls':calls}))
print('Prepared',len(calls),'material detail operations')
