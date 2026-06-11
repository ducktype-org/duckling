#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Unit type or value (Empty parenthesis expression).
	 */
	class UnitExpr final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(UnitExpr, ExprElement);

	public:
		explicit UnitExpr(const LangParserState& state): ExprElement(state, 0) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~UnitExpr() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Unit Expression";
		}
	};
}
