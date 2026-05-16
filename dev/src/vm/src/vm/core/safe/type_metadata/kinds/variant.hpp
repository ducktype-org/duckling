#pragma once

#include "../definitions.hpp"

#include <vector>

namespace vm::kind {
	struct Variant {
		Bytes type_tag_size;   /// Numer of bytes needed for the type tag - e.g. 1, 2, 4, 8
		Bytes payload_offset;  /// Offset of the aligned payload inside the variant storage.
		std::vector<TypeRef> alternatives;
	};
}
