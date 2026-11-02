// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../definitions.hpp"

#include <vector>

namespace vm::kind {
	struct Variant final {
		Bytes type_tag_size;  /// Numer of bytes needed for the type tag - e.g. 1, 2, 4, 8
		std::vector<TypeRef> alternatives;
	};
}
