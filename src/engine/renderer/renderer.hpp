#pragma once

#include "common/types.hpp"

#include <array>
#include <vector>

struct SDL_GPUBuffer;
struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUSampler;
struct SDL_GPUTexture;
struct SDL_GPUTransferBuffer;
struct SDL_Window;

enum class SpriteIcon : u8 {
	Ore,
	Gear,
	Ingot,
	Engine,
	Propeller,
	Smelter,
	Count
};

class Renderer {
  public:
	Renderer() = default;
	~Renderer();
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;

	bool init(SDL_Window* target_window);
	void release();
	void begin_frame(vec4 clear = rgba(22, 27, 34));
	bool end_frame();
	void set_view(vec2 position, f32 zoom);

	void draw_hex(vec2 center, f32 radius, vec4 fill, f32 border_width = 0.0F,
				  vec4 border = vec4{0.0F});
	void draw_circle(vec2 center, f32 radius, vec4 fill, f32 border_width = 0.0F,
					 vec4 border = vec4{0.0F});
	void draw_capsule(vec2 start, vec2 end, f32 width, vec4 color);
	void draw_rounded_rect(vec2 origin, vec2 size, f32 radius, vec4 fill, f32 border_width = 0.0F,
						   vec4 border = vec4{0.0F});
	void draw_sprite(SpriteIcon icon, vec2 center, vec2 size, vec4 tint = vec4{1.0F});

  private:
	struct Instance {
		vec2 center;
		vec2 size;
		vec4 color;
		vec4 parameters;
		vec4 auxiliary;
	};
	struct Texture {
		SDL_GPUTexture* handle{nullptr};
	};
	struct DrawCommand {
		SDL_GPUTexture* texture{nullptr};
		u32 first{0};
		u32 count{0};
	};

	static constexpr usize MAX_INSTANCES = 32768;
	static constexpr usize ICON_COUNT = static_cast<usize>(SpriteIcon::Count);

	SDL_Window* window{nullptr};
	SDL_GPUDevice* device{nullptr};
	SDL_GPUGraphicsPipeline* pipeline{nullptr};
	SDL_GPUBuffer* vertex_buffer{nullptr};
	SDL_GPUBuffer* index_buffer{nullptr};
	SDL_GPUBuffer* instance_buffer{nullptr};
	SDL_GPUTransferBuffer* transfer_buffer{nullptr};
	SDL_GPUSampler* sampler{nullptr};
	Texture white_texture{};
	std::array<Texture, ICON_COUNT> icons{};
	std::vector<Instance> instances{};
	std::vector<DrawCommand> commands{};
	vec2 view_position{-640.0F, -360.0F};
	f32 view_zoom{1.0F};
	vec4 clear_color{rgba(22, 27, 34)};

	bool create_pipeline();
	bool create_buffers();
	bool upload_static_geometry();
	bool load_textures();
	bool load_texture(Texture& texture, const char* relative_path);
	bool upload_texture(Texture& texture, u32 width, u32 height, const u8* pixels);
	void queue(SDL_GPUTexture* texture, const Instance& instance);
};
