#pragma once

#include "common/constants.hpp"
#include "core/item/item.hpp"
#include "core/target/target.hpp"

#include <vector>

struct BeltSimulationData {
	explicit BeltSimulationData(u32 tile_count);

	void tick();
	bool try_transfer(Item item);

	// Visits items from the belt exit back toward its entrance. The position is the
	// item center measured in tiles from the entrance and is derived directly from
	// the integer gap representation used by the simulation.
	template <typename Visitor>
	void for_each_item(Visitor&& visitor) const {
		if (count_ == 0)
			return;

		u32 center_position = length_ - slots_[head_].gap - BELT_ITEM_SIZE / 2;
		for (u32 offset = 0; offset < count_; ++offset) {
			const u32 index = (head_ + offset) % capacity_;
			const Slot& slot = slots_[index];
			visitor(slot.item,
					static_cast<f32>(center_position) / static_cast<f32>(BELT_POSITIONS_PER_TILE));
			if (offset + 1 < count_) {
				const u32 next_index = (head_ + offset + 1) % capacity_;
				center_position -= BELT_ITEM_SIZE + slots_[next_index].gap;
			}
		}
	}

	Target target;

  private:
	struct Slot {
		Item item{};
		u32 gap = 0;
	};

	void pop_head();

	u32 length_ = 0;
	u32 capacity_ = 0;

	std::vector<Slot> slots_;

	// Ring-buffer state.
	u32 head_ = 0;
	u32 count_ = 0;

	// Free space between position 0 and the back edge
	// of the last item.
	//
	// When empty: back_gap_ == length_.
	u32 back_gap_ = 0;

	// Logical offset from head_ to the first positive INTERNAL gap.
	//
	// [1, count_-1] -> points at a positive gap
	// count_        -> no positive internal gap
	//
	// This only moves toward the tail while the head is blocked,
	// which avoids repeatedly scanning compressed items.
	u32 first_gap_offset_ = 0;
};

struct BeltRenderData {
	std::vector<vec2i> waypoints{};
};

struct BeltConnectionData {};

void belt_draw(BeltRenderData& render_data, BeltConnectionData& connection_data);
