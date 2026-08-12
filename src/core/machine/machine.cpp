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
	constexpr f32 OUTLINE_THICKNESS = 3.0F;
	constexpr f32 HALF_OUTLINE = OUTLINE_THICKNESS * 0.5F;
	constexpr f32 FONT_SIZE = 14.0F;
	constexpr f32 TEXT_PADDING = 4.0F;
	constexpr f32 PIN_SIZE = BELT_WIDTH;
	constexpr vec4 BODY_COLOR = rgba(247, 242, 232);
	constexpr vec2 SHADOW_OFFSET{2.0F, 3.0F};
	constexpr f32 SHADOW_SPREAD = 1.0F;
	constexpr vec4 SHADOW_COLOR = rgba(184, 170, 153);
	constexpr u8 SHADOW_LAYER = 4;
	constexpr u8 ACCENT_LAYER = 5;
	constexpr u8 BODY_LAYER = 6;
	constexpr u8 PIN_LAYER = 7;
	constexpr u8 TEXT_LAYER = 8;

	// The expanded, offset silhouette gives the whole chassis one coherent shadow.
	g_renderer->draw_rounded_rect(world_min + SHADOW_OFFSET - vec2{SHADOW_SPREAD},
								  world_size + vec2{SHADOW_SPREAD * 2.0F}, ROUNDING + SHADOW_SPREAD,
								  SHADOW_COLOR, 0.0F, SHADOW_LAYER);

	// Accent-colored background and header.
	g_renderer->draw_rounded_rect(world_min, world_size, ROUNDING, md.accent_color, 0.0F,
								  ACCENT_LAYER);

	// Cream lower panel, inset to leave the accent background visible as an outline.
	const vec2 inner_size = world_size - vec2{OUTLINE_THICKNESS};
	const f32 inner_rounding = ROUNDING - HALF_OUTLINE;
	const f32 body_top = world_min.y + CELL_SIZE;
	const f32 body_bottom = world_min.y + world_size.y - HALF_OUTLINE;
	const vec2 body_size{inner_size.x, body_bottom - body_top};
	const vec2 body_origin{world_min.x + HALF_OUTLINE, body_top};

	constexpr AntialiasEdge BODY_EDGES =
		AntialiasEdge::Right | AntialiasEdge::Bottom | AntialiasEdge::Left;

	g_renderer->draw_rounded_rect(body_origin, body_size,
								  vec4{0.0F, 0.0F, inner_rounding, inner_rounding}, BODY_COLOR,
								  0.0F, BODY_LAYER, BODY_EDGES);

	// Pins use the same fixed shadow direction as the chassis.
	const auto draw_stubs = [&](u8 count, bool left, vec4 color) {
		const f32 first_y =
			world_position.y - static_cast<f32>(count - 1) * CELL_SIZE * 0.5f - PIN_SIZE;
		const f32 x = left ? (world_min.x - PIN_SIZE) : (world_min.x + world_size.x - PIN_SIZE);
		for (u8 i = 0; i < count; ++i) {
			const vec2 position{x, first_y + static_cast<f32>(i) * CELL_SIZE};
			g_renderer->draw_circle(position + SHADOW_OFFSET, PIN_SIZE, SHADOW_COLOR, SHADOW_LAYER);
			g_renderer->draw_circle(position, PIN_SIZE, color, PIN_LAYER);
		}
	};

	draw_stubs(md.input_count, !rd.flipped, rgba(76, 175, 80));
	draw_stubs(md.output_count, rd.flipped, rgba(255, 167, 38));

	const f32 text_y = world_min.y + (CELL_SIZE - FONT_SIZE) * 0.5F;
	g_renderer->draw_text(g_assets->font<FontAsset::Inconsolata>(), md.name,
						  vec2{world_min.x + TEXT_PADDING, text_y}, FONT_SIZE, vec4{1.0F},
						  TEXT_LAYER);
}
