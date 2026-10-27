import bpy
import json
path=r"C:\unreal\uprj\MyProject2\ExternalAssets\Mall\PolyHaven\decorative_book_set_01\decorative_book_set_01_2k.blend"
bpy.ops.wm.open_mainfile(filepath=path, load_ui=False, use_scripts=False)
obj=bpy.data.objects['book_hardcover_01_cover02']
print('UV', [uv.name for uv in obj.data.uv_layers])
mat=obj.data.materials[0]
print('NODES',json.dumps([{'name':n.name,'type':n.type,'image':n.image.filepath if n.type=='TEX_IMAGE' and n.image else None, 'uv_map':n.uv_map if n.type=='UVMAP' else None} for n in mat.node_tree.nodes]))
print('LINKS',json.dumps([(x.from_node.name,x.from_socket.name,x.to_node.name,x.to_socket.name) for x in mat.node_tree.links]))
group=mat.node_tree.nodes['Book Cover Tear'].node_tree
print('GROUPNODES', json.dumps([{'name':n.name,'type':n.type,'operation':n.operation if n.type in ('MATH','VECT_MATH') else None,'inputs':[(s.name,str(s.default_value)) for s in n.inputs if hasattr(s,'default_value')]} for n in group.nodes]))
print('GROUPLINKS',json.dumps([(x.from_node.name,x.from_socket.name,x.to_node.name,x.to_socket.name) for x in group.links]))
