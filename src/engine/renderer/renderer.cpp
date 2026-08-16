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
		f32 zoom;
		vec3 padding{0.0F};
	};
	constexpr std::array<Vertex, 4> VERTICES{{
		{{-0.5F, -0.5F}, {0.0F, 0.0F}},
		{{0.5F, -0.5F}, {1.0F, 0.0F}},
		{{0.5F, 0.5F}, {1.0F, 1.0F}},
		{{-0.5F, 0.5F}, {0.0F, 1.0F}},
	}};
	constexpr std::array<u16, 6> INDICES{{0, 1, 2, 0, 2, 3}};

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
							   u32 uniforms, u32 samplers) {
		const std::string filename = std::string{name} + "." + shader_extension();
		const char* base = SDL_GetBasePath();
		const std::string path = std::string{base != nullptr ? base : ""} + "shaders/" + filename;
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
			.num_uniform_buffers = uniforms,
		};
		SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
		SDL_free(data);
		if (shader == nullptr)
			log::error("Failed to create shader {}: {}", filename, SDL_GetError());
		return shader;
	}
} // namespace

Renderer::~Renderer() {
	release();
}

bool Renderer::init(SDL_Window* target_window) {
	window = target_window;
	instances.reserve(MAX_INSTANCES);
	commands.reserve(256);
#if defined(NDEBUG)
	constexpr bool DEBUG_DEVICE = false;
#else
	constexpr bool DEBUG_DEVICE = true;
#endif
	device = SDL_CreateGPUDevice(shader_format(), DEBUG_DEVICE, nullptr);
	if (device == nullptr || !SDL_ClaimWindowForGPUDevice(device, window)) {
		log::error("Failed to initialize SDL_gpu: {}", SDL_GetError());
		release();
		return false;
	}
	if (!create_pipeline() || !create_buffers() || !upload_static_geometry() || !load_textures()) {
		release();
		return false;
	}
	log::info("Hex Factory renderer initialized with {}", SDL_GetGPUDeviceDriver(device));
	return true;
}

bool Renderer::create_pipeline() {
	SDL_GPUShader* vertex = load_shader(device, "factory.vert", SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
	SDL_GPUShader* fragment =
		load_shader(device, "factory.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 1);
	if (vertex == nullptr || fragment == nullptr) {
		if (vertex)
			SDL_ReleaseGPUShader(device, vertex);
		if (fragment)
			SDL_ReleaseGPUShader(device, fragment);
		return false;
	}
	const std::array<SDL_GPUVertexBufferDescription, 2> descriptions{{
		{.slot = 0, .pitch = sizeof(Vertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX},
		{.slot = 1, .pitch = sizeof(Instance), .input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE},
	}};
	const std::array<SDL_GPUVertexAttribute, 7> attributes{{
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
		 .offset = offsetof(Instance, center)},
		{.location = 3,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
		 .offset = offsetof(Instance, size)},
		{.location = 4,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		 .offset = offsetof(Instance, color)},
		{.location = 5,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		 .offset = offsetof(Instance, parameters)},
		{.location = 6,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		 .offset = offsetof(Instance, auxiliary)},
	}};
	const SDL_GPUVertexInputState input{
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
	const SDL_GPUGraphicsPipelineCreateInfo info{
		.vertex_shader = vertex,
		.fragment_shader = fragment,
		.vertex_input_state = input,
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL,
							 .cull_mode = SDL_GPU_CULLMODE_NONE,
							 .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE},
		.multisample_state = {.sample_count = SDL_GPU_SAMPLECOUNT_1},
		.target_info = {.color_target_descriptions = &target, .num_color_targets = 1},
	};
	pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
	SDL_ReleaseGPUShader(device, vertex);
	SDL_ReleaseGPUShader(device, fragment);
	if (pipeline == nullptr)
		log::error("Failed to create factory pipeline: {}", SDL_GetError());
	return pipeline != nullptr;
}

bool Renderer::create_buffers() {
	const SDL_GPUBufferCreateInfo vertex_info{.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
											  .size = sizeof(VERTICES)};
	const SDL_GPUBufferCreateInfo index_info{.usage = SDL_GPU_BUFFERUSAGE_INDEX,
											 .size = sizeof(INDICES)};
	const SDL_GPUBufferCreateInfo instance_info{
		.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
		.size = static_cast<u32>(MAX_INSTANCES * sizeof(Instance))};
	const SDL_GPUTransferBufferCreateInfo transfer_info{.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
														.size = instance_info.size};
	vertex_buffer = SDL_CreateGPUBuffer(device, &vertex_info);
	index_buffer = SDL_CreateGPUBuffer(device, &index_info);
	instance_buffer = SDL_CreateGPUBuffer(device, &instance_info);
	transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
	const SDL_GPUSamplerCreateInfo sampler_info{
		.min_filter = SDL_GPU_FILTER_LINEAR,
		.mag_filter = SDL_GPU_FILTER_LINEAR,
		.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	};
	sampler = SDL_CreateGPUSampler(device, &sampler_info);
	if (!vertex_buffer || !index_buffer || !instance_buffer || !transfer_buffer || !sampler)
		log::error("Failed to allocate renderer buffers: {}", SDL_GetError());
	return vertex_buffer && index_buffer && instance_buffer && transfer_buffer && sampler;
}

bool Renderer::upload_static_geometry() {
	void* mapped = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
	if (!mapped)
		return false;
	std::memcpy(mapped, VERTICES.data(), sizeof(VERTICES));
	std::memcpy(static_cast<u8*>(mapped) + sizeof(VERTICES), INDICES.data(), sizeof(INDICES));
	SDL_UnmapGPUTransferBuffer(device, transfer_buffer);
	SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
	if (!command)
		return false;
	SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(command);
	const SDL_GPUTransferBufferLocation vertex_source{.transfer_buffer = transfer_buffer,
													  .offset = 0};
	const SDL_GPUBufferRegion vertex_dest{
		.buffer = vertex_buffer, .offset = 0, .size = sizeof(VERTICES)};
	SDL_UploadToGPUBuffer(copy, &vertex_source, &vertex_dest, false);
	const SDL_GPUTransferBufferLocation index_source{.transfer_buffer = transfer_buffer,
													 .offset = sizeof(VERTICES)};
	const SDL_GPUBufferRegion index_dest{
		.buffer = index_buffer, .offset = 0, .size = sizeof(INDICES)};
	SDL_UploadToGPUBuffer(copy, &index_source, &index_dest, false);
	SDL_EndGPUCopyPass(copy);
	return SDL_SubmitGPUCommandBuffer(command);
}

bool Renderer::upload_texture(Texture& texture, u32 width, u32 height, const u8* pixels) {
	const u32 bytes = width * height * 4;
	const SDL_GPUTextureCreateInfo texture_info{.type = SDL_GPU_TEXTURETYPE_2D,
												.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
												.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
												.width = width,
												.height = height,
												.layer_count_or_depth = 1,
												.num_levels = 1,
												.sample_count = SDL_GPU_SAMPLECOUNT_1};
	SDL_GPUTexture* handle = SDL_CreateGPUTexture(device, &texture_info);
	const SDL_GPUTransferBufferCreateInfo upload_info{.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
													  .size = bytes};
	SDL_GPUTransferBuffer* upload = SDL_CreateGPUTransferBuffer(device, &upload_info);
	if (!handle || !upload)
		return false;
	void* mapped = SDL_MapGPUTransferBuffer(device, upload, false);
	if (!mapped)
		return false;
	std::memcpy(mapped, pixels, bytes);
	SDL_UnmapGPUTransferBuffer(device, upload);
	SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
	SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(command);
	const SDL_GPUTextureTransferInfo source{
		.transfer_buffer = upload, .pixels_per_row = width, .rows_per_layer = height};
	const SDL_GPUTextureRegion destination{.texture = handle, .w = width, .h = height, .d = 1};
	SDL_UploadToGPUTexture(copy, &source, &destination, false);
	SDL_EndGPUCopyPass(copy);
	const bool success = SDL_SubmitGPUCommandBuffer(command);
	SDL_ReleaseGPUTransferBuffer(device, upload);
	if (!success) {
		SDL_ReleaseGPUTexture(device, handle);
		return false;
	}
	texture.handle = handle;
	return true;
}

bool Renderer::load_texture(Texture& texture, const char* relative_path) {
	const char* base = SDL_GetBasePath();
	const std::string path = std::string{base != nullptr ? base : ""} + "res/" + relative_path;
	SDL_Surface* loaded = IMG_Load(path.c_str());
	if (!loaded) {
		log::error("Failed to load {}: {}", relative_path, SDL_GetError());
		return false;
	}
	SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
	SDL_DestroySurface(loaded);
	if (!rgba)
		return false;
	const usize row_bytes = static_cast<usize>(rgba->w) * 4;
	std::vector<u8> pixels(row_bytes * static_cast<usize>(rgba->h));
	for (i32 row = 0; row < rgba->h; ++row)
		std::memcpy(pixels.data() + static_cast<usize>(row) * row_bytes,
					static_cast<const u8*>(rgba->pixels) + static_cast<usize>(row) * rgba->pitch,
					row_bytes);
	const bool result = upload_texture(texture, static_cast<u32>(rgba->w),
									   static_cast<u32>(rgba->h), pixels.data());
	SDL_DestroySurface(rgba);
	return result;
}

bool Renderer::load_textures() {
	constexpr std::array<u8, 4> WHITE{{255, 255, 255, 255}};
	if (!upload_texture(white_texture, 1, 1, WHITE.data()))
		return false;
	constexpr std::array<const char*, ICON_COUNT> PATHS{{
		"sprites/ore.png",
		"sprites/gear.png",
		"sprites/ingot.png",
		"sprites/engine.png",
		"sprites/propeller.png",
		"sprites/smelter.png",
	}};
	for (usize index = 0; index < PATHS.size(); ++index)
		if (!load_texture(icons[index], PATHS[index]))
			return false;
	return true;
}

void Renderer::begin_frame(vec4 clear) {
	clear_color = clear;
	instances.clear();
	commands.clear();
}

void Renderer::set_view(vec2 position, f32 zoom) {
	view_position = position;
	view_zoom = std::max(zoom, 0.001F);
}

void Renderer::queue(SDL_GPUTexture* texture, const Instance& instance) {
	if (!texture || instances.size() >= MAX_INSTANCES || instance.size.x <= 0.0F ||
		instance.size.y <= 0.0F)
		return;
	const u32 index = static_cast<u32>(instances.size());
	instances.push_back(instance);
	if (!commands.empty() && commands.back().texture == texture &&
		commands.back().first + commands.back().count == index)
		++commands.back().count;
	else
		commands.push_back({texture, index, 1});
}

void Renderer::draw_hex(vec2 center, f32 radius, vec4 fill, f32 border_width, vec4 border) {
	queue(white_texture.handle, {center, vec2{radius * 2.0F}, fill,
								 vec4{1.0F, 0.0F, std::max(border_width, 0.0F), 0.0F}, border});
}

void Renderer::draw_circle(vec2 center, f32 radius, vec4 fill, f32 border_width, vec4 border) {
	queue(white_texture.handle, {center, vec2{radius * 2.0F}, fill,
								 vec4{2.0F, 0.0F, std::max(border_width, 0.0F), 0.0F}, border});
}

void Renderer::draw_capsule(vec2 start, vec2 end, f32 width, vec4 color) {
	const vec2 delta = end - start;
	const f32 length = glm::length(delta);
	if (length <= 0.001F)
		return;
	queue(white_texture.handle, {(start + end) * 0.5F,
								 {length, width},
								 color,
								 vec4{3.0F, std::atan2(delta.y, delta.x), 0.0F, 0.0F},
								 vec4{0.0F}});
}

void Renderer::draw_rounded_rect(vec2 origin, vec2 size, f32 radius, vec4 fill, f32 border_width,
								 vec4 border) {
	queue(white_texture.handle,
		  {origin + size * 0.5F, size, fill,
		   vec4{4.0F, 0.0F, std::max(border_width, 0.0F), std::max(radius, 0.0F)}, border});
}

void Renderer::draw_sprite(SpriteIcon icon, vec2 center, vec2 size, vec4 tint) {
	const usize index = static_cast<usize>(icon);
	if (index < icons.size())
		queue(icons[index].handle, {center, size, tint, vec4{0.0F}, vec4{0.0F}});
}

bool Renderer::end_frame() {
	SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
	if (!command)
		return false;
	if (!instances.empty()) {
		const u32 bytes = static_cast<u32>(instances.size() * sizeof(Instance));
		void* mapped = SDL_MapGPUTransferBuffer(device, transfer_buffer, true);
		if (!mapped) {
			SDL_CancelGPUCommandBuffer(command);
			return false;
		}
		std::memcpy(mapped, instances.data(), bytes);
		SDL_UnmapGPUTransferBuffer(device, transfer_buffer);
		SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(command);
		const SDL_GPUTransferBufferLocation source{.transfer_buffer = transfer_buffer, .offset = 0};
		const SDL_GPUBufferRegion destination{
			.buffer = instance_buffer, .offset = 0, .size = bytes};
		SDL_UploadToGPUBuffer(copy, &source, &destination, true);
		SDL_EndGPUCopyPass(copy);
	}
	SDL_GPUTexture* swapchain = nullptr;
	u32 width = 0, height = 0;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(command, window, &swapchain, &width, &height)) {
		SDL_CancelGPUCommandBuffer(command);
		return false;
	}
	if (!swapchain)
		return SDL_SubmitGPUCommandBuffer(command);
	const SDL_GPUColorTargetInfo target{
		.texture = swapchain,
		.clear_color = {clear_color.r, clear_color.g, clear_color.b, clear_color.a},
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_STORE};
	SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(command, &target, 1, nullptr);
	if (!instances.empty()) {
		SDL_BindGPUGraphicsPipeline(pass, pipeline);
		const CameraUniform camera{
			{static_cast<f32>(width), static_cast<f32>(height)}, view_position, view_zoom};
		SDL_PushGPUVertexUniformData(command, 0, &camera, sizeof(camera));
		const std::array<SDL_GPUBufferBinding, 2> bindings{{
			{.buffer = vertex_buffer, .offset = 0},
			{.buffer = instance_buffer, .offset = 0},
		}};
		SDL_BindGPUVertexBuffers(pass, 0, bindings.data(), static_cast<u32>(bindings.size()));
		const SDL_GPUBufferBinding index_binding{.buffer = index_buffer, .offset = 0};
		SDL_BindGPUIndexBuffer(pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		for (const DrawCommand& draw : commands) {
			const SDL_GPUTextureSamplerBinding texture{.texture = draw.texture, .sampler = sampler};
			SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
			SDL_DrawGPUIndexedPrimitives(pass, static_cast<u32>(INDICES.size()), draw.count, 0, 0,
										 draw.first);
		}
	}
	SDL_EndGPURenderPass(pass);
	return SDL_SubmitGPUCommandBuffer(command);
}

void Renderer::release() {
	if (!device) {
		window = nullptr;
		return;
	}
	SDL_WaitForGPUIdle(device);
	for (Texture& icon : icons) {
		if (icon.handle)
			SDL_ReleaseGPUTexture(device, icon.handle);
		icon = {};
	}
	if (white_texture.handle)
		SDL_ReleaseGPUTexture(device, white_texture.handle);
	if (sampler)
		SDL_ReleaseGPUSampler(device, sampler);
	if (transfer_buffer)
		SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
	if (instance_buffer)
		SDL_ReleaseGPUBuffer(device, instance_buffer);
	if (index_buffer)
		SDL_ReleaseGPUBuffer(device, index_buffer);
	if (vertex_buffer)
		SDL_ReleaseGPUBuffer(device, vertex_buffer);
	if (pipeline)
		SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
	if (window)
		SDL_ReleaseWindowFromGPUDevice(device, window);
	SDL_DestroyGPUDevice(device);
	window = nullptr;
	device = nullptr;
	pipeline = nullptr;
	vertex_buffer = nullptr;
	index_buffer = nullptr;
	instance_buffer = nullptr;
	transfer_buffer = nullptr;
	sampler = nullptr;
	white_texture = {};
}
