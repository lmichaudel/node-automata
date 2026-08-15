#pragma once

#include "common/types.hpp"
#include "core/terrain/tile/tile.hpp"

#include <array>
#include <algorithm>

constexpr usize CHUNK_SIZE = 32;
constexpr f32 TILE_SIZE = 32.0F;

struct Chunk {
	std::array<Tile, CHUNK_SIZE * CHUNK_SIZE> tiles{};
	u64 revision{1};

	static constexpr bool contains(vec2i coordinate) {
		return coordinate.x >= 0 && coordinate.y >= 0 &&
			   coordinate.x < static_cast<i32>(CHUNK_SIZE) &&
			   coordinate.y < static_cast<i32>(CHUNK_SIZE);
	}

	Tile& at(vec2i coordinate) {
		return tiles[static_cast<usize>(coordinate.y) * CHUNK_SIZE +
					 static_cast<usize>(coordinate.x)];
	}

	const Tile& at(vec2i coordinate) const {
		return tiles[static_cast<usize>(coordinate.y) * CHUNK_SIZE +
					 static_cast<usize>(coordinate.x)];
	}

	void set_kind(vec2i coordinate, TileKind kind) {
		if (!contains(coordinate) || at(coordinate).kind == kind) {
			return;
		}
		Tile& tile = at(coordinate);
		tile.kind = kind;
		if (kind == TileKind::WATER) {
			tile.elevation = 0;
		} else if (tile.elevation == 0) {
			tile.elevation = 1;
		}
		++revision;
	}

	void set_elevation(vec2i coordinate, u8 elevation) {
		if (!contains(coordinate)) {
			return;
		}
		Tile& tile = at(coordinate);
		const u8 clamped = tile.kind == TileKind::WATER
			? 0
			: std::clamp<u8>(elevation, 1, MAX_TERRAIN_ELEVATION);
		if (tile.elevation == clamped) {
			return;
		}
		tile.elevation = clamped;
		++revision;
	}
};
