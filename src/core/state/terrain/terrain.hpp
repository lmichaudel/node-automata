#pragma once

#include "chunk/chunk.hpp"

#include <array>

constexpr usize TERRAIN_CHUNKS_PER_AXIS = 3;
constexpr usize TERRAIN_CHUNK_COUNT =
	TERRAIN_CHUNKS_PER_AXIS * TERRAIN_CHUNKS_PER_AXIS;
constexpr usize TERRAIN_SIZE = CHUNK_SIZE * TERRAIN_CHUNKS_PER_AXIS;

class Terrain {
  public:
	std::array<Chunk, TERRAIN_CHUNK_COUNT> chunks{};
	Terrain() = default;

	static constexpr bool contains(vec2i coordinate) {
		return coordinate.x >= 0 && coordinate.y >= 0 &&
			coordinate.x < static_cast<i32>(TERRAIN_SIZE) &&
			coordinate.y < static_cast<i32>(TERRAIN_SIZE);
	}

	Chunk& chunk_at(vec2i coordinate) {
		return chunks[static_cast<usize>(coordinate.y) * TERRAIN_CHUNKS_PER_AXIS +
			static_cast<usize>(coordinate.x)];
	}

	const Chunk& chunk_at(vec2i coordinate) const {
		return chunks[static_cast<usize>(coordinate.y) * TERRAIN_CHUNKS_PER_AXIS +
			static_cast<usize>(coordinate.x)];
	}

	Tile& at(vec2i coordinate) {
		return chunk_at(coordinate / static_cast<i32>(CHUNK_SIZE)).at(
			coordinate % static_cast<i32>(CHUNK_SIZE));
	}

	const Tile& at(vec2i coordinate) const {
		return chunk_at(coordinate / static_cast<i32>(CHUNK_SIZE)).at(
			coordinate % static_cast<i32>(CHUNK_SIZE));
	}

	void set_kind(vec2i coordinate, TileKind kind) {
		if (!contains(coordinate)) {
			return;
		}
		chunk_at(coordinate / static_cast<i32>(CHUNK_SIZE)).set_kind(
			coordinate % static_cast<i32>(CHUNK_SIZE), kind);
	}

	void set_elevation(vec2i coordinate, u8 elevation) {
		if (!contains(coordinate)) {
			return;
		}
		chunk_at(coordinate / static_cast<i32>(CHUNK_SIZE)).set_elevation(
			coordinate % static_cast<i32>(CHUNK_SIZE), elevation);
	}

	u64 revision() const {
		u64 total = 0;
		for (const Chunk& chunk : chunks) {
			total += chunk.revision;
		}
		return total;
	}
};
