// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/module_tree/module_id.hpp>

#include <query_framework/context/context_fd.hpp>

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace pst {
	class TopLevel;
}

namespace compiler::repl {
	/**
	 * @brief Extract top-level statement source texts from an already-parsed PST root.
	 *
	 * Iterates root statements in parser-established order and slices original source text
	 * using each statement SourcePosition. This overload avoids reparsing/requerying PST
	 * when the caller already has the root element.
	 *
	 * @param ctx Active query context used to unlock PST statement elements.
	 * @param root Parsed PST top-level element to extract statements from.
	 * @return One string per top-level statement, in source order.
	 */
	std::vector<std::string> extractStatementSources(const pst::TopLevel& root);


	/**
	 * @brief Parse raw REPL input and split it into individual statement source strings.
	 *
	 * Convenience overload that creates its own query context.
	 *
	 * Call this overload only when there is no active query context at the call site.
	 * If the caller is already inside query::utils::withContextDo, it must call the
	 * context-taking overload to avoid invalid nested context creation.
	 *
	 * @param input The raw source text entered by the user
	 * @return Statement source strings on success, or an error message on parse failure
	 */
	std::expected<std::vector<std::string>, std::string> splitInputIntoStatements(
		std::string_view input
	);

}  // namespace compiler::repl
