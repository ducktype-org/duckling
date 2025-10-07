#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Unit type or value (Empty parenthesis expression).
	 */
	class UnitExpr final: public ExprElement {
	public:
		explicit UnitExpr(const dia::SourcePosition& pos): ExprElement(pos, 0) {}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

		~UnitExpr() override = default;
		void dprint(std::ostream& out) const final;
		void acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Unit Expression";
		}
	};
}
