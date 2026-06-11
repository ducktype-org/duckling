#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Element representing a string value in an expression
	 */
	class ExprStrValue final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ExprStrValue, ExprElement, string);
	protected:
		tpc::StringValue string;

	public:
		[[nodiscard]]
		tpc::StringValue getValue() const {
			return string;
		}

		explicit ExprStrValue(const LangParserState& state, tpc::StringValue value):
			  ExprElement(state, 0),
			  string(value) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~ExprStrValue() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "String Value Expr";
		}
	};
}
