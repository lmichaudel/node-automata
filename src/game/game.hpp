#pragma once

#include "common/pool.hpp"
#include "core/belt/belt.hpp"
#include "core/junction/junction.hpp"
#include "core/machine/machine.hpp"

class Game {
  public:
	Game() = default;

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

  private:
	Pool<MachineSimulationData, MachineRenderData> machines;
	Pool<BeltSimulationData, BeltRenderData> belts;
	Pool<JunctionSimulationData, JunctionRenderData> junctions;
};