#pragma once

#include "core/item/item.hpp"

struct Target {
	enum class Mode : u8 {
		NONE,
		DISCARD,

		BELT,
		MACHINE,

		JUNCTION_BUFFER_A,
		JUNCTION_BUFFER_B,
		JUNCTION_BUFFER_C,
	};

	Mode mode{Mode::NONE};
	ID id{INVALID_ID};

	bool try_transfer(Item item);
};
