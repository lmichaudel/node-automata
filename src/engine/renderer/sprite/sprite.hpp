#pragma once

#include "common/types.hpp"

class Texture;

enum class SpriteKind : u8 {
	Texture,
	Circle,
	RoundedRectangle
};

struct Sprite {
	Texture* texture{nullptr};
	vec2 position{0.0F};
	vec2 size{1.0F};
	vec4 uv_rect{0.0F, 0.0F, 1.0F, 1.0F};
	vec4 color{1.0F};
	f32 rotation{0.0F};
	f32 corner_radius{0.0F};
	SpriteKind kind{SpriteKind::Texture};
	u8 layer{0};
};
