#pragma once

#include "core/recipe/recipe.hpp"

#include <tuple>

template <typename... Inputs, typename... Outputs>
constexpr Recipe recipe(u16 ttc, std::tuple<Inputs...> inputs, std::tuple<Outputs...> outputs) {
	static_assert(sizeof...(Inputs) <= MACHINE_MIC);
	static_assert(sizeof...(Outputs) <= MACHINE_MOC);

	Recipe result{};
	result.ttc = ttc;

	std::apply(
		[&](auto... values) {
			std::size_t i = 0;
			((result.inputs[i++] = values), ...);
		},
		inputs);

	std::apply(
		[&](auto... values) {
			std::size_t i = 0;
			((result.outputs[i++] = values), ...);
		},
		outputs);

	return result;
}

template <typename... T>
constexpr auto in(T... values) {
	return std::tuple{values...};
}

template <typename... T>
constexpr auto out(T... values) {
	return std::tuple{values...};
}

constexpr Stack stack(Item item, u32 size = 1) {
	return {item, size};
}

constexpr Stack operator*(u32 size, Item item) {
	return {item, size};
}

// Recipes deliberately follow Item's numeric order so an item's value is also its recipe id.
inline constexpr std::array<Recipe, static_cast<usize>(Item::COUNT)> RECIPES = {
	// Existing prototype content
	recipe(0, in(), out()),

	// Raw resources
	recipe(60, in(), out(1 * Item::IRON_ORE)),
};

static_assert(RECIPES.size() <= RECIPE_COUNT);
