#pragma once

#include <base/collections/optional.hpp>

#include <diagnostic/source_position.hpp>

namespace query {
	struct Context;
}

namespace pst {
	class LangElement;
	class ExprStmt;
	class Stmt;
	template<typename>
	class AccessLocked;

	/**
	 * @brief If @p root contains exactly one child and that child is an ExprStmt, returns it.
	 *
	 * Useful for distinguishing a standalone expression input (to be evaluated and printed)
	 * from a definition input (function, variable, class, etc.).
	 *
	 * @note This function is used in the REPL to determine how to handle a single statement.
	 *
	 * @param ctx  Query context for PST access
	 * @param root The PST root element to examine
	 * @return The single ExprStmt if present, empty otherwise
	 */
	base::Optional<AccessLocked<ExprStmt>> extractSingleExpression(
		query::Context& ctx, const AccessLocked<LangElement>& root
	);

	/**
	 * @brief If @p root contains exactly one child that is a control-flow or block
	 * statement (If, While, For, or Block), returns it as a Stmt.
	 *
	 * @param ctx  Query context for PST access
	 * @param root The PST root element to examine
	 * @return The single instruction Stmt if present, empty otherwise
	 *
	 * @note This also works for named loops (e.g. `while MyLoop(cond) {}`), which are still treated
	 * as instructions, not definitions.
	 */
	base::Optional<AccessLocked<Stmt>> extractSingleInstruction(
		query::Context& ctx, const AccessLocked<LangElement>& root
	);

	/**
	 * @brief If @p root contains exactly one child and it is a Stmt, returns it.
	 *
	 * This is useful as a fallback in REPL dispatch when expression and instruction
	 * extraction both fail and we want to treat the input as a single
	 * top-level definition statement.
	 */
	base::Optional<AccessLocked<Stmt>> extractSingleTopLevelStatement(
		query::Context& ctx, const AccessLocked<LangElement>& root
	);
}

namespace pst::internal {
	void printHighlight(dia::SourcePosition pos, const std::string& message);
}
