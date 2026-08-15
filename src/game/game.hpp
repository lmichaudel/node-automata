#pragma once

#include "core/terrain/terrain.hpp"
class Game {
  public:
	Game();

	void tick();
	void update();
	void draw();

  private:
	Terrain terrain{};
	vec2 view_position{TILE_SIZE * 15.0F, TILE_SIZE * 40.0F};
	f32 view_zoom{1.25F};
};
