#pragma once

#include "common/types.hpp"

#include <array>

enum class Item : u32 {
	NONE = 0,
	IRON_ORE,
	IRON_INGOT,
	IRON_PLATE,
	COUNT,
};

struct ItemData {
	const char* name;
	u32 stack_size;
};

inline constexpr std::array<ItemData, static_cast<usize>(Item::COUNT)> ITEM_DATA{{
	{"NONE", 0},
	{"Iron ore", 100},
	{"Iron ingot", 50},
	{"Iron plate", 50},
}};

inline constexpr const ItemData& get_item_data(Item item) {
	return ITEM_DATA[static_cast<usize>(item)];
}

struct Stack {
	Item item = Item::NONE;
	u32 size = 0;
};
