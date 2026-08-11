#include "junction.hpp"

#include <cassert>

void JunctionSimulationData::tick() {
	assert(rri < buffers.size());

	for (usize checked = 0; checked < buffers.size(); ++checked) {
		auto& buffer = buffers[rri];

		if (buffer == Item::NONE) {
			rri = static_cast<u8>((rri + 1) % buffers.size());
			continue;
		}

		if (target.try_transfer(buffer)) {
			buffer = Item::NONE;
			rri = static_cast<u8>((rri + 1) % buffers.size());
		}
		return;
	}
}

bool JunctionSimulationData::try_transfer(Item item, u8 buffer_id) {
	assert(buffer_id < 3);

	auto& buffer = buffers[buffer_id];

	if (buffer == Item::NONE) {
		buffer = item;
		return true;
	}

	return false;
}
