#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Element representing a number value in an expression
	 */
	class ExprValue final: public ExprElement {
		lexer::Value                 number;
		base::Optional<lexer::Value> type_specifier;

	public:
		[[nodiscard]]
		lexer::Value getValue() const {
			return number;
		}

		[[nodiscard]]
		base::Optional<lexer::Value> getTypeSpecifier() const {
			return type_specifier;
		}

		explicit ExprValue(
			const dia::SourcePosition&   position,
			lexer::Value                 value,
			base::Optional<lexer::Value> type_specifier = {}
		):
			  ExprElement(position, 0),
			  number(value),
			  type_specifier(std::move(type_specifier)) {}

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
