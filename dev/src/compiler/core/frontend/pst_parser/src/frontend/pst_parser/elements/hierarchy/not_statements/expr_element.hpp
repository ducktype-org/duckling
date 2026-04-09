#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Common root for expression sub-elements.
	 */
	class ExprElement: public NotStmt {
		const i64 PRECEDENCE;

	protected:
		/**
		 * @brief Sanity check of non-emptyness length.
		 */
		static bool checkNonEmpty(LangParserState& state);

		explicit ExprElement(const LangParserState& state, i64 precedence):
			  NotStmt(state),
			  PRECEDENCE(precedence) {
			this->element_kind = ElementKind::ExprElement;
		}

	public:
		virtual void acceptExprVisitor(expr::PstExprVisitor& visitor) const = 0;
	};
}
