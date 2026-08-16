#pragma once

#include "common/types.hpp"
#include "core/state/state.hpp"

class Viewer {
	const State& state;
	vec2 view_position{-640.0F, -360.0F};
	f32 view_zoom{1.0F};
	u32 shown_score{UINT32_MAX};

  public:
	explicit Viewer(const State& state);
	void update();
	void draw();
	vec2 screen_to_world(vec2 screen) const {
		return view_position + screen / view_zoom;
	}
	Hex screen_to_hex(vec2 screen) const {
		return State::world_to_hex(screen_to_world(screen));
	}
	vec2 camera_position() const {
		return view_position;
	}
	f32 zoom() const {
		return view_zoom;
	}
};
