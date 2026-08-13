#pragma once

#include "engine/renderer/font/font.hpp"
#include "engine/renderer/texture/texture.hpp"

#include <array>
#include <string>
#include <string_view>

struct SDL_GPUDevice;

enum class FontAsset : u8 {
	Inconsolata,
	Count,
};

enum class SpriteAsset : u8 {
	Ore,
	Gear,
	Ingot,
	Engine,
	Propeller,
	Smelter,
	Count,
};

class Assets {
  public:
	Assets() = default;
	~Assets();
	Assets(const Assets&) = delete;
	Assets& operator=(const Assets&) = delete;

	bool init(SDL_GPUDevice* device, std::string_view root = "res");
	void release();

	template <FontAsset Asset>
	constexpr const Font& font() const noexcept {
		static_assert(Asset != FontAsset::Count);
		return fonts[static_cast<usize>(Asset)].font;
	}

	template <SpriteAsset Asset>
	constexpr Texture* texture() noexcept {
		static_assert(Asset != SpriteAsset::Count);
		return &textures[static_cast<usize>(Asset)];
	}
	constexpr Texture* texture(SpriteAsset asset) noexcept {
		return asset == SpriteAsset::Count ? nullptr : &textures[static_cast<usize>(asset)];
	}

  private:
	struct FontResource {
		Texture texture{};
		Font font{};
	};

	static constexpr usize FONT_COUNT = static_cast<usize>(FontAsset::Count);
	static constexpr usize SPRITE_COUNT = static_cast<usize>(SpriteAsset::Count);
	static constexpr std::array<std::string_view, FONT_COUNT> FONT_PATHS{
		"fonts/inconsolata",
	};
	static constexpr std::array<std::string_view, SPRITE_COUNT> SPRITE_PATHS{
		"sprites/ore.png",
		"sprites/gear.png",
		"sprites/ingot.png",
		"sprites/engine.png",
		"sprites/propeller.png",
		"sprites/smelter.png",
	};

	SDL_GPUDevice* device{nullptr};
	std::string root{};
	std::array<FontResource, FONT_COUNT> fonts{};
	std::array<Texture, SPRITE_COUNT> textures{};

	bool load_texture(Texture& texture, std::string_view path,
					  TextureFilter filter = TextureFilter::Linear) const;
	bool load_font(usize index, std::string_view path);
};
