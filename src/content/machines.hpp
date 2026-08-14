#pragma once

#include "common/types.hpp"
#include "content/colors.hpp"

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
	color accent_color;
	u16 recipe_id;
	u8 input_count;
	u8 output_count;
};

constexpr std::array<MachineTypeData, static_cast<usize>(MachineType::COUNT)> MACHINE_TYPES = {{
	{"Miner", {4, 4}, COLOR::MACHINE_MINER, 1, 0, 1},
	{"Smelter", {5, 3}, COLOR::MACHINE_SMELTER, 2, 1, 1},
	{"Assembler", {10, 5}, COLOR::MACHINE_ASSEMBLER, 0, 2, 1},
}};

constexpr const MachineTypeData& get_machine_type_data(MachineType machine_type) {
	return MACHINE_TYPES[static_cast<usize>(machine_type)];
}