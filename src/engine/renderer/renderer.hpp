#pragma once

#include "common/types.hpp"
#include "engine/debug/debugger.hpp"
#include "engine/renderer/buffer/buffer.hpp"
#include "engine/renderer/program/program.hpp"
#include "engine/renderer/sampler/sampler.hpp"
#include "engine/renderer/sprite/sprite.hpp"

#include <array>
#include <string_view>
#include <vector>

struct SDL_GPUDevice;
struct SDL_Window;
class Texture;
class Font;

class Renderer {
  public:
	static constexpr usize LAYER_COUNT = 12;

	Renderer() = default;
	~Renderer();
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;

	bool init(SDL_Window* window);
	void release();
	// Sets the world-space point shown at the top-left and the number of pixels per world unit.
	void set_view(vec2 position, f32 zoom = 1.0F);
	void begin_frame(vec4 clear_color = rgba(232, 225, 213));
	void draw(const Sprite& sprite);
	void draw_texture(Texture* texture, vec2 origin, vec2 size, vec4 color = vec4{1.0F},
					  vec4 uv_rect = vec4{0.0F, 0.0F, 1.0F, 1.0F}, f32 rotation = 0.0F,
					  u8 layer = 0);
	void draw_circle(vec2 origin, f32 radius, vec4 color, u8 layer = 0);
	// Draws a 90-degree ring from the left edge to either the bottom edge
	// (clockwise) or top edge (counter-clockwise), before applying rotation.
	void draw_quarter_ring(vec2 origin, f32 size, f32 thickness, bool clockwise, vec4 color,
						   f32 rotation = 0.0F, u8 layer = 0);
	void draw_grid(f32 cell_size, f32 line_width, f32 minimum_pixel_width, u32 supergrid_interval,
				   vec4 line_color, vec4 supergrid_color, u8 layer = 0);
	void draw_rounded_rect(vec2 origin, vec2 size, f32 radius, vec4 color, f32 rotation = 0.0F,
						   u8 layer = 0, AntialiasEdge antialiased_edges = AntialiasEdge::All);
	// Corner radii are ordered top-left, top-right, bottom-right, bottom-left.
	void draw_rounded_rect(vec2 origin, vec2 size, vec4 corner_radii, vec4 color,
						   f32 rotation = 0.0F, u8 layer = 0,
						   AntialiasEdge antialiased_edges = AntialiasEdge::All);
	void draw_text(const Font& font, std::string_view text, vec2 top_left, f32 font_size,
				   vec4 color = vec4{1.0F}, u8 layer = 0);
	bool end_frame();

	SDL_GPUDevice* gpu_device() const;
	Debugger& debugger() {
		return debug_overlay;
	}

  private:
	struct Instance {
		vec2 origin;
		vec2 size;
		vec4 uv_rect;
		vec4 color;
		vec4 parameters;
		vec4 corner_radii;
	};

	struct DrawCommand {
		Texture* texture;
		u32 first_instance;
		u32 instance_count;
	};

	struct Batch {
		std::vector<Instance> instances{};
		std::vector<DrawCommand> commands{};
	};

	static constexpr usize MAX_SPRITES = 16'384;

	SDL_Window* window{nullptr};
	SDL_GPUDevice* device{nullptr};
	Program program{};
	Buffer vertex_buffer{};
	Buffer index_buffer{};
	Buffer instance_buffer{};
	Sampler nearest_sampler{};
	Sampler linear_sampler{};
	Debugger debug_overlay{};
	Texture* white_texture{nullptr};
	std::vector<Instance> instances{};
	std::array<Batch, LAYER_COUNT> batches{};
	vec4 clear_color{0.0F};
	vec2 view_position{0.0F};
	f32 view_zoom{1.0F};
	usize sprite_count{0};

	bool overflow_reported{false};

	bool create_program();
	bool create_buffers();
	bool create_samplers();
	bool upload_static_data();
};
