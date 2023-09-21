#pragma once

#include "../definitions.hpp"

namespace vm::kind {
	struct StaticTable {
		TypeRef inner_type;
		u64     size;
	};
}
