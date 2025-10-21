#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Element representing a number value in an expression
	 */
	class ExprValue final: public ExprElement {
		tpc::NumericValue value;

	public:
		[[nodiscard]]
		tpc::NumericValue getValue() const {
			return value;
		}

		explicit ExprValue(const dia::SourcePosition& position): ExprElement(position, 0) {}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

		~ExprValue() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Value Expr";
		}
	};
}
