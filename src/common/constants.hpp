#pragma once

#include "types.hpp"

constexpr u32 TPS = 60;
constexpr f32 DT = 1.0f / static_cast<f32>(TPS);
constexpr u32 TICKS_PER_MINUTE = TPS * 60;

constexpr usize MACHINE_MIC = 3;
constexpr usize MACHINE_MOC = 2;
constexpr usize RECIPE_COUNT = 64;

constexpr u32 BELT_POSITIONS_PER_TILE = 7680;
constexpr u32 BELT_ITEM_SIZE = 3840;
constexpr u32 BELT_MK1_SPEED = 450;
constexpr u32 BELT_MOVE_PER_TICK =
	static_cast<u32>((static_cast<u64>(BELT_ITEM_SIZE) * BELT_MK1_SPEED) / TICKS_PER_MINUTE);

constexpr u32 CELL_SIZE = 20;

constexpr f32 cell(i32 c) {
	return CELL_SIZE * c;
}

constexpr vec2 cell(vec2i c) {
	return vec2{CELL_SIZE} * (vec2)c;
}

constexpr f32 BELT_WIDTH = CELL_SIZE / 5.0f;