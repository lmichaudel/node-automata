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

// Global back-to-front render order. Keep all world rendering on these named layers.
namespace render_layer {
	constexpr u8 BACKGROUND = 0;
	constexpr u8 AMBIENT_SHADOW = 1;
	constexpr u8 CONTACT_SHADOW = 2;

	constexpr u8 BELT_TRACK = 3;
	constexpr u8 BELT = 4;
	constexpr u8 BELT_DETAIL = 5;

	constexpr u8 MACHINE_CHASSIS = 6;
	constexpr u8 MACHINE_BODY = 7;
	constexpr u8 MACHINE_PIN = 8;
	constexpr u8 WORLD_TEXT = 9;
	constexpr u8 DEBUG = 11;
	constexpr usize COUNT = 12;
} // namespace render_layer

constexpr f32 cell(i32 c) {
	return CELL_SIZE * c;
}

constexpr vec2 cell(vec2i c) {
	return vec2{CELL_SIZE} * (vec2)c;
}

constexpr f32 BELT_WIDTH = CELL_SIZE / 2.5f;
constexpr vec4 BELT_COLOR = rgb(99, 99, 99);
constexpr f32 BELT_RAIL_WIDTH = 0.0F;
constexpr vec4 BELT_RAIL_COLOR = rgb(29, 29, 32);
