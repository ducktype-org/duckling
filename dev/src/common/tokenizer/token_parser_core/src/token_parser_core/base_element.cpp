// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "base_element.hpp"

#include <base/except/exceptions.hpp>

namespace tpc {
	Element::~Element() = default;

	bool Element::trailingSemicolon() {
		// @IDEA: not Panic
		CORE_PANIC("trailingSemicolon called on illegal object");
	}
}
