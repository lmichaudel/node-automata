#pragma once

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
	MachineType machine_type{MachineType::MINER};
	vec2i position{0, 0};
	bool flipped{false};
};