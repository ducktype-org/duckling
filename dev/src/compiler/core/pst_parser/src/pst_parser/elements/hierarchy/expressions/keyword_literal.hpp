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
		KeywordLiteral(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

		~KeywordLiteral() override = default;
		void dprint(std::ostream& out) const final;
		void acceptExprVisitor(PstExprVisitor& visitor) const final;

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
