#pragma once

#include "common/types.hpp"

struct ConnectionState {
	enum class Kind : u32 {
		UNCONNECTED,
		INPUT,
		OUTPUT
	};

	Kind kind{Kind::UNCONNECTED};
	ID who{INVALID_ID};
};