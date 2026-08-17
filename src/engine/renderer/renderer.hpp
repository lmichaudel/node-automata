#pragma once

#include "common/types.hpp"

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
	Renderer();
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
	void draw_rounded_rect(vec2 origin, vec2 size, f32 radius, vec4 fill,
						   f32 border_width = 0.0F, vec4 border = vec4{0.0F});
	void draw_sprite(SpriteIcon icon, vec2 center, vec2 size, vec4 tint = vec4{1.0F});

  private:
	struct Impl;
	Impl* impl{nullptr};
	SDL_Window* window{nullptr};
	vec2 view_position{-640.0F, -360.0F};
	f32 view_zoom{1.0F};

	vec2 to_screen(vec2 point) const;
	bool resize_framebuffer(i32 width, i32 height);
	bool load_images();
};
