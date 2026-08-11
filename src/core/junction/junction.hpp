#pragma once

#include "core/target/target.hpp"

#include <array>

struct JunctionRenderData {};

struct JunctionSimulationData {
	std::array<Item, 3> buffers{};
	u8 rri = 0;
	Target target{};

  public:
	void tick();
	bool try_transfer(Item item, u8 buffer_id);
};
