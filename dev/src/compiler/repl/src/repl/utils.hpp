/**
 * @file utils.hpp
 * @brief Utility helpers for the Duckling REPL.
 */

#pragma once

#include <frontend/module_tree/module_id.hpp>

#include <query_framework/context/context_fd.hpp>

#include <string>
#include <vector>

namespace compiler::repl {

	/**
	 * @brief Extract the source text of each top-level statement from a module.
	 *
	 * Iterates the TopLevel PST of the given module and slices the original source text
	 * using each statement's SourcePosition character range. Returns one string per
	 * top-level statement in source order.
	 */
	std::vector<std::string> extractStatementSources(
		query::Context& ctx, frontend::ModuleID module_id
	);

}  // namespace compiler::repl
