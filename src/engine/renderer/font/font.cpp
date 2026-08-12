#include "font.hpp"
#include "common/log.hpp"
#include "engine/renderer/texture/texture.hpp"

#include <nlohmann/json.hpp>
#include <string>

namespace {
	using Json = nlohmann::json;

	bool number(const Json& object, const char* key, f32& output) {
		const auto found = object.find(key);
		if (found == object.end() || !found->is_number()) {
			return false;
		}
		output = found->get<f32>();
		return true;
	}

	bool codepoint(const Json& object, const char* key, u32& output) {
		const auto found = object.find(key);
		if (found == object.end() || !found->is_number_unsigned()) {
			return false;
		}
		output = found->get<u32>();
		return true;
	}

	bool bounds(const Json& object, const char* key, vec4& output) {
		const auto found = object.find(key);
		if (found == object.end() || !found->is_object()) {
			return false;
		}
		f32 left = 0.0F;
		f32 right = 0.0F;
		f32 top = 0.0F;
		f32 bottom = 0.0F;
		if (!number(*found, "left", left) || !number(*found, "right", right) ||
			!number(*found, "top", top) || !number(*found, "bottom", bottom)) {
			return false;
		}
		output = vec4{left, top, right, bottom};
		return true;
	}

	u64 pair_key(u32 left, u32 right) {
		return static_cast<u64>(left) << 32U | right;
	}
} // namespace

bool Font::init(Texture* texture, std::string_view metadata, std::string_view name) {
	if (texture == nullptr || metadata.empty()) {
		return false;
	}

	const Json root = Json::parse(metadata.begin(), metadata.end(), nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		log::error("Failed to parse MSDF font metadata '{}'", name);
		return false;
	}
	const auto atlas_data = root.find("atlas");
	const auto metrics = root.find("metrics");
	const auto glyph_data = root.find("glyphs");
	if (atlas_data == root.end() || !atlas_data->is_object() || metrics == root.end() ||
		!metrics->is_object() || glyph_data == root.end() || !glyph_data->is_array()) {
		log::error("MSDF font metadata '{}' is missing atlas, metrics, or glyphs", name);
		return false;
	}

	const auto type = atlas_data->find("type");
	if (type == atlas_data->end() || !type->is_string() || type->get<std::string>() != "msdf") {
		log::error("Font '{}' must use an MSDF atlas", name);
		return false;
	}
	f32 atlas_width = 0.0F;
	f32 atlas_height = 0.0F;
	if (!number(*atlas_data, "distanceRange", distance_range) || distance_range <= 0.0F ||
		!number(*atlas_data, "width", atlas_width) ||
		!number(*atlas_data, "height", atlas_height) ||
		atlas_width != static_cast<f32>(texture->width()) ||
		atlas_height != static_cast<f32>(texture->height()) ||
		!number(*metrics, "ascender", ascender) || !number(*metrics, "lineHeight", line_height) ||
		line_height <= 0.0F) {
		log::error("MSDF font metadata '{}' has invalid metrics", name);
		return false;
	}

	bool top_origin = false;
	const auto y_origin = atlas_data->find("yOrigin");
	if (y_origin != atlas_data->end() && y_origin->is_string()) {
		const std::string origin = y_origin->get<std::string>();
		if (origin != "top" && origin != "bottom") {
			log::error("MSDF font '{}' has unsupported yOrigin '{}'", name, origin);
			return false;
		}
		top_origin = origin == "top";
	}
	if (!top_origin) {
		ascender = -ascender;
	}

	for (const Json& data : *glyph_data) {
		if (!data.is_object()) {
			continue;
		}
		u32 unicode = 0;
		Glyph glyph{};
		if (!codepoint(data, "unicode", unicode) || !number(data, "advance", glyph.advance)) {
			continue;
		}

		vec4 atlas_bounds{};
		glyph.drawable = bounds(data, "planeBounds", glyph.plane_bounds) &&
						 bounds(data, "atlasBounds", atlas_bounds);
		if (glyph.drawable) {
			if (!top_origin) {
				glyph.plane_bounds.y = -glyph.plane_bounds.y;
				glyph.plane_bounds.w = -glyph.plane_bounds.w;
				atlas_bounds =
					vec4{atlas_bounds.x, static_cast<f32>(texture->height()) - atlas_bounds.y,
						 atlas_bounds.z, static_cast<f32>(texture->height()) - atlas_bounds.w};
			}
			glyph.uv_bounds = atlas_bounds / vec4{static_cast<f32>(texture->width()),
												  static_cast<f32>(texture->height()),
												  static_cast<f32>(texture->width()),
												  static_cast<f32>(texture->height())};
		}
		glyphs.emplace(unicode, glyph);
	}

	if (glyphs.empty()) {
		log::error("MSDF font '{}' contains no Unicode glyphs", name);
		return false;
	}

	const auto kerning_data = root.find("kerning");
	if (kerning_data != root.end() && kerning_data->is_array()) {
		for (const Json& data : *kerning_data) {
			u32 left = 0;
			u32 right = 0;
			f32 advance = 0.0F;
			if (data.is_object() && codepoint(data, "unicode1", left) &&
				codepoint(data, "unicode2", right) && number(data, "advance", advance)) {
				kerning.emplace(pair_key(left, right), advance);
			}
		}
	}

	atlas = texture;
	return true;
}

void Font::release() {
	atlas = nullptr;
	glyphs.clear();
	kerning.clear();
	distance_range = 4.0F;
	ascender = 0.0F;
	line_height = 1.0F;
}

const Font::Glyph* Font::find_glyph(u32 codepoint) const {
	const auto found = glyphs.find(codepoint);
	return found != glyphs.end() ? &found->second : nullptr;
}

f32 Font::kerning_adjustment(u32 left, u32 right) const {
	const auto found = kerning.find(pair_key(left, right));
	return found != kerning.end() ? found->second : 0.0F;
}

u32 Font::next_codepoint(std::string_view text, usize& offset) {
	const u8 first = static_cast<u8>(text[offset++]);
	if (first < 0x80) {
		return first;
	}

	u32 value = 0;
	u32 continuation_count = 0;
	if ((first & 0xE0) == 0xC0) {
		value = first & 0x1F;
		continuation_count = 1;
	} else if ((first & 0xF0) == 0xE0) {
		value = first & 0x0F;
		continuation_count = 2;
	} else if ((first & 0xF8) == 0xF0) {
		value = first & 0x07;
		continuation_count = 3;
	} else {
		return 0xFFFD;
	}

	if (offset + continuation_count > text.size()) {
		offset = text.size();
		return 0xFFFD;
	}
	for (u32 index = 0; index < continuation_count; ++index) {
		const u8 next = static_cast<u8>(text[offset]);
		if ((next & 0xC0) != 0x80) {
			return 0xFFFD;
		}
		++offset;
		value = value << 6U | (next & 0x3F);
	}

	const bool overlong = (continuation_count == 1 && value < 0x80) ||
						  (continuation_count == 2 && value < 0x800) ||
						  (continuation_count == 3 && value < 0x10000);
	if (overlong || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
		return 0xFFFD;
	}
	return value;
}
