#include "viewer.hpp"

#include "common/globals.hpp"

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace {
	vec4 item_color(ItemKind item) {
		switch (item) {
		case ItemKind::Ore:
			return rgba(129, 183, 151);
		case ItemKind::Ingot:
			return rgba(230, 205, 132);
		case ItemKind::Gear:
			return rgba(143, 190, 238);
		}
		return vec4{1.0F};
	}

	vec2 quadratic_point(vec2 start, vec2 control, vec2 end, f32 amount) {
		const f32 inverse = 1.0F - amount;
		return inverse * inverse * start + 2.0F * inverse * amount * control +
			   amount * amount * end;
	}

	vec2 quadratic_tangent(vec2 start, vec2 control, vec2 end, f32 amount) {
		return 2.0F * (1.0F - amount) * (control - start) +
			   2.0F * amount * (end - control);
	}

	void draw_belt_curve(vec2 start, vec2 control, vec2 end, vec4 color, f32 phase) {
		constexpr i32 CURVE_SEGMENTS = 12;
		constexpr f32 LINE_WIDTH = 5.5F;
		vec2 previous = start;
		f32 approximate_length = 0.0F;
		for (i32 segment = 1; segment <= CURVE_SEGMENTS; ++segment) {
			const f32 amount = static_cast<f32>(segment) / static_cast<f32>(CURVE_SEGMENTS);
			const vec2 current = quadratic_point(start, control, end, amount);
			g_renderer->draw_capsule(previous, current, LINE_WIDTH, color);
			approximate_length += glm::length(current - previous);
			previous = current;
		}

		const i32 chevron_count = std::max(1, static_cast<i32>(approximate_length / 34.0F));
		for (i32 index = 0; index < chevron_count; ++index) {
			const f32 amount = std::fmod(phase +
									 static_cast<f32>(index) / static_cast<f32>(chevron_count),
								 1.0F);
			const vec2 point = quadratic_point(start, control, end, amount);
			vec2 tangent = quadratic_tangent(start, control, end, amount);
			if (glm::length(tangent) <= 0.001F)
				continue;
			tangent = glm::normalize(tangent);
			const vec2 normal{-tangent.y, tangent.x};
			const vec2 tip = point + tangent * 5.5F;
			const vec2 tail = point - tangent * 4.0F;
			g_renderer->draw_capsule(tail + normal * 5.0F, tip, 2.3F, color);
			g_renderer->draw_capsule(tail - normal * 5.0F, tip, 2.3F, color);
		}
	}

	void draw_inserter_socket(vec2 center, vec2 machine, vec2 belt, f32 animation) {
		constexpr f32 PI = 3.14159265358979323846F;
		constexpr f32 TWO_PI = PI * 2.0F;
		const vec4 shadow = rgba(24, 29, 35);
		const vec4 shell = rgba(229, 209, 163);
		const vec4 pivot = rgba(224, 146, 55);
		const vec4 status = rgba(89, 220, 225);

		const vec2 machine_direction = glm::normalize(machine - center);
		const f32 machine_angle = std::atan2(machine_direction.y, machine_direction.x);
		const vec2 belt_direction = glm::normalize(belt - center);
		const f32 belt_angle = std::atan2(belt_direction.y, belt_direction.x);
		f32 sweep = belt_angle - machine_angle;
		while (sweep > PI)
			sweep -= TWO_PI;
		while (sweep < -PI)
			sweep += TWO_PI;
		const f32 angle = machine_angle + sweep * animation;
		const vec2 direction{std::cos(angle), std::sin(angle)};
		const vec2 normal{-direction.y, direction.x};
		const vec2 wrist = center + direction * 12.0F;
		const vec2 claw = center + direction * 17.0F;

		g_renderer->draw_circle(center + vec2{1.5F, 2.0F}, 11.0F, shadow);
		g_renderer->draw_circle(center, 9.0F, shell, 1.5F, shadow);
		g_renderer->draw_circle(center + vec2{0.0F, 7.0F}, 2.1F, status);
		g_renderer->draw_capsule(center, wrist, 7.0F, shadow);
		g_renderer->draw_capsule(center, wrist, 4.5F, shell);
		g_renderer->draw_capsule(wrist + normal * 3.5F, claw + normal * 5.0F, 4.5F, shadow);
		g_renderer->draw_capsule(wrist - normal * 3.5F, claw - normal * 5.0F, 4.5F, shadow);
		g_renderer->draw_capsule(wrist + normal * 3.5F, claw + normal * 5.0F, 2.6F, pivot);
		g_renderer->draw_capsule(wrist - normal * 3.5F, claw - normal * 5.0F, 2.6F, pivot);
		g_renderer->draw_circle(center, 5.3F, pivot, 1.4F, shadow);
		g_renderer->draw_circle(center, 2.4F, shell);
	}
} // namespace

Viewer::Viewer(const State& state) : state(state) {
}

void Viewer::update() {
	constexpr f32 PAN_SPEED = 600.0F;
	constexpr f32 ZOOM_SPEED = 0.15F;
	constexpr f32 MIN_ZOOM = 0.35F;
	constexpr f32 MAX_ZOOM = 3.0F;
	vec2 direction{0.0F};
	if (g_input->key_down(SDL_SCANCODE_A) || g_input->key_down(SDL_SCANCODE_LEFT))
		direction.x -= 1.0F;
	if (g_input->key_down(SDL_SCANCODE_D) || g_input->key_down(SDL_SCANCODE_RIGHT))
		direction.x += 1.0F;
	if (g_input->key_down(SDL_SCANCODE_W) || g_input->key_down(SDL_SCANCODE_UP))
		direction.y -= 1.0F;
	if (g_input->key_down(SDL_SCANCODE_S) || g_input->key_down(SDL_SCANCODE_DOWN))
		direction.y += 1.0F;
	if (dot(direction, direction) > 0.0F)
		view_position += normalize(direction) * PAN_SPEED * g_platform->delta_time() / view_zoom;
	if (g_input->mouse_button_down(SDL_BUTTON_MIDDLE) ||
		g_input->mouse_button_down(SDL_BUTTON_RIGHT))
		view_position -= g_input->mouse_delta() / view_zoom;
	const f32 wheel = g_input->wheel_delta().y;
	if (wheel != 0.0F) {
		const vec2 mouse = g_input->mouse_position();
		const vec2 anchor = screen_to_world(mouse);
		view_zoom = std::clamp(view_zoom * std::exp(wheel * ZOOM_SPEED), MIN_ZOOM, MAX_ZOOM);
		view_position = anchor - mouse / view_zoom;
	}
	g_renderer->set_view(view_position, view_zoom);
	if (shown_score != state.delivered_items()) {
		shown_score = state.delivered_items();
		const std::string title = std::format("Hex Factory  |  Delivered: {}  |  1 Belt  2 Miner  "
											  "3 Smelter  4 Assembler  5 Depot  6 Erase",
											  shown_score);
		SDL_SetWindowTitle(g_platform->window_handle(), title.c_str());
	}
}

void Viewer::draw() {
	const vec4 tile_fill = rgba(35, 44, 54);
	const vec4 tile_edge = rgba(47, 59, 70);
	for (i32 r = -State::GRID_RADIUS; r <= State::GRID_RADIUS; ++r) {
		for (i32 q = -State::GRID_RADIUS; q <= State::GRID_RADIUS; ++q) {
			const Hex cell{q, r};
			if (State::contains(cell))
				g_renderer->draw_hex(State::hex_to_world(cell), State::HEX_RADIUS - 1.15F,
									 tile_fill, 0.75F, tile_edge);
		}
	}

	const vec4 belt_color = rgba(105, 176, 190);
	const f32 belt_phase =
		std::fmod(static_cast<f32>(state.simulation_ticks()) * 0.018F, 1.0F);
	struct RenderEndpoint {
		const BeltEndpoint* endpoint;
		vec2 belt;
		vec2 machine;
		vec2 socket;
	};
	std::vector<RenderEndpoint> render_endpoints;
	for (const BeltEndpoint& endpoint : state.get_endpoints()) {
		const Building* building = nullptr;
		for (const Building& candidate : state.get_buildings())
			if (candidate.id == endpoint.building)
				building = &candidate;
		if (!building)
			continue;
		const vec2 belt_center = State::hex_to_world(endpoint.belt);
		vec2 closest = State::hex_to_world(building->origin);
		f32 closest_distance = dot(closest - belt_center, closest - belt_center);
		for (Hex cell : state.occupied_cells(*building)) {
			const vec2 candidate = State::hex_to_world(cell);
			const f32 candidate_distance = dot(candidate - belt_center, candidate - belt_center);
			if (candidate_distance < closest_distance) {
				closest = candidate;
				closest_distance = candidate_distance;
			}
		}
		const vec2 socket = glm::mix(closest, belt_center, 0.68F);
		render_endpoints.push_back({&endpoint, belt_center, closest, socket});
	}
	for (const Belt& belt : state.get_belts()) {
		const vec2 center = State::hex_to_world(belt.cell);
		std::vector<vec2> starts;
		std::vector<vec2> ends;
		for (usize direction = 0; direction < HEX_DIRECTIONS.size(); ++direction) {
			const vec2 neighbor = State::hex_to_world(belt.cell + HEX_DIRECTIONS[direction]);
			const vec2 edge = glm::mix(center, neighbor, 0.52F);
			if ((belt.inputs & (1U << direction)) != 0)
				starts.push_back(edge);
			if ((belt.outputs & (1U << direction)) != 0)
				ends.push_back(edge);
		}
		if (starts.empty())
			starts.push_back(center);
		if (ends.empty())
			ends.push_back(center);
		for (vec2 start : starts)
			for (vec2 end : ends)
				if (glm::length(end - start) > 0.001F)
					draw_belt_curve(start, center, end, belt_color, belt_phase);
	}
	for (const RenderEndpoint& render_endpoint : render_endpoints) {
		const vec2 start = render_endpoint.endpoint->building_to_belt ? render_endpoint.socket
															 : render_endpoint.belt;
		const vec2 end = render_endpoint.endpoint->building_to_belt ? render_endpoint.belt
														 : render_endpoint.socket;
		draw_belt_curve(start, glm::mix(start, end, 0.5F), end, belt_color, belt_phase);
	}

	for (const Building& building : state.get_buildings()) {
		const BuildingDefinition& def = State::definition(building.kind);
		const std::vector<Hex> cells = state.occupied_cells(building);
		vec2 center{0.0F};
		// Two group-wide passes leave the shadow only on the outer silhouette. The
		// slightly overlapping fills erase internal seams and read as one hex blob.
		for (Hex cell : cells)
			g_renderer->draw_hex(State::hex_to_world(cell), State::HEX_RADIUS + 1.6F,
								 rgba(15, 20, 26, 205));
		for (Hex cell : cells) {
			center += State::hex_to_world(cell);
			g_renderer->draw_hex(State::hex_to_world(cell), State::HEX_RADIUS + 0.2F, def.color);
		}
		center /= static_cast<f32>(def.footprint.size());
		const f32 icon_size = def.footprint.size() == 1 ? 38.0F : 47.0F;
		g_renderer->draw_circle(center, icon_size * 0.48F, rgba(20, 25, 31, 175));
		g_renderer->draw_sprite(def.icon, center, vec2{icon_size}, rgba(247, 244, 232));
		if (building.kind != BuildingKind::Sink) {
			const f32 amount =
				static_cast<f32>(building.progress) /
				static_cast<f32>(std::max(State::definition(building.kind).work_ticks, 1U));
			g_renderer->draw_capsule(center + vec2{-20.0F, 31.0F}, center + vec2{20.0F, 31.0F},
									 5.0F, rgba(24, 30, 37));
			g_renderer->draw_capsule(center + vec2{-20.0F, 31.0F},
									 center + vec2{-20.0F + 40.0F * amount, 31.0F}, 3.0F,
									 rgba(235, 221, 151));
		}
	}

	const f32 inserter_animation =
		0.5F - 0.5F * std::cos(static_cast<f32>(state.simulation_ticks()) * 0.09F);
	for (const RenderEndpoint& render_endpoint : render_endpoints)
		draw_inserter_socket(render_endpoint.socket, render_endpoint.machine,
							 render_endpoint.belt, inserter_animation);

	for (const Belt& belt : state.get_belts()) {
		if (!belt.item)
			continue;
		const vec2 center = State::hex_to_world(belt.cell);
		vec2 start = center;
		vec2 end = center;
		for (usize direction = 0; direction < HEX_DIRECTIONS.size(); ++direction) {
			if ((belt.inputs & (1U << direction)) != 0) {
				start = glm::mix(center,
							 State::hex_to_world(belt.cell + HEX_DIRECTIONS[direction]), 0.52F);
				break;
			}
		}
		for (usize direction = 0; direction < HEX_DIRECTIONS.size(); ++direction) {
			if ((belt.outputs & (1U << direction)) != 0) {
				end = glm::mix(center, State::hex_to_world(belt.cell + HEX_DIRECTIONS[direction]),
							   0.48F);
				break;
			}
		}
		for (const RenderEndpoint& render_endpoint : render_endpoints) {
			if (render_endpoint.endpoint->belt != belt.cell)
				continue;
			if (render_endpoint.endpoint->building_to_belt)
				start = render_endpoint.socket;
			else
				end = render_endpoint.socket;
		}
		const vec2 position = quadratic_point(start, center, end, belt.item->progress);
		g_renderer->draw_circle(position, 7.0F, item_color(belt.item->kind), 1.5F,
								rgba(25, 31, 37));
	}
}
