#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Common ancestor for prefix operator elements.
	 */
	class PrefixOperator: public ExprElement {
	protected:
		NAMED_CHILD(expr, ExprElement);
		Operator op;

	public:
		explicit PrefixOperator(const LangParserState& state, Operator op, i64 precedence):
			  ExprElement(state, precedence),
			  op(op) {}

		~PrefixOperator() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Prefix Operator";
		}

		[[nodiscard]]
		lexer::Operator getOperator() const;
		[[nodiscard]]
		AccessLocked<ExprElement> getExpr() const;
	};
}
