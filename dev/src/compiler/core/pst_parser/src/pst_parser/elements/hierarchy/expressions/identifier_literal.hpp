#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Element that represents an identifier literal in an expression
	 */
	class IdentifierLiteral final: public ExprElement {
		tpc::Identifier                             name;
		base::Optional<AccessInternal<ExprElement>> template_specifier;

	public:
		IdentifierLiteral(const dia::SourcePosition& pos): ExprElement(pos, 300) {}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

		~IdentifierLiteral() override = default;
		void dprint(std::ostream& out) const final;
		void acceptExprVisitor(PstExprVisitor& visitor) const final;

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
