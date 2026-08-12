#include "renderer.hpp"
#include "common/log.hpp"
#include "engine/debug/metrics.hpp"
#include "engine/renderer/font/font.hpp"
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
		vec2 view_position;
		f32 zoom;
		vec3 padding{0.0F};
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
	if (!debug_overlay.init(window, device, SDL_GetGPUSwapchainTextureFormat(device, window))) {
		log::error("Failed to initialize ImGui debugger");
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
		 .offset = offsetof(Instance, origin)},
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
		{.location = 7,
		 .buffer_slot = 1,
		 .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
		 .offset = offsetof(Instance, corner_radii)},
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
	static_assert(sizeof(Instance) == 80);
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

	const bool uses_texture = sprite.kind == SpriteKind::Texture || sprite.kind == SpriteKind::Msdf;
	Texture* texture = uses_texture && sprite.texture != nullptr ? sprite.texture : white_texture;
	Batch& batch = batches[sprite.layer];
	const u32 instance_index = static_cast<u32>(batch.instances.size());
	batch.instances.push_back(Instance{
		.origin = sprite.origin,
		.size = sprite.size,
		.uv_rect = sprite.uv_rect,
		.color = sprite.color,
		.parameters =
			vec4{sprite.rotation, static_cast<f32>(sprite.antialiased_edges),
				 static_cast<f32>(sprite.kind),
				 sprite.kind == SpriteKind::Msdf ? sprite.msdf_range : sprite.blur_radius},
		.corner_radii = max(sprite.corner_radii, vec4{0.0F}),
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
	METRIC_SCOPE("Renderer/CPU frame");
	{
		METRIC_SCOPE("Renderer/CPU batch assembly");
		instances.clear();
		for (const Batch& batch : batches) {
			instances.insert(instances.end(), batch.instances.begin(), batch.instances.end());
		}
	}

	u64 draw_calls = 0;
	u64 populated_layers = 0;
	for (const Batch& batch : batches) {
		draw_calls += batch.commands.size();
		populated_layers += !batch.instances.empty();
	}
	metrics::set("Renderer/Draw calls", static_cast<f64>(draw_calls), "calls");
	metrics::set("Renderer/World draw calls", static_cast<f64>(draw_calls), "calls");
	metrics::set("Renderer/Instances", static_cast<f64>(instances.size()), "instances");
	metrics::set("Renderer/Vertices", static_cast<f64>(instances.size() * 4), "vertices");
	metrics::set("Renderer/Triangles", static_cast<f64>(instances.size() * 2), "triangles");
	metrics::set("Renderer/Populated layers", static_cast<f64>(populated_layers), "layers");

	SDL_GPUCommandBuffer* command = nullptr;
	{
		METRIC_SCOPE("Renderer/CPU acquire command buffer");
		command = SDL_AcquireGPUCommandBuffer(device);
	}
	if (command == nullptr) {
		log::error("Failed to acquire GPU command buffer: {}", SDL_GetError());
		return false;
	}

	SDL_GPUTexture* swapchain = nullptr;
	u32 width = 0;
	u32 height = 0;
	bool acquired = false;
	{
		METRIC_SCOPE("Renderer/CPU acquire swapchain");
		acquired =
			SDL_WaitAndAcquireGPUSwapchainTexture(command, window, &swapchain, &width, &height);
	}
	if (!acquired) {
		log::error("Failed to acquire GPU swapchain texture: {}", SDL_GetError());
		SDL_CancelGPUCommandBuffer(command);
		return false;
	}
	if (swapchain == nullptr) {
		// Complete the ImGui frame even while minimized, or its next NewFrame would be invalid.
		debug_overlay.prepare_draw_data(command);
		return SDL_SubmitGPUCommandBuffer(command);
	}

	if (!instances.empty()) {
		METRIC_SCOPE("Renderer/CPU instance upload");
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
	{
		METRIC_SCOPE("Renderer/CPU world pass encode");
		SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command, &color_target, 1, nullptr);
		SDL_BindGPUGraphicsPipeline(render_pass, program.handle());

		if (!instances.empty()) {
			const CameraUniform camera{
				.viewport_size = vec2{static_cast<f32>(width), static_cast<f32>(height)},
				.view_position = view_position,
				.zoom = view_zoom,
			};
			SDL_PushGPUVertexUniformData(command, 0, &camera, sizeof(camera));
			const std::array<SDL_GPUBufferBinding, 2> vertex_bindings{{
				{.buffer = vertex_buffer.handle(), .offset = 0},
				{.buffer = instance_buffer.handle(), .offset = 0},
			}};
			const SDL_GPUBufferBinding index_binding{.buffer = index_buffer.handle(), .offset = 0};
			SDL_BindGPUVertexBuffers(render_pass, 0, vertex_bindings.data(),
									 vertex_bindings.size());
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
					SDL_DrawGPUIndexedPrimitives(render_pass, QUAD_INDICES.size(),
												 draw.instance_count, 0, 0,
												 layer_offset + draw.first_instance);
				}
				layer_offset += static_cast<u32>(batch.instances.size());
			}
		}

		SDL_EndGPURenderPass(render_pass);
	}

	{
		METRIC_SCOPE("Renderer/CPU ImGui pass encode");
		debug_overlay.prepare_draw_data(command);
		const SDL_GPUColorTargetInfo debug_target{
			.texture = swapchain,
			.mip_level = 0,
			.layer_or_depth_plane = 0,
			.clear_color = {},
			.load_op = SDL_GPU_LOADOP_LOAD,
			.store_op = SDL_GPU_STOREOP_STORE,
			.resolve_texture = nullptr,
			.resolve_mip_level = 0,
			.resolve_layer = 0,
			.cycle = false,
			.cycle_resolve_texture = false,
			.padding1 = 0,
			.padding2 = 0,
		};
		SDL_GPURenderPass* debug_pass = SDL_BeginGPURenderPass(command, &debug_target, 1, nullptr);
		debug_overlay.render(command, debug_pass);
		SDL_EndGPURenderPass(debug_pass);
	}

	bool submitted = false;
	{
		METRIC_SCOPE("Renderer/CPU submit");
		submitted = SDL_SubmitGPUCommandBuffer(command);
	}
	if (!submitted) {
		log::error("Failed to submit GPU command buffer: {}", SDL_GetError());
		return false;
	}
	return true;
}

void Renderer::release() {
	if (device != nullptr) {
		SDL_WaitForGPUIdle(device);
		debug_overlay.release();

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

void Renderer::set_view(vec2 position, f32 zoom) {
	view_position = position;
	view_zoom = max(zoom, 0.0001F);
}

void Renderer::draw_texture(Texture* texture, vec2 origin, vec2 size, vec4 color, vec4 uv_rect,
							f32 rotation, u8 layer) {
	draw(Sprite{.texture = texture,
				.origin = origin,
				.size = size,
				.uv_rect = uv_rect,
				.color = color,
				.rotation = rotation,
				.kind = SpriteKind::Texture,
				.layer = layer});
}

void Renderer::draw_circle(vec2 origin, f32 radius, vec4 color, u8 layer, f32 blur_radius) {
	if (radius > 0.0F) {
		draw(Sprite{.origin = origin,
					.size = vec2{radius * 2.0F},
					.color = color,
					.blur_radius = max(blur_radius, 0.0F),
					.kind = SpriteKind::Circle,
					.layer = layer});
	}
}

void Renderer::draw_line(vec2 start, vec2 end, f32 width, vec4 color, u8 layer,
					 f32 blur_radius) {
	const vec2 delta = end - start;
	const f32 line_length = length(delta);
	if (line_length > 0.0F && width > 0.0F) {
		const vec2 size{line_length, width};
		draw(Sprite{.origin = (start + end - size) * 0.5F,
					.size = size,
					.color = color,
					.rotation = atan(delta.y, delta.x),
					.blur_radius = max(blur_radius, 0.0F),
					.kind = SpriteKind::Line,
					.layer = layer});
	}
}

void Renderer::draw_rounded_line_90(vec2 center, f32 radius, f32 width, bool clockwise, vec4 color,
								f32 rotation, u8 layer, f32 blur_radius) {
	if (radius > 0.0F && width > 0.0F) {
		draw(Sprite{.origin = center - vec2{radius},
					.size = vec2{radius * 2.0F},
					.color = color,
					.rotation = rotation,
					.corner_radii = vec4{radius, width, clockwise ? 1.0F : 0.0F, 0.0F},
					.blur_radius = max(blur_radius, 0.0F),
					.kind = SpriteKind::RoundedLine90,
					.layer = layer});
	}
}

void Renderer::draw_quarter_ring(vec2 origin, f32 size, f32 thickness, bool clockwise, vec4 color,
								 f32 rotation, u8 layer, f32 blur_radius) {
	if (size > 0.0F && thickness > 0.0F) {
		draw(Sprite{.origin = origin,
					.size = vec2{size},
					.color = color,
					.rotation = rotation,
					.corner_radii = vec4{thickness, clockwise ? 1.0F : 0.0F, 0.0F, 0.0F},
					.blur_radius = max(blur_radius, 0.0F),
					.kind = SpriteKind::QuarterRing,
					.layer = layer});
	}
}

void Renderer::draw_grid(f32 cell_size, f32 line_width, f32 minimum_pixel_width,
						 u32 supergrid_interval, vec4 line_color, vec4 supergrid_color, u8 layer) {
	if (cell_size <= 0.0F || line_width <= 0.0F || minimum_pixel_width <= 0.0F ||
		supergrid_interval == 0) {
		return;
	}

	draw(Sprite{
		.size = vec2{1.0F},
		.uv_rect = supergrid_color,
		.color = line_color,
		.corner_radii =
			vec4{cell_size, line_width, minimum_pixel_width, static_cast<f32>(supergrid_interval)},
		.kind = SpriteKind::Grid,
		.layer = layer,
	});
}

void Renderer::draw_rounded_rect(vec2 origin, vec2 size, f32 radius, vec4 color, f32 rotation,
								 u8 layer, AntialiasEdge antialiased_edges, f32 blur_radius) {
	draw_rounded_rect(origin, size, vec4{radius}, color, rotation, layer, antialiased_edges,
					  blur_radius);
}

void Renderer::draw_rounded_rect(vec2 origin, vec2 size, vec4 corner_radii, vec4 color,
								 f32 rotation, u8 layer, AntialiasEdge antialiased_edges,
								 f32 blur_radius) {
	draw(Sprite{.origin = origin,
				.size = size,
				.color = color,
				.rotation = rotation,
				.corner_radii = max(corner_radii, vec4{0.0F}),
				.antialiased_edges = antialiased_edges,
				.blur_radius = max(blur_radius, 0.0F),
				.kind = SpriteKind::RoundedRectangle,
				.layer = layer});
}

void Renderer::draw_text(const Font& font, std::string_view text, vec2 top_left, f32 font_size,
						 vec4 color, u8 layer) {
	if (font_size <= 0.0F || layer >= LAYER_COUNT) {
		return;
	}

	vec2 pen{top_left.x, top_left.y - font.ascender * font_size};
	const f32 line_start = top_left.x;
	u32 previous = 0;
	usize offset = 0;
	while (offset < text.size()) {
		u32 codepoint = Font::next_codepoint(text, offset);
		if (codepoint == '\r') {
			continue;
		}
		if (codepoint == '\n') {
			pen.x = line_start;
			pen.y += font.line_height * font_size;
			previous = 0;
			continue;
		}

		const Font::Glyph* glyph = font.find_glyph(codepoint);
		if (glyph == nullptr) {
			codepoint = '?';
			glyph = font.find_glyph(codepoint);
		}
		if (glyph == nullptr) {
			previous = 0;
			continue;
		}

		if (previous != 0) {
			pen.x += font.kerning_adjustment(previous, codepoint) * font_size;
		}

		if (glyph->drawable) {
			const f32 left = pen.x + glyph->plane_bounds.x * font_size;
			const f32 top = pen.y + glyph->plane_bounds.y * font_size;
			const f32 right = pen.x + glyph->plane_bounds.z * font_size;
			const f32 bottom = pen.y + glyph->plane_bounds.w * font_size;
			const vec2 glyph_size{right - left, bottom - top};

			draw(Sprite{
				.texture = font.atlas,
				.origin = vec2{left, top},
				.size = glyph_size,
				.uv_rect = glyph->uv_bounds,
				.color = color,
				.msdf_range = font.distance_range,
				.kind = SpriteKind::Msdf,
				.layer = layer,
			});
		}

		pen.x += glyph->advance * font_size;
		previous = codepoint;
	}
}

SDL_GPUDevice* Renderer::gpu_device() const {
	return device;
}
