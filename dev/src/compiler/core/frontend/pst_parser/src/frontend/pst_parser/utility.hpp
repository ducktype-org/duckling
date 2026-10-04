#pragma once

#include <base/collections/optional.hpp>

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
	 * @brief If @p root contains exactly one child and that child is a statement, returns it.
	 *
	 * This helper centralizes the common "unlock root -> inspect children -> enforce single item"
	 * sequence used by statement classification utilities in the REPL and script handling code.
	 *
	 * @note This is a generic helper that extracts ANY single statement without filtering by type
	 * (expression, instruction, definition, action, or any other statement kind).
	 * Unlike the more specific functions in this file (extractSingleExpression,
	 * extractSingleInstruction, extractSingleDefinition), this function does not apply
	 * statement-kind restrictions.
	 *
	 * @param ctx  Query context for PST access
	 * @param root The PST root element to examine
	 * @return The single statement if present, empty otherwise
	 */
	base::Optional<AccessLocked<Stmt>> extractSingleStatement(
		query::Context& ctx, const AccessLocked<LangElement>& root
	);

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
	 * @brief If @p root contains exactly one declaration/definition statement, returns it.
	 *
	 * A statement is considered declaration/definition when `Stmt::isDeclaration()`
	 * returns `DeclKind::Symbol` or `DeclKind::Transparent`.
	 *
	 * This is the explicit REPL/script path for statements such as functions, variables,
	 * constants, classes, namespaces, usings and imports.
	 *
	 * @param ctx  Query context for PST access
	 * @param root The PST root element to examine
	 * @return The single declaration/definition Stmt if present, empty otherwise
	 */
	base::Optional<AccessLocked<Stmt>> extractSingleDefinition(
		query::Context& ctx, const AccessLocked<LangElement>& root
	);
}
