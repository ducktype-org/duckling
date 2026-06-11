#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Element representing a Char value in an expression
	 */
	class ExprCharValue final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ExprCharValue, ExprElement, string);

	protected:
		tpc::CharValue string;

	public:
		[[nodiscard]]
		tpc::CharValue getValue() const {
			return string;
		}

		explicit ExprCharValue(const LangParserState& state, tpc::CharValue value):
			  ExprElement(state, 0),
			  string(value) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~ExprCharValue() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Char Value Expr";
		}
	};
}
