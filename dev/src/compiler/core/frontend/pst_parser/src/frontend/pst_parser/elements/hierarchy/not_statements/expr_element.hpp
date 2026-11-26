// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Common root for expression sub-elements.
	 */
	class ExprElement: public NotStmt {
		THIS_CLASS(ExprElement);
		PARENT_CLASS(NotStmt);

	protected:
		const i64 PRECEDENCE;

	protected:
		/**
		 * @brief Sanity check of non-emptyness length.
		 */
		static bool checkNonEmpty(LangParserState& state);

		/**
		 * @brief Checks if the expression is a single keyword prefix operator (e.g. `ref`), which
		 * is used as a value instead of being applied to an operand.
		 */
		static bool isStandaloneKeywordOperator(const LangParserState& state);

		/**
		 * @brief Whether the token at @p fwd is a part of a prefix operator: a prefix operator
		 * itself, or the `mut` of a writable view like `ref mut`, `ptr mut`, etc.
		 */
		static bool isPrefixOperatorPart(const LangParserState& state, i64 fwd);

		ELEMENT_CLONE_DECL(ExprElement, PRECEDENCE);

		explicit ExprElement(LangElementConstructionData state, i64 precedence):
			  NotStmt(state),
			  PRECEDENCE(precedence) {
			this->element_kind = ElementKind::ExprElement;
		}

	public:
		virtual void acceptExprVisitor(expr::PstExprVisitor& visitor) const = 0;
	};
}
