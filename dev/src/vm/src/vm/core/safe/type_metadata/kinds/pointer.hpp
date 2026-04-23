#pragma once

#include "../definitions.hpp"

namespace vm::kind {
	struct Pointer {
		TypeCRef inner_type;
	};
}
