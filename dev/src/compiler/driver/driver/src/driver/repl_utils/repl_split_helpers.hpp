#pragma once

#include <frontend/module_tree/module_id.hpp>

#include <query_framework/context/context_fd.hpp>

#include <expected>
#include <string>
#include <string_view>
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

	/**
	 * @brief Parse raw REPL input and split it into individual statement source strings.
	 *
	 * Creates an probe REPL module (Script mode, ordered parsing) and extracts
	 * all top-level statements. On parse error, prints diagnostics to stderr and returns
	 * an error string describing the failure.
	 *
	 * @param input The raw source text entered by the user
	 * @return Statement source strings on success, or an error message on parse failure
	 */
	std::expected<std::vector<std::string>, std::string> splitInputIntoStatements(
		std::string_view input
	);

}  // namespace compiler::repl
