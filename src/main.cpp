#include "common/globals.hpp"

#include <SDL3/SDL_main.h>
#include <cstdlib>

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

		Assets assets{};
		g_assets = &assets;

		if (!platform.init() || !renderer.init(platform.window_handle()) ||
			!assets.init(renderer.gpu_device())) {
			return EXIT_FAILURE;
		}

		Game game{};
		g_game = &game;

		platform.start(tick, update, draw);
	}

	return EXIT_SUCCESS;
}
