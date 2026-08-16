#pragma once

#include "common/types.hpp"
#include "engine/renderer/renderer.hpp"

#include <array>
#include <optional>
#include <span>
#include <vector>

struct Hex {
	i32 q{0};
	i32 r{0};
	constexpr bool operator==(const Hex&) const = default;
};

constexpr std::array<Hex, 6> HEX_DIRECTIONS{{
	{1, 0},
	{1, -1},
	{0, -1},
	{-1, 0},
	{-1, 1},
	{0, 1},
}};
constexpr Hex operator+(Hex lhs, Hex rhs) {
	return {lhs.q + rhs.q, lhs.r + rhs.r};
}
constexpr Hex operator-(Hex lhs, Hex rhs) {
	return {lhs.q - rhs.q, lhs.r - rhs.r};
}

enum class ItemKind : u8 {
	Ore,
	Ingot,
	Gear
};
enum class BuildingKind : u8 {
	Miner,
	Smelter,
	Assembler,
	Sink,
	Count
};

struct BuildingDefinition {
	const char* name;
	std::span<const Hex> footprint;
	SpriteIcon icon;
	vec4 color;
	ItemKind output;
	u32 work_ticks;
};

struct Building {
	ID id{INVALID_ID};
	BuildingKind kind{BuildingKind::Miner};
	Hex origin{};
	u32 input{0};
	u32 output{0};
	u32 progress{0};
};

struct BeltItem {
	ItemKind kind{ItemKind::Ore};
	f32 progress{0.0F};
};

struct Belt {
	Hex cell{};
	u8 inputs{0};
	u8 outputs{0};
	std::optional<BeltItem> item{};
	u8 round_robin{0};
};

struct BeltEndpoint {
	ID building{INVALID_ID};
	Hex belt{};
	bool building_to_belt{false};
};

class State {
  public:
	static constexpr i32 GRID_RADIUS = 13;
	static constexpr f32 HEX_RADIUS = 34.0F;

	State();
	void tick();
	void update();

	static vec2 hex_to_world(Hex hex);
	static Hex world_to_hex(vec2 world);
	static bool contains(Hex hex);
	static i32 direction_between(Hex from, Hex to);

	const std::vector<Building>& get_buildings() const {
		return buildings;
	}
	const std::vector<Belt>& get_belts() const {
		return belts;
	}
	const std::vector<BeltEndpoint>& get_endpoints() const {
		return endpoints;
	}
	u64 simulation_ticks() const {
		return ticks;
	}
	u32 delivered_items() const {
		return delivered;
	}

	static const BuildingDefinition& definition(BuildingKind kind);
	std::vector<Hex> occupied_cells(const Building& building) const;
	const Building* building_at(Hex cell) const;
	Building* building_at(Hex cell);
	const Belt* belt_at(Hex cell) const;
	Belt* belt_at(Hex cell);

	bool can_place_building(BuildingKind kind, Hex origin) const;
	ID place_building(BuildingKind kind, Hex origin);
	bool can_place_belt(Hex cell) const;
	void place_belt_path(std::span<const Hex> path, ID source = INVALID_ID,
						 ID destination = INVALID_ID);
	void erase_at(Hex cell);

  private:
	std::vector<Building> buildings{};
	std::vector<Belt> belts{};
	std::vector<BeltEndpoint> endpoints{};
	ID next_building_id{1};
	u64 ticks{0};
	u32 delivered{0};

	Belt& ensure_belt(Hex cell);
	void connect_belts(Hex from, Hex to);
	void connect_endpoint(ID building, Hex belt, bool building_to_belt);
	bool building_accepts(Building& building, ItemKind item);
	void advance_buildings();
	void advance_belts();
};
