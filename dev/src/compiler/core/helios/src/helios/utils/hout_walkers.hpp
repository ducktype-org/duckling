// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/hout/hout.hpp>
#include <helios/symbols/symbol_id.hpp>

namespace compiler::helios::code {
	/**
	 * @brief Collect the symbols of every function called from `fun`.
	 *
	 * Walks the whole body and, for each `code::CallExpr` whose callee resolves to a
	 * plain identifier, records its `SymID`. Calls with a non-identifier callee
	 * (e.g. pointers to functions, lambdas) are skipped. The result is deduplicated.
	 */
	[[nodiscard]]
	std::vector<SymID> collectCalledSymbolsFromHOUT(const HOUTFunction& fun);


	/**
	 * @brief Collect the symbols of every function called from the expression tree.
	 */
	[[nodiscard]]
	std::vector<SymID> collectCalledSymbolsFromHOUT(const Expr& expr);
}
