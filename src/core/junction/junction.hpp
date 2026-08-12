#pragma once

#include "common/connection_state.hpp"
#include "core/target/target.hpp"

#include <array>

struct JunctionSimulationData {
	std::array<Item, 3> buffers{};
	u8 rri = 0;
	Target target{};

  public:
	void tick();
	bool try_transfer(Item item, u8 buffer_id);
};

struct JunctionRenderData {
	vec2i grid_position;

  public:
	void draw();
};

struct JunctionConnectionData {
	std::array<ConnectionState, 4> connection_states;
};

void junction_draw(JunctionRenderData& render_data, JunctionConnectionData& connection_data);