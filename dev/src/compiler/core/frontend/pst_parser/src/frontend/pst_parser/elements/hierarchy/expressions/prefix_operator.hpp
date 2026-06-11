#pragma once

#include "../not_statements/wrapper_elements/operator_wrapper.hpp"
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Common ancestor for prefix operator elements.
	 */
	class PrefixOperator: public ExprElement {
		THIS_CLASS(PrefixOperator);
		PARENT_CLASS(ExprElement);
	protected:
		CLONE_SUBELEMENTS();
	protected:
		NAMED_CHILD(op, OperatorWrapper);
		NAMED_CHILD(expr, ExprElement);

	public:
		ELEMENT_CLONE_DECL(PrefixOperator);
		explicit PrefixOperator(const LangParserState& state, i64 precedence):
			  ExprElement(state, precedence) {}

		~PrefixOperator() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Prefix Operator";
		}

		[[nodiscard]]
		AccessLocked<OperatorWrapper> getOperator() const;
		[[nodiscard]]
		AccessLocked<ExprElement> getExpr() const;
	};
}
