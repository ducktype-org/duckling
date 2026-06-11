#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Element representing a number value in an expression
	 */
	class ExprNumericValue final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ExprNumericValue, ExprElement, value);
	protected:
		tpc::NumericValue value;

	public:
		[[nodiscard]]
		tpc::NumericValue getValue() const {
			return value;
		}

		explicit ExprNumericValue(const LangParserState& state): ExprElement(state, 0) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~ExprNumericValue() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Numeric Value Expr";
		}
	};
}
