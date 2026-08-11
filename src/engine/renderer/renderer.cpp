#include "renderer.hpp"
#include "common/log.hpp"
#include "engine/renderer/texture/texture.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstddef>

namespace {
	struct Vertex {
		vec2 corner;
		vec2 uv;
	};

	struct alignas(16) CameraUniform {
		vec2 viewport_size;
		vec2 padding{0.0F};
	};

	constexpr std::array<Vertex, 4> QUAD_VERTICES{{
		{{-0.5F, -0.5F}, {0.0F, 0.0F}},
		{{0.5F, -0.5F}, {1.0F, 0.0F}},
		{{0.5F, 0.5F}, {1.0F, 1.0F}},
		{{-0.5F, 0.5F}, {0.0F, 1.0F}},
	}};
	constexpr std::array<u16, 6> QUAD_INDICES{{0, 1, 2, 0, 2, 3}};

	static_assert(sizeof(Vertex) == 16);
} // namespace

Renderer::~Renderer() {
	release();
}

bool Renderer::init(SDL_Window* target_window) {
	if (device != nullptr || target_window == nullptr) {
		return false;
	}
	window = target_window;
	instances.reserve(MAX_SPRITES);
	for (Batch& batch : batches) {
		batch.instances.reserve(MAX_SPRITES / LAYER_COUNT);
		batch.commands.reserve(32);
	}

#if defined(NDEBUG)
	constexpr bool debug_device = false;
#else
	constexpr bool debug_device = true;
#endif
	device = SDL_CreateGPUDevice(Program::native_shader_format(), debug_device, nullptr);
	if (device == nullptr) {
		log::error("Failed to create SDL GPU device: {}", SDL_GetError());
		release();
		return false;
	}
	if (!SDL_ClaimWindowForGPUDevice(device, window)) {
		log::error("Failed to claim window for SDL GPU: {}", SDL_GetError());
		SDL_DestroyGPUDevice(device);
		device = nullptr;
		release();
		return false;
	}

	if (!create_program() || !create_buffers() || !create_samplers() || !upload_static_data()) {
		release();
		return false;
	}

	constexpr std::array<u8, 4> white_pixel{255, 255, 255, 255};
	white_texture = new Texture{};
	if (!white_texture->init(device, 1, 1, white_pixel, TextureFilter::Linear)) {
		delete white_texture;
		white_texture = nullptr;
		release();
		return false;
	}

	log::info("SDL GPU renderer initialized with {}", SDL_GetGPUDeviceDriver(device));
	return true;
}

bool Renderer::create_program() {
	const std::array<SDL_GPUVertexBufferDescription, 2> buffer_descriptions{{
		{.slot = 0,
		 .pitch = sizeof(Vertex),
		 .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
		 .instance_step_rate = 0},
		{.slot = 1,
		 .pitch = sizeof(Instance),
		 .input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE,
		 .instance_step_rate = 0},
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
		 .offset = offsetof(Instance, position)},
		{.location = 3,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
		 .offset = offsetof(Instance, size)},
		{.location = 4,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		 .offset = offsetof(Instance, uv_rect)},
		{.location = 5,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		 .offset = offsetof(Instance, color)},
		{.location = 6,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		 .offset = offsetof(Instance, parameters)},
	}};
	const SDL_GPUVertexInputState vertex_input{
		.vertex_buffer_descriptions = buffer_descriptions.data(),
		.num_vertex_buffers = buffer_descriptions.size(),
		.vertex_attributes = attributes.data(),
		.num_vertex_attributes = attributes.size(),
	};
	return program.init(device, SDL_GetGPUSwapchainTextureFormat(device, window), vertex_input);
}

bool Renderer::create_buffers() {
	static_assert(sizeof(Instance) == 64);
	return vertex_buffer.init(device, sizeof(QUAD_VERTICES), BufferUsage::Vertex) &&
		   index_buffer.init(device, sizeof(QUAD_INDICES), BufferUsage::Index) &&
		   instance_buffer.init(device, static_cast<u32>(MAX_SPRITES * sizeof(Instance)),
								BufferUsage::Vertex, true);
}

bool Renderer::create_samplers() {
	return nearest_sampler.init(device, SamplerFilter::Nearest) &&
		   linear_sampler.init(device, SamplerFilter::Linear);
}

bool Renderer::upload_static_data() {
	SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
	if (command == nullptr) {
		log::error("Failed to acquire geometry upload command buffer: {}", SDL_GetError());
		return false;
	}
	if (!vertex_buffer.upload(command, QUAD_VERTICES.data(), sizeof(QUAD_VERTICES)) ||
		!index_buffer.upload(command, QUAD_INDICES.data(), sizeof(QUAD_INDICES))) {
		SDL_CancelGPUCommandBuffer(command);
		return false;
	}
	const bool submitted = SDL_SubmitGPUCommandBuffer(command);
	if (!submitted) {
		log::error("Failed to submit static geometry upload: {}", SDL_GetError());
	}
	return submitted;
}

void Renderer::draw(const Sprite& sprite) {
	if (sprite.layer >= LAYER_COUNT || sprite.size.x <= 0.0F || sprite.size.y <= 0.0F) {
		return;
	}
	if (sprite_count >= MAX_SPRITES) {
		if (!overflow_reported) {
			log::warn("Sprite batch is full; additional sprites will be skipped");
			overflow_reported = true;
		}
		return;
	}

	Texture* texture = sprite.kind == SpriteKind::Texture && sprite.texture != nullptr
						   ? sprite.texture
						   : white_texture;
	Batch& batch = batches[sprite.layer];
	const u32 instance_index = static_cast<u32>(batch.instances.size());
	batch.instances.push_back(Instance{
		.position = sprite.position,
		.size = sprite.size,
		.uv_rect = sprite.uv_rect,
		.color = sprite.color,
		.parameters =
			vec4{sprite.rotation, sprite.corner_radius, static_cast<f32>(sprite.kind), 0.0F},
	});

	if (batch.commands.empty() || batch.commands.back().texture != texture) {
		batch.commands.push_back(
			DrawCommand{.texture = texture, .first_instance = instance_index, .instance_count = 1});
	} else {
		++batch.commands.back().instance_count;
	}
	++sprite_count;
}

bool Renderer::end_frame() {
	instances.clear();
	for (const Batch& batch : batches) {
		instances.insert(instances.end(), batch.instances.begin(), batch.instances.end());
	}

	SDL_GPUCommandBuffer* command = SDL_AcquireGPUCommandBuffer(device);
	if (command == nullptr) {
		log::error("Failed to acquire GPU command buffer: {}", SDL_GetError());
		return false;
	}

	SDL_GPUTexture* swapchain = nullptr;
	u32 width = 0;
	u32 height = 0;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(command, window, &swapchain, &width, &height)) {
		log::error("Failed to acquire GPU swapchain texture: {}", SDL_GetError());
		SDL_CancelGPUCommandBuffer(command);
		return false;
	}
	if (swapchain == nullptr) {
		return SDL_SubmitGPUCommandBuffer(command);
	}

	if (!instances.empty()) {
		const u32 byte_count = static_cast<u32>(instances.size() * sizeof(Instance));
		if (!instance_buffer.upload(command, instances.data(), byte_count, true)) {
			SDL_SubmitGPUCommandBuffer(command);
			return false;
		}
	}

	const SDL_GPUColorTargetInfo color_target{
		.texture = swapchain,
		.mip_level = 0,
		.layer_or_depth_plane = 0,
		.clear_color = {clear_color.r, clear_color.g, clear_color.b, clear_color.a},
		.load_op = SDL_GPU_LOADOP_CLEAR,
		.store_op = SDL_GPU_STOREOP_STORE,
		.resolve_texture = nullptr,
		.resolve_mip_level = 0,
		.resolve_layer = 0,
		.cycle = false,
		.cycle_resolve_texture = false,
		.padding1 = 0,
		.padding2 = 0,
	};
	SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command, &color_target, 1, nullptr);
	SDL_BindGPUGraphicsPipeline(render_pass, program.handle());

	if (!instances.empty()) {
		const CameraUniform camera{.viewport_size =
									   vec2{static_cast<f32>(width), static_cast<f32>(height)}};
		SDL_PushGPUVertexUniformData(command, 0, &camera, sizeof(camera));
		const std::array<SDL_GPUBufferBinding, 2> vertex_bindings{{
			{.buffer = vertex_buffer.handle(), .offset = 0},
			{.buffer = instance_buffer.handle(), .offset = 0},
		}};
		const SDL_GPUBufferBinding index_binding{.buffer = index_buffer.handle(), .offset = 0};
		SDL_BindGPUVertexBuffers(render_pass, 0, vertex_bindings.data(), vertex_bindings.size());
		SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

		u32 layer_offset = 0;
		for (const Batch& batch : batches) {
			for (const DrawCommand& draw : batch.commands) {
				SDL_GPUSampler* sampler = draw.texture->filter == TextureFilter::Nearest
											  ? nearest_sampler.handle()
											  : linear_sampler.handle();
				const SDL_GPUTextureSamplerBinding binding{
					.texture = draw.texture->gpu_texture,
					.sampler = sampler,
				};
				SDL_BindGPUFragmentSamplers(render_pass, 0, &binding, 1);
				SDL_DrawGPUIndexedPrimitives(render_pass, QUAD_INDICES.size(), draw.instance_count,
											 0, 0, layer_offset + draw.first_instance);
			}
			layer_offset += static_cast<u32>(batch.instances.size());
		}
	}

	SDL_EndGPURenderPass(render_pass);
	if (!SDL_SubmitGPUCommandBuffer(command)) {
		log::error("Failed to submit GPU command buffer: {}", SDL_GetError());
		return false;
	}
	return true;
}

void Renderer::release() {
	if (device != nullptr) {
		SDL_WaitForGPUIdle(device);

		delete white_texture;
		nearest_sampler.release();
		linear_sampler.release();

		vertex_buffer.release();
		index_buffer.release();
		instance_buffer.release();
		program.release();

		SDL_ReleaseWindowFromGPUDevice(device, window);
		SDL_DestroyGPUDevice(device);
	}

	instances.clear();
	for (Batch& batch : batches) {
		batch.instances.clear();
		batch.commands.clear();
	}
	sprite_count = 0;
	white_texture = nullptr;
	device = nullptr;
	window = nullptr;
}

void Renderer::begin_frame(vec4 clear_color) {
	this->clear_color = clear_color;
	instances.clear();
	for (Batch& batch : batches) {
		batch.instances.clear();
		batch.commands.clear();
	}
	sprite_count = 0;
	overflow_reported = false;
}

void Renderer::draw_texture(Texture* texture, vec2 center, vec2 size, vec4 color, vec4 uv_rect,
							f32 rotation, u8 layer) {
	draw(Sprite{.texture = texture,
				.position = center,
				.size = size,
				.uv_rect = uv_rect,
				.color = color,
				.rotation = rotation,
				.kind = SpriteKind::Texture,
				.layer = layer});
}

void Renderer::draw_circle(vec2 center, f32 radius, vec4 color, u8 layer) {
	if (radius > 0.0F) {
		draw(Sprite{.position = center,
					.size = vec2{radius * 2.0F},
					.color = color,
					.kind = SpriteKind::Circle,
					.layer = layer});
	}
}

void Renderer::draw_rounded_rect(vec2 center, vec2 size, f32 radius, vec4 color, f32 rotation,
								 u8 layer) {
	draw(Sprite{.position = center,
				.size = size,
				.color = color,
				.rotation = rotation,
				.corner_radius = std::max(radius, 0.0F),
				.kind = SpriteKind::RoundedRectangle,
				.layer = layer});
}

SDL_GPUDevice* Renderer::gpu_device() const {
	return device;
}
