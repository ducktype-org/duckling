#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Common ancestor for suffix operator elements.
	 */
	class SuffixOperator: public ExprElement {
	protected:
		Operator op;
		NAMED_CHILD(expr, ExprElement);

	public:
		explicit SuffixOperator(const LangParserState& state, Operator op, i64 precedence):
			  ExprElement(state, precedence),
			  op(op) {}

		~SuffixOperator() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Suffix Operator";
		}

		[[nodiscard]]
		lexer::Operator getOperator() const;
		[[nodiscard]]
		AccessLocked<ExprElement> getExpr() const;
	};
}
