#include "platform.hpp"
#include "common/globals.hpp"
#include "common/log.hpp"
#include "engine/debug/metrics.hpp"

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

void Platform::start(void (*tick)(void), void (*update)(void), void (*draw)(void)) {
	const u64 performance_frequency = SDL_GetPerformanceFrequency();
	u64 last_counter = SDL_GetPerformanceCounter();
	f32 accumulator = 0.0f;

	is_running = true;

	SDL_Event event;
	while (is_running) {
		metrics::begin_frame();
		g_input->begin_frame();
		while (SDL_PollEvent(&event)) {
			g_renderer->debugger().process_event(event);
			g_input->handle_event(event);

			if (event.type == SDL_EVENT_QUIT) {
				is_running = false;
			}
		}
		g_renderer->debugger().begin_frame();
		g_input->set_capture(g_renderer->debugger().wants_keyboard(),
							 g_renderer->debugger().wants_mouse());

		u64 now = SDL_GetPerformanceCounter();
		dt_ = static_cast<f32>(now - last_counter) / static_cast<f32>(performance_frequency);
		last_counter = now;
		accumulator += dt_;

		while (accumulator >= DT) {
			METRIC_SCOPE("Frame/Fixed tick");
			tick();
			accumulator -= DT;
		}

		{
			METRIC_SCOPE("Frame/Update");
			update();
		}

		g_renderer->begin_frame();
		{
			METRIC_SCOPE("Frame/Game draw");
			draw();
		}
		if (!g_renderer->end_frame()) {
			is_running = false;
		}
	}
}

Platform::~Platform() {
	SDL_DestroyWindow(window);
	SDL_Quit();
}
