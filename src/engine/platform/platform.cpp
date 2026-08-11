#include "platform.hpp"
#include "common/globals.hpp"
#include "common/log.hpp"

#include <SDL3/SDL.h>
#include <cstdlib>

bool Platform::init() {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		log::error("Failed to initialize SDL: {}", SDL_GetError());
		return false;
	}

	window = SDL_CreateWindow("Node Automata", 1280, 720, 0);
	if (!window) {
		log::error("Failed to create window: {}", SDL_GetError());
		SDL_Quit();
		return false;
	}

	return true;
}

void Platform::start() {
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

		g_renderer->draw_circle(vec2{640.0F, 360.0F}, 100.0F, vec4{1.0F});
		if (!g_renderer->end_frame()) {
			is_running = false;
		}
	}
}

Platform::~Platform() {
	SDL_DestroyWindow(window);
	SDL_Quit();
}
