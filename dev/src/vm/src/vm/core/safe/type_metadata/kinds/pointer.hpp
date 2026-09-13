#pragma once

#include "../definitions.hpp"

namespace vm::kind {
	struct Pointer final {
		TypeCRef inner_type;
	};
}
