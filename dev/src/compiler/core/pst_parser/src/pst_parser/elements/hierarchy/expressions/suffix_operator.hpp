#pragma once

#include "preamble.hpp"

namespace pst::expr {
	/**
	 * @brief Common ancestor for suffix operator elements.
	 */
	class SuffixOperator: public ExprElement {
	protected:
		Operator                    op;
		AccessInternal<ExprElement> expr;

	public:
		explicit SuffixOperator(const dia::SourcePosition& pos, Operator op, i64 precedence):
			  ExprElement(pos, precedence),
			  op(op) {}

		~SuffixOperator() override = default;
		void dprint(std::ostream& out) const final;
		void acceptExprVisitor(PstExprVisitor& visitor) const final;

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
