#pragma once

#include "common/types.hpp"

#include <string_view>
#include <unordered_map>

class Assets;
class Renderer;
class Texture;

class Font {
  public:
	Font() = default;
	Font(const Font&) = delete;
	Font& operator=(const Font&) = delete;

  private:
	friend class Assets;
	friend class Renderer;

	struct Glyph {
		f32 advance{0.0F};
		vec4 plane_bounds{0.0F}; // left, top, right, bottom; positive Y points down
		vec4 uv_bounds{0.0F};	 // left, top, right, bottom
		bool drawable{false};
	};

	Texture* atlas{nullptr};
	std::unordered_map<u32, Glyph> glyphs{};
	std::unordered_map<u64, f32> kerning{};
	f32 distance_range{4.0F};
	f32 ascender{0.0F};
	f32 line_height{1.0F};

	bool init(Texture* texture, std::string_view metadata, std::string_view name);
	void release();
	const Glyph* find_glyph(u32 codepoint) const;
	f32 kerning_adjustment(u32 left, u32 right) const;
	static u32 next_codepoint(std::string_view text, usize& offset);
};
