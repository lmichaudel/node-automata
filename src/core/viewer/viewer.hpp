#pragma once

#include "common/types.hpp"

class State;

class Viewer {
	const State& state;

	vec2 view_position{15.0F, 40.0F};
	f32 view_zoom{1.25F};

  public:
	Viewer(const State& state);

	void update();
	void draw();
};