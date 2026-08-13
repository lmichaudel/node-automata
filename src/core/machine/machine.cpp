#include "machine.hpp"

#include "common/constants.hpp"
#include "common/globals.hpp"
#include "content/machines.hpp"
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

void machine_draw(MachineRenderData& rd, MachineConnectionData& cd) {
	(void)cd;

	const auto& md = get_machine_type_data(rd.machine_type);

	const vec2 world_size = cell(md.size);
	const vec2 world_min = (vec2)rd.grid_position;
	const vec2 world_position = world_min + world_size * 0.5F;

	constexpr f32 ROUNDING = 4.0F;
	constexpr f32 ICON_SCALE = 0.7F;
	constexpr f32 SOCKET_WIDTH = CELL_SIZE * 0.6F;
	constexpr f32 SOCKET_HEIGHT = CELL_SIZE;
	constexpr f32 SOCKET_ROUNDING = 2.0F;
	constexpr f32 SOCKET_CHEVRON_LENGTH = SOCKET_WIDTH * 0.4F;
	constexpr f32 SOCKET_CHEVRON_SPREAD = SOCKET_HEIGHT * 0.3F;
	constexpr f32 SOCKET_CHEVRON_STROKE = 1.0F;
	constexpr vec4 SOCKET_COLOR = rgb(63, 62, 66);
	constexpr vec4 SOCKET_CHEVRON_COLOR = rgba(255, 255, 255, 115);

	// Solid accent-colored machine body.
	g_renderer->draw_rounded_rect(world_min, world_size, ROUNDING, md.accent_color, 0.0F,
								  render_layer::MACHINE_CHASSIS);

	// Neutral sockets frame each output and indicate its working direction.
	if (md.output_count > 0) {
		const f32 first_output_y =
			world_position.y - (static_cast<f32>(md.output_count) - 1.0F) * CELL_SIZE * 0.5F;
		const f32 output_x = rd.flipped ? world_min.x : world_min.x + world_size.x;
		const vec2 output_direction = rd.flipped ? vec2{-1.0F, 0.0F} : vec2{1.0F, 0.0F};
		const vec2 output_normal{-output_direction.y, output_direction.x};
		for (u8 output = 0; output < md.output_count; ++output) {
			const vec2 socket_center{output_x,
									 first_output_y + static_cast<f32>(output) * CELL_SIZE};
			g_renderer->draw_rounded_rect(socket_center -
										  vec2{SOCKET_WIDTH * 0.5F, SOCKET_HEIGHT * 0.5F},
									  vec2{SOCKET_WIDTH, SOCKET_HEIGHT}, SOCKET_ROUNDING,
									  SOCKET_COLOR, 0.0F, render_layer::MACHINE_PIN);

			const vec2 tip = socket_center + output_direction * (SOCKET_CHEVRON_LENGTH * 0.5F);
			const vec2 tail = socket_center - output_direction * (SOCKET_CHEVRON_LENGTH * 0.5F);
			const vec2 wing = output_normal * (SOCKET_CHEVRON_SPREAD * 0.5F);
			g_renderer->draw_line(tail - wing, tip, SOCKET_CHEVRON_STROKE,
								  SOCKET_CHEVRON_COLOR, render_layer::MACHINE_PIN);
			g_renderer->draw_line(tail + wing, tip, SOCKET_CHEVRON_STROKE,
								  SOCKET_CHEVRON_COLOR, render_layer::MACHINE_PIN);
		}
	}

	const f32 icon_size = min(world_size.x, world_size.y) * ICON_SCALE;
	g_renderer->draw_texture(g_assets->texture<SpriteAsset::Smelter>(),
							 world_position - vec2{icon_size * 0.5F}, vec2{icon_size}, vec4{1.0F},
							 vec4{0.0F, 0.0F, 1.0F, 1.0F}, 0.0F, render_layer::MACHINE_BODY);
}
