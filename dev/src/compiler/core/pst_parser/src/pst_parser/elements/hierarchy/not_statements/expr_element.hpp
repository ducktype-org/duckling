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
		 * @brief Skips tokens, used to preserve position in case of error.
		 */
		static void fastForward(LangParserState& state, i64 length);

		/**
		 * @brief Sanity check of length.
		 */
		static bool checkLength(LangParserState& state, i64 length);

		explicit ExprElement(const dia::SourcePosition& position, i64 precedence):
			  NotStmt(position),
			  PRECEDENCE(precedence) {
			this->element_kind = ElementKind::ExprElement;
		}

	public:
		virtual void acceptExprVisitor(expr::PstExprVisitor& visitor) const = 0;
	};
}

