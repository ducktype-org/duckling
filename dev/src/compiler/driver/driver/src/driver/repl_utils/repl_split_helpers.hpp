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
	// PR: maybe move some of it to frontend?


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
	std::vector<std::string> extractStatementSources(query::Context& ctx, const pst::TopLevel& root);

	/**
	 * @brief Extract the source text of each top-level statement from a module.
	 *
	 * Iterates the TopLevel PST of the given module and slices the original source text
	 * using each statement's SourcePosition character range.
	 *
	 * This is a convenience overload that resolves module -> main file -> PST root,
	 * then delegates to the root-taking overload.
	 *
	 * @param ctx Active query context used for module/PST queries.
	 * @param module_id Module whose main file top-level statements should be extracted.
	 * @return One string per top-level statement, in source order.
	 */
	std::vector<std::string> extractStatementSources(
		query::Context& ctx, frontend::ModuleID module_id
	);

	/**
	 * @brief Parse raw REPL input and split it into individual statement source strings.
	 *
	 * Creates a probe REPL module (Script mode, ordered parsing) and extracts
	 * all top-level statements using the provided query context.
	 *
	 * Use this overload whenever the caller already holds an active query context.
	 * Creating a nested withContextDo() inside an active query context is invalid
	 * query-system usage.
	 *
	 * @param ctx Active query context for PST and module queries; required because
	 *        splitting uses query-based PST access and must not create a nested context.
	 * @param input The raw source text entered by the user
	 * @return Statement source strings on success, or an error message on parse failure
	 */
	std::expected<std::vector<std::string>, std::string> splitInputIntoStatements(
		query::Context& ctx, std::string_view input
	);

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
