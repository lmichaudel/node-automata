#include "common/globals.hpp"

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
		g_game->tick();
		break;
	}
}

void update() {
	switch (app_state) {
	case AppState::IN_GAME:
		g_game->update();
		break;
	}
}

void draw() {
	switch (app_state) {
	case AppState::IN_GAME:
		g_game->draw();
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

		auto game = std::make_unique<Game>();
		g_game = game.get();

		platform.start(tick, update, draw);
	}

	return EXIT_SUCCESS;
}
