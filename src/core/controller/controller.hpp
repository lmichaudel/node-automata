#pragma once

#include "core/state/state.hpp"

#include <vector>

enum class Tool : u8 {
	Belt,
	Miner,
	Smelter,
	Assembler,
	Sink,
	Erase
};

class Controller {
	State& state;
	Tool tool{Tool::Belt};
	Hex hovered{};
	bool dragging_belt{false};
	ID belt_source{INVALID_ID};
	std::vector<Hex> belt_path{};

  public:
	explicit Controller(State& state);
	void update();
	void draw();

  private:
	void select_hotkey();
	void update_belt_tool();
	void append_belt_cell(Hex cell);
	void draw_hotbar();
};
