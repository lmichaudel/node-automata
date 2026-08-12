#include "belt.hpp"
#include "common/globals.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

BeltSimulationData::BeltSimulationData(u32 tile_count) {
	assert(tile_count > 0);

	const u64 length = static_cast<u64>(tile_count) * BELT_POSITIONS_PER_TILE;
	assert(length <= UINT32_MAX); // >500k tiles long....

	length_ = static_cast<u32>(length);

	capacity_ = length_ / BELT_ITEM_SIZE;

	slots_.resize(capacity_);

	back_gap_ = length_;
}

bool BeltSimulationData::try_transfer(Item item) {
	// Empty belt.
	if (count_ == 0) {
		// Normally impossible for a tile-based belt, but keeps the
		// invariant explicit.
		if (length_ < BELT_ITEM_SIZE)
			return false;

		slots_[head_] = Slot{
			.item = std::move(item),
			.gap = length_ - BELT_ITEM_SIZE,
		};

		count_ = 1;
		back_gap_ = 0;

		// count_ means "there is no internal positive gap".
		first_gap_offset_ = 1;

		return true;
	}

	// Need enough room for the whole item at the belt entrance.
	if (back_gap_ < BELT_ITEM_SIZE)
		return false;

	if (count_ == capacity_)
		return false;

	const u32 old_count = count_;

	const u32 tail = (head_ + count_) % capacity_;

	// The new item is placed with its rear edge at position 0.
	//
	// Whatever input space remains becomes the gap between
	// this new item and the previous last item.
	const u32 new_gap = back_gap_ - BELT_ITEM_SIZE;

	slots_[tail] = Slot{
		.item = std::move(item),
		.gap = new_gap,
	};

	back_gap_ = 0;
	++count_;

	// There previously wasn't any positive internal gap.
	// The newly-created gap may now be the first one.
	if (first_gap_offset_ == old_count) {
		if (new_gap != 0)
			first_gap_offset_ = old_count;
		else
			first_gap_offset_ = count_;
	}

	return true;
}

void BeltSimulationData::tick() {
	u32 movement = BELT_MOVE_PER_TICK;

	while (movement != 0 && count_ != 0) {
		Slot& head = slots_[head_];

		//
		// Normal case:
		//
		// There is room in front of the first item, so the entire
		// belt contents move together.
		//
		if (head.gap != 0) {
			const u32 moved = std::min(movement, head.gap);

			head.gap -= moved;
			back_gap_ += moved;
			movement -= moved;

			if (movement == 0)
				return;
		}

		//
		// head.gap == 0:
		// the first item is exactly at the belt end.
		//

		if (target.try_transfer(head.item)) {
			pop_head();

			// Transfer itself consumes no belt movement.
			// Whatever fraction of this tick remains can still move
			// the rest of the belt.
			continue;
		}

		//
		// Output is blocked.
		//
		// The head cannot move, so movement instead consumes the
		// first non-zero gap behind the compressed front section.
		//

		if (first_gap_offset_ >= count_) {
			// No gaps anywhere.
			// Entire belt is completely compressed.
			return;
		}

		const u32 gap_index = (head_ + first_gap_offset_) % capacity_;

		Slot& slot = slots_[gap_index];

		const u32 moved = std::min(movement, slot.gap);

		slot.gap -= moved;
		back_gap_ += moved;
		movement -= moved;

		//
		// Once this gap reaches zero it will stay zero until items
		// leave the head. Advance the cached cursor instead of
		// searching from the beginning next tick.
		//
		if (slot.gap == 0) {
			++first_gap_offset_;

			while (first_gap_offset_ < count_) {
				const u32 index = (head_ + first_gap_offset_) % capacity_;

				if (slots_[index].gap != 0)
					break;

				++first_gap_offset_;
			}
		}
	}
}

void BeltSimulationData::pop_head() {
	const u32 old_count = count_;
	const u32 old_first_gap = first_gap_offset_;

	//
	// Only item on the belt.
	//
	if (count_ == 1) {
		head_ = (head_ + 1) % capacity_;

		count_ = 0;
		back_gap_ = length_;
		first_gap_offset_ = 0;

		return;
	}

	//
	// Remove the old head.
	//
	// The next item's old gap was:
	//
	//     [old head] -- gap -- [next item]
	//
	// Once the old head disappears, its BELT_ITEM_SIZE also becomes
	// free space in front of the next item:
	//
	//     -------- BELT_ITEM_SIZE + gap -------- [next item]
	//

	head_ = (head_ + 1) % capacity_;
	--count_;

	slots_[head_].gap += BELT_ITEM_SIZE;

	//
	// Maintain the cached first-positive-internal-gap offset.
	//

	if (old_first_gap == old_count) {
		// There were no positive internal gaps before.
		first_gap_offset_ = count_;
	} else if (old_first_gap > 1) {
		// Everything shifted one logical position toward the head.
		first_gap_offset_ = old_first_gap - 1;
	} else {
		// old offset 1 became the new head, so its gap is now the
		// terminal head gap rather than an internal gap.
		//
		// Find the next internal positive gap.
		first_gap_offset_ = 1;

		while (first_gap_offset_ < count_) {
			const u32 index = (head_ + first_gap_offset_) % capacity_;

			if (slots_[index].gap != 0)
				break;

			++first_gap_offset_;
		}
	}
}

void belt_draw(BeltRenderData& rd, BeltConnectionData& cd) {
	(void)cd;

	if (rd.waypoints.size() < 2)
		return;

	constexpr f32 TURN_RADIUS = CELL_SIZE * 0.5F;
	constexpr vec4 BELT_COLOR = rgba(247, 242, 232);
	constexpr vec2 SHADOW_OFFSET{2.0F, 3.0F};
	constexpr f32 SHADOW_SPREAD = 1.0F;
	constexpr vec4 SHADOW_COLOR = rgba(184, 170, 153);
	constexpr u8 SHADOW_LAYER = 1;
	constexpr u8 BELT_LAYER = 2;
	const auto is_corner = [&](usize waypoint) {
		if (waypoint == 0 || waypoint + 1 >= rd.waypoints.size())
			return false;

		const vec2i incoming = rd.waypoints[waypoint] - rd.waypoints[waypoint - 1];
		const vec2i outgoing = rd.waypoints[waypoint + 1] - rd.waypoints[waypoint];
		return (incoming.x == 0) != (outgoing.x == 0);
	};
	const auto direction_between = [&](usize from, usize to) {
		const vec2 delta = vec2{rd.waypoints[to] - rd.waypoints[from]};
		return delta / (std::abs(delta.x) + std::abs(delta.y));
	};

	for (usize i = 1; i < rd.waypoints.size(); ++i) {
		const vec2i from = rd.waypoints[i - 1];
		const vec2i to = rd.waypoints[i];
		const vec2i delta = to - from;

		assert((delta.x == 0) != (delta.y == 0));

		const f32 segment_length =
			std::abs(static_cast<f32>(delta.x)) + std::abs(static_cast<f32>(delta.y));
		const vec2 direction = vec2{delta} / segment_length;
		const f32 start_trim = is_corner(i - 1) ? TURN_RADIUS : 0.0F;
		const f32 end_trim = is_corner(i) ? TURN_RADIUS : 0.0F;
		const f32 straight_length = segment_length - start_trim - end_trim;
		assert(straight_length >= 0.0F);
		const vec2 size =
			delta.x == 0 ? vec2{BELT_WIDTH, straight_length} : vec2{straight_length, BELT_WIDTH};
		const vec2 center = vec2{from} + direction * (start_trim + straight_length * 0.5F);

		if (straight_length > 0.0F) {
			const vec2 shadow_size = delta.x == 0
										 ? vec2{BELT_WIDTH + SHADOW_SPREAD, straight_length}
										 : vec2{straight_length, BELT_WIDTH + SHADOW_SPREAD};
			g_renderer->draw_rounded_rect(center + SHADOW_OFFSET - shadow_size * 0.5F, shadow_size,
										  0.0F, SHADOW_COLOR, 0.0F, SHADOW_LAYER);
			g_renderer->draw_rounded_rect(center - size * 0.5F, size, 0.0F, BELT_COLOR, 0.0F,
										  BELT_LAYER);
		}
	}

	// Each bend is an analytic quarter ring contained by the one-cell square
	// centered on its waypoint. Its ends are tangent to the trimmed straights.
	for (usize i = 1; i + 1 < rd.waypoints.size(); ++i) {
		if (!is_corner(i))
			continue;

		const vec2 incoming = direction_between(i - 1, i);
		const vec2 outgoing = direction_between(i, i + 1);
		const f32 cross = incoming.x * outgoing.y - incoming.y * outgoing.x;
		const f32 rotation = std::atan2(incoming.y, incoming.x);
		const vec2 origin = vec2{rd.waypoints[i]} - vec2{TURN_RADIUS};

		g_renderer->draw_quarter_ring(origin + SHADOW_OFFSET, CELL_SIZE, BELT_WIDTH + SHADOW_SPREAD,
									  cross > 0.0F, SHADOW_COLOR, rotation, SHADOW_LAYER);
		g_renderer->draw_quarter_ring(origin, CELL_SIZE, BELT_WIDTH, cross > 0.0F, BELT_COLOR,
									  rotation, BELT_LAYER);
	}
}
