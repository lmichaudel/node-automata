#include "common/globals.hpp"

#include <SDL3/SDL_main.h>
#include <cstdlib>

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
		platform.start();
	}

	return EXIT_SUCCESS;
}
