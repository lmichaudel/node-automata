#include "state.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace {
	constexpr std::array<Hex, 1> MINER_FOOTPRINT{{{0, 0}}};
	constexpr std::array<Hex, 3> SMELTER_FOOTPRINT{{{0, 0}, {1, 0}, {0, 1}}};
	constexpr std::array<Hex, 4> ASSEMBLER_FOOTPRINT{{{0, 0}, {1, 0}, {0, 1}, {-1, 1}}};
	constexpr std::array<Hex, 3> SINK_FOOTPRINT{{{0, 0}, {1, 0}, {1, -1}}};

	const std::array<BuildingDefinition, static_cast<usize>(BuildingKind::Count)> DEFINITIONS{{
		{"Miner", MINER_FOOTPRINT, SpriteIcon::Ore, rgba(96, 174, 136), ItemKind::Ore, 75},
		{"Smelter", SMELTER_FOOTPRINT, SpriteIcon::Smelter, rgba(225, 139, 77), ItemKind::Ingot,
		 105},
		{"Assembler", ASSEMBLER_FOOTPRINT, SpriteIcon::Gear, rgba(98, 145, 214), ItemKind::Gear,
		 135},
		{"Depot", SINK_FOOTPRINT, SpriteIcon::Engine, rgba(176, 105, 190), ItemKind::Gear, 1},
	}};

	i32 hex_distance(Hex hex) {
		return (std::abs(hex.q) + std::abs(hex.r) + std::abs(hex.q + hex.r)) / 2;
	}
} // namespace

const BuildingDefinition& State::definition(BuildingKind kind) {
	return DEFINITIONS[static_cast<usize>(kind)];
}

State::State() {
	const ID miner = place_building(BuildingKind::Miner, {-8, 1});
	const ID smelter = place_building(BuildingKind::Smelter, {-3, 0});
	const ID assembler = place_building(BuildingKind::Assembler, {3, -1});
	const ID sink = place_building(BuildingKind::Sink, {8, -2});
	place_belt_path(std::array<Hex, 4>{{{-7, 1}, {-6, 1}, {-5, 1}, {-4, 1}}}, miner, smelter);
	place_belt_path(std::array<Hex, 4>{{{-1, 0}, {0, 0}, {1, 0}, {2, 0}}}, smelter, assembler);
	place_belt_path(std::array<Hex, 4>{{{4, -1}, {5, -1}, {6, -1}, {7, -1}}}, assembler, sink);
}

vec2 State::hex_to_world(Hex hex) {
	constexpr f32 SQRT_3 = 1.7320508075688772F;
	return {HEX_RADIUS * SQRT_3 * (static_cast<f32>(hex.q) + static_cast<f32>(hex.r) * 0.5F),
			HEX_RADIUS * 1.5F * static_cast<f32>(hex.r)};
}

Hex State::world_to_hex(vec2 world) {
	constexpr f32 SQRT_3 = 1.7320508075688772F;
	const f32 q = (SQRT_3 / 3.0F * world.x - world.y / 3.0F) / HEX_RADIUS;
	const f32 r = (2.0F / 3.0F * world.y) / HEX_RADIUS;
	const f32 s = -q - r;
	i32 rq = static_cast<i32>(std::round(q));
	i32 rr = static_cast<i32>(std::round(r));
	i32 rs = static_cast<i32>(std::round(s));
	const f32 dq = std::abs(static_cast<f32>(rq) - q);
	const f32 dr = std::abs(static_cast<f32>(rr) - r);
	const f32 ds = std::abs(static_cast<f32>(rs) - s);
	if (dq > dr && dq > ds)
		rq = -rr - rs;
	else if (dr > ds)
		rr = -rq - rs;
	return {rq, rr};
}

bool State::contains(Hex hex) {
	return hex_distance(hex) <= GRID_RADIUS;
}

i32 State::direction_between(Hex from, Hex to) {
	const Hex delta = to - from;
	for (usize index = 0; index < HEX_DIRECTIONS.size(); ++index) {
		if (HEX_DIRECTIONS[index] == delta)
			return static_cast<i32>(index);
	}
	return -1;
}

std::vector<Hex> State::occupied_cells(const Building& building) const {
	std::vector<Hex> result;
	for (Hex offset : definition(building.kind).footprint)
		result.push_back(building.origin + offset);
	return result;
}

const Building* State::building_at(Hex cell) const {
	for (const Building& building : buildings) {
		for (Hex offset : definition(building.kind).footprint) {
			if (building.origin + offset == cell)
				return &building;
		}
	}
	return nullptr;
}

Building* State::building_at(Hex cell) {
	return const_cast<Building*>(std::as_const(*this).building_at(cell));
}

const Belt* State::belt_at(Hex cell) const {
	const auto found = std::find_if(belts.begin(), belts.end(), [cell](const Belt& belt) {
		return belt.cell == cell;
	});
	return found == belts.end() ? nullptr : &*found;
}

Belt* State::belt_at(Hex cell) {
	return const_cast<Belt*>(std::as_const(*this).belt_at(cell));
}

bool State::can_place_building(BuildingKind kind, Hex origin) const {
	for (Hex offset : definition(kind).footprint) {
		const Hex cell = origin + offset;
		if (!contains(cell) || building_at(cell) != nullptr || belt_at(cell) != nullptr)
			return false;
	}
	return true;
}

ID State::place_building(BuildingKind kind, Hex origin) {
	if (!can_place_building(kind, origin))
		return INVALID_ID;
	const ID id = next_building_id++;
	buildings.push_back({.id = id, .kind = kind, .origin = origin});
	return id;
}

bool State::can_place_belt(Hex cell) const {
	return contains(cell) && building_at(cell) == nullptr;
}

Belt& State::ensure_belt(Hex cell) {
	if (Belt* existing = belt_at(cell))
		return *existing;
	belts.push_back({.cell = cell});
	return belts.back();
}

void State::connect_belts(Hex from, Hex to) {
	const i32 direction = direction_between(from, to);
	if (direction < 0)
		return;
	ensure_belt(from);
	ensure_belt(to);
	Belt* source = belt_at(from);
	Belt* destination = belt_at(to);
	source->outputs |= static_cast<u8>(1U << static_cast<u32>(direction));
	destination->inputs |= static_cast<u8>(1U << static_cast<u32>((direction + 3) % 6));
}

void State::connect_endpoint(ID building, Hex belt, bool building_to_belt) {
	if (building == INVALID_ID || belt_at(belt) == nullptr)
		return;
	const auto duplicate =
		std::find_if(endpoints.begin(), endpoints.end(), [&](const BeltEndpoint& endpoint) {
			return endpoint.building == building && endpoint.belt == belt &&
				   endpoint.building_to_belt == building_to_belt;
		});
	if (duplicate == endpoints.end())
		endpoints.push_back({building, belt, building_to_belt});
}

void State::place_belt_path(std::span<const Hex> path, ID source, ID destination) {
	if (path.empty())
		return;
	std::vector<Hex> valid;
	for (Hex cell : path) {
		if (can_place_belt(cell) && (valid.empty() || valid.back() != cell))
			valid.push_back(cell);
	}
	if (valid.empty())
		return;
	for (Hex cell : valid)
		ensure_belt(cell);
	for (usize index = 1; index < valid.size(); ++index)
		connect_belts(valid[index - 1], valid[index]);
	connect_endpoint(source, valid.front(), true);
	connect_endpoint(destination, valid.back(), false);
}

void State::erase_at(Hex cell) {
	if (Building* building = building_at(cell)) {
		const ID id = building->id;
		std::erase_if(buildings, [id](const Building& candidate) {
			return candidate.id == id;
		});
		std::erase_if(endpoints, [id](const BeltEndpoint& endpoint) {
			return endpoint.building == id;
		});
		return;
	}
	if (belt_at(cell) == nullptr)
		return;
	std::erase_if(belts, [cell](const Belt& belt) {
		return belt.cell == cell;
	});
	std::erase_if(endpoints, [cell](const BeltEndpoint& endpoint) {
		return endpoint.belt == cell;
	});
	for (Belt& belt : belts) {
		const i32 direction = direction_between(belt.cell, cell);
		if (direction >= 0) {
			belt.inputs &= static_cast<u8>(~(1U << static_cast<u32>(direction)));
			belt.outputs &= static_cast<u8>(~(1U << static_cast<u32>(direction)));
		}
	}
}

bool State::building_accepts(Building& building, ItemKind item) {
	switch (building.kind) {
	case BuildingKind::Miner:
		return false;
	case BuildingKind::Smelter:
		if (item != ItemKind::Ore || building.input >= 6)
			return false;
		++building.input;
		return true;
	case BuildingKind::Assembler:
		if (item != ItemKind::Ingot || building.input >= 8)
			return false;
		++building.input;
		return true;
	case BuildingKind::Sink:
		delivered += item == ItemKind::Gear ? 5U : 1U;
		return true;
	case BuildingKind::Count:
		return false;
	}
	return false;
}

void State::advance_buildings() {
	for (Building& building : buildings) {
		const BuildingDefinition& def = definition(building.kind);
		if (building.kind == BuildingKind::Sink)
			continue;
		const u32 required = building.kind == BuildingKind::Miner
								 ? 0U
								 : (building.kind == BuildingKind::Assembler ? 2U : 1U);
		if (building.output >= 5 || building.input < required)
			continue;
		if (++building.progress >= def.work_ticks) {
			building.progress = 0;
			building.input -= required;
			++building.output;
		}
	}
	for (Building& building : buildings) {
		if (building.output == 0)
			continue;
		for (const BeltEndpoint& endpoint : endpoints) {
			if (endpoint.building != building.id || !endpoint.building_to_belt)
				continue;
			Belt* belt = belt_at(endpoint.belt);
			if (belt != nullptr && !belt->item) {
				belt->item = BeltItem{definition(building.kind).output, 0.0F};
				--building.output;
				break;
			}
		}
	}
}

void State::advance_belts() {
	constexpr f32 BELT_SPEED = 0.055F;
	for (Belt& belt : belts) {
		if (belt.item)
			belt.item->progress = std::min(1.0F, belt.item->progress + BELT_SPEED);
	}
	for (Belt& belt : belts) {
		if (!belt.item || belt.item->progress < 1.0F)
			continue;
		std::array<i32, 6> choices{};
		u32 count = 0;
		for (i32 direction = 0; direction < 6; ++direction) {
			if ((belt.outputs & (1U << static_cast<u32>(direction))) != 0)
				choices[count++] = direction;
		}
		bool moved = false;
		for (u32 attempt = 0; attempt < count; ++attempt) {
			const u32 choice = (static_cast<u32>(belt.round_robin) + attempt) % count;
			Belt* target = belt_at(belt.cell + HEX_DIRECTIONS[static_cast<usize>(choices[choice])]);
			if (target != nullptr && !target->item) {
				target->item = BeltItem{belt.item->kind, 0.0F};
				belt.item.reset();
				belt.round_robin = static_cast<u8>((choice + 1) % count);
				moved = true;
				break;
			}
		}
		if (moved)
			continue;
		for (const BeltEndpoint& endpoint : endpoints) {
			if (endpoint.belt != belt.cell || endpoint.building_to_belt)
				continue;
			auto found =
				std::find_if(buildings.begin(), buildings.end(), [&](const Building& building) {
					return building.id == endpoint.building;
				});
			if (found != buildings.end() && building_accepts(*found, belt.item->kind)) {
				belt.item.reset();
				break;
			}
		}
	}
}

void State::tick() {
	++ticks;
	advance_buildings();
	advance_belts();
}

void State::update() {
}
