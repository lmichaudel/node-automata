#pragma once

#include "common/pool.hpp"
#include "core/belt/belt.hpp"
#include "core/junction/junction.hpp"
#include "core/machine/machine.hpp"

class Game {
  public:
	Game();

	void tick();
	void update();
	void draw();

	auto& get_machines() {
		return machines;
	}

	auto& get_belts() {
		return belts;
	}

	auto& get_junctions() {
		return junctions;
	}

	ID create_machine(MachineType type, vec2i position);
	ID create_belt();
	ID create_junction(vec2i position);

  private:
	vec2 view_position{0.0F};
	f32 view_zoom{1.0F};
	f32 preview_item_distance{0.0F};

	Pool<MachineSimulationData, MachineRenderData, MachineConnectionData> machines{};
	Pool<BeltSimulationData, BeltRenderData, BeltConnectionData> belts{};
	Pool<JunctionSimulationData, JunctionRenderData, JunctionConnectionData> junctions{};
};
