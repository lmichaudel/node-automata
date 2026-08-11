#pragma once

#include "common/types.hpp"

#include <array>

enum class MachineType : u16 {
	MINER,
	SMELTER,
	ASSEMBLER,
	COUNT
};

struct MachineTypeData {
	const char* name;
	ivec2 size;
	vec4 accent_color;
	u16 recipe_id;
	u8 input_count;
	u8 output_count;
};

inline const std::array<MachineTypeData, static_cast<usize>(MachineType::COUNT)> MACHINE_TYPES = {{
	{"Miner", {5, 5}, {0.12f, 0.53f, 0.90f, 1.0f}, 1, 0, 1},
	{"Smelter", {5, 3}, {0.90f, 0.22f, 0.21f, 1.0f}, 2, 1, 1},
	{"Assembler", {10, 5}, {0.26f, 0.63f, 0.28f, 1.0f}, 0, 2, 1},
}};

inline const MachineTypeData& get_machine_type_data(MachineType machine_type) {
	return MACHINE_TYPES[static_cast<usize>(machine_type)];
}