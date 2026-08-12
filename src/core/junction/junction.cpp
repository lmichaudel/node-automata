#include "junction.hpp"
#include "common/globals.hpp"

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

void junction_draw(JunctionRenderData& rd, JunctionConnectionData& cd) {
	(void)cd;

	g_renderer->draw_rounded_rect((vec2)rd.grid_position - vec2{9.5f}, vec2{20.0f}, 2.0f,
								  vec4{1.0f});
}
