#pragma once

#include "../definitions.hpp"

namespace vm::kind {
	struct FixedSizeTable final {
		TypeCRef inner_type;
		u64     size;
	};
}
