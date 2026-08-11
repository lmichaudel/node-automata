#include "input.hpp"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>

void Input::begin_frame() {
	keys_pressed.reset();
	keys_released.reset();
	mouse_buttons_pressed = 0;
	mouse_buttons_released = 0;
	mouse_motion = vec2{0.0F};
	wheel_motion = vec2{0.0F};
}

void Input::handle_event(const SDL_Event& event) {
	switch (event.type) {
	case SDL_EVENT_KEY_DOWN: {
		const SDL_Scancode key = event.key.scancode;
		if (valid_key(key)) {
			const usize index = static_cast<usize>(key);
			if (!event.key.repeat && !keys_down.test(index)) {
				keys_pressed.set(index);
			}
			keys_down.set(index);
		}
		break;
	}
	case SDL_EVENT_KEY_UP: {
		const SDL_Scancode key = event.key.scancode;
		if (valid_key(key)) {
			const usize index = static_cast<usize>(key);
			if (keys_down.test(index)) {
				keys_released.set(index);
			}
			keys_down.reset(index);
		}
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_DOWN: {
		const u32 mask = mouse_button_mask(event.button.button);
		if ((mouse_buttons_down & mask) == 0) {
			mouse_buttons_pressed |= mask;
		}
		mouse_buttons_down |= mask;
		mouse_pos = vec2{event.button.x, event.button.y};
		break;
	}
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		const u32 mask = mouse_button_mask(event.button.button);
		if ((mouse_buttons_down & mask) != 0) {
			mouse_buttons_released |= mask;
		}
		mouse_buttons_down &= ~mask;
		mouse_pos = vec2{event.button.x, event.button.y};
		break;
	}
	case SDL_EVENT_MOUSE_MOTION:
		mouse_pos = vec2{event.motion.x, event.motion.y};
		mouse_motion += vec2{event.motion.xrel, event.motion.yrel};
		break;
	case SDL_EVENT_MOUSE_WHEEL: {
		const f32 direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0F : 1.0F;
		wheel_motion += direction * vec2{event.wheel.x, event.wheel.y};
		break;
	}
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		release_all();
		break;
	default:
		break;
	}
}

bool Input::key_down(SDL_Scancode key) const {
	return valid_key(key) && keys_down.test(static_cast<usize>(key));
}

bool Input::key_pressed(SDL_Scancode key) const {
	return valid_key(key) && keys_pressed.test(static_cast<usize>(key));
}

bool Input::key_released(SDL_Scancode key) const {
	return valid_key(key) && keys_released.test(static_cast<usize>(key));
}

bool Input::mouse_button_down(u8 button) const {
	return (mouse_buttons_down & mouse_button_mask(button)) != 0;
}

bool Input::mouse_button_pressed(u8 button) const {
	return (mouse_buttons_pressed & mouse_button_mask(button)) != 0;
}

bool Input::mouse_button_released(u8 button) const {
	return (mouse_buttons_released & mouse_button_mask(button)) != 0;
}

bool Input::valid_key(SDL_Scancode key) {
	return key > SDL_SCANCODE_UNKNOWN && key < SDL_SCANCODE_COUNT;
}

u32 Input::mouse_button_mask(u8 button) {
	if (button == 0 || button > 32) {
		return 0;
	}
	return u32{1} << (button - 1);
}

void Input::release_all() {
	keys_released |= keys_down;
	keys_down.reset();
	mouse_buttons_released |= mouse_buttons_down;
	mouse_buttons_down = 0;
}
