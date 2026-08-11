#pragma once

#include "common/types.hpp"

struct SDL_Window;

class Platform {
  public:
	Platform() = default;
	~Platform();

	bool init();
	void start(void (*tick)(void), void (*update)(void), void (*draw)(void));
	SDL_Window* window_handle() const {
		return window;
	}

  private:
	SDL_Window* window{nullptr};

	f32 dt_;

	bool is_running{false};
};
