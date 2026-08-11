#pragma once

#include "common/constants.hpp"
#include "core/item/item.hpp"

#include <array>

struct Recipe {
	std::array<Stack, MACHINE_MIC> inputs;
	std::array<Stack, MACHINE_MOC> outputs;
	u16 ttc;
};