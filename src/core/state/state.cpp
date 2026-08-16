#include "state.hpp"

#include <algorithm>
#include <cmath>

namespace {
	f32 random_value(vec2 coordinate) {
		const f32 value = std::sin(dot(coordinate, vec2{127.1F, 311.7F})) * 43758.5453F;
		return value - std::floor(value);
	}

	f32 value_noise(vec2 position) {
		const vec2 cell = glm::floor(position);
		const vec2 offset = glm::fract(position);
		const vec2 blend = offset * offset * (3.0F - 2.0F * offset);
		const f32 top =
			glm::mix(random_value(cell), random_value(cell + vec2{1.0F, 0.0F}), blend.x);
		const f32 bottom = glm::mix(random_value(cell + vec2{0.0F, 1.0F}),
									random_value(cell + vec2{1.0F, 1.0F}), blend.x);
		return glm::mix(top, bottom, blend.y);
	}

	f32 fractal_noise(vec2 position) {
		f32 result = 0.0F;
		f32 amplitude = 0.5F;
		for (i32 octave = 0; octave < 4; ++octave) {
			result += value_noise(position) * amplitude;
			position = position * 2.03F + vec2{17.1F, 9.2F};
			amplitude *= 0.5F;
		}
		return result / 0.9375F;
	}

	f32 elliptical_peak(vec2 position, vec2 center, vec2 radii, f32 rotation) {
		const vec2 offset = position - center;
		const f32 cosine = std::cos(rotation);
		const f32 sine = std::sin(rotation);
		const vec2 rotated{
			offset.x * cosine + offset.y * sine,
			-offset.x * sine + offset.y * cosine,
		};
		return 1.0F - length(rotated / radii);
	}
} // namespace

State::State() {
	for (i32 y = 0; y < static_cast<i32>(TERRAIN_SIZE); ++y) {
		for (i32 x = 0; x < static_cast<i32>(TERRAIN_SIZE); ++x) {
			const vec2 position{static_cast<f32>(x), static_cast<f32>(y)};

			// Warp an ellipse with layered noise to create bays, headlands, and an
			// asymmetric coastline while retaining a dependable ocean border.
			const vec2 coast_warp{
				(fractal_noise(position * 0.055F + vec2{4.0F, 11.0F}) - 0.5F) * 9.0F,
				(fractal_noise(position * 0.055F + vec2{23.0F, 2.0F}) - 0.5F) * 9.0F,
			};
			const vec2 island_offset = position + coast_warp - vec2{47.5F, 49.0F};
			const f32 coast_detail =
				(fractal_noise(position * 0.13F + vec2{8.0F, 19.0F}) - 0.5F) * 0.14F;
			const f32 island_field =
				1.0F - length(island_offset / vec2{42.0F, 44.0F}) + coast_detail;
			const bool island = island_field > 0.0F;

			TileKind kind = island ? TileKind::LAND : TileKind::WATER;
			u8 elevation = island ? 1 : 0;

			if (island) {
				// Several overlapping, rotated masses produce a ridge rather than a
				// circular mound. Fine noise breaks up each elevation contour.
				const f32 ridge = std::max({
					elliptical_peak(position, vec2{57.0F, 34.0F}, vec2{23.0F, 12.0F}, 0.55F),
					elliptical_peak(position, vec2{44.0F, 29.0F}, vec2{14.0F, 9.0F}, 0.30F),
					elliptical_peak(position, vec2{67.0F, 43.0F}, vec2{14.0F, 8.0F}, 0.80F),
				});
				const f32 mountain_detail =
					(fractal_noise(position * 0.16F + vec2{31.0F, 7.0F}) - 0.5F) * 0.24F;
				const f32 mountain = ridge + mountain_detail;
				if (mountain > 0.08F) {
					kind = TileKind::MOUNTAIN;
					elevation = 2;
					if (mountain > 0.32F)
						elevation = 3;
					if (mountain > 0.55F)
						elevation = 4;
					if (mountain > 0.75F)
						elevation = 5;
				}

				// A narrow highland stream becomes a wider meandering river as it
				// travels south to the coast.
				if (y >= 31) {
					const f32 progress = std::clamp((position.y - 31.0F) / 62.0F, 0.0F, 1.0F);
					const f32 river_center = 58.0F - progress * 20.0F +
											 std::sin(progress * 8.5F) * 3.2F +
											 std::sin(progress * 19.0F) * 1.25F;
					const f32 river_width = 0.75F + progress * 1.55F;
					if (std::abs(position.x - river_center) < river_width) {
						kind = TileKind::WATER;
						elevation = 0;
					}
				}
			}
			terrain.set_kind({x, y}, kind);
			terrain.set_elevation({x, y}, elevation);
		}
	}
}

void State::tick() {
}

void State::update() {
}
