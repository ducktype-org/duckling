#pragma once

#include "../definitions.hpp"

namespace vm::kind {
	struct StaticTable {
		TypeRef inner_type;
		std::uint64_t size;
	};
}