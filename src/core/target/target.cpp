#include "target.hpp"

#include "common/globals.hpp"

#include <cassert>

bool Target::try_transfer(Item item) {
	if (mode != Mode::NONE && mode != Mode::DISCARD) {
		assert(id != INVALID_ID);
	}

	switch (mode) {
	case Mode::DISCARD:
		return true;
	case Mode::BELT: {
		BeltSimulationData& data = g_game->get_belts().get<BeltSimulationData>(id);
		return data.try_transfer(item);
	}
	case Mode::MACHINE: {
		MachineSimulationData& data = g_game->get_machines().get<MachineSimulationData>(id);
		return data.try_transfer(item);
	}
	case Mode::JUNCTION_BUFFER_A: {
		JunctionSimulationData& data = g_game->get_junctions().get<JunctionSimulationData>(id);
		return data.try_transfer(item, 0);
	} break;
	case Mode::JUNCTION_BUFFER_B: {
		JunctionSimulationData& data = g_game->get_junctions().get<JunctionSimulationData>(id);
		return data.try_transfer(item, 1);
	}
	case Mode::JUNCTION_BUFFER_C: {
		JunctionSimulationData& data = g_game->get_junctions().get<JunctionSimulationData>(id);
		return data.try_transfer(item, 2);
	}
	case Mode::NONE:
		break;
	}

	return false;
}
