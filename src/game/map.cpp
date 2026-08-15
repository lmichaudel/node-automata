#include "map.hpp"

#include "common/constants.hpp"
#include "common/globals.hpp"
#include "content/colors.hpp"

#include <array>
#include <cmath>
#include <span>

namespace {
	constexpr vec4 WATER_COLOR = rgb(82, 151, 169);
	constexpr vec4 LAND_COLOR = rgb(211, 205, 178);
	constexpr vec4 FOREST_COLOR = rgb(137, 172, 121);
	constexpr vec4 ROCK_COLOR = rgb(177, 168, 151);
	constexpr vec4 MOUNTAIN_COLOR = rgb(148, 143, 133);
	constexpr vec4 PEAK_COLOR = rgb(119, 120, 119);
	constexpr vec4 RIVER_COLOR = rgb(91, 163, 177);

	vec4 terrain_color(TileTerrain terrain) {
		switch (terrain) {
		case TileTerrain::Forest:
			return FOREST_COLOR;
		case TileTerrain::Rock:
			return ROCK_COLOR;
		case TileTerrain::Mountain:
			return MOUNTAIN_COLOR;
		case TileTerrain::Peak:
			return PEAK_COLOR;
		default:
			return LAND_COLOR;
		}
	}
} // namespace

Map::Map(vec2i map_size, vec2i map_origin)
	: size{max(map_size, vec2i{1})}, origin{map_origin},
	  tiles(static_cast<usize>(size.x * size.y)) {
	const vec2 center = vec2{origin} + vec2{size} * 0.5F;
	const vec2 radius = vec2{size} * 0.49F;

	for (i32 y = origin.y; y < origin.y + size.y; ++y) {
		for (i32 x = origin.x; x < origin.x + size.x; ++x) {
			const vec2 normalized = (vec2{x + 0.5F, y + 0.5F} - center) / radius;
			const f32 superellipse =
				std::pow(std::abs(normalized.x), 4.0F) + std::pow(std::abs(normalized.y), 4.0F);
			const f32 coast = 0.98F + 0.055F * std::sin(x * 1.71F + y * 0.37F) +
							  0.035F * std::sin(x * 0.43F - y * 1.13F);
			Tile& tile = tiles[index({x, y})];
			if (superellipse >= coast) {
				continue;
			}

			tile.terrain = TileTerrain::Grass;
			const vec2 point{x + 0.5F, y + 0.5F};
			const f32 forest_noise = std::sin(x * 1.37F + y * 0.71F) * 0.7F;
			const f32 north_forest = length((point - vec2{17.0F, 16.5F}) / vec2{1.2F, 1.0F});
			const f32 south_forest = length((point - vec2{31.0F, 28.0F}) / vec2{1.4F, 0.9F});
			if (north_forest < 7.4F + forest_noise || south_forest < 5.8F + forest_noise) {
				tile.terrain = TileTerrain::Forest;
			}

			const f32 mountain_distance = length((point - vec2{51.0F, 9.5F}) / vec2{1.15F, 0.9F});
			if (mountain_distance < 7.2F) {
				tile.terrain = TileTerrain::Rock;
			}
			if (mountain_distance < 4.8F) {
				tile.terrain = TileTerrain::Mountain;
			}
			if (mountain_distance < 2.35F) {
				tile.terrain = TileTerrain::Peak;
			}
		}
	}

	const auto carve_river = [&](std::span<const vec2i> waypoints) {
		vec2i cursor = waypoints.front();
		const auto mark = [&](vec2i position) {
			if (contains(position) && tiles[index(position)].is_land()) {
				tiles[index(position)].river = true;
			}
		};
		mark(cursor);
		for (const vec2i target : waypoints.subspan(1)) {
			while (cursor.x != target.x) {
				cursor.x += target.x > cursor.x ? 1 : -1;
				mark(cursor);
			}
			while (cursor.y != target.y) {
				cursor.y += target.y > cursor.y ? 1 : -1;
				mark(cursor);
			}
		}
	};
	constexpr std::array main_river{
		vec2i{51, 9}, vec2i{49, 14}, vec2i{52, 19}, vec2i{50, 24}, vec2i{54, 29}, vec2i{55, 35},
	};
	constexpr std::array east_fork{
		vec2i{50, 11}, vec2i{54, 12}, vec2i{56, 15}, vec2i{60, 16}, vec2i{63, 18},
	};
	carve_river(main_river);
	carve_river(east_fork);
}

void Map::draw(f32 water_time) const {
	for (i32 y = origin.y; y < origin.y + size.y; ++y) {
		for (i32 x = origin.x; x < origin.x + size.x; ++x) {
			const vec2i position{x, y};
			const auto is_land = [&](vec2i neighbor) {
				const Tile* tile = tile_at(neighbor);
				return tile != nullptr && tile->is_land();
			};
			const vec4 land_neighbors{
				is_land(position + vec2i{0, -1}) ? 1.0F : 0.0F,
				is_land(position + vec2i{1, 0}) ? 1.0F : 0.0F,
				is_land(position + vec2i{0, 1}) ? 1.0F : 0.0F,
				is_land(position + vec2i{-1, 0}) ? 1.0F : 0.0F,
			};
			g_renderer->draw_water_tile(cell(position), vec2{CELL_SIZE}, WATER_COLOR,
										land_neighbors, water_time, render_layer::WATER);
		}
	}

	for (i32 y = origin.y; y < origin.y + size.y; ++y) {
		for (i32 x = origin.x; x < origin.x + size.x; ++x) {
			const vec2i position{x, y};
			const Tile& tile = tiles[index(position)];
			if (tile.is_land()) {
				const bool show_grid =
					tile.terrain == TileTerrain::Grass || tile.terrain == TileTerrain::Forest;
				g_renderer->draw_land_tile(cell(position), vec2{CELL_SIZE},
										   terrain_color(tile.terrain), show_grid,
										   render_layer::LAND);
			}
		}
	}

	for (i32 y = origin.y; y < origin.y + size.y; ++y) {
		for (i32 x = origin.x; x < origin.x + size.x; ++x) {
			const vec2i position{x, y};
			if (!tiles[index(position)].river) {
				continue;
			}
			const auto connects_to_water = [&](vec2i neighbor) {
				const Tile* tile = tile_at(neighbor);
				return tile != nullptr && (tile->river || !tile->is_land());
			};
			const vec4 connections{
				connects_to_water(position + vec2i{0, -1}) ? 1.0F : 0.0F,
				connects_to_water(position + vec2i{1, 0}) ? 1.0F : 0.0F,
				connects_to_water(position + vec2i{0, 1}) ? 1.0F : 0.0F,
				connects_to_water(position + vec2i{-1, 0}) ? 1.0F : 0.0F,
			};
			g_renderer->draw_river_tile(cell(position), vec2{CELL_SIZE}, RIVER_COLOR, connections,
										water_time, render_layer::RIVER);
		}
	}
}

bool Map::is_buildable(vec2i position) const {
	const Tile* tile = tile_at(position);
	return tile != nullptr && tile->is_buildable();
}

bool Map::contains(vec2i position) const {
	return position.x >= origin.x && position.y >= origin.y && position.x < origin.x + size.x &&
		   position.y < origin.y + size.y;
}

usize Map::index(vec2i position) const {
	return static_cast<usize>((position.y - origin.y) * size.x + position.x - origin.x);
}

const Tile* Map::tile_at(vec2i position) const {
	return contains(position) ? &tiles[index(position)] : nullptr;
}
