#include "machine.hpp"

#include "content/recipes.hpp"

void MachineSimulationData::set_recipe(u16 id) {
	const Recipe& recipe = RECIPES[id];
	recipe_id = id;

	for (usize i = 0; i < recipe.inputs.size(); ++i) {
		input_buffers[i].item = recipe.inputs[i].item;
	}

	for (usize i = 0; i < recipe.outputs.size(); ++i) {
		output_buffers[i].item = recipe.outputs[i].item;
	}
}

bool MachineSimulationData::has_required_inputs() const {
	assert(recipe_id);
	const Recipe& recipe = RECIPES[recipe_id];

	for (usize i = 0; i < recipe.inputs.size(); ++i) {
		Stack input = recipe.inputs[i];
		if (input.item != Item::NONE && input.size > input_buffers[i].size) {
			return false;
		}
	}

	return true;
}

bool MachineSimulationData::has_room_for_outputs() const {
	assert(recipe_id);
	const Recipe& recipe = RECIPES[recipe_id];

	for (usize i = 0; i < recipe.outputs.size(); ++i) {
		Stack output = recipe.outputs[i];
		if (output.item != Item::NONE && output_buffers[i].size >= output.size * 2) {
			return false;
		}
	}

	return true;
}

void MachineSimulationData::consume_inputs() {
	assert(has_required_inputs());
	const Recipe& recipe = RECIPES[recipe_id];

	for (usize i = 0; i < recipe.inputs.size(); ++i) {
		Stack input = recipe.inputs[i];
		if (input.item != Item::NONE) {
			input_buffers[i].size -= input.size;
		}
	}
}

void MachineSimulationData::append_outputs() {
	assert(has_room_for_outputs());
	const Recipe& recipe = RECIPES[recipe_id];

	for (usize i = 0; i < recipe.outputs.size(); ++i) {
		Stack output = recipe.outputs[i];
		if (output.item != Item::NONE) {
			output_buffers[i].size += output.size;
		}
	}
}

void MachineSimulationData::push_outputs() {
	for (usize i = 0; i < output_buffers.size(); ++i) {
		Stack& output = output_buffers[i];
		if (output.item != Item::NONE && output.size > 0) {
			if (output_targets[i].try_transfer(output.item)) {
				output.size--;
			}
		}
	}
}

void MachineSimulationData::tick() {
	if (recipe_id == 0) {
		return;
	}

	if (!is_crafting) {
		if (has_required_inputs() && has_room_for_outputs()) {
			consume_inputs();
			is_crafting = true;
		}
	}

	if (is_crafting) {
		t++;

		if (t == RECIPES[recipe_id].ttc) {
			append_outputs();
			is_crafting = false;
			t = 0;
		}
	}

	push_outputs();
}

bool MachineSimulationData::try_transfer(Item item) {
	const Recipe& recipe = RECIPES[recipe_id];

	for (usize i = 0; i < recipe.inputs.size(); ++i) {
		// the item is needed ...
		Stack input = recipe.inputs[i];
		if (input.item == item) {
			// ... and there is roam in the machine
			if (input_buffers[i].size < 2 * input.size) {
				input_buffers[i].size++;
				return true;
			}
		}
	}

	return false;
}
