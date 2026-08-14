#include "game.hpp"
#include "common/globals.hpp"
#include "common/log.hpp"
#include "content/colors.hpp"
#include "content/machines.hpp"
#include "core/belt/belt.hpp"
#include "core/junction/junction.hpp"
#include "core/machine/machine.hpp"
#include "engine/debug/metrics.hpp"

#include <SDL3/SDL_mouse.h>
#include <algorithm>
#include <array>
#include <cmath>

Game::Game() {
	constexpr vec2i SMELTER_POSITION{100, 100};
	create_machine(MachineType::SMELTER, SMELTER_POSITION);

	create_junction({150, 100});

	const auto& smelter = get_machine_type_data(MachineType::SMELTER);
	const vec2i smelter_output =
		SMELTER_POSITION + vec2i{smelter.size.x * static_cast<i32>(CELL_SIZE),
								 smelter.size.y * static_cast<i32>(CELL_SIZE) / 2};

	const ID belt = create_belt();
	belts.get<BeltRenderData>(belt).waypoints = {
		smelter_output, {310, smelter_output.y},
		{310, 310},		{450, 310},
		{450, 190},		{570, 190},
		{570, 390},		{750, 390},
		{750, 270},		{890, 270},
	};
}

void Game::tick() {
	{
		METRIC_SCOPE("Game/Tick Belt");
		for (auto& belt : belts.dense<BeltSimulationData>()) {
			belt.tick();
		}
	}

	{
		METRIC_SCOPE("Game/Tick Junction");
		for (auto& junction : junctions.dense<JunctionSimulationData>()) {
			junction.tick();
		}
	}

	{
		METRIC_SCOPE("Game/Tick Machine");
		for (auto& machine : machines.dense<MachineSimulationData>()) {
			machine.tick();
		}
	}
}

void Game::update() {
	constexpr f32 PAN_SPEED = 500.0F;
	constexpr f32 PREVIEW_ITEM_SPEED = 35.0F;
	constexpr f32 ZOOM_SPEED = 0.15F;
	constexpr f32 MIN_ZOOM = 0.1F;
	constexpr f32 MAX_ZOOM = 8.0F;

	vec2 pan_direction{0.0F};
	if (g_input->key_down(SDL_SCANCODE_A) || g_input->key_down(SDL_SCANCODE_LEFT)) {
		pan_direction.x -= 1.0F;
	}
	if (g_input->key_down(SDL_SCANCODE_D) || g_input->key_down(SDL_SCANCODE_RIGHT)) {
		pan_direction.x += 1.0F;
	}
	if (g_input->key_down(SDL_SCANCODE_W) || g_input->key_down(SDL_SCANCODE_UP)) {
		pan_direction.y -= 1.0F;
	}
	if (g_input->key_down(SDL_SCANCODE_S) || g_input->key_down(SDL_SCANCODE_DOWN)) {
		pan_direction.y += 1.0F;
	}
	if (dot(pan_direction, pan_direction) > 0.0F) {
		view_position +=
			normalize(pan_direction) * PAN_SPEED * g_platform->delta_time() / view_zoom;
	}

	if (g_input->mouse_button_down(SDL_BUTTON_MIDDLE) ||
		g_input->mouse_button_down(SDL_BUTTON_RIGHT)) {
		view_position -= g_input->mouse_delta() / view_zoom;
	}

	const f32 wheel = g_input->wheel_delta().y;
	if (wheel != 0.0F) {
		const vec2 mouse = g_input->mouse_position();
		const vec2 world_under_mouse = view_position + mouse / view_zoom;
		view_zoom = std::clamp(view_zoom * std::exp(wheel * ZOOM_SPEED), MIN_ZOOM, MAX_ZOOM);
		view_position = world_under_mouse - mouse / view_zoom;
	}

	g_renderer->set_view(view_position, view_zoom);
	preview_item_distance += PREVIEW_ITEM_SPEED * g_platform->delta_time();
}

void Game::draw() {
	constexpr f32 GRID_LINE_WIDTH = 0.7f;
	constexpr f32 GRID_MIN_PIXEL_WIDTH = 1.f;
	constexpr u32 SUPERGRID_INTERVAL = 7;
	// Minor and major world grid lines behind the factory.
	g_renderer->draw_grid(CELL_SIZE, GRID_LINE_WIDTH, GRID_MIN_PIXEL_WIDTH, SUPERGRID_INTERVAL,
						  g_renderer->debugger().grid_color(),
						  g_renderer->debugger().supergrid_color(), render_layer::BACKGROUND);

	for (auto [rd, cd] : belts.each<BeltRenderData, BeltConnectionData>()) {
		belt_draw(rd, cd, preview_item_distance);

		// Temporary artificial items moving along the belt's rounded centerline.
		if (rd.waypoints.size() >= 2) {
			constexpr f32 TURN_RADIUS = CELL_SIZE * 0.5F;
			constexpr f32 HALF_PI = 1.57079632679F;
			constexpr f32 ITEM_SPACING = CELL_SIZE * 2.25F;
			constexpr f32 ITEM_RADIUS = BELT_WIDTH * 0.6;
			constexpr f32 ITEM_SPRITE_SIZE = ITEM_RADIUS * 1.35F;
			constexpr std::array ITEM_SPRITES{
				SpriteAsset::Ore,	 SpriteAsset::Gear,		 SpriteAsset::Ingot,
				SpriteAsset::Engine, SpriteAsset::Propeller,
			};
			constexpr std::array ITEM_COLORS{
				rgb(139, 162, 246), rgb(240, 184, 78),	rgb(109, 207, 190),
				rgb(239, 125, 116), rgb(190, 148, 239),
			};

			const auto direction_between = [&](usize from, usize to) {
				const vec2 delta = vec2{rd.waypoints[to] - rd.waypoints[from]};
				return delta / (std::abs(delta.x) + std::abs(delta.y));
			};
			const auto is_corner = [&](usize waypoint) {
				if (waypoint == 0 || waypoint + 1 >= rd.waypoints.size())
					return false;
				return dot(direction_between(waypoint - 1, waypoint),
						   direction_between(waypoint, waypoint + 1)) == 0.0F;
			};
			f32 path_length = 0.0F;
			for (usize i = 1; i < rd.waypoints.size(); ++i) {
				const vec2 direction = direction_between(i - 1, i);
				const f32 segment_length =
					dot(vec2{rd.waypoints[i] - rd.waypoints[i - 1]}, direction);
				path_length += segment_length - (is_corner(i - 1) ? TURN_RADIUS : 0.0F) -
							   (is_corner(i) ? TURN_RADIUS : 0.0F);
				if (is_corner(i))
					path_length += TURN_RADIUS * HALF_PI;
			}
			const auto position_on_path = [&](f32 distance) {
				for (usize i = 1; i < rd.waypoints.size(); ++i) {
					const vec2 incoming = direction_between(i - 1, i);
					const vec2 start = vec2{rd.waypoints[i - 1]} +
									   (is_corner(i - 1) ? incoming * TURN_RADIUS : vec2{0.0F});
					const vec2 end = vec2{rd.waypoints[i]} -
									 (is_corner(i) ? incoming * TURN_RADIUS : vec2{0.0F});
					const f32 straight_length = dot(end - start, incoming);
					if (distance <= straight_length)
						return start + incoming * distance;
					distance -= straight_length;

					if (!is_corner(i))
						continue;
					const f32 arc_length = TURN_RADIUS * HALF_PI;
					if (distance <= arc_length) {
						const vec2 outgoing = direction_between(i, i + 1);
						const f32 cross = incoming.x * outgoing.y - incoming.y * outgoing.x;
						const f32 angle = (cross > 0.0F ? 1.0F : -1.0F) * distance / TURN_RADIUS;
						const f32 sine = std::sin(angle);
						const f32 cosine = std::cos(angle);
						const vec2 radius = -outgoing * TURN_RADIUS;
						const vec2 rotated_radius{radius.x * cosine - radius.y * sine,
												  radius.x * sine + radius.y * cosine};
						const vec2 center =
							vec2{rd.waypoints[i]} - incoming * TURN_RADIUS + outgoing * TURN_RADIUS;
						return center + rotated_radius;
					}
					distance -= arc_length;
				}
				return vec2{rd.waypoints.back()};
			};

			const u32 item_count = max(1U, static_cast<u32>(path_length / ITEM_SPACING));
			for (u32 item = 0; item < item_count; ++item) {
				const f32 distance =
					std::fmod(preview_item_distance + item * ITEM_SPACING, path_length);
				const vec2 center = position_on_path(distance);

				g_renderer->draw_circle(center - vec2{ITEM_RADIUS}, ITEM_RADIUS,
										COLOR::BELT_ITEM_TOKEN, render_layer::BELT_DETAIL);

				u32 sprite_seed = item * 0x9E3779B9U;
				sprite_seed ^= static_cast<u32>(rd.waypoints.front().x) * 0x85EBCA6BU;
				sprite_seed ^= static_cast<u32>(rd.waypoints.front().y) * 0xC2B2AE35U;
				const usize sprite_index = sprite_seed % ITEM_SPRITES.size();
				const SpriteAsset sprite = ITEM_SPRITES[sprite_index];
				g_renderer->draw_texture(
					g_assets->texture(sprite), center - vec2{ITEM_SPRITE_SIZE * 0.5F},
					vec2{ITEM_SPRITE_SIZE}, ITEM_COLORS[sprite_index], vec4{0.0F, 0.0F, 1.0F, 1.0F},
					0.0F, render_layer::BELT_DETAIL);
			}
		}
	}

	for (auto [rd, cd] : junctions.each<JunctionRenderData, JunctionConnectionData>()) {
		junction_draw(rd, cd);
	}

	for (auto [rd, cd] : machines.each<MachineRenderData, MachineConnectionData>()) {
		machine_draw(rd, cd);
	}
}

ID Game::create_machine(MachineType type, vec2i position) {
	MachineSimulationData sd{};
	MachineRenderData rd{};
	MachineConnectionData cd{};

	rd.machine_type = type;
	rd.grid_position = position;

	return machines.emplace(sd, rd, cd);
}

ID Game::create_belt() {
	BeltSimulationData sd{1};
	BeltRenderData rd{};
	BeltConnectionData cd{};

	return belts.emplace(sd, rd, cd);
}

ID Game::create_junction(vec2i position) {
	JunctionSimulationData sd{};
	JunctionRenderData rd{};
	JunctionConnectionData cd{};

	rd.grid_position = position;

	return junctions.emplace(sd, rd, cd);
}
