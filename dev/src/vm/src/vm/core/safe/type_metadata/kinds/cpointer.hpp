// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../definitions.hpp"

#include <base/collections/optional.hpp>

namespace vm::kind {
	/**
	 * @brief A raw C pointer: an 8-byte native address crossing the FFI boundary. An absent
	 * inner type means an unknown pointee (C's `void*`).
	 */
	struct CPointer final {
		base::Optional<TypeCRef> inner_type;
	};
}
