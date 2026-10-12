// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../mir_structure/mir_structure.hpp"

namespace compiler::mir {

	/**
	 * @brief Validates function whether local variables are not being shadowed.
	 */
	base::OkBad validateFunction(query::Context& ctx, const Function&);
}
