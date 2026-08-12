#include "game.hpp"
#include "common/globals.hpp"
#include "common/log.hpp"
#include "content/machines.hpp"
#include "core/belt/belt.hpp"
#include "core/junction/junction.hpp"
#include "core/machine/machine.hpp"
#include "engine/debug/metrics.hpp"

#include <SDL3/SDL_mouse.h>
#include <algorithm>
#include <cmath>

Game::Game() {
	create_machine(MachineType::SMELTER, {100, 100});

	create_junction({150, 100});

	const ID belt = create_belt();
	belts.get<BeltRenderData>(belt).waypoints = {
		{110, 210}, {310, 210}, {310, 310}, {450, 310}, {450, 190},
		{570, 190}, {570, 390}, {750, 390}, {750, 270}, {890, 270},
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
}

void Game::draw() {
	constexpr f32 GRID_LINE_WIDTH = 0.6f;
	constexpr f32 GRID_MIN_PIXEL_WIDTH = 1.f;
	constexpr u32 SUPERGRID_INTERVAL = 5;
	constexpr vec4 GRID_COLOR{0.30F, 0.34F, 0.43F, 0.16F};
	constexpr vec4 SUPERGRID_COLOR{0.42F, 0.47F, 0.58F, 0.28F};
	g_renderer->draw_grid(CELL_SIZE, GRID_LINE_WIDTH, GRID_MIN_PIXEL_WIDTH, SUPERGRID_INTERVAL,
						  GRID_COLOR, SUPERGRID_COLOR);

	g_renderer->draw_circle(vec2{360}, 40, vec4{1.0f});
	g_renderer->draw_text(g_assets->font<FontAsset::Inconsolata>(),
						  "Node Automata\nMSDF text rendering", vec2{24.0F}, 30.0F,
						  vec4{0.9F, 0.95F, 1.0F, 1.0F}, 11);

	for (auto [rd, cd] : belts.each<BeltRenderData, BeltConnectionData>()) {
		belt_draw(rd, cd);
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
