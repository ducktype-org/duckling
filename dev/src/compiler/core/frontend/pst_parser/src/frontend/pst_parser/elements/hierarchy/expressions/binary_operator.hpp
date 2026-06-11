#pragma once

#include "../not_statements/wrapper_elements/operator_wrapper.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Common ancestor for binary operator elements.
	 */
	class BinaryOperator: public ExprElement {
		THIS_CLASS(BinaryOperator);
		PARENT_CLASS(ExprElement);
	protected:
		CLONE_SUBELEMENTS();
	protected:
		NAMED_CHILD(left, ExprElement);
		NAMED_CHILD(op, OperatorWrapper);
		NAMED_CHILD(right, ExprElement);

	public:
		ELEMENT_CLONE_DECL(BinaryOperator);
		explicit BinaryOperator(const LangParserState& state, i64 precedence):
			  ExprElement(state, precedence) {}

		~BinaryOperator() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		AccessLocked<ExprElement> getLeftOperand() const;
		[[nodiscard]]
		AccessLocked<ExprElement> getRightOperand() const;
		[[nodiscard]]
		AccessLocked<OperatorWrapper> getOperator() const;

		[[nodiscard]]
		std::string elementType() const override {
			return "Binary Operator";
		}
	};
}
