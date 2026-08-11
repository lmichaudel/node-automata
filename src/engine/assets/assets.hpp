#pragma once

#include "engine/renderer/texture/texture.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

struct SDL_GPUDevice;

class Assets {
  public:
	Assets() = default;
	~Assets();
	Assets(const Assets&) = delete;
	Assets& operator=(const Assets&) = delete;

	bool init(SDL_GPUDevice* device, std::string_view root = "assets");
	void release();

	Texture* load_texture(std::string_view path, TextureFilter filter = TextureFilter::Linear);
	Texture* get_texture(std::string_view path, TextureFilter filter = TextureFilter::Linear) const;
	bool unload_texture(std::string_view path, TextureFilter filter = TextureFilter::Linear);

  private:
	SDL_GPUDevice* device{nullptr};
	std::string root{};
	std::unordered_map<std::string, std::unique_ptr<Texture>> textures{};

	std::string texture_key(std::string_view path, TextureFilter filter) const;
};
