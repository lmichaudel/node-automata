#pragma once

#include "common/types.hpp"

class Texture;

enum class SpriteKind : u8 {
	Texture,
	Circle,
	RoundedRectangle,
	Msdf,
	Grid,
	QuarterRing,
	Line,
	RoundedLine90,
	Water,
	Land,
	River,
};

enum class AntialiasEdge : u8 {
	None = 0,
	Top = 1 << 0,
	Right = 1 << 1,
	Bottom = 1 << 2,
	Left = 1 << 3,
	All = Top | Right | Bottom | Left,
};

constexpr AntialiasEdge operator|(AntialiasEdge left, AntialiasEdge right) {
	return static_cast<AntialiasEdge>(static_cast<u8>(left) | static_cast<u8>(right));
}

struct Sprite {
	Texture* texture{nullptr};
	vec2 origin{0.0F};
	vec2 size{1.0F};
	vec4 uv_rect{0.0F, 0.0F, 1.0F, 1.0F};
	vec4 color{1.0F};
	f32 rotation{0.0F};
	// Top-left, top-right, bottom-right, bottom-left.
	vec4 corner_radii{0.0F};
	AntialiasEdge antialiased_edges{AntialiasEdge::All};
	f32 msdf_range{4.0F};
	f32 blur_radius{0.0F};
	SpriteKind kind{SpriteKind::Texture};
	u8 layer{0};
};
