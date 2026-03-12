#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Element that represents an keyword literal in an expression
	 */
	class KeywordLiteral final: public ExprElement {
		Keyword keyword = Keyword::NotAKeyword;
		NAMED_CHILD_OPT(template_specifier, ExprElement);

	public:
		KeywordLiteral(const LangParserState& state): ExprElement(state, 300) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~KeywordLiteral() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		Keyword getKeyword() const {
			return keyword;
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Keyword Expression";
		}
	};
}
