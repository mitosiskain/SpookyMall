"""Assemble measured asset context and the user's brief for Aura level design."""
from pathlib import Path
import json,base64

root=Path(r'C:\unreal\uprj\MyProject2')
imports=json.loads((root/'Saved/MallBuild/import_report.json').read_text())
retail=json.loads((root/'ExternalAssets/Mall/RetailKit/ImportReady/import_plan.json').read_text())
refs=Path(r'D:\Users\mitos\Downloads\refs')
images=[]
for name in ['ChatGPT Image Oct 10, 2026, 01_56_37 PM.jpg','ChatGPT Image Oct 10, 2026, 01_56_12 PM.jpg','ChatGPT Image Oct 10, 2026, 01_55_52 PM.jpg']:
    images.append('data:image/jpeg;base64,'+base64.b64encode((refs/name).read_bytes()).decode())
material_result=root/'Automation/Mall/material_details_result.json'
bindings={k:'/Game/Generated_Materials/'+v['material_name'] for k,v in imports['materials'].items()}
bindings.update({k:'/Game/Generated_Materials/M_MallRetail_'+k for k in ['floor','carcass','dark','metal','glass','surface','accent']})
bindings['floor']='/Game/Generated_Materials/M_MallFloorMarble'
bindings.update({'Stone':'/Game/Generated_Materials/M_MallFloorMarble','Stainless_steel':'/Game/Generated_Materials/M_MallRetail_metal','Advanced_Procedural_Glass_for_Cycles':'/Game/Generated_Materials/M_MallRetail_glass'})
request={
 'tool':'level_design_agent',
 'arguments':{
  'request_text':"Build a separate empty mall test map for future PIE tests, with a ground floor and second-floor central atrium/gallery so the user can test the scale. Use 4m clear room height and0.5m between floors reserved for structure/HVAC, giving4.5m floor-to-floor spacing. Do not model HVAC internals. Follow the user's shopping-mall references; the central atrium should be lively and inviting through daylight, clean marble flooring, planted areas, seating, and the already animated Hebe fountain. Include enterable store shells and use imported retail kit components. Use supplied balcony straight/corner modules and originalmarbletexture; arrange sensible continuous walkable floors and guarded openings. Install one working up/down450cm-escalator pair between floors, with unobstructed walking approaches and departures. No NPCs, merchants, selling gameplay, or signage in this iteration. Design repeatable floor modules and organize floor/roof/planting/retail actors so it can grow to3-5floors later; make the current roof/skylight removable. Keep the existing player/flashlight animations and original demo map unchanged. Save the completed map as /Game/Environment/Mall/Maps/Lvl_MallAtrium_Test, set an appropriate PlayerStart and use the existing ThirdPerson GameMode for movement tests. This is an environment scale test with real importedassets, not only bare checkerboard geometry. Use sensible lighting and practical collision. Verify visually and report actual layout bounds, player start, actor groups, escalator actor names/origins, store entry points and saved map path.",
  'short_description':'Build expandable two-floor mall atrium test map',
  'reference_image_urls':images,
  'relevant_data':{
    'level_info':{'current_world':'/Temp/Untitled_1091','seed_actors':['VerificationFloor','VerificationSun','VerificationSkyLight','VerificationPlayerStart'],'new_map_save_path':'/Game/Environment/Mall/Maps/Lvl_MallAtrium_Test','existing_map_do_not_modify':'/Game/ThirdPerson/Lvl_ThirdPerson'},
    'selected_actors':[],
    'spatial_constraints':{'clear_room_height_cm':400,'structural_services_zone_cm':50,'floor_to_floor_cm':450,'initial_floor_count':2,'future_floor_count':[3,4,5],'no_hvac_internals':True,'esc_escalator_rise_cm':450,'escalator_run_cm':779.4228634059948,'escalator_landing_cm':130,'escalator_length_cm':1039.4228634059948,'escalator_width_cm':136.01,'balcony_deck_cm':[400,200,50],'balcony_walk_z_local_cm':0,'balcony_guard_top_above_deck_cm':132.367},
    'style_notes':['Welcoming central atrium, cream/warm-white plaster with user supplied marble floors, steel/glass rails, planted islands, fountain and comfortable seating.','References include derelict areas as overall architecture guidance; defer damage/abandonment to later wings.','Open airy sightlines and daylight; no merchandise interaction or signage.','Maintain genuine4m clear height under450cm gallery floors; slab undersides400cm above lowerwalkingfloor.'],
    'new_mesh_metadata':imports['meshes'],
    'retail_source_metadata':retail['assets'],
    'native_fbx_orientation_notes':'The native FBX importer reflects Blender Y. In Unreal the retail customer-facing side is +Y; balcony straight module long guard is -Y. Straight module has open ends, corner module has short guard return. Imported foliage dimensions/pivots were reviewed; SM_PottedPlant01 bottom is1.37cm abovepivot, place accordingly.',
    'asset_paths':{
      'up_escalator':'/Game/Environment/Mall/Blueprints/BP_MallEscalator_450',
      'down_escalator':'/Game/Environment/Mall/Blueprints/BP_MallEscalator_450_Down',
      'balcony_straight':'/Game/Environment/Mall/Models/Architecture/SM_MallBalconyStraight_400',
      'balcony_corner':'/Game/Environment/Mall/Models/Architecture/SM_MallBalconyModule_400',
      'fountain':'/Game/HebeFountain/Blueprints/BP_Fountain',
      'restaurant_seating':'/Game/Fab/Restaurant_Table_And_Chairs/restaurant_table_and_chairs/StaticMeshes/restaurant_table_and_chairs',
      'bistrot_seating':'/Game/Fab/Bistrot_table_and_chair/bistrot_table_and_chair/StaticMeshes/bistrot_table_and_chair',
      'waiting_seating':'/Game/Fab/Waiting_Chair/waiting_chair/StaticMeshes/waiting_chair',
      'leather_chair':'/Game/Fab/Designer_Leather_Tub_Chair___Curved_Design___Cushion_Seat___Leather_Stitched/chair/StaticMeshes/chair',
      'leather_chair_measured_bounds_cm':{'min':[-230.8973,-233.0842,0.2771],'max':[229.8309,233.0842,746.2055],'note':'Source imported at oversizedscale; use anotherchair or correctscale usingmeasuredseat/overallheight beforeplacement.'},
      'original_floor_marble':'/Game/Fab/Modular_Balcony_Structure_with_Glass_Railing___Outdoor_Balcony_Railing_Asset/railing/Materials/Stone',
      'game_mode':'/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode',
      'player_character':'/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter'
    },
    'material_creation_result':{'status':'Completed and verified via dedicated material operations; world-mapped marble, masked two-sided leaves, glass, PBR surface maps, UV1 book covers. All required outputs connected; no orphan expressions.','slot_to_material':bindings,'note':'Bind these existing materials to each new mesh material slot before placement and save only new meshes. Do not create materials with Python. Escalator shell slots metal/glass/dark/safety_yellow must use originals at /Game/Environment/Escalator/Meshes/<slot>, not retail map. Marble uses WorldXY / SlabSizeCm200 mapping. Retail light fixtures need actual light actors; their material is not emissive.'},
    'gathered_level_context':json.loads((root/'Automation/Mall/layout_context_result.json').read_text()),
    'save_scope':'Save only new mall assets and this new map. Original user imports recovered and verified; their source backups are underExternalAssets/Mall/UserImports and Saved/MallRecoverySnapshot. Do not alter/delete their originalassets.'
  },
  'previous_context':'The orchestrator created a clean transient verificationlevel and imported40staticmeshes66textures with purposespecificAuraPython. All newassets are saved in isolatedMallfolders. Escalator450Blueprints derive from the existing workingnativeAMallEscalator; defaults are Rise450,Run779.42286,ShowEndPlatesfalse,Runningtrue,TravelSpeed50. Treads move separately from static collisiondecks. Newblueprints mustbeverifiedready before placement. Newmaterials were built withpurposebuiltmaterialtools. The Hebe fountain earlier logged Nanite warnings for translucent water/stream meshes; inspect it and use per-componentDisallowNanite or a safeinstanceoverrideifnecessary rather than changing originalcontent. NoComputerUse. Userhasauthorizedbuildingthemap anddownloadingfreeassets; do not stopatplanning only. Use standardagentworkflowand completecreation/save. Preserveallothermaps andcurrentassets.'
 }
}
(root/'Automation/Mall/level_agent_request.json').write_text(json.dumps(request),encoding='utf-8')
print('Prepared level-design request with measured asset context and three user references')
