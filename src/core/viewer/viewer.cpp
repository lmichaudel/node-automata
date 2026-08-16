#include "viewer.hpp"

#include "common/globals.hpp"
#include "core/state/state.hpp"

#include <SDL3/SDL_mouse.h>

Viewer::Viewer(const State& state) : state(state) {
}

void Viewer::update() {
	constexpr f32 PAN_SPEED = 500.0F;
	constexpr f32 ZOOM_SPEED = 0.15F;
	constexpr f32 MIN_ZOOM = 0.2F;
	constexpr f32 MAX_ZOOM = 5.0F;

	vec2 direction{0.0F};
	if (g_input->key_down(SDL_SCANCODE_A) || g_input->key_down(SDL_SCANCODE_LEFT))
		direction.x -= 1.0F;
	if (g_input->key_down(SDL_SCANCODE_D) || g_input->key_down(SDL_SCANCODE_RIGHT))
		direction.x += 1.0F;
	if (g_input->key_down(SDL_SCANCODE_W) || g_input->key_down(SDL_SCANCODE_UP))
		direction.y -= 1.0F;
	if (g_input->key_down(SDL_SCANCODE_S) || g_input->key_down(SDL_SCANCODE_DOWN))
		direction.y += 1.0F;
	if (dot(direction, direction) > 0.0F) {
		view_position += normalize(direction) * PAN_SPEED * g_platform->delta_time() / view_zoom;
	}
	if (g_input->mouse_button_down(SDL_BUTTON_MIDDLE) ||
		g_input->mouse_button_down(SDL_BUTTON_RIGHT)) {
		view_position -= g_input->mouse_delta() / view_zoom;
	}
	const f32 wheel = g_input->wheel_delta().y;
	if (wheel != 0.0F) {
		const vec2 mouse = g_input->mouse_position();
		const vec2 anchor = view_position + mouse / view_zoom;
		view_zoom = std::clamp(view_zoom * std::exp(wheel * ZOOM_SPEED), MIN_ZOOM, MAX_ZOOM);
		view_position = anchor - mouse / view_zoom;
	}
	g_renderer->set_view(view_position, view_zoom);
}

void Viewer::draw() {
	g_renderer->draw_terrain(state.get_terrain());
	for (const Building& building : state.get_buildings()) {
		g_renderer->draw_building(building.origin, building.footprint, building.icon,
								  building.color);
	}
}