#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Element that represents an identifier literal in an expression
	 */
	class IdentifierLiteral final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(IdentifierLiteral, ExprElement);
		CLONE_SUBELEMENTS();
	protected:
		NAMED_CHILD(name, IdentifierWrapper);
		NAMED_CHILD_OPT(template_specifier, ExprElement);

	public:
		IdentifierLiteral(const LangParserState& state): ExprElement(state, 300) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~IdentifierLiteral() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getName() const {
			return name.give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Identifier Expression";
		}
	};
}
