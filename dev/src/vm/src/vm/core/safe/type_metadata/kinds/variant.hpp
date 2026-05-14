#pragma once

#include "../definitions.hpp"

#include <vector>

namespace vm::kind {
	struct Variant {
		Bytes type_tag_size;  /// Numer of bytes needed for the type tag - e.g. 1, 2, 4, 8
		std::vector<TypeRef> alternatives;
	};
}
