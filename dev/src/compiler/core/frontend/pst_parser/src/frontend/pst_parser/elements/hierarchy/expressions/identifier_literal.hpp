#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Element that represents an identifier literal in an expression
	 */
	class IdentifierLiteral final: public ExprElement {
		tpc::Identifier name;
		NAMED_CHILD_OPT(template_specifier, ExprElement);

	public:
		IdentifierLiteral(const LangParserState& state): ExprElement(state, 300) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~IdentifierLiteral() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		const tpc::Identifier& getName() const {
			return name;
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Identifier Expression";
		}
	};
}
