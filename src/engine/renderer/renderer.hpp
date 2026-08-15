#pragma once

#include "common/types.hpp"
#include "core/terrain/terrain.hpp"

#include <array>
#include <vector>

struct SDL_GPUBuffer;
struct SDL_GPUCommandBuffer;
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
	Count,
};

class Renderer {
  public:
	Renderer() = default;
	~Renderer();
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;

	bool init(SDL_Window* target_window);
	void release();

	void begin_frame(vec4 clear = rgba(31, 35, 42));
	bool end_frame();

	void set_view(vec2 position, f32 zoom);
	void draw_terrain(const Terrain& terrain);
	void draw_sprite(SpriteIcon icon, vec2 origin, vec2 size, vec4 tint = vec4{1.0F});
	void draw_rounded_rect(vec2 origin, vec2 size, f32 radius, vec4 fill,
		f32 border_width = 0.0F, vec4 border = vec4{0.0F});
	void draw_building(vec2i grid_origin, vec2i footprint, SpriteIcon icon, vec4 fill,
		vec4 border = rgba(48, 54, 59));
  private:
	struct TerrainInstance {
		vec2 origin;
		u32 neighborhood;
	};
	struct SpriteInstance {
		vec2 origin;
		vec2 size;
		vec4 uv_rect;
		vec4 color;
		vec4 parameters;
		vec4 corner_radii;
	};
	struct SpriteTexture {
		SDL_GPUTexture* handle{nullptr};
		u32 width{0};
		u32 height{0};
	};
	struct SpriteDrawCommand {
		SDL_GPUTexture* texture{nullptr};
		u32 first_instance{0};
		u32 instance_count{0};
	};

	static constexpr usize MAX_TERRAIN_INSTANCES = TERRAIN_SIZE * TERRAIN_SIZE;
	static constexpr usize MAX_SPRITE_INSTANCES = 4096;
	static constexpr usize SPRITE_ICON_COUNT = static_cast<usize>(SpriteIcon::Count);

	SDL_Window* window{nullptr};
	SDL_GPUDevice* device{nullptr};
	SDL_GPUGraphicsPipeline* terrain_pipeline{nullptr};
	SDL_GPUGraphicsPipeline* sprite_pipeline{nullptr};
	SDL_GPUBuffer* vertex_buffer{nullptr};
	SDL_GPUBuffer* index_buffer{nullptr};
	SDL_GPUBuffer* instance_buffer{nullptr};
	SDL_GPUTransferBuffer* transfer_buffer{nullptr};
	SDL_GPUBuffer* sprite_vertex_buffer{nullptr};
	SDL_GPUBuffer* sprite_index_buffer{nullptr};
	SDL_GPUBuffer* sprite_instance_buffer{nullptr};
	SDL_GPUTransferBuffer* sprite_transfer_buffer{nullptr};
	SDL_GPUSampler* sprite_sampler{nullptr};
	SpriteTexture white_texture{};
	std::array<SpriteTexture, SPRITE_ICON_COUNT> sprite_icons{};

	const Terrain* pending_terrain{nullptr};
	const Terrain* cached_terrain{nullptr};
	u64 uploaded_revision{0};
	std::vector<TerrainInstance> terrain_instances{};
	std::vector<SpriteInstance> sprite_instances{};
	std::vector<SpriteDrawCommand> sprite_commands{};

	vec2 view_position{0.0F};
	f32 view_zoom{1.0F};
	vec4 clear_color{rgba(31, 35, 42)};

	bool create_terrain_pipeline();
	bool create_sprite_pipeline();
	bool create_buffers();
	bool upload_static_geometry();
	bool load_sprite_textures();
	bool load_sprite_texture(SpriteTexture& texture, const char* relative_path);
	bool upload_texture(SpriteTexture& texture, u32 width, u32 height, const u8* pixels);
	bool upload_terrain(SDL_GPUCommandBuffer* command);
	bool upload_sprites(SDL_GPUCommandBuffer* command);
	void queue_sprite(SDL_GPUTexture* texture, const SpriteInstance& instance);
	void rebuild_terrain_instances(const Terrain& terrain);
};
