#pragma once

#include "../definitions.hpp"

namespace vm::kind {
	struct FixedSizeTable {
		TypeRef inner_type;
		u64     size;
	};
}
