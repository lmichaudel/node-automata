#pragma once

#include "common/types.hpp"

#include <vector>

enum class TileTerrain : u8 {
	Water,
	Grass,
	Forest,
	Rock,
	Mountain,
	Peak,
};

struct Tile {
	TileTerrain terrain{TileTerrain::Water};
	bool river{false};

	bool is_land() const {
		return terrain != TileTerrain::Water;
	}

	bool is_buildable() const {
		return !river && (terrain == TileTerrain::Grass || terrain == TileTerrain::Forest);
	}
};

class Map {
  public:
	Map(vec2i size, vec2i origin = {});

	void draw(f32 water_time) const;
	bool is_buildable(vec2i position) const;

	const std::vector<Tile>& tile_data() const {
		return tiles;
	}

  private:
	vec2i size{};
	vec2i origin{};
	std::vector<Tile> tiles{};

	bool contains(vec2i position) const;
	usize index(vec2i position) const;
	const Tile* tile_at(vec2i position) const;
};
