#pragma once

#include "../definitions.hpp"

#include <vector>

namespace vm::kind {
	struct Variant {
		usize type_tag_size_bytes;  /// ceil(log2(alternatives.size() + 1) / 8)
		std::vector<TypeRef> alternatives;
	};
}
