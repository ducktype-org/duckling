#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Common ancestor for suffix operator elements.
	 */
	class SuffixOperator: public ExprElement {
	protected:
		NAMED_CHILD(op, OperatorWrapper);
		NAMED_CHILD(expr, ExprElement);

	public:
		explicit SuffixOperator(const LangParserState& state, i64 precedence):
			  ExprElement(state, precedence) {}

		~SuffixOperator() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Suffix Operator";
		}

		[[nodiscard]]
		AccessLocked<OperatorWrapper> getOperator() const;
		[[nodiscard]]
		AccessLocked<ExprElement> getExpr() const;
	};
}
