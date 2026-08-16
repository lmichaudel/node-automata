#pragma once

#include "engine/renderer/renderer.hpp"
#include "terrain/terrain.hpp"

#include <vector>

struct Building {
	vec2i origin;
	vec2i footprint;
	SpriteIcon icon;
	vec4 color;
};

class State {
  public:
	State();

	void tick();
	void update();

	const std::vector<Building>& get_buildings() const {
		return buildings;
	}

	const Terrain& get_terrain() const {
		return terrain;
	}

  private:
	std::vector<Building> buildings{

		Building{{22, 50}, {2, 2}, SpriteIcon::Ore, rgba(137, 157, 148)},
		Building{{26, 49}, {3, 2}, SpriteIcon::Gear, rgba(199, 158, 92)},
		Building{{31, 49}, {2, 3}, SpriteIcon::Ingot, rgba(137, 158, 177)},
		Building{{35, 48}, {4, 3}, SpriteIcon::Engine, rgba(178, 116, 101)},
		Building{{25, 54}, {3, 3}, SpriteIcon::Propeller, rgba(105, 156, 170)},
		Building{{30, 54}, {3, 2}, SpriteIcon::Smelter, rgba(177, 132, 91)},
	};

	Terrain terrain{};
};
