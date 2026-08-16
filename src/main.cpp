#include "common/globals.hpp"
#include "core/controller/controller.hpp"

#include <SDL3/SDL_main.h>
#include <cstdlib>
#include <memory>

enum class AppState {
	IN_GAME
};

AppState app_state{AppState::IN_GAME};

void tick() {
	switch (app_state) {
	case AppState::IN_GAME:
		g_state->tick();
		break;
	}
}

void update() {
	switch (app_state) {
	case AppState::IN_GAME:
		g_state->update();
		g_controller->update();
		g_viewer->update();
		break;
	}
}

void draw() {
	switch (app_state) {
	case AppState::IN_GAME:
		g_viewer->draw();
		g_controller->draw();
		break;
	}
}

int main(int, char**) {
	{
		Input input{};
		g_input = &input;

		Platform platform{};
		g_platform = &platform;

		Renderer renderer{};
		g_renderer = &renderer;

		if (!platform.init() || !renderer.init(platform.window_handle())) {
			return EXIT_FAILURE;
		}

		auto state = std::make_unique<State>();
		g_state = state.get();

		Controller controller{*state.get()};
		g_controller = &controller;

		Viewer viewer{*state.get()};
		g_viewer = &viewer;

		platform.start(tick, update, draw);
	}

	return EXIT_SUCCESS;
}
