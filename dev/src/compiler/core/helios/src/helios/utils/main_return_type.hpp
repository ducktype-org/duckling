// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/tsh/symbol_type.hpp>

#include <query_framework/context/context_fd.hpp>

namespace compiler::helios {
	/**
	 * @return the main return type required by the toolchain entry points.
	 */
	tsh::SymbolType<> requiredMainReturnType(query::Context& ctx);
}
