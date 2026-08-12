#pragma once

#include <cstddef>
#include <cstdint>

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using usize = std::size_t;

using f32 = float;
using f64 = double;

using ID = u32;
constexpr ID INVALID_ID = UINT32_MAX;

#include <glm/glm.hpp>
using namespace glm;

using vec2i = vec<2, i32>;
constexpr vec4 rgba(u8 r, u8 g, u8 b, u8 a = 255) {
	return vec4{r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
}

constexpr vec4 rgb(u8 r, u8 g, u8 b, u8 a = 255) {
	return vec4{r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
}
