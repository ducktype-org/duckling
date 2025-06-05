#pragma once

#include "preamble.hpp"

namespace pst::expr {
	/**
	 * @brief Common ancestor for binary operator elements.
	 */
	class BinaryOperator: public ExprElement {
	protected:
		AccessInternal<ExprElement> left;
		Operator                    op;
		AccessInternal<ExprElement> right;

	public:
		explicit BinaryOperator(const dia::SourcePosition& pos, Operator op, i64 precedence):
			  ExprElement(pos, precedence),
			  op(op) {}

		~BinaryOperator() override = default;
		void dprint(std::ostream& out) const final;
		void acceptExprVisitor(PstExprVisitor& visitor) const final;

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
