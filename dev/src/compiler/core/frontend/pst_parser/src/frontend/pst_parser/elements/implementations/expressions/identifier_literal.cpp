#include "../../hierarchy/expressions/identifier_literal.hpp"

#include "../../hierarchy/expressions/template_specifier.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> IdentifierLiteral::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		auto out = makeBox<IdentifierLiteral>(state);

		PARSE().one(&out->name);

		if (state.ctokens().size() >= 2 && state[0].is(NamedOperator::Colon)
		    && state[1].isBracketGroup(Token::Curly))
			PARSE().with(&out->template_specifier, TemplateSpecifier::parse);

		PST_RETURN out;
	}

	void IdentifierLiteral::dprint(std::ostream& out) const {
		out << "{";

		out << R"("name": )";
		nullAwareDprint(name, out);
		if (template_specifier) {
			out << R"(, "template": )";
			nullAwareDprint(template_specifier.value(), out);
		}

		out << "}";
	}

	HashAlg& IdentifierLiteral::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, template_specifier.has_value());
		return partial_hash;
	}

	void IdentifierLiteral::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitIdentifierLiteral(*this);
	}
}
