#pragma once

#include "expr_common.hpp"

#include "../not_statements/wrapper_elements/keyword_wrapper.hpp" // IWYU pragma: keep

namespace pst::expr {
	/**
	 * @brief Element that represents an keyword literal in an expression
	 */
	class KeywordLiteral final: public ExprElement {
		NAMED_CHILD(keyword, KeywordWrapper);
		NAMED_CHILD_OPT(template_specifier, ExprElement);

	public:
		KeywordLiteral(const LangParserState& state): ExprElement(state, 300) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~KeywordLiteral() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		AccessLocked<KeywordWrapper> getKeyword() const {
			return keyword.give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Keyword Expression";
		}
	};
}
