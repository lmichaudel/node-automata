#pragma once

#include "common/types.hpp"

#include <SDL3/SDL_scancode.h>
#include <bitset>

union SDL_Event;

class Input {
  public:
	bool key_down(SDL_Scancode key) const;
	bool key_pressed(SDL_Scancode key) const;
	bool key_released(SDL_Scancode key) const;

	bool mouse_button_down(u8 button) const;
	bool mouse_button_pressed(u8 button) const;
	bool mouse_button_released(u8 button) const;

	vec2 mouse_position() const {
		return mouse_pos;
	}
	vec2 mouse_delta() const {
		return mouse_motion;
	}
	vec2 wheel_delta() const {
		return wheel_motion;
	}

  private:
	using KeyState = std::bitset<SDL_SCANCODE_COUNT>;

	KeyState keys_down{};
	KeyState keys_pressed{};
	KeyState keys_released{};

	u32 mouse_buttons_down{0};
	u32 mouse_buttons_pressed{0};
	u32 mouse_buttons_released{0};

	vec2 mouse_pos{0.0F};
	vec2 mouse_motion{0.0F};
	vec2 wheel_motion{0.0F};

	friend class Platform;
	void begin_frame();
	void handle_event(const SDL_Event& event);

	static bool valid_key(SDL_Scancode key);
	static u32 mouse_button_mask(u8 button);
	void release_all();
};
