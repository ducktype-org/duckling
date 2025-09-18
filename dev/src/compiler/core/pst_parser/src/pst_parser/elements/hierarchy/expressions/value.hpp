#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Element representing a number value in an expression
	 */
	class ExprValue final: public ExprElement {
		lexer::Value number;

	public:
		[[nodiscard]]
		lexer::Value getValue() const {
			return number;
		}

		explicit ExprValue(const dia::SourcePosition& position, lexer::Value value):
			  ExprElement(position, 0),
			  number(value) {}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

		~ExprValue() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& calcStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Value Expr";
		}
	};
}
