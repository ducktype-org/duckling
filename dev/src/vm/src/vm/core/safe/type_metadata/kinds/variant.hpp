#pragma once

#include "../definitions.hpp"

#include <vector>

namespace vm::kind {
	struct Variant {
		Bytes type_tag_size;   /// Number of bytes needed for the type tag - e.g. 1, 2, 4, 8
		Bytes payload_offset;  /// This is type_tag_size + padding
		std::vector<TypeRef> alternatives;
	};
}
