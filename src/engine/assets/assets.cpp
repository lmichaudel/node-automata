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
		if (base_path == nullptr) {
			return nullptr;
		}
		return IMG_Load(join_path(base_path, relative_path).c_str());
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
	return true;
}

void Assets::release() {
	textures.clear();
	root.clear();
	device = nullptr;
}

Texture* Assets::load_texture(std::string_view path, TextureFilter filter) {
	if (device == nullptr || path.empty()) {
		return nullptr;
	}

	const std::string key = texture_key(path, filter);
	if (const auto found = textures.find(key); found != textures.end()) {
		return found->second.get();
	}

	SDL_Surface* loaded = load_surface(root, path);
	if (loaded == nullptr) {
		log::error("Failed to load texture '{}': {}", path, SDL_GetError());
		return nullptr;
	}
	SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
	SDL_DestroySurface(loaded);
	if (rgba == nullptr) {
		log::error("Failed to convert texture '{}' to RGBA8: {}", path, SDL_GetError());
		return nullptr;
	}
	if (rgba->w <= 0 || rgba->h <= 0 || !SDL_LockSurface(rgba)) {
		log::error("Failed to access texture '{}': {}", path, SDL_GetError());
		SDL_DestroySurface(rgba);
		return nullptr;
	}

	const usize row_size = static_cast<usize>(rgba->w) * 4;
	std::vector<u8> pixels(row_size * static_cast<usize>(rgba->h));
	for (i32 row = 0; row < rgba->h; ++row) {
		const auto* source =
			static_cast<const u8*>(rgba->pixels) + static_cast<usize>(row) * rgba->pitch;
		std::memcpy(pixels.data() + static_cast<usize>(row) * row_size, source, row_size);
	}
	SDL_UnlockSurface(rgba);

	auto texture = std::make_unique<Texture>();
	const bool uploaded =
		texture->init(device, static_cast<u32>(rgba->w), static_cast<u32>(rgba->h), pixels, filter);
	SDL_DestroySurface(rgba);
	if (!uploaded) {
		log::error("Failed to upload texture '{}'", path);
		return nullptr;
	}

	Texture* result = texture.get();
	textures.emplace(key, std::move(texture));
	return result;
}

Texture* Assets::get_texture(std::string_view path, TextureFilter filter) const {
	const auto found = textures.find(texture_key(path, filter));
	return found != textures.end() ? found->second.get() : nullptr;
}

bool Assets::unload_texture(std::string_view path, TextureFilter filter) {
	return textures.erase(texture_key(path, filter)) != 0;
}

std::string Assets::texture_key(std::string_view path, TextureFilter filter) const {
	std::string key;
	key.reserve(path.size() + 2);
	key.push_back(filter == TextureFilter::Nearest ? 'n' : 'l');
	key.push_back(':');
	key.append(path);
	return key;
}
