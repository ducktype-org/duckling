// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/symbols/symbol_kind.hpp>

#include <query_framework/context/context_fd.hpp>

#include <string>
#include <vector>

namespace compiler::repl {
	/**
	 * @brief A user-facing summary of one symbol visible from a REPL session.
	 *
	 * This keeps REPL presentation code independent from HELIOS private scope internals while still
	 * preserving enough identity to map symbols back to their statement module/history entry.
	 */
	struct ReplVisibleSymbol final {
		helios::SymID      symbol;
		frontend::ModuleID module_id;
		helios::SymbolKind kind;
		std::string        kind_label;
		std::string        name;
		std::string        details;
	};

	/**
	 * @brief Return top-level symbols visible from the terminal REPL module.
	 *
	 * The returned list follows REPL history order. It intentionally reads compiler symbol data
	 * from HELIOS scopes rather than DVM lowering state, so names and types match normal lookup.
	 */
	std::vector<ReplVisibleSymbol> queryVisibleReplSymbols(
		query::Context& ctx, frontend::ModuleID terminal_module_id
	);

	/**
	 * @brief Format detailed user-facing information about a symbol.
	 *
	 * Class and namespace symbols include their direct members. Other symbols include the best
	 * available signature or type summary from HELIOS queries.
	 */
	std::string formatReplSymbolDetails(query::Context& ctx, helios::SymID symbol);
}  // namespace compiler::repl
