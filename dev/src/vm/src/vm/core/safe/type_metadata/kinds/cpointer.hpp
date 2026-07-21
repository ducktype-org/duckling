#pragma once

#include "../definitions.hpp"

#include <base/collections/optional.hpp>

namespace vm::kind {
	/**
	 * @brief A raw C pointer: an 8-byte native address crossing the FFI boundary. An absent
	 * inner type means an unknown pointee (C's `void*`).
	 */
	struct CPointer {
		base::Optional<TypeCRef> inner_type;
	};
}
