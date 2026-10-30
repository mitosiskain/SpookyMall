@tool
class_name MallPbrMaterialLibrary
extends RefCounted
## Megamall Anomaly — Master 2K PBR Material, Shader & Mesh Beautification Library.
## Loads and caches the verified 2K PBR texture sets from:
## - ExternalAssets/Mall/UserImports/Prepared/Textures/ (Stone Marble, Brushed Stainless Steel)
## - ExternalAssets/Mall/Surfaces/ (Terrazzo Tiles, White Plaster 02, Acoustic Ceiling Tiles 0013, Painted Metal Shutter)
## - ExternalAssets/Mall/PolyHaven/ (Pachira Bark & Alpha-Scissor Leaves, Potted Plant, Steel Frame Shelves, Modular Street Seating, Hardcover/Softcover Books)
## Provides World-Space Triplanar architectural materials and automatic multi-slot PBR material assignment for all imported .fbx / .glb meshes.

static var _mat_cache: Dictionary = {}
static var _tex_cache: Dictionary = {}

static func _load_tex(path: String) -> Texture2D:
	if _tex_cache.has(path):
		return _tex_cache[path] as Texture2D
	if ResourceLoader.exists(path):
		var t := load(path) as Texture2D
		if t != null:
			_tex_cache[path] = t
			return t
	return null

## 1. Polished Mall Stone Marble Floor (2K Stone_basecolor + Stone_normal + Stone_roughness, World Triplanar)
static func get_marble_floor_material(tint: Color = Color(0.95, 0.93, 0.89), triplanar_scale: float = 0.28) -> StandardMaterial3D:
	var key := "marble_%s_%.3f" % [tint.to_html(), triplanar_scale]
	if _mat_cache.has(key):
		return _mat_cache[key] as StandardMaterial3D

	var mat := StandardMaterial3D.new()
	mat.albedo_color = tint
	var diff := _load_tex("res://ExternalAssets/Mall/UserImports/Prepared/Textures/Stone_basecolor.jpg")
	var norm := _load_tex("res://ExternalAssets/Mall/UserImports/Prepared/Textures/Stone_normal.jpg")
	var rough := _load_tex("res://ExternalAssets/Mall/UserImports/Prepared/Textures/Stone_roughness.png")
	if diff:
		mat.albedo_texture = diff
	if norm:
		mat.normal_enabled = true
		mat.normal_scale = 0.55
		mat.normal_texture = norm
	if rough:
		mat.roughness = 0.34
		mat.roughness_texture = rough
	else:
		mat.roughness = 0.26
	mat.metallic = 0.04
	mat.metallic_specular = 0.65
	mat.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC
	mat.uv1_triplanar = true
	mat.uv1_world_triplanar = true
	mat.uv1_triplanar_sharpness = 4.0
	mat.uv1_scale = Vector3(triplanar_scale, triplanar_scale, triplanar_scale)
	_mat_cache[key] = mat
	return mat

## 2. Terrazzo Tile Promenade & Bridge Deck Material (2K PolyHaven Terrazzo Tiles, World Triplanar)
static func get_terrazzo_material(tint: Color = Color(0.92, 0.91, 0.88), triplanar_scale: float = 0.42) -> StandardMaterial3D:
	var key := "terrazzo_%s_%.3f" % [tint.to_html(), triplanar_scale]
	if _mat_cache.has(key):
		return _mat_cache[key] as StandardMaterial3D

	var mat := StandardMaterial3D.new()
	mat.albedo_color = tint
	var diff := _load_tex("res://ExternalAssets/Mall/Surfaces/terrazzo_tiles/terrazzo_tiles_diff_2k.png")
	var norm := _load_tex("res://ExternalAssets/Mall/Surfaces/terrazzo_tiles/terrazzo_tiles_nor_dx_2k.png")
	var rough := _load_tex("res://ExternalAssets/Mall/Surfaces/terrazzo_tiles/terrazzo_tiles_rough_2k.png")
	var ao := _load_tex("res://ExternalAssets/Mall/Surfaces/terrazzo_tiles/terrazzo_tiles_ao_2k.png")
	if diff:
		mat.albedo_texture = diff
	if norm:
		mat.normal_enabled = true
		mat.normal_scale = 0.65
		mat.normal_texture = norm
	if rough:
		mat.roughness = 0.32
		mat.roughness_texture = rough
	else:
		mat.roughness = 0.28
	if ao:
		mat.ao_enabled = true
		mat.ao_light_affect = 0.45
		mat.ao_texture = ao
	mat.metallic = 0.05
	mat.metallic_specular = 0.62
	mat.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC
	mat.uv1_triplanar = true
	mat.uv1_world_triplanar = true
	mat.uv1_triplanar_sharpness = 4.0
	mat.uv1_scale = Vector3(triplanar_scale, triplanar_scale, triplanar_scale)
	_mat_cache[key] = mat
	return mat

## 3. Architectural Cream Plaster / Travertine Wall Material for Columns, Fascias, Walls & Soffits (2K World Triplanar)
static func get_plaster_material(tint: Color = Color(0.95, 0.93, 0.89), triplanar_scale: float = 0.22) -> StandardMaterial3D:
	var key := "plaster_%s_%.3f" % [tint.to_html(), triplanar_scale]
	if _mat_cache.has(key):
		return _mat_cache[key] as StandardMaterial3D

	var mat := StandardMaterial3D.new()
	mat.albedo_color = tint.lerp(Color(0.97, 0.95, 0.91), 0.45)
	var diff := _load_tex("res://ExternalAssets/Mall/Surfaces/texturecan_tiles_0013/tiles_0013_color_2k.jpg")
	var norm := _load_tex("res://ExternalAssets/Mall/Surfaces/white_plaster_02/white_plaster_02_nor_dx_2k.png")
	var rough := _load_tex("res://ExternalAssets/Mall/Surfaces/white_plaster_02/white_plaster_02_rough_2k.png")
	if diff:
		mat.albedo_texture = diff
	if norm:
		mat.normal_enabled = true
		mat.normal_scale = 0.22
		mat.normal_texture = norm
	if rough:
		mat.roughness = 0.52
		mat.roughness_texture = rough
	else:
		mat.roughness = 0.48
	mat.metallic = 0.04
	mat.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC
	mat.uv1_triplanar = true
	mat.uv1_world_triplanar = true
	mat.uv1_triplanar_sharpness = 4.0
	mat.uv1_scale = Vector3(triplanar_scale, triplanar_scale, triplanar_scale)
	_mat_cache[key] = mat
	return mat

## 4. Acoustic Ceiling Tiles 0013 Material (2K TextureCan, World Triplanar)
static func get_ceiling_tile_material(tint: Color = Color(0.88, 0.89, 0.91), triplanar_scale: float = 0.45) -> StandardMaterial3D:
	var key := "ceiling_%s_%.3f" % [tint.to_html(), triplanar_scale]
	if _mat_cache.has(key):
		return _mat_cache[key] as StandardMaterial3D

	var mat := StandardMaterial3D.new()
	mat.albedo_color = tint
	var diff := _load_tex("res://ExternalAssets/Mall/Surfaces/texturecan_tiles_0013/tiles_0013_color_2k.jpg")
	var norm := _load_tex("res://ExternalAssets/Mall/Surfaces/texturecan_tiles_0013/tiles_0013_normal_opengl_2k.png")
	var rough := _load_tex("res://ExternalAssets/Mall/Surfaces/texturecan_tiles_0013/tiles_0013_roughness_2k.jpg")
	var ao := _load_tex("res://ExternalAssets/Mall/Surfaces/texturecan_tiles_0013/tiles_0013_ao_2k.jpg")
	var metal := _load_tex("res://ExternalAssets/Mall/Surfaces/texturecan_tiles_0013/tiles_0013_metallic_2k.jpg")
	if diff:
		mat.albedo_texture = diff
	if norm:
		mat.normal_enabled = true
		mat.normal_scale = 0.75
		mat.normal_texture = norm
	if rough:
		mat.roughness = 0.72
		mat.roughness_texture = rough
	if ao:
		mat.ao_enabled = true
		mat.ao_light_affect = 0.55
		mat.ao_texture = ao
	if metal:
		mat.metallic = 0.25
		mat.metallic_texture = metal
	mat.uv1_triplanar = true
	mat.uv1_world_triplanar = true
	mat.uv1_triplanar_sharpness = 4.0
	mat.uv1_scale = Vector3(triplanar_scale, triplanar_scale, triplanar_scale)
	_mat_cache[key] = mat
	return mat

## 5. Painted Metal Shutter & Service Door Material (2K PolyHaven, World Triplanar)
static func get_painted_shutter_material(tint: Color = Color(0.76, 0.79, 0.83), triplanar_scale: float = 0.45) -> StandardMaterial3D:
	var key := "shutter_%s_%.3f" % [tint.to_html(), triplanar_scale]
	if _mat_cache.has(key):
		return _mat_cache[key] as StandardMaterial3D

	var mat := StandardMaterial3D.new()
	mat.albedo_color = tint
	var diff := _load_tex("res://ExternalAssets/Mall/Surfaces/painted_metal_shutter/painted_metal_shutter_diff_2k.png")
	var norm := _load_tex("res://ExternalAssets/Mall/Surfaces/painted_metal_shutter/painted_metal_shutter_nor_dx_2k.png")
	var rough := _load_tex("res://ExternalAssets/Mall/Surfaces/painted_metal_shutter/painted_metal_shutter_rough_2k.png")
	var ao := _load_tex("res://ExternalAssets/Mall/Surfaces/painted_metal_shutter/painted_metal_shutter_ao_2k.png")
	if diff:
		mat.albedo_texture = diff
	if norm:
		mat.normal_enabled = true
		mat.normal_scale = 0.85
		mat.normal_texture = norm
	if rough:
		mat.roughness = 0.52
		mat.roughness_texture = rough
	if ao:
		mat.ao_enabled = true
		mat.ao_light_affect = 0.5
		mat.ao_texture = ao
	mat.metallic = 0.42
	mat.uv1_triplanar = true
	mat.uv1_world_triplanar = true
	mat.uv1_triplanar_sharpness = 4.0
	mat.uv1_scale = Vector3(triplanar_scale, triplanar_scale, triplanar_scale)
	_mat_cache[key] = mat
	return mat

## 6. Brushed Stainless Steel Material (Balustrade Handrails, Stanchions, Geodesic Dome Ribs, Fixture Frames)
static func get_brushed_steel_material(tint: Color = Color(0.76, 0.79, 0.83)) -> StandardMaterial3D:
	var key := "steel_%s" % tint.to_html()
	if _mat_cache.has(key):
		return _mat_cache[key] as StandardMaterial3D

	var mat := StandardMaterial3D.new()
	mat.albedo_color = tint
	var mr := _load_tex("res://ExternalAssets/Mall/UserImports/Prepared/Textures/Stainless_steel_Metalness-Stainless_steel_Roughness__1_.png")
	var norm := _load_tex("res://ExternalAssets/Mall/UserImports/Prepared/Textures/Stainless_steel_NormalGL.jpg")
	mat.metallic = 0.82
	mat.roughness = 0.28
	if mr:
		mat.roughness_texture = mr
		mat.roughness_texture_channel = BaseMaterial3D.TEXTURE_CHANNEL_GREEN
	if norm:
		mat.normal_enabled = true
		mat.normal_scale = 0.35
		mat.normal_texture = norm
	mat.uv1_triplanar = true
	mat.uv1_world_triplanar = true
	mat.uv1_scale = Vector3(0.6, 0.6, 0.6)
	_mat_cache[key] = mat
	return mat

## 7. Tempered Architectural Glass Material (Balustrades, Shopfront Windows, Skylight Dome)
static func get_architectural_glass_material(tint: Color = Color(0.76, 0.87, 0.92, 0.26)) -> StandardMaterial3D:
	var key := "glass_%s" % tint.to_html()
	if _mat_cache.has(key):
		return _mat_cache[key] as StandardMaterial3D

	var mat := StandardMaterial3D.new()
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	mat.albedo_color = tint
	mat.metallic = 0.18
	mat.metallic_specular = 0.88
	mat.roughness = 0.07
	_mat_cache[key] = mat
	return mat

## 8. Warm Architectural Timber / Wood Veneer Material (Benches, Tables, Sauna, Display Counters)
static func get_wood_timber_material(tint: Color = Color(0.72, 0.54, 0.38)) -> StandardMaterial3D:
	var key := "wood_%s" % tint.to_html()
	if _mat_cache.has(key):
		return _mat_cache[key] as StandardMaterial3D

	var mat := StandardMaterial3D.new()
	mat.albedo_color = tint
	var diff := _load_tex("res://ExternalAssets/Mall/PolyHaven/modular_street_seating/textures/modular_street_seating_timber_diff_2k.png")
	var norm := _load_tex("res://ExternalAssets/Mall/PolyHaven/modular_street_seating/textures/modular_street_seating_timber_nor_dx_2k.png")
	var rough := _load_tex("res://ExternalAssets/Mall/PolyHaven/modular_street_seating/textures/modular_street_seating_timber_rough_2k.png")
	if diff:
		mat.albedo_texture = diff
	if norm:
		mat.normal_enabled = true
		mat.normal_scale = 0.6
		mat.normal_texture = norm
	if rough:
		mat.roughness = 0.58
		mat.roughness_texture = rough
	else:
		mat.roughness = 0.52
	mat.uv1_triplanar = true
	mat.uv1_scale = Vector3(0.65, 0.65, 0.65)
	_mat_cache[key] = mat
	return mat

## 9. Soft Warm Architectural Cove Light Material (Desaturated Warm Champagne / Soft Ice, Not Oversaturated Neon!)
static func get_cove_accent_material(tint: Color = Color(0.96, 0.89, 0.76), energy: float = 0.75) -> StandardMaterial3D:
	var key := "cove_%s_%.2f" % [tint.to_html(), energy]
	if _mat_cache.has(key):
		return _mat_cache[key] as StandardMaterial3D

	var mat := StandardMaterial3D.new()
	mat.albedo_color = tint
	mat.roughness = 0.32
	mat.emission_enabled = true
	mat.emission = tint
	mat.emission_energy_multiplier = energy
	_mat_cache[key] = mat
	return mat

## 10. Animated Ripple Water Shader Material (Hebe Fountain, Geothermal Hot Springs, 20m Waterfall, Onsen Pool)
static func get_animated_water_material(deep_col: Color = Color(0.14, 0.42, 0.48, 0.78), shallow_col: Color = Color(0.32, 0.68, 0.74, 0.62)) -> ShaderMaterial:
	var key := "water_%s_%s" % [deep_col.to_html(), shallow_col.to_html()]
	if _mat_cache.has(key):
		return _mat_cache[key] as ShaderMaterial

	var shader := Shader.new()
	shader.code = """
shader_type spatial;
render_mode blend_mix, depth_draw_always, cull_disabled, specular_schlick_ggx;

uniform vec4 deep_color : source_color = vec4(0.14, 0.42, 0.48, 0.78);
uniform vec4 shallow_color : source_color = vec4(0.32, 0.68, 0.74, 0.62);
uniform vec3 foam_color : source_color = vec3(0.82, 0.92, 0.95);
uniform float wave_speed = 0.85;
uniform float wave_scale = 3.2;

varying vec3 world_pos;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	float w = sin(world_pos.x * 2.4 + TIME * wave_speed * 2.0) * cos(world_pos.z * 2.4 - TIME * wave_speed * 1.7) * 0.012;
	VERTEX.y += w;
}

void fragment() {
	vec2 uv1 = world_pos.xz * wave_scale * 0.15 + vec2(TIME * 0.04 * wave_speed, -TIME * 0.03 * wave_speed);
	vec2 uv2 = world_pos.xz * wave_scale * 0.23 + vec2(-TIME * 0.035 * wave_speed, TIME * 0.045 * wave_speed);
	float r1 = sin(uv1.x * 18.0 + cos(uv1.y * 16.0)) * 0.5 + 0.5;
	float r2 = cos(uv2.y * 22.0 - sin(uv2.x * 19.0)) * 0.5 + 0.5;
	float ripple = (r1 + r2) * 0.5;

	float fresnel = pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), 3.0);
	vec3 base_col = mix(deep_color.rgb, shallow_color.rgb, clamp(ripple * 0.65 + fresnel * 0.45, 0.0, 1.0));
	float highlight = smoothstep(0.78, 0.96, ripple);
	ALBEDO = mix(base_col, foam_color, highlight * 0.35);
	ROUGHNESS = mix(0.06, 0.16, highlight);
	METALLIC = 0.12;
	SPECULAR = 0.85;
	NORMAL_MAP = normalize(vec3((r1 - 0.5) * 0.22, (r2 - 0.5) * 0.22, 1.0));
	ALPHA = mix(deep_color.a, 0.90, fresnel);
}
"""
	var sm := ShaderMaterial.new()
	sm.shader = shader
	sm.set_shader_parameter("deep_color", deep_col)
	sm.set_shader_parameter("shallow_color", shallow_col)
	_mat_cache[key] = sm
	return sm

## Smartly maps a flat/oversaturated box color to the appropriate 2K PBR architectural material!
static func get_smart_box_material(size: Vector3, color: Color, roughness: float) -> Material:
	# Glass / balustrade detection
	if roughness <= 0.21 and color.b > 0.72 and (size.x < 0.25 or size.z < 0.25):
		return get_architectural_glass_material(Color(0.74, 0.86, 0.92, 0.28))

	# Floor / walkway / platform slabs
	if size.y <= 0.65 and size.x >= 2.2 and size.z >= 2.2:
		if size.x >= 40.0 and size.z >= 30.0:
			return get_marble_floor_material(Color(0.93, 0.91, 0.87), 0.26)
		return get_terrazzo_material(Color(0.90, 0.89, 0.86).lerp(color.lightened(0.45), 0.22), 0.38)

	# Ceiling slabs
	if size.y <= 0.65 and size.x >= 8.0 and size.z >= 8.0:
		return get_ceiling_tile_material(Color(0.90, 0.91, 0.93), 0.45)

	# Structural columns
	if size.y >= 6.0 and size.x <= 1.2 and size.z <= 1.2:
		return get_plaster_material(Color(0.93, 0.91, 0.87), 0.40)

	# Storefront & perimeter walls
	if size.y >= 2.5 and (size.x <= 0.6 or size.z <= 0.6):
		var wall_tint := Color(0.90, 0.88, 0.84).lerp(color.lightened(0.55), 0.20)
		return get_plaster_material(wall_tint, 0.32)

	# Counters, podiums, wooden furniture
	if color.r > color.b + 0.08 and size.y < 2.0:
		return get_wood_timber_material(color.lightened(0.28))

	# Metallic / industrial boxes
	if roughness <= 0.35 and absf(color.r - color.b) < 0.15:
		return get_brushed_steel_material(color.lightened(0.35))

	return get_plaster_material(color.lightened(0.42), 0.45)

## Automatically assigns 2K PBR textures & materials to all MeshInstance3D surfaces in an instantiated .fbx / .glb model!
static func beautify_model_materials(root: Node, res_path: String, fallback_tint: Color = Color(0.85, 0.84, 0.82)) -> void:
	var lower_path := res_path.to_lower()
	# Preserve original materials on character and animation models (e.g. MikuDoomer_Chen.glb)
	if "characters/" in lower_path or "mikudoomer" in lower_path or "animations/" in lower_path:
		return
	var meshes: Array[MeshInstance3D] = []
	_collect_mesh_instances(root, meshes)
	if meshes.is_empty():
		return

	# 1. PolyHaven Tropical Trees: SM_PachiraA / B / C / D
	if "pachira" in lower_path:
		var bark_mat := _get_pachira_bark_material()
		var leaf_mat := _get_pachira_leaf_material()
		for mi in meshes:
			if mi.mesh == null:
				continue
			var sc := mi.mesh.get_surface_count()
			if sc >= 2:
				mi.set_surface_override_material(0, bark_mat)
				for s in range(1, sc):
					mi.set_surface_override_material(s, leaf_mat)
			elif sc == 1:
				var mname := mi.name.to_lower()
				mi.set_surface_override_material(0, leaf_mat if "leaf" in mname or "leaves" in mname else bark_mat)
		return

	# 2. PolyHaven Potted Plant 01: SM_PottedPlant01
	if "pottedplant" in lower_path or "potted_plant" in lower_path:
		var pot_mat := _get_potted_plant_pot_material()
		var leaf_mat := _get_potted_plant_leaf_material()
		for mi in meshes:
			if mi.mesh == null:
				continue
			var sc := mi.mesh.get_surface_count()
			if sc >= 2:
				mi.set_surface_override_material(0, pot_mat)
				for s in range(1, sc):
					mi.set_surface_override_material(s, leaf_mat)
			elif sc == 1:
				mi.set_surface_override_material(0, leaf_mat)
		return

	# 3. PolyHaven Steel Frame Shelves: SM_RetailShelves01
	if "retailshelves" in lower_path or "steel_frame_shelves" in lower_path:
		var shelf_mat := _get_steel_shelves_material()
		for mi in meshes:
			if mi.mesh:
				for s in range(mi.mesh.get_surface_count()):
					mi.set_surface_override_material(s, shelf_mat)
		return

	# 4. PolyHaven Decorative Books: SM_BookHardcover02 / SM_BookSoftcover04
	if "bookhardcover" in lower_path or "book_hardcover" in lower_path:
		var bmat := _get_book_material(true)
		for mi in meshes:
			if mi.mesh:
				for s in range(mi.mesh.get_surface_count()):
					mi.set_surface_override_material(s, bmat)
		return
	if "booksoftcover" in lower_path or "book_softcover" in lower_path:
		var bmat := _get_book_material(false)
		for mi in meshes:
			if mi.mesh:
				for s in range(mi.mesh.get_surface_count()):
					mi.set_surface_override_material(s, bmat)
		return

	# 5. South Tangquan Steaming Onsen Pool: SM_Bath_SteamingOnsenPool
	if "steamingonsenpool" in lower_path:
		var stone_rim := get_terrazzo_material(Color(0.86, 0.76, 0.78), 0.45)
		var pink_water := get_animated_water_material(Color(0.68, 0.34, 0.46, 0.82), Color(0.88, 0.56, 0.68, 0.68))
		for mi in meshes:
			if mi.mesh == null:
				continue
			var sc := mi.mesh.get_surface_count()
			for s in range(sc):
				var sname := _get_surface_name_lower(mi.mesh, s)
				if "water" in sname or "glass" in sname or (sc >= 2 and s == sc - 1):
					mi.set_surface_override_material(s, pink_water)
				else:
					mi.set_surface_override_material(s, stone_rim)
		return

	# 6. Hinoki Cedar Sauna Pavilion: SM_Bath_GlassSaunaPavilion
	if "glasssaunapavilion" in lower_path:
		var cedar_mat := get_wood_timber_material(Color(0.82, 0.60, 0.38))
		var glass_mat := get_architectural_glass_material(Color(0.78, 0.88, 0.92, 0.28))
		var steel_mat := get_brushed_steel_material()
		for mi in meshes:
			if mi.mesh == null:
				continue
			var sc := mi.mesh.get_surface_count()
			for s in range(sc):
				var sname := _get_surface_name_lower(mi.mesh, s)
				if "glass" in sname:
					mi.set_surface_override_material(s, glass_mat)
				elif "metal" in sname or "dark" in sname:
					mi.set_surface_override_material(s, steel_mat)
				else:
					mi.set_surface_override_material(s, cedar_mat)
		return

	# 7. RetailKit & Mall Furnishings (SM_Retail_* / Benches / Mannequins / Cash Desks / Gondolas / Planters / Tables)
	var mat_carcass := get_plaster_material(Color(0.90, 0.89, 0.86), 0.55)
	var mat_wood := get_wood_timber_material(fallback_tint.lightened(0.22))
	var mat_steel := get_brushed_steel_material(Color(0.78, 0.80, 0.84))
	var mat_dark := get_painted_shutter_material(Color(0.24, 0.26, 0.30), 0.65)
	var mat_glass := get_architectural_glass_material(Color(0.76, 0.88, 0.94, 0.30))
	var mat_accent := get_plaster_material(fallback_tint.lightened(0.18), 0.5)
	var mat_foliage := _get_pachira_leaf_material()

	var is_mannequin := "mannequin" in lower_path or "torsoform" in lower_path or "bustform" in lower_path or "legform" in lower_path
	var is_bench_or_table := "bench" in lower_path or "table" in lower_path or "cashdesk" in lower_path or "gondola" in lower_path
	var is_planter := "planter" in lower_path

	for mi in meshes:
		if mi.mesh == null:
			continue
		# Skip overriding if mesh already has an embedded textured material (e.g. .glb characters)
		var existing_mat := mi.mesh.surface_get_material(0) if mi.mesh.get_surface_count() > 0 else null
		if existing_mat is StandardMaterial3D and (existing_mat as StandardMaterial3D).albedo_texture != null:
			continue

		var sc := mi.mesh.get_surface_count()
		for s in range(sc):
			var sname := _get_surface_name_lower(mi.mesh, s)
			if "glass" in sname or "mirror" in sname:
				mi.set_surface_override_material(s, mat_glass)
			elif "metal" in sname or "steel" in sname or "chrome" in sname:
				mi.set_surface_override_material(s, mat_steel)
			elif "wood" in sname or "surface" in sname or "timber" in sname:
				mi.set_surface_override_material(s, mat_wood)
			elif "dark" in sname or "soil" in sname or "rubber" in sname:
				mi.set_surface_override_material(s, mat_dark)
			elif "leaf" in sname or "foliage" in sname or "plant" in sname:
				mi.set_surface_override_material(s, mat_foliage)
			elif "accent" in sname or "light" in sname:
				mi.set_surface_override_material(s, mat_accent)
			elif is_mannequin:
				mi.set_surface_override_material(s, mat_carcass if s == 0 else mat_steel)
			elif is_bench_or_table:
				var cycle := [mat_wood, mat_steel, mat_carcass, mat_dark]
				mi.set_surface_override_material(s, cycle[s % cycle.size()])
			elif is_planter:
				var p_cycle := [get_terrazzo_material(Color(0.84, 0.83, 0.80), 0.5), mat_dark, mat_steel]
				mi.set_surface_override_material(s, p_cycle[s % p_cycle.size()])
			else:
				var def_cycle := [mat_carcass, mat_steel, mat_wood, mat_dark, mat_accent]
				mi.set_surface_override_material(s, def_cycle[s % def_cycle.size()])

static func _collect_mesh_instances(node: Node, out_list: Array[MeshInstance3D]) -> void:
	if node is MeshInstance3D:
		out_list.append(node as MeshInstance3D)
	for c in node.get_children():
		_collect_mesh_instances(c, out_list)

static func _get_surface_name_lower(mesh: Mesh, surf_idx: int) -> String:
	var n := ""
	if mesh.has_method("surface_get_name"):
		n = str(mesh.call("surface_get_name", surf_idx)).to_lower()
	var m := mesh.surface_get_material(surf_idx)
	if m != null and not m.resource_name.is_empty():
		n += " " + m.resource_name.to_lower()
	return n

static func _get_pachira_bark_material() -> StandardMaterial3D:
	if _mat_cache.has("pachira_bark"):
		return _mat_cache["pachira_bark"] as StandardMaterial3D
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Color(0.88, 0.84, 0.78)
	var diff := _load_tex("res://ExternalAssets/Mall/PolyHaven/pachira_aquatica_01/textures/pachira_aquatica_01_bark_diff_2k.png")
	var norm := _load_tex("res://ExternalAssets/Mall/PolyHaven/pachira_aquatica_01/textures/pachira_aquatica_01_bark_nor_dx_2k.png")
	var rough := _load_tex("res://ExternalAssets/Mall/PolyHaven/pachira_aquatica_01/textures/pachira_aquatica_01_bark_rough_2k.png")
	var ao := _load_tex("res://ExternalAssets/Mall/PolyHaven/pachira_aquatica_01/textures/pachira_aquatica_01_bark_ao_2k.png")
	if diff:
		mat.albedo_texture = diff
	if norm:
		mat.normal_enabled = true
		mat.normal_texture = norm
	if rough:
		mat.roughness_texture = rough
	if ao:
		mat.ao_enabled = true
		mat.ao_texture = ao
	_mat_cache["pachira_bark"] = mat
	return mat

static func _get_pachira_leaf_material() -> StandardMaterial3D:
	if _mat_cache.has("pachira_leaf"):
		return _mat_cache["pachira_leaf"] as StandardMaterial3D
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Color(0.76, 0.92, 0.72)
	mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	var diff := _load_tex("res://ExternalAssets/Mall/PolyHaven/pachira_aquatica_01/textures/pachira_aquatica_01_leaves_diff_2k.png")
	var norm := _load_tex("res://ExternalAssets/Mall/PolyHaven/pachira_aquatica_01/textures/pachira_aquatica_01_leaves_nor_dx_2k.png")
	var rough := _load_tex("res://ExternalAssets/Mall/PolyHaven/pachira_aquatica_01/textures/pachira_aquatica_01_leaves_rough_2k.png")
	if diff:
		mat.albedo_texture = diff
	if norm:
		mat.normal_enabled = true
		mat.normal_texture = norm
	if rough:
		mat.roughness = 0.45
		mat.roughness_texture = rough
	mat.backlight_enabled = true
	mat.backlight = Color(0.28, 0.46, 0.22)
	_mat_cache["pachira_leaf"] = mat
	return mat

static func _get_potted_plant_pot_material() -> StandardMaterial3D:
	if _mat_cache.has("potted_pot"):
		return _mat_cache["potted_pot"] as StandardMaterial3D
	var mat := StandardMaterial3D.new()
	var diff := _load_tex("res://ExternalAssets/Mall/PolyHaven/potted_plant_01/textures/potted_plant_01_pot_diff_2k.png")
	var norm := _load_tex("res://ExternalAssets/Mall/PolyHaven/potted_plant_01/textures/potted_plant_01_pot_nor_dx_2k.png")
	var rough := _load_tex("res://ExternalAssets/Mall/PolyHaven/potted_plant_01/textures/potted_plant_01_pot_rough_2k.png")
	if diff:
		mat.albedo_texture = diff
	if norm:
		mat.normal_enabled = true
		mat.normal_texture = norm
	if rough:
		mat.roughness_texture = rough
	_mat_cache["potted_pot"] = mat
	return mat

static func _get_potted_plant_leaf_material() -> StandardMaterial3D:
	if _mat_cache.has("potted_leaf"):
		return _mat_cache["potted_leaf"] as StandardMaterial3D
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Color(0.75, 0.90, 0.72)
	mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	var diff := _load_tex("res://ExternalAssets/Mall/PolyHaven/potted_plant_01/textures/potted_plant_01_leaves_diff_2k.png")
	var norm := _load_tex("res://ExternalAssets/Mall/PolyHaven/potted_plant_01/textures/potted_plant_01_leaves_nor_gl_2k.png")
	var rough := _load_tex("res://ExternalAssets/Mall/PolyHaven/potted_plant_01/textures/potted_plant_01_leaves_rough_2k.png")
	if diff:
		mat.albedo_texture = diff
	if norm:
		mat.normal_enabled = true
		mat.normal_texture = norm
	if rough:
		mat.roughness_texture = rough
	mat.backlight_enabled = true
	mat.backlight = Color(0.25, 0.42, 0.20)
	_mat_cache["potted_leaf"] = mat
	return mat

static func _get_steel_shelves_material() -> StandardMaterial3D:
	if _mat_cache.has("steel_shelves"):
		return _mat_cache["steel_shelves"] as StandardMaterial3D
	var mat := StandardMaterial3D.new()
	var diff := _load_tex("res://ExternalAssets/Mall/PolyHaven/steel_frame_shelves_01/textures/steel_frame_shelves_01_diff_2k.png")
	var metal := _load_tex("res://ExternalAssets/Mall/PolyHaven/steel_frame_shelves_01/textures/steel_frame_shelves_01_metal_2k.png")
	var norm := _load_tex("res://ExternalAssets/Mall/PolyHaven/steel_frame_shelves_01/textures/steel_frame_shelves_01_nor_dx_2k.png")
	var rough := _load_tex("res://ExternalAssets/Mall/PolyHaven/steel_frame_shelves_01/textures/steel_frame_shelves_01_rough_2k.png")
	if diff:
		mat.albedo_texture = diff
	if metal:
		mat.metallic = 0.75
		mat.metallic_texture = metal
	if norm:
		mat.normal_enabled = true
		mat.normal_texture = norm
	if rough:
		mat.roughness_texture = rough
	_mat_cache["steel_shelves"] = mat
	return mat

static func _get_book_material(hardcover: bool) -> StandardMaterial3D:
	var key := "book_hc" if hardcover else "book_sc"
	if _mat_cache.has(key):
		return _mat_cache[key] as StandardMaterial3D
	var mat := StandardMaterial3D.new()
	var prefix := "res://ExternalAssets/Mall/PolyHaven/decorative_book_set_01/textures/book_hardcover_01_" if hardcover else "res://ExternalAssets/Mall/PolyHaven/decorative_book_set_01/textures/book_softcover_01_"
	var diff := _load_tex(prefix + ("cover02_2k.png" if hardcover else "cover04_2k.png"))
	if diff == null:
		diff = _load_tex(prefix + "diff_2k.png")
	var norm := _load_tex(prefix + "nor_dx_2k.png")
	var rough := _load_tex(prefix + "roughness_2k.png")
	if diff:
		mat.albedo_texture = diff
	if norm:
		mat.normal_enabled = true
		mat.normal_texture = norm
	if rough:
		mat.roughness_texture = rough
	_mat_cache[key] = mat
	return mat
