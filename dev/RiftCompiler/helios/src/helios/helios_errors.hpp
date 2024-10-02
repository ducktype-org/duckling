#pragma once

namespace compiler::helios::errors {
	/**
	 * @brief An error indicating a queried symbol was not found.
	 */
	struct SymbolNotFound {};

	/**
	 * @brief An error indicating an ambiguity in queried symbols.
	 */
	struct Ambiguity {};

	/**
	 * @brief An error reported upon expression parsing failure.
	 */
	struct InvalidExpr {};

	/**
	 * @brief An error indicating a symbol inside an expression has failed to evaluate.
	 */
	struct SymbolQueryFailed {};

	/**
	 * @brief A general error indicating, that a query has failed, but also that the
	 * compiler error was already reported.
	 * This is an important assumption: As long as we do not have enough information
	 * to print a good error, we propagate specific errors (or transform them). After
	 * reporting, and if an error is irrecoverable, return a general error.
	 */
	struct Failed {};
}
