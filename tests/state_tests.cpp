#include "core/state/state.hpp"

#include <array>
#include <bit>
#include <cassert>

int main() {
	for (i32 r = -State::GRID_RADIUS; r <= State::GRID_RADIUS; ++r) {
		for (i32 q = -State::GRID_RADIUS; q <= State::GRID_RADIUS; ++q) {
			const Hex cell{q, r};
			if (State::contains(cell))
				assert(State::world_to_hex(State::hex_to_world(cell)) == cell);
		}
	}

	State state;
	assert(!state.can_place_building(BuildingKind::Miner, {-8, 1}));
	const u32 initial_score = state.delivered_items();
	for (i32 tick = 0; tick < 2400; ++tick)
		state.tick();
	assert(state.delivered_items() > initial_score);

	// Branch from the starter line and then merge another path into the same tile.
	state.place_belt_path(std::array<Hex, 3>{{{0, 0}, {0, -1}, {1, -1}}});
	state.place_belt_path(std::array<Hex, 2>{{{-1, 1}, {0, 0}}});
	const Belt* junction = state.belt_at({0, 0});
	assert(junction != nullptr);
	assert(std::popcount(junction->outputs) >= 2);
	assert(std::popcount(junction->inputs) >= 2);

	state.erase_at({0, -1});
	assert(state.belt_at({0, -1}) == nullptr);
	return 0;
}
