#include "assets.hpp"
#include "common/log.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <cstring>
#include <vector>

namespace {
	bool absolute_path(std::string_view path) {
		return path.starts_with('/') || path.starts_with('\\') ||
			   (path.size() > 1 && path[1] == ':');
	}

	std::string join_path(std::string_view left, std::string_view right) {
		if (left.empty()) {
			return std::string{right};
		}
		std::string result{left};
		if (!result.ends_with('/') && !result.ends_with('\\')) {
			result.push_back('/');
		}
		result.append(right);
		return result;
	}

	SDL_Surface* load_surface(std::string_view root, std::string_view path) {
		if (absolute_path(path)) {
			return IMG_Load(std::string{path}.c_str());
		}

		const std::string relative_path = join_path(root, path);
		if (SDL_Surface* surface = IMG_Load(relative_path.c_str())) {
			return surface;
		}

		const char* base_path = SDL_GetBasePath();
		return base_path != nullptr ? IMG_Load(join_path(base_path, relative_path).c_str())
									: nullptr;
	}

	bool load_text(std::string& output, std::string_view root, std::string_view path) {
		auto try_load = [&output](const std::string& filename) {
			size_t size = 0;
			void* data = SDL_LoadFile(filename.c_str(), &size);
			if (data == nullptr) {
				return false;
			}
			output.assign(static_cast<const char*>(data), size);
			SDL_free(data);
			return true;
		};

		if (absolute_path(path)) {
			return try_load(std::string{path});
		}
		const std::string relative_path = join_path(root, path);
		if (try_load(relative_path)) {
			return true;
		}
		const char* base_path = SDL_GetBasePath();
		return base_path != nullptr && try_load(join_path(base_path, relative_path));
	}
} // namespace

Assets::~Assets() {
	release();
}

bool Assets::init(SDL_GPUDevice* target_device, std::string_view asset_root) {
	if (device != nullptr || target_device == nullptr) {
		return false;
	}
	device = target_device;
	root = asset_root;

	for (usize index = 0; index < FONT_COUNT; ++index) {
		if (!load_font(index, FONT_PATHS[index])) {
			release();
			return false;
		}
	}
	for (usize index = 0; index < SPRITE_COUNT; ++index) {
		if (!load_texture(textures[index], SPRITE_PATHS[index])) {
			release();
			return false;
		}
	}
	return true;
}

void Assets::release() {
	for (Texture& texture : textures) {
		texture.release();
	}
	for (FontResource& resource : fonts) {
		resource.font.release();
		resource.texture.release();
	}
	root.clear();
	device = nullptr;
}

bool Assets::load_texture(Texture& texture, std::string_view path, TextureFilter filter) const {
	SDL_Surface* loaded = load_surface(root, path);
	if (loaded == nullptr) {
		log::error("Failed to load texture '{}': {}", path, SDL_GetError());
		return false;
	}
	SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
	SDL_DestroySurface(loaded);
	if (rgba == nullptr) {
		log::error("Failed to convert texture '{}' to RGBA8: {}", path, SDL_GetError());
		return false;
	}
	if (rgba->w <= 0 || rgba->h <= 0 || !SDL_LockSurface(rgba)) {
		log::error("Failed to access texture '{}': {}", path, SDL_GetError());
		SDL_DestroySurface(rgba);
		return false;
	}

	const usize row_size = static_cast<usize>(rgba->w) * 4;
	std::vector<u8> pixels(row_size * static_cast<usize>(rgba->h));
	for (i32 row = 0; row < rgba->h; ++row) {
		const auto* source =
			static_cast<const u8*>(rgba->pixels) + static_cast<usize>(row) * rgba->pitch;
		std::memcpy(pixels.data() + static_cast<usize>(row) * row_size, source, row_size);
	}
	SDL_UnlockSurface(rgba);

	const bool uploaded =
		texture.init(device, static_cast<u32>(rgba->w), static_cast<u32>(rgba->h), pixels, filter);
	SDL_DestroySurface(rgba);
	if (!uploaded) {
		log::error("Failed to upload texture '{}': {}", path, SDL_GetError());
		return false;
	}
	return true;
}

bool Assets::load_font(usize index, std::string_view path) {
	const std::string image_path = std::string{path} + ".png";
	const std::string metadata_path = std::string{path} + ".json";

	FontResource& resource = fonts[index];
	if (!load_texture(resource.texture, image_path)) {
		return false;
	}

	std::string metadata;
	if (!load_text(metadata, root, metadata_path)) {
		log::error("Failed to load font metadata '{}': {}", metadata_path, SDL_GetError());
		return false;
	}

	if (!resource.font.init(&resource.texture, metadata, metadata_path)) {
		return false;
	}
	return true;
}
