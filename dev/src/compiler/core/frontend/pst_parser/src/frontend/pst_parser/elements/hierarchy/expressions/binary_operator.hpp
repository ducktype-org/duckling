#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Common ancestor for binary operator elements.
	 */
	class BinaryOperator: public ExprElement {
	protected:
		NAMED_CHILD(left, ExprElement);
		Operator op;
		NAMED_CHILD(right, ExprElement);

	public:
		explicit BinaryOperator(const LangParserState& state, Operator op, i64 precedence):
			  ExprElement(state, precedence),
			  op(op) {}

		~BinaryOperator() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		AccessLocked<ExprElement> getLeftOperand() const;
		[[nodiscard]]
		AccessLocked<ExprElement> getRightOperand() const;
		[[nodiscard]]
		lexer::Operator getOperator() const;

		[[nodiscard]]
		std::string elementType() const override {
			return "Binary Operator";
		}
	};
}
