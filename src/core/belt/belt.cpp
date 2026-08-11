#include "belt.hpp"

#include <algorithm>
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
