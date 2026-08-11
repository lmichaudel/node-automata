#pragma once

struct SDL_Window;

class Platform {
  public:
	Platform() = default;
	~Platform();

	bool init();
	void start();
	SDL_Window* window_handle() const {
		return window;
	}

  private:
	SDL_Window* window{nullptr};

	bool is_running{false};
};
