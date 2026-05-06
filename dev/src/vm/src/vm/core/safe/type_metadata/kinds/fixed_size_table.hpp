#pragma once

#include "../definitions.hpp"

namespace vm::kind {
	struct FixedSizeTable final {
		TypeRef inner_type;
		u64     element_count;
	};
}
