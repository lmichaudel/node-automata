#include "controller.hpp"

#include "common/globals.hpp"

#include <SDL3/SDL_mouse.h>

#include <algorithm>
#include <array>

namespace {
	i32 distance(Hex hex) {
		return (std::abs(hex.q) + std::abs(hex.r) + std::abs(hex.q + hex.r)) / 2;
	}
	vec4 tool_color(Tool tool) {
		switch (tool) {
		case Tool::Belt:
			return rgba(88, 133, 154);
		case Tool::Miner:
			return State::definition(BuildingKind::Miner).color;
		case Tool::Smelter:
			return State::definition(BuildingKind::Smelter).color;
		case Tool::Assembler:
			return State::definition(BuildingKind::Assembler).color;
		case Tool::Sink:
			return State::definition(BuildingKind::Sink).color;
		case Tool::Erase:
			return rgba(205, 87, 92);
		}
		return vec4{1.0F};
	}
} // namespace

Controller::Controller(State& state) : state(state) {
	belt_path.reserve(128);
}

void Controller::select_hotkey() {
	if (g_input->key_pressed(SDL_SCANCODE_1))
		tool = Tool::Belt;
	if (g_input->key_pressed(SDL_SCANCODE_2))
		tool = Tool::Miner;
	if (g_input->key_pressed(SDL_SCANCODE_3))
		tool = Tool::Smelter;
	if (g_input->key_pressed(SDL_SCANCODE_4))
		tool = Tool::Assembler;
	if (g_input->key_pressed(SDL_SCANCODE_5))
		tool = Tool::Sink;
	if (g_input->key_pressed(SDL_SCANCODE_6) || g_input->key_pressed(SDL_SCANCODE_X))
		tool = Tool::Erase;
}

void Controller::append_belt_cell(Hex cell) {
	if (!state.can_place_belt(cell))
		return;
	if (belt_path.empty()) {
		belt_path.push_back(cell);
		return;
	}
	while (belt_path.back() != cell) {
		const Hex current = belt_path.back();
		if (belt_path.size() >= 2 && belt_path[belt_path.size() - 2] == cell) {
			belt_path.pop_back();
			return;
		}
		Hex best = current;
		i32 best_distance = distance(cell - current);
		for (Hex direction : HEX_DIRECTIONS) {
			const Hex candidate = current + direction;
			const i32 candidate_distance = distance(cell - candidate);
			if (candidate_distance < best_distance && state.can_place_belt(candidate)) {
				best = candidate;
				best_distance = candidate_distance;
			}
		}
		if (best == current ||
			std::find(belt_path.begin(), belt_path.end(), best) != belt_path.end())
			return;
		belt_path.push_back(best);
	}
}

void Controller::update_belt_tool() {
	if (g_input->mouse_button_pressed(SDL_BUTTON_LEFT)) {
		dragging_belt = true;
		belt_path.clear();
		belt_source = INVALID_ID;
		if (const Building* building = state.building_at(hovered))
			belt_source = building->id;
		else
			append_belt_cell(hovered);
	}
	if (dragging_belt && g_input->mouse_button_down(SDL_BUTTON_LEFT) &&
		state.building_at(hovered) == nullptr)
		append_belt_cell(hovered);
	if (dragging_belt && g_input->mouse_button_released(SDL_BUTTON_LEFT)) {
		ID destination = INVALID_ID;
		if (const Building* building = state.building_at(hovered))
			destination = building->id;
		state.place_belt_path(belt_path, belt_source, destination);
		belt_path.clear();
		dragging_belt = false;
		belt_source = INVALID_ID;
	}
}

void Controller::update() {
	select_hotkey();
	const vec2 mouse = g_input->mouse_position();
	const vec2 window_size = g_platform->window_size();
	// The bottom hotbar consumes clicks before they can reach the factory floor.
	if (mouse.y >= window_size.y - 78.0F) {
		if (g_input->mouse_button_pressed(SDL_BUTTON_LEFT)) {
			const i32 index = static_cast<i32>((mouse.x - (window_size.x * 0.5F - 210.0F)) / 70.0F);
			if (index >= 0 && index < 6)
				tool = static_cast<Tool>(index);
		}
		return;
	}
	hovered = g_viewer->screen_to_hex(mouse);
	if (tool == Tool::Belt) {
		update_belt_tool();
		return;
	}
	if (tool == Tool::Erase) {
		if (g_input->mouse_button_down(SDL_BUTTON_LEFT))
			state.erase_at(hovered);
		return;
	}
	if (!g_input->mouse_button_pressed(SDL_BUTTON_LEFT))
		return;
	const BuildingKind kind =
		tool == Tool::Miner
			? BuildingKind::Miner
			: (tool == Tool::Smelter
				   ? BuildingKind::Smelter
				   : (tool == Tool::Assembler ? BuildingKind::Assembler : BuildingKind::Sink));
	state.place_building(kind, hovered);
}

void Controller::draw_hotbar() {
	const f32 zoom = g_viewer->zoom();
	const vec2 window_size = g_platform->window_size();
	const vec2 background_origin =
		g_viewer->screen_to_world({window_size.x * 0.5F - 224.0F, window_size.y - 84.0F});
	g_renderer->draw_rounded_rect(background_origin, vec2{448.0F, 76.0F} / zoom, 14.0F / zoom,
								  rgba(17, 22, 28, 235), 1.5F / zoom, rgba(74, 88, 101));
	for (i32 index = 0; index < 6; ++index) {
		const Tool slot = static_cast<Tool>(index);
		const vec2 center = g_viewer->screen_to_world(
			{window_size.x * 0.5F - 176.0F + static_cast<f32>(index) * 70.0F,
			 window_size.y - 46.0F});
		const vec2 size{54.0F / zoom};
		const bool selected = slot == tool;
		g_renderer->draw_rounded_rect(center - size * 0.5F, size, 11.0F / zoom,
									  selected ? tool_color(slot) : rgba(39, 49, 59),
									  selected ? 3.0F / zoom : 1.0F / zoom,
									  selected ? rgba(240, 226, 163) : rgba(75, 89, 102));
		if (slot == Tool::Belt) {
			g_renderer->draw_capsule(center - vec2{16.0F, 0.0F} / zoom,
									 center + vec2{16.0F, 0.0F} / zoom, 10.0F / zoom,
									 rgba(169, 200, 213));
			g_renderer->draw_circle(center, 6.0F / zoom, rgba(65, 85, 98));
		} else if (slot == Tool::Erase) {
			g_renderer->draw_capsule(center - vec2{13.0F, 13.0F} / zoom,
									 center + vec2{13.0F, 13.0F} / zoom, 6.0F / zoom,
									 rgba(250, 220, 214));
			g_renderer->draw_capsule(center + vec2{-13.0F, 13.0F} / zoom,
									 center + vec2{13.0F, -13.0F} / zoom, 6.0F / zoom,
									 rgba(250, 220, 214));
		} else {
			const BuildingKind kind =
				slot == Tool::Miner
					? BuildingKind::Miner
					: (slot == Tool::Smelter ? BuildingKind::Smelter
											 : (slot == Tool::Assembler ? BuildingKind::Assembler
																		: BuildingKind::Sink));
			g_renderer->draw_sprite(State::definition(kind).icon, center, vec2{37.0F / zoom});
		}
	}
}

void Controller::draw() {
	const vec2 hover_center = State::hex_to_world(hovered);
	if (State::contains(hovered)) {
		if (tool == Tool::Belt || tool == Tool::Erase) {
			g_renderer->draw_hex(
				hover_center, State::HEX_RADIUS - 2.0F,
				vec4{tool_color(tool).r, tool_color(tool).g, tool_color(tool).b, 0.22F}, 2.0F,
				tool_color(tool));
		} else {
			const BuildingKind kind =
				tool == Tool::Miner
					? BuildingKind::Miner
					: (tool == Tool::Smelter ? BuildingKind::Smelter
											 : (tool == Tool::Assembler ? BuildingKind::Assembler
																		: BuildingKind::Sink));
			const bool valid = state.can_place_building(kind, hovered);
			const vec4 color = valid ? State::definition(kind).color : rgba(210, 72, 78);
			for (Hex offset : State::definition(kind).footprint)
				g_renderer->draw_hex(State::hex_to_world(hovered + offset),
									 State::HEX_RADIUS - 3.0F,
									 vec4{color.r, color.g, color.b, 0.50F}, 2.0F, color);
		}
	}
	for (usize index = 0; index < belt_path.size(); ++index) {
		const vec2 center = State::hex_to_world(belt_path[index]);
		g_renderer->draw_circle(center, 8.0F, rgba(143, 202, 218, 210));
		if (index > 0)
			g_renderer->draw_capsule(State::hex_to_world(belt_path[index - 1]), center, 12.0F,
									 rgba(143, 202, 218, 210));
	}
	draw_hotbar();
}
