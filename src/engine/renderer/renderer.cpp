#include "renderer.hpp"
#include "common/log.hpp"

#include <SDL3/SDL.h>
#include <blend2d.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

struct Renderer::Impl {
	static constexpr usize ICON_COUNT = static_cast<usize>(SpriteIcon::Count);

	BLImage framebuffer{};
	BLContext context{};
	std::array<BLImage, ICON_COUNT> icons{};
	SDL_Surface* present_surface{nullptr};
	i32 width{0};
	i32 height{0};
	bool drawing{false};
};

namespace {
	u8 channel(f32 value) {
		return static_cast<u8>(std::round(std::clamp(value, 0.0F, 1.0F) * 255.0F));
	}

	BLRgba32 bl_color(vec4 value) {
		return BLRgba32(channel(value.r), channel(value.g), channel(value.b), channel(value.a));
	}

	BLPoint point(vec2 value) {
		return BLPoint{static_cast<f64>(value.x), static_cast<f64>(value.y)};
	}

	BLPath hex_path(vec2 center, f32 radius) {
		constexpr f64 PI = 3.14159265358979323846;
		BLPath path;
		for (i32 index = 0; index < 6; ++index) {
			const f64 angle = -PI * 0.5 + static_cast<f64>(index) * PI / 3.0;
			const f64 x = static_cast<f64>(center.x) + std::cos(angle) * radius;
			const f64 y = static_cast<f64>(center.y) + std::sin(angle) * radius;
			if (index == 0)
				path.moveTo(x, y);
			else
				path.lineTo(x, y);
		}
		path.close();
		return path;
	}
} // namespace

Renderer::Renderer() = default;

Renderer::~Renderer() {
	release();
}

bool Renderer::init(SDL_Window* target_window) {
	window = target_window;
	impl = new Impl{};
	if (!load_images()) {
		release();
		return false;
	}
	SDL_SetWindowSurfaceVSync(window, 1);
	log::info("Hex Factory renderer initialized with Blend2D");
	return true;
}

bool Renderer::load_images() {
	constexpr std::array<const char*, Impl::ICON_COUNT> PATHS{{
		"sprites/ore.png",
		"sprites/gear.png",
		"sprites/ingot.png",
		"sprites/engine.png",
		"sprites/propeller.png",
		"sprites/smelter.png",
	}};
	const char* base = SDL_GetBasePath();
	for (usize index = 0; index < PATHS.size(); ++index) {
		const std::string path = std::string{base != nullptr ? base : ""} + "res/" + PATHS[index];
		if (impl->icons[index].readFromFile(path.c_str()) != BL_SUCCESS) {
			log::error("Blend2D failed to load {}", path);
			return false;
		}
	}
	return true;
}

bool Renderer::resize_framebuffer(i32 width, i32 height) {
	if (width <= 0 || height <= 0)
		return false;
	if (impl->width == width && impl->height == height && impl->present_surface)
		return true;
	if (impl->present_surface) {
		SDL_DestroySurface(impl->present_surface);
		impl->present_surface = nullptr;
	}
	if (impl->framebuffer.create(width, height, BL_FORMAT_PRGB32) != BL_SUCCESS)
		return false;
	BLImageData data{};
	if (impl->framebuffer.makeMutable(&data) != BL_SUCCESS)
		return false;
	impl->present_surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_BGRA32,
											data.pixelData, static_cast<i32>(data.stride));
	if (!impl->present_surface) {
		log::error("Failed to create Blend2D presentation surface: {}", SDL_GetError());
		return false;
	}
	impl->width = width;
	impl->height = height;
	return true;
}

void Renderer::begin_frame(vec4 clear) {
	if (!impl || !window)
		return;
	SDL_Surface* window_surface = SDL_GetWindowSurface(window);
	if (!window_surface || !resize_framebuffer(window_surface->w, window_surface->h))
		return;
	if (impl->context.begin(impl->framebuffer) != BL_SUCCESS)
		return;
	impl->drawing = true;
	impl->context.setCompOp(BL_COMP_OP_SRC_COPY);
	impl->context.fillAll(bl_color(clear));
	impl->context.setCompOp(BL_COMP_OP_SRC_OVER);
}

void Renderer::set_view(vec2 position, f32 zoom) {
	view_position = position;
	view_zoom = std::max(zoom, 0.001F);
}

vec2 Renderer::to_screen(vec2 value) const {
	return (value - view_position) * view_zoom;
}

void Renderer::draw_hex(vec2 center, f32 radius, vec4 fill, f32 border_width, vec4 border) {
	if (!impl || !impl->drawing || radius <= 0.0F)
		return;
	const vec2 screen_center = to_screen(center);
	const f32 screen_radius = radius * view_zoom;
	const f32 screen_border = std::max(border_width, 0.0F) * view_zoom;
	if (screen_border > 0.0F)
		impl->context.fillPath(hex_path(screen_center, screen_radius), bl_color(border));
	const f32 inner_radius = std::max(screen_radius - screen_border, 0.0F);
	if (inner_radius > 0.0F)
		impl->context.fillPath(hex_path(screen_center, inner_radius), bl_color(fill));
}

void Renderer::draw_circle(vec2 center, f32 radius, vec4 fill, f32 border_width, vec4 border) {
	if (!impl || !impl->drawing || radius <= 0.0F)
		return;
	const vec2 screen_center = to_screen(center);
	const f32 screen_radius = radius * view_zoom;
	const f32 screen_border = std::max(border_width, 0.0F) * view_zoom;
	if (screen_border > 0.0F)
		impl->context.fillCircle(screen_center.x, screen_center.y, screen_radius, bl_color(border));
	const f32 inner_radius = std::max(screen_radius - screen_border, 0.0F);
	if (inner_radius > 0.0F)
		impl->context.fillCircle(screen_center.x, screen_center.y, inner_radius, bl_color(fill));
}

void Renderer::draw_capsule(vec2 start, vec2 end, f32 width, vec4 fill) {
	if (!impl || !impl->drawing || width <= 0.0F)
		return;
	const vec2 screen_start = to_screen(start);
	const vec2 screen_end = to_screen(end);
	impl->context.setStrokeWidth(width * view_zoom);
	impl->context.setStrokeCaps(BL_STROKE_CAP_ROUND);
	impl->context.strokeLine(point(screen_start), point(screen_end), bl_color(fill));
}

void Renderer::draw_rounded_rect(vec2 origin, vec2 size, f32 radius, vec4 fill,
								 f32 border_width, vec4 border) {
	if (!impl || !impl->drawing || size.x <= 0.0F || size.y <= 0.0F)
		return;
	const vec2 screen_origin = to_screen(origin);
	const vec2 screen_size = size * view_zoom;
	const f32 screen_radius = std::max(radius, 0.0F) * view_zoom;
	const f32 screen_border = std::max(border_width, 0.0F) * view_zoom;
	if (screen_border > 0.0F)
		impl->context.fillRoundRect(screen_origin.x, screen_origin.y, screen_size.x,
									screen_size.y, screen_radius, screen_radius, bl_color(border));
	const vec2 inner_size = screen_size - vec2{screen_border * 2.0F};
	if (inner_size.x > 0.0F && inner_size.y > 0.0F)
		impl->context.fillRoundRect(screen_origin.x + screen_border,
									screen_origin.y + screen_border, inner_size.x, inner_size.y,
									std::max(screen_radius - screen_border, 0.0F),
									std::max(screen_radius - screen_border, 0.0F), bl_color(fill));
}

void Renderer::draw_sprite(SpriteIcon icon, vec2 center, vec2 size, vec4 tint) {
	if (!impl || !impl->drawing || size.x <= 0.0F || size.y <= 0.0F)
		return;
	const usize index = static_cast<usize>(icon);
	if (index >= impl->icons.size())
		return;
	const vec2 screen_center = to_screen(center);
	const vec2 screen_size = size * view_zoom;
	const i32 width = std::max(static_cast<i32>(std::round(screen_size.x)), 1);
	const i32 height = std::max(static_cast<i32>(std::round(screen_size.y)), 1);
	BLImage scaled;
	if (BLImage::scale(scaled, impl->icons[index], BLSizeI{width, height},
					   BL_IMAGE_SCALE_FILTER_BILINEAR) != BL_SUCCESS)
		return;
	BLImage tinted{width, height, BL_FORMAT_PRGB32};
	BLContext tint_context{tinted};
	tint_context.setCompOp(BL_COMP_OP_SRC_COPY);
	tint_context.fillAll(bl_color(tint));
	tint_context.setCompOp(BL_COMP_OP_DST_IN);
	tint_context.blitImage(BLPointI{0, 0}, scaled);
	tint_context.end();
	impl->context.blitImage(BLPoint{screen_center.x - width * 0.5,
									screen_center.y - height * 0.5}, tinted);
}

bool Renderer::end_frame() {
	if (!impl || !impl->drawing)
		return false;
	impl->context.end();
	impl->drawing = false;
	SDL_Surface* window_surface = SDL_GetWindowSurface(window);
	if (!window_surface || !SDL_BlitSurface(impl->present_surface, nullptr, window_surface, nullptr)) {
		log::error("Failed to present Blend2D framebuffer: {}", SDL_GetError());
		return false;
	}
	return SDL_UpdateWindowSurface(window);
}

void Renderer::release() {
	if (!impl) {
		window = nullptr;
		return;
	}
	if (impl->drawing)
		impl->context.end();
	if (impl->present_surface)
		SDL_DestroySurface(impl->present_surface);
	delete impl;
	impl = nullptr;
	window = nullptr;
}
