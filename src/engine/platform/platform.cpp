#include "platform.hpp"
#include "common/constants.hpp"
#include "common/globals.hpp"
#include "common/log.hpp"

#include <SDL3/SDL.h>
#include <cstdlib>

bool Platform::init() {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		log::error("Failed to initialize SDL: {}", SDL_GetError());
		return false;
	}

	window = SDL_CreateWindow("Hex Factory", 1280, 720, SDL_WINDOW_RESIZABLE);
	if (!window) {
		log::error("Failed to create window: {}", SDL_GetError());
		SDL_Quit();
		return false;
	}

	return true;
}

void Platform::start(void (*tick)(void), void (*update)(void), void (*draw)(void)) {
	const u64 performance_frequency = SDL_GetPerformanceFrequency();
	u64 last_counter = SDL_GetPerformanceCounter();
	f32 accumulator = 0.0f;

	is_running = true;

	SDL_Event event;
	while (is_running) {
		g_input->begin_frame();
		while (SDL_PollEvent(&event)) {
			g_input->handle_event(event);

			if (event.type == SDL_EVENT_QUIT) {
				is_running = false;
			}
		}
		g_renderer->begin_frame();
		g_input->set_capture(false, false);

		u64 now = SDL_GetPerformanceCounter();
		dt_ = static_cast<f32>(now - last_counter) / static_cast<f32>(performance_frequency);
		last_counter = now;
		accumulator += dt_;

		while (accumulator >= DT) {
			tick();
			accumulator -= DT;
		}

		update();
		draw();
		if (!g_renderer->end_frame()) {
			is_running = false;
		}
	}
}

Platform::~Platform() {
	SDL_DestroyWindow(window);
	SDL_Quit();
}

vec2 Platform::window_size() const {
	i32 width = 0;
	i32 height = 0;
	SDL_GetWindowSize(window, &width, &height);
	return {static_cast<f32>(width), static_cast<f32>(height)};
}
