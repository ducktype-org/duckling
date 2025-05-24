#pragma once

#include "preamble.hpp"

namespace pst::expr {
	/**
	 * @brief Element representing a string value in an expression
	 */
	class ExprStrValue final: public ExprElement {
		tpc::StringValue string;

	public:
		[[nodiscard]]
		tpc::StringValue getValue() const {
			return string;
		}

		explicit ExprStrValue(const dia::SourcePosition& position, tpc::StringValue value):
			  ExprElement(position, 0),
			  string(value) {}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

		~ExprStrValue() override = default;
		void dprint(std::ostream& out) const final;
		void acceptExprVisitor(PstExprVisitor& visitor) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "String Value Expr";
		}
	};
}
