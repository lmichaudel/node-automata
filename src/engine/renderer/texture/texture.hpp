#pragma once

#include "common/types.hpp"

#include <span>

struct SDL_GPUDevice;
struct SDL_GPUTexture;
class Renderer;

enum class TextureFilter : u8 {
	Nearest,
	Linear,
};

class Texture {
  public:
	Texture() = default;
	~Texture();
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	bool init(SDL_GPUDevice* device, u32 width, u32 height, std::span<const u8> pixels,
			  TextureFilter filter = TextureFilter::Linear);
	void release();

	u32 width() const {
		return texture_width;
	}
	u32 height() const {
		return texture_height;
	}
	TextureFilter filtering() const {
		return filter;
	}

  private:
	friend class Renderer;

	SDL_GPUDevice* device{nullptr};
	SDL_GPUTexture* gpu_texture{nullptr};
	u32 texture_width{0};
	u32 texture_height{0};
	TextureFilter filter{TextureFilter::Linear};
};
