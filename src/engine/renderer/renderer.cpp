#include "renderer.hpp"
#include "common/log.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>

namespace {
	struct Vertex {
		vec2 corner;
		vec2 uv;
	};

	struct alignas(16) CameraUniform {
		vec2 viewport_size;
		vec2 view_position;
		vec2 cursor_world_position;
		f32 zoom;
		f32 transition_radius;
		f32 height_darkening;
		f32 tile_size;
		f32 grid_world_width;
		f32 grid_min_pixel_width;
		f32 grid_strength;
		f32 grid_enabled;
		f32 grid_fade_radius;
	};

	struct alignas(16) SpriteCameraUniform {
		vec2 viewport_size;
		vec2 view_position;
		f32 zoom;
		vec3 padding{0.0F};
	};

	constexpr std::array<Vertex, 4> QUAD_VERTICES{{
		{{0.0F, 0.0F}, {0.0F, 0.0F}},
		{{1.0F, 0.0F}, {1.0F, 0.0F}},
		{{1.0F, 1.0F}, {1.0F, 1.0F}},
		{{0.0F, 1.0F}, {0.0F, 1.0F}},
	}};
	constexpr std::array<Vertex, 4> CENTERED_QUAD_VERTICES{{
		{{-0.5F, -0.5F}, {0.0F, 0.0F}},
		{{0.5F, -0.5F}, {1.0F, 0.0F}},
		{{0.5F, 0.5F}, {1.0F, 1.0F}},
		{{-0.5F, 0.5F}, {0.0F, 1.0F}},
	}};
	constexpr std::array<u16, 6> QUAD_INDICES{{0, 1, 2, 0, 2, 3}};

	constexpr u32 CELL_BITS = 5U;
	constexpr u32 CELL_KIND_MASK = 0x3U;
	constexpr f32 TRANSITION_RADIUS = 9.0F;
	constexpr f32 HEIGHT_DARKENING = 0.075F;
	constexpr f32 GRID_WORLD_WIDTH = 0.65F;
	constexpr f32 GRID_STRENGTH = 0.16F;
	constexpr f32 GRID_FADE_RADIUS = TILE_SIZE * 6.0F;
	static_assert(static_cast<u32>(TileKind::COUNT) <= 4U);
	static_assert(MAX_TERRAIN_ELEVATION <= 7U);

	u32 pack_cell(const Tile& tile) {
		return (static_cast<u32>(tile.kind) & CELL_KIND_MASK) |
			   (static_cast<u32>(tile.elevation) << 2U);
	}

	const char* shader_extension() {
#if defined(__APPLE__)
		return "msl";
#elif defined(_WIN32)
		return "dxil";
#else
		return "spv";
#endif
	}

	const char* shader_entrypoint() {
#if defined(__APPLE__)
		return "main0";
#else
		return "main";
#endif
	}

	SDL_GPUShaderFormat shader_format() {
#if defined(__APPLE__)
		return SDL_GPU_SHADERFORMAT_MSL;
#elif defined(_WIN32)
		return SDL_GPU_SHADERFORMAT_DXIL;
#else
		return SDL_GPU_SHADERFORMAT_SPIRV;
#endif
	}

	SDL_GPUShader* load_shader(SDL_GPUDevice* device, const char* name, SDL_GPUShaderStage stage,
							   u32 uniform_buffers, u32 samplers = 0) {
		const std::string filename = std::string{name} + "." + shader_extension();
		const char* base_path = SDL_GetBasePath();
		const std::string path =
			std::string{base_path != nullptr ? base_path : ""} + "shaders/" + filename;
		size_t size = 0;
		void* data = SDL_LoadFile(path.c_str(), &size);
		if (data == nullptr) {
			log::error("Failed to load shader {}: {}", filename, SDL_GetError());
			return nullptr;
		}
		const SDL_GPUShaderCreateInfo info{
			.code_size = size,
			.code = static_cast<const u8*>(data),
			.entrypoint = shader_entrypoint(),
			.format = shader_format(),
			.stage = stage,
			.num_samplers = samplers,
			.num_storage_textures = 0,
			.num_storage_buffers = 0,
			.num_uniform_buffers = uniform_buffers,
		};
		SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
		SDL_free(data);
		if (shader == nullptr) {
			log::error("Failed to create shader {}: {}", filename, SDL_GetError());
		}
		return shader;
	}

} // namespace

Renderer::~Renderer() {
	release();
}

bool Renderer::init(SDL_Window* target_window) {
	if (device != nullptr || target_window == nullptr) {
		return false;
	}
	window = target_window;
	terrain_instances.reserve(MAX_TERRAIN_INSTANCES);
	sprite_instances.reserve(MAX_SPRITE_INSTANCES);
	sprite_commands.reserve(64);

#if defined(NDEBUG)
	constexpr bool DEBUG_DEVICE = false;
#else
	constexpr bool DEBUG_DEVICE = true;
#endif
	device = SDL_CreateGPUDevice(shader_format(), DEBUG_DEVICE, nullptr);
	if (device == nullptr) {
		log::error("Failed to create SDL GPU device: {}", SDL_GetError());
		release();
		return false;
	}
	if (!SDL_ClaimWindowForGPUDevice(device, window)) {
		log::error("Failed to claim window for SDL GPU: {}", SDL_GetError());
		release();
		return false;
	}
	if (!create_terrain_pipeline() || !create_sprite_pipeline() || !create_buffers() ||
		!upload_static_geometry() || !load_sprite_textures()) {
		release();
		return false;
	}

	log::info("SDL_gpu terrain renderer initialized with {}", SDL_GetGPUDeviceDriver(device));
	return true;
}

bool Renderer::create_terrain_pipeline() {
	SDL_GPUShader* vertex = load_shader(device, "terrain.vert", SDL_GPU_SHADERSTAGE_VERTEX, 1);
	SDL_GPUShader* fragment = load_shader(device, "terrain.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 0);
	if (vertex == nullptr || fragment == nullptr) {
		if (vertex != nullptr)
			SDL_ReleaseGPUShader(device, vertex);
		if (fragment != nullptr)
			SDL_ReleaseGPUShader(device, fragment);
		return false;
	}

	const std::array<SDL_GPUVertexBufferDescription, 2> descriptions{{
		{.slot = 0,
		 .pitch = sizeof(Vertex),
		 .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
		 .instance_step_rate = 0},
		{.slot = 1,
		 .pitch = sizeof(TerrainInstance),
		 .input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE,
		 .instance_step_rate = 0},
	}};
	const std::array<SDL_GPUVertexAttribute, 4> attributes{{
		{.location = 0,
		 .buffer_slot = 0,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
		 .offset = offsetof(Vertex, corner)},
		{.location = 1,
		 .buffer_slot = 0,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
		 .offset = offsetof(Vertex, uv)},
		{.location = 2,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
		 .offset = offsetof(TerrainInstance, origin)},
		{.location = 3,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
		 .offset = offsetof(TerrainInstance, neighborhood)},
	}};
	const SDL_GPUVertexInputState vertex_input{
		.vertex_buffer_descriptions = descriptions.data(),
		.num_vertex_buffers = static_cast<u32>(descriptions.size()),
		.vertex_attributes = attributes.data(),
		.num_vertex_attributes = static_cast<u32>(attributes.size()),
	};
	const SDL_GPUColorTargetDescription target{
		.format = SDL_GetGPUSwapchainTextureFormat(device, window),
		.blend_state = {.enable_blend = false},
	};
	const SDL_GPUGraphicsPipelineCreateInfo pipeline_info{
		.vertex_shader = vertex,
		.fragment_shader = fragment,
		.vertex_input_state = vertex_input,
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.rasterizer_state =
			{
				.fill_mode = SDL_GPU_FILLMODE_FILL,
				.cull_mode = SDL_GPU_CULLMODE_NONE,
				.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
			},
		.multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
		.target_info = {.color_target_descriptions = &target, .num_color_targets = 1},
	};
	terrain_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
	SDL_ReleaseGPUShader(device, vertex);
	SDL_ReleaseGPUShader(device, fragment);
	if (terrain_pipeline == nullptr) {
		log::error("Failed to create terrain pipeline: {}", SDL_GetError());
		return false;
	}
	return true;
}

bool Renderer::create_sprite_pipeline() {
	SDL_GPUShader* vertex = load_shader(device, "sprite.vert", SDL_GPU_SHADERSTAGE_VERTEX, 1);
	SDL_GPUShader* fragment =
		load_shader(device, "sprite.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 1);
	if (vertex == nullptr || fragment == nullptr) {
		if (vertex != nullptr)
			SDL_ReleaseGPUShader(device, vertex);
		if (fragment != nullptr)
			SDL_ReleaseGPUShader(device, fragment);
		return false;
	}

	const std::array<SDL_GPUVertexBufferDescription, 2> descriptions{{
		{.slot = 0,
		 .pitch = sizeof(Vertex),
		 .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
		 .instance_step_rate = 0},
		{.slot = 1,
		 .pitch = sizeof(SpriteInstance),
		 .input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE,
		 .instance_step_rate = 0},
	}};
	const std::array<SDL_GPUVertexAttribute, 8> attributes{{
		{.location = 0,
		 .buffer_slot = 0,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
		 .offset = offsetof(Vertex, corner)},
		{.location = 1,
		 .buffer_slot = 0,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
		 .offset = offsetof(Vertex, uv)},
		{.location = 2,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
		 .offset = offsetof(SpriteInstance, origin)},
		{.location = 3,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
		 .offset = offsetof(SpriteInstance, size)},
		{.location = 4,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		 .offset = offsetof(SpriteInstance, uv_rect)},
		{.location = 5,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		 .offset = offsetof(SpriteInstance, color)},
		{.location = 6,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		 .offset = offsetof(SpriteInstance, parameters)},
		{.location = 7,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		 .offset = offsetof(SpriteInstance, corner_radii)},
	}};
	const SDL_GPUVertexInputState vertex_input{
		.vertex_buffer_descriptions = descriptions.data(),
		.num_vertex_buffers = static_cast<u32>(descriptions.size()),
		.vertex_attributes = attributes.data(),
		.num_vertex_attributes = static_cast<u32>(attributes.size()),
	};
	const SDL_GPUColorTargetDescription target{
		.format = SDL_GetGPUSwapchainTextureFormat(device, window),
		.blend_state =
			{
				.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
				.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
				.color_blend_op = SDL_GPU_BLENDOP_ADD,
				.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
				.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
				.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
				.enable_blend = true,
			},
	};
	const SDL_GPUGraphicsPipelineCreateInfo pipeline_info{
		.vertex_shader = vertex,
		.fragment_shader = fragment,
		.vertex_input_state = vertex_input,
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.rasterizer_state =
			{
				.fill_mode = SDL_GPU_FILLMODE_FILL,
				.cull_mode = SDL_GPU_CULLMODE_NONE,
				.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
			},
		.multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
		.target_info = {.color_target_descriptions = &target, .num_color_targets = 1},
	};
	sprite_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
	SDL_ReleaseGPUShader(device, vertex);
	SDL_ReleaseGPUShader(device, fragment);
	if (sprite_pipeline == nullptr) {
		log::error("Failed to create sprite pipeline: {}", SDL_GetError());
		return false;
	}
	return true;
}

bool Renderer::create_buffers() {
	const SDL_GPUBufferCreateInfo vertex_info{
		.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
		.size = sizeof(QUAD_VERTICES),
	};
	const SDL_GPUBufferCreateInfo index_info{
		.usage = SDL_GPU_BUFFERUSAGE_INDEX,
		.size = sizeof(QUAD_INDICES),
	};
	const SDL_GPUBufferCreateInfo instance_info{
		.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
		.size = static_cast<u32>(MAX_TERRAIN_INSTANCES * sizeof(TerrainInstance)),
	};
	const SDL_GPUTransferBufferCreateInfo transfer_info{
		.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		.size = instance_info.size,
	};
	const SDL_GPUBufferCreateInfo sprite_vertex_info{
		.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
		.size = sizeof(CENTERED_QUAD_VERTICES),
	};
	const SDL_GPUBufferCreateInfo sprite_index_info{
		.usage = SDL_GPU_BUFFERUSAGE_INDEX,
		.size = sizeof(QUAD_INDICES),
	};
	const SDL_GPUBufferCreateInfo sprite_instance_info{
		.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
		.size = static_cast<u32>(MAX_SPRITE_INSTANCES * sizeof(SpriteInstance)),
	};
	const SDL_GPUTransferBufferCreateInfo sprite_transfer_info{
		.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		.size = sprite_instance_info.size,
	};
	vertex_buffer = SDL_CreateGPUBuffer(device, &vertex_info);
	index_buffer = SDL_CreateGPUBuffer(device, &index_info);
	instance_buffer = SDL_CreateGPUBuffer(device, &instance_info);
	transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
	sprite_vertex_buffer = SDL_CreateGPUBuffer(device, &sprite_vertex_info);
	sprite_index_buffer = SDL_CreateGPUBuffer(device, &sprite_index_info);
	sprite_instance_buffer = SDL_CreateGPUBuffer(device, &sprite_instance_info);
	sprite_transfer_buffer = SDL_CreateGPUTransferBuffer(device, &sprite_transfer_info);
	const SDL_GPUSamplerCreateInfo sampler_info{
		.min_filter = SDL_GPU_FILTER_LINEAR,
		.mag_filter = SDL_GPU_FILTER_LINEAR,
		.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	};
	sprite_sampler = SDL_CreateGPUSampler(device, &sampler_info);
	if (vertex_buffer == nullptr || index_buffer == nullptr || instance_buffer == nullptr ||
		transfer_buffer == nullptr || sprite_vertex_buffer == nullptr ||
		sprite_index_buffer == nullptr || sprite_instance_buffer == nullptr ||
		sprite_transfer_buffer == nullptr || sprite_sampler == nullptr) {
		log::error("Failed to create renderer buffers: {}", SDL_GetError());
		return false;
	}
	return true;
}

bool Renderer::upload_static_geometry() {
	void* mapped = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
	if (mapped == nullptr) {
		log::error("Failed to map terrain transfer buffer: {}", SDL_GetError());
		return false;
	}
	std::memcpy(mapped, QUAD_VERTICES.data(), sizeof(QUAD_VERTICES));
	std::memcpy(static_cast<u8*>(mapped) + sizeof(QUAD_VERTICES), QUAD_INDICES.data(),
				sizeof(QUAD_INDICES));
	SDL_UnmapGPUTransferBuffer(device, transfer_buffer);
	void* sprite_mapped = SDL_MapGPUTransferBuffer(device, sprite_transfer_buffer, false);
	if (sprite_mapped == nullptr) {
		log::error("Failed to map sprite transfer buffer: {}", SDL_GetError());
		return false;
	}
	std::memcpy(sprite_mapped, CENTERED_QUAD_VERTICES.data(), sizeof(CENTERED_QUAD_VERTICES));
	std::memcpy(static_cast<u8*>(sprite_mapped) + sizeof(CENTERED_QUAD_VERTICES),
				QUAD_INDICES.data(), sizeof(QUAD_INDICES));
	SDL_UnmapGPUTransferBuffer(device, sprite_transfer_buffer);

	SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
	if (command == nullptr) {
		log::error("Failed to acquire static upload command buffer: {}", SDL_GetError());
		return false;
	}
	SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(command);
	const SDL_GPUTransferBufferLocation vertex_source{.transfer_buffer = transfer_buffer,
													  .offset = 0};
	const SDL_GPUBufferRegion vertex_destination{
		.buffer = vertex_buffer, .offset = 0, .size = sizeof(QUAD_VERTICES)};
	SDL_UploadToGPUBuffer(copy, &vertex_source, &vertex_destination, false);
	const SDL_GPUTransferBufferLocation index_source{.transfer_buffer = transfer_buffer,
													 .offset = sizeof(QUAD_VERTICES)};
	const SDL_GPUBufferRegion index_destination{
		.buffer = index_buffer, .offset = 0, .size = sizeof(QUAD_INDICES)};
	SDL_UploadToGPUBuffer(copy, &index_source, &index_destination, false);
	const SDL_GPUTransferBufferLocation sprite_vertex_source{
		.transfer_buffer = sprite_transfer_buffer, .offset = 0};
	const SDL_GPUBufferRegion sprite_vertex_destination{
		.buffer = sprite_vertex_buffer, .offset = 0, .size = sizeof(CENTERED_QUAD_VERTICES)};
	SDL_UploadToGPUBuffer(copy, &sprite_vertex_source, &sprite_vertex_destination, false);
	const SDL_GPUTransferBufferLocation sprite_index_source{
		.transfer_buffer = sprite_transfer_buffer, .offset = sizeof(CENTERED_QUAD_VERTICES)};
	const SDL_GPUBufferRegion sprite_index_destination{
		.buffer = sprite_index_buffer, .offset = 0, .size = sizeof(QUAD_INDICES)};
	SDL_UploadToGPUBuffer(copy, &sprite_index_source, &sprite_index_destination, false);
	SDL_EndGPUCopyPass(copy);
	if (!SDL_SubmitGPUCommandBuffer(command)) {
		log::error("Failed to submit static terrain geometry: {}", SDL_GetError());
		return false;
	}
	return true;
}

bool Renderer::upload_texture(SpriteTexture& texture, u32 width, u32 height, const u8* pixels) {
	const u32 byte_count = width * height * 4;
	const SDL_GPUTextureCreateInfo texture_info{
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
		.width = width,
		.height = height,
		.layer_count_or_depth = 1,
		.num_levels = 1,
		.sample_count = SDL_GPU_SAMPLECOUNT_1,
	};
	SDL_GPUTexture* handle = SDL_CreateGPUTexture(device, &texture_info);
	const SDL_GPUTransferBufferCreateInfo transfer_info{
		.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		.size = byte_count,
	};
	SDL_GPUTransferBuffer* upload = SDL_CreateGPUTransferBuffer(device, &transfer_info);
	if (handle == nullptr || upload == nullptr) {
		log::error("Failed to allocate {}x{} sprite texture: {}", width, height, SDL_GetError());
		if (upload != nullptr)
			SDL_ReleaseGPUTransferBuffer(device, upload);
		if (handle != nullptr)
			SDL_ReleaseGPUTexture(device, handle);
		return false;
	}
	void* mapped = SDL_MapGPUTransferBuffer(device, upload, false);
	if (mapped == nullptr) {
		log::error("Failed to map sprite texture upload: {}", SDL_GetError());
		SDL_ReleaseGPUTransferBuffer(device, upload);
		SDL_ReleaseGPUTexture(device, handle);
		return false;
	}
	std::memcpy(mapped, pixels, byte_count);
	SDL_UnmapGPUTransferBuffer(device, upload);
	SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
	if (command == nullptr) {
		SDL_ReleaseGPUTransferBuffer(device, upload);
		SDL_ReleaseGPUTexture(device, handle);
		return false;
	}
	SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(command);
	const SDL_GPUTextureTransferInfo source{
		.transfer_buffer = upload,
		.pixels_per_row = width,
		.rows_per_layer = height,
	};
	const SDL_GPUTextureRegion destination{
		.texture = handle,
		.w = width,
		.h = height,
		.d = 1,
	};
	SDL_UploadToGPUTexture(copy, &source, &destination, false);
	SDL_EndGPUCopyPass(copy);
	if (!SDL_SubmitGPUCommandBuffer(command)) {
		log::error("Failed to submit sprite texture upload: {}", SDL_GetError());
		SDL_ReleaseGPUTransferBuffer(device, upload);
		SDL_ReleaseGPUTexture(device, handle);
		return false;
	}
	SDL_ReleaseGPUTransferBuffer(device, upload);
	texture = {.handle = handle, .width = width, .height = height};
	return true;
}

bool Renderer::load_sprite_texture(SpriteTexture& texture, const char* relative_path) {
	const char* base_path = SDL_GetBasePath();
	const std::string path =
		std::string{base_path != nullptr ? base_path : ""} + "res/" + relative_path;
	SDL_Surface* loaded = IMG_Load(path.c_str());
	if (loaded == nullptr) {
		log::error("Failed to load sprite '{}': {}", relative_path, SDL_GetError());
		return false;
	}
	SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
	SDL_DestroySurface(loaded);
	if (rgba == nullptr || rgba->w <= 0 || rgba->h <= 0 || !SDL_LockSurface(rgba)) {
		log::error("Failed to convert sprite '{}': {}", relative_path, SDL_GetError());
		if (rgba != nullptr)
			SDL_DestroySurface(rgba);
		return false;
	}
	const usize row_bytes = static_cast<usize>(rgba->w) * 4;
	std::vector<u8> pixels(row_bytes * static_cast<usize>(rgba->h));
	for (i32 row = 0; row < rgba->h; ++row) {
		std::memcpy(pixels.data() + static_cast<usize>(row) * row_bytes,
					static_cast<const u8*>(rgba->pixels) + static_cast<usize>(row) * rgba->pitch,
					row_bytes);
	}
	SDL_UnlockSurface(rgba);
	const bool result = upload_texture(texture, static_cast<u32>(rgba->w),
									   static_cast<u32>(rgba->h), pixels.data());
	SDL_DestroySurface(rgba);
	return result;
}

bool Renderer::load_sprite_textures() {
	constexpr std::array<const char*, SPRITE_ICON_COUNT> PATHS{
		"sprites/ore.png",	  "sprites/gear.png",	   "sprites/ingot.png",
		"sprites/engine.png", "sprites/propeller.png", "sprites/smelter.png",
	};
	constexpr std::array<u8, 4> WHITE{255, 255, 255, 255};
	if (!upload_texture(white_texture, 1, 1, WHITE.data())) {
		return false;
	}
	for (usize index = 0; index < PATHS.size(); ++index) {
		if (!load_sprite_texture(sprite_icons[index], PATHS[index])) {
			return false;
		}
	}
	return true;
}

void Renderer::begin_frame(vec4 clear) {
	clear_color = clear;
	pending_terrain = nullptr;
	sprite_instances.clear();
	sprite_commands.clear();
}

void Renderer::set_view(vec2 position, f32 zoom) {
	view_position = position;
	view_zoom = std::max(zoom, 0.001F);
}

void Renderer::draw_terrain(const Terrain& terrain) {
	pending_terrain = &terrain;
}

void Renderer::queue_sprite(SDL_GPUTexture* texture, const SpriteInstance& instance) {
	if (texture == nullptr || sprite_instances.size() >= MAX_SPRITE_INSTANCES ||
		instance.size.x <= 0.0F || instance.size.y <= 0.0F) {
		return;
	}
	const u32 index = static_cast<u32>(sprite_instances.size());
	sprite_instances.push_back(instance);
	if (!sprite_commands.empty() && sprite_commands.back().texture == texture &&
		sprite_commands.back().first_instance + sprite_commands.back().instance_count == index) {
		++sprite_commands.back().instance_count;
	} else {
		sprite_commands.push_back({texture, index, 1});
	}
}

void Renderer::draw_sprite(SpriteIcon icon, vec2 origin, vec2 size, vec4 tint) {
	const usize index = static_cast<usize>(icon);
	if (index >= sprite_icons.size()) {
		return;
	}
	queue_sprite(sprite_icons[index].handle, SpriteInstance{
												 .origin = origin,
												 .size = size,
												 .uv_rect = vec4{0.0F, 0.0F, 1.0F, 1.0F},
												 .color = tint,
												 .parameters = vec4{0.0F, 15.0F, 0.0F, 0.0F},
												 .corner_radii = vec4{0.0F},
											 });
}

void Renderer::draw_rounded_rect(vec2 origin, vec2 size, f32 radius, vec4 fill, f32 border_width,
								 vec4 border) {
	queue_sprite(white_texture.handle,
				 SpriteInstance{
					 .origin = origin,
					 .size = size,
					 // Rounded rectangles repurpose the auxiliary field for RGB border color
					 // and world-space border width.
					 .uv_rect = vec4{vec3{border}, std::max(border_width, 0.0F)},
					 .color = fill,
					 .parameters = vec4{0.0F, 15.0F, 2.0F, 0.0F},
					 .corner_radii = vec4{std::max(radius, 0.0F)},
				 });
}

void Renderer::draw_building(vec2i grid_origin, vec2i footprint, SpriteIcon icon, vec4 fill,
							 vec4 border) {
	if (footprint.x <= 0 || footprint.y <= 0) {
		return;
	}
	const vec2 origin = vec2{grid_origin} * TILE_SIZE;
	const vec2 size = vec2{footprint} * TILE_SIZE;
	const f32 inset = 3.0F;
	draw_rounded_rect(origin + inset, size - inset * 2.0F,
					  std::min(TILE_SIZE * 0.28F, std::min(size.x, size.y) * 0.2F), fill, 2.5F,
					  border);
	const f32 icon_size = std::min(size.x, size.y) * 0.48F;
	draw_sprite(icon, origin + (size - vec2{icon_size}) * 0.5F, vec2{icon_size});
}

void Renderer::rebuild_terrain_instances(const Terrain& terrain) {
	terrain_instances.clear();
	const auto encoded_cell = [&](i32 x, i32 y) {
		const vec2i coordinate{x, y};
		if (!Terrain::contains(coordinate)) {
			return u32{0};
		}
		return pack_cell(terrain.at(coordinate));
	};

	for (i32 y = 0; y < static_cast<i32>(TERRAIN_SIZE); ++y) {
		for (i32 x = 0; x < static_cast<i32>(TERRAIN_SIZE); ++x) {
			// Five 5-bit cells fit in one uint: center, top, right, bottom, left.
			const u32 neighborhood = encoded_cell(x, y) | (encoded_cell(x, y - 1) << CELL_BITS) |
									 (encoded_cell(x + 1, y) << (CELL_BITS * 2U)) |
									 (encoded_cell(x, y + 1) << (CELL_BITS * 3U)) |
									 (encoded_cell(x - 1, y) << (CELL_BITS * 4U));
			terrain_instances.push_back(TerrainInstance{
				.origin = vec2{static_cast<f32>(x), static_cast<f32>(y)} * TILE_SIZE,
				.neighborhood = neighborhood,
			});
		}
	}
}

bool Renderer::upload_terrain(SDL_GPUCommandBuffer* command) {
	if (pending_terrain == nullptr) {
		return true;
	}
	const u64 revision = pending_terrain->revision();
	if (cached_terrain == pending_terrain && uploaded_revision == revision) {
		return true;
	}
	rebuild_terrain_instances(*pending_terrain);
	const u32 bytes = static_cast<u32>(terrain_instances.size() * sizeof(TerrainInstance));
	void* mapped = SDL_MapGPUTransferBuffer(device, transfer_buffer, true);
	if (mapped == nullptr) {
		log::error("Failed to map terrain instance upload: {}", SDL_GetError());
		return false;
	}
	std::memcpy(mapped, terrain_instances.data(), bytes);
	SDL_UnmapGPUTransferBuffer(device, transfer_buffer);
	SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(command);
	const SDL_GPUTransferBufferLocation source{.transfer_buffer = transfer_buffer, .offset = 0};
	const SDL_GPUBufferRegion destination{.buffer = instance_buffer, .offset = 0, .size = bytes};
	SDL_UploadToGPUBuffer(copy, &source, &destination, true);
	SDL_EndGPUCopyPass(copy);
	cached_terrain = pending_terrain;
	uploaded_revision = revision;
	return true;
}

bool Renderer::upload_sprites(SDL_GPUCommandBuffer* command) {
	if (sprite_instances.empty()) {
		return true;
	}
	const u32 bytes = static_cast<u32>(sprite_instances.size() * sizeof(SpriteInstance));
	void* mapped = SDL_MapGPUTransferBuffer(device, sprite_transfer_buffer, true);
	if (mapped == nullptr) {
		log::error("Failed to map sprite instance upload: {}", SDL_GetError());
		return false;
	}
	std::memcpy(mapped, sprite_instances.data(), bytes);
	SDL_UnmapGPUTransferBuffer(device, sprite_transfer_buffer);
	SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(command);
	const SDL_GPUTransferBufferLocation source{.transfer_buffer = sprite_transfer_buffer,
											   .offset = 0};
	const SDL_GPUBufferRegion destination{
		.buffer = sprite_instance_buffer, .offset = 0, .size = bytes};
	SDL_UploadToGPUBuffer(copy, &source, &destination, true);
	SDL_EndGPUCopyPass(copy);
	return true;
}

bool Renderer::end_frame() {
	SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
	if (command == nullptr) {
		log::error("Failed to acquire GPU command buffer: {}", SDL_GetError());
		return false;
	}
	if (!upload_terrain(command) || !upload_sprites(command)) {
		SDL_CancelGPUCommandBuffer(command);
		return false;
	}
	SDL_GPUTexture* swapchain = nullptr;
	u32 width = 0;
	u32 height = 0;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(command, window, &swapchain, &width, &height)) {
		log::error("Failed to acquire swapchain texture: {}", SDL_GetError());
		SDL_CancelGPUCommandBuffer(command);
		return false;
	}
	if (swapchain == nullptr) {
		return SDL_SubmitGPUCommandBuffer(command);
	}

	const SDL_GPUColorTargetInfo target{
		.texture = swapchain,
		.clear_color = {clear_color.r, clear_color.g, clear_color.b, clear_color.a},
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_STORE,
	};
	SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(command, &target, 1, nullptr);
	if (pending_terrain != nullptr && !terrain_instances.empty()) {
		SDL_BindGPUGraphicsPipeline(pass, terrain_pipeline);
		f32 mouse_x = 0.0F;
		f32 mouse_y = 0.0F;
		SDL_GetMouseState(&mouse_x, &mouse_y);
		const vec2 cursor_world_position =
			view_position + vec2{mouse_x, mouse_y} / view_zoom;
		const CameraUniform camera{
			.viewport_size = vec2{static_cast<f32>(width), static_cast<f32>(height)},
			.view_position = view_position,
			.cursor_world_position = cursor_world_position,
			.zoom = view_zoom,
			.transition_radius = TRANSITION_RADIUS,
			.height_darkening = HEIGHT_DARKENING,
			.tile_size = TILE_SIZE,
			.grid_world_width = GRID_WORLD_WIDTH,
			.grid_min_pixel_width = 1.0F,
			.grid_strength = GRID_STRENGTH,
			.grid_enabled = 1.0F,
			.grid_fade_radius = GRID_FADE_RADIUS,
		};
		SDL_PushGPUVertexUniformData(command, 0, &camera, sizeof(camera));
		const std::array<SDL_GPUBufferBinding, 2> bindings{{
			{.buffer = vertex_buffer, .offset = 0},
			{.buffer = instance_buffer, .offset = 0},
		}};
		SDL_BindGPUVertexBuffers(pass, 0, bindings.data(), static_cast<u32>(bindings.size()));
		const SDL_GPUBufferBinding index_binding{.buffer = index_buffer, .offset = 0};
		SDL_BindGPUIndexBuffer(pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_DrawGPUIndexedPrimitives(pass, static_cast<u32>(QUAD_INDICES.size()),
									 static_cast<u32>(terrain_instances.size()), 0, 0, 0);
	}
	if (!sprite_instances.empty()) {
		SDL_BindGPUGraphicsPipeline(pass, sprite_pipeline);
		const SpriteCameraUniform camera{
			.viewport_size = vec2{static_cast<f32>(width), static_cast<f32>(height)},
			.view_position = view_position,
			.zoom = view_zoom,
		};
		SDL_PushGPUVertexUniformData(command, 0, &camera, sizeof(camera));
		const std::array<SDL_GPUBufferBinding, 2> bindings{{
			{.buffer = sprite_vertex_buffer, .offset = 0},
			{.buffer = sprite_instance_buffer, .offset = 0},
		}};
		SDL_BindGPUVertexBuffers(pass, 0, bindings.data(), static_cast<u32>(bindings.size()));
		const SDL_GPUBufferBinding index_binding{.buffer = sprite_index_buffer, .offset = 0};
		SDL_BindGPUIndexBuffer(pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		for (const SpriteDrawCommand& draw : sprite_commands) {
			const SDL_GPUTextureSamplerBinding texture_binding{
				.texture = draw.texture,
				.sampler = sprite_sampler,
			};
			SDL_BindGPUFragmentSamplers(pass, 0, &texture_binding, 1);
			SDL_DrawGPUIndexedPrimitives(pass, static_cast<u32>(QUAD_INDICES.size()),
										 draw.instance_count, 0, 0, draw.first_instance);
		}
	}
	SDL_EndGPURenderPass(pass);

	if (!SDL_SubmitGPUCommandBuffer(command)) {
		log::error("Failed to submit GPU command buffer: {}", SDL_GetError());
		return false;
	}
	return true;
}

void Renderer::release() {
	if (device == nullptr) {
		window = nullptr;
		return;
	}
	SDL_WaitForGPUIdle(device);
	for (SpriteTexture& texture : sprite_icons) {
		if (texture.handle != nullptr)
			SDL_ReleaseGPUTexture(device, texture.handle);
		texture = {};
	}
	if (white_texture.handle != nullptr)
		SDL_ReleaseGPUTexture(device, white_texture.handle);
	white_texture = {};
	if (sprite_sampler != nullptr)
		SDL_ReleaseGPUSampler(device, sprite_sampler);
	if (sprite_transfer_buffer != nullptr)
		SDL_ReleaseGPUTransferBuffer(device, sprite_transfer_buffer);
	if (sprite_instance_buffer != nullptr)
		SDL_ReleaseGPUBuffer(device, sprite_instance_buffer);
	if (sprite_index_buffer != nullptr)
		SDL_ReleaseGPUBuffer(device, sprite_index_buffer);
	if (sprite_vertex_buffer != nullptr)
		SDL_ReleaseGPUBuffer(device, sprite_vertex_buffer);
	if (transfer_buffer != nullptr)
		SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
	if (instance_buffer != nullptr)
		SDL_ReleaseGPUBuffer(device, instance_buffer);
	if (index_buffer != nullptr)
		SDL_ReleaseGPUBuffer(device, index_buffer);
	if (vertex_buffer != nullptr)
		SDL_ReleaseGPUBuffer(device, vertex_buffer);
	if (terrain_pipeline != nullptr)
		SDL_ReleaseGPUGraphicsPipeline(device, terrain_pipeline);
	if (sprite_pipeline != nullptr)
		SDL_ReleaseGPUGraphicsPipeline(device, sprite_pipeline);
	if (window != nullptr)
		SDL_ReleaseWindowFromGPUDevice(device, window);
	SDL_DestroyGPUDevice(device);
	transfer_buffer = nullptr;
	sprite_transfer_buffer = nullptr;
	instance_buffer = nullptr;
	sprite_instance_buffer = nullptr;
	index_buffer = nullptr;
	sprite_index_buffer = nullptr;
	vertex_buffer = nullptr;
	sprite_vertex_buffer = nullptr;
	terrain_pipeline = nullptr;
	sprite_pipeline = nullptr;
	sprite_sampler = nullptr;
	device = nullptr;
	window = nullptr;
	cached_terrain = nullptr;
	pending_terrain = nullptr;
	terrain_instances.clear();
	sprite_instances.clear();
	sprite_commands.clear();
}
