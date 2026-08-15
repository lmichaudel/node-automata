#pragma once

#include "common/types.hpp"

enum class TileKind : u8 {
	WATER = 0,
	LAND,
	MOUNTAIN,
	ROAD,
	COUNT
};

constexpr u8 MAX_TERRAIN_ELEVATION = 5;

constexpr const char* tile_kind_name(TileKind kind) {
	switch (kind) {
	case TileKind::WATER:
		return "Water";
	case TileKind::LAND:
		return "Land";
	case TileKind::MOUNTAIN:
		return "Mountain";
	case TileKind::ROAD:
		return "Road";
	case TileKind::COUNT:
		break;
	}
	return "Unknown";
}

struct Tile {
	TileKind kind{TileKind::LAND};
	u8 elevation{1};
};
