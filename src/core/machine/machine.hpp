#pragma once

#include "common/connection_state.hpp"
#include "common/constants.hpp"
#include "content/machines.hpp"
#include "core/target/target.hpp"

struct MachineSimulationData {
	u16 recipe_id{0};
	u16 t{0};
	bool is_crafting{false};

	std::array<Stack, MACHINE_MIC> input_buffers{};
	std::array<Stack, MACHINE_MOC> output_buffers{};

	std::array<Target, MACHINE_MOC> output_targets{};

  public:
	MachineSimulationData() = default;

	void tick();
	bool try_transfer(Item item);

	void set_recipe(u16 id);

  private:
	bool has_required_inputs() const;
	bool has_room_for_outputs() const;

	void consume_inputs();
	void append_outputs();
	void push_outputs();
};

struct MachineRenderData {
	vec2i grid_position{0, 0};

	MachineType machine_type{MachineType::MINER};
	bool flipped{false};
};

struct MachineConnectionData {
	std::array<ConnectionState, MACHINE_MIC> input_connection_states;
	std::array<ConnectionState, MACHINE_MOC> output_connection_states;
};

void machine_draw(MachineRenderData& render_data, MachineConnectionData& connection_data);