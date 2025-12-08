#include "../../hierarchy/expressions/identifier_literal.hpp"

#include "../../hierarchy/expressions/template_specifier.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> IdentifierLiteral::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		auto out = makeBox<IdentifierLiteral>(state.getPosition());

		state.parse(out).one(&out->name);

		if (length > 2 && state[0].is(NamedOperator::Colon) && state[1].isBracketGroup(Token::Curly))
			state.parse(out).with(&out->template_specifier, TemplateSpecifier::parse, 2L);

		return out;
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

	LangElement::HashAlg& IdentifierLiteral::addElementDataToStableHash(HashAlg& partial_hash
	) const {
		addToHash(partial_hash, name);
		addToHash(partial_hash, template_specifier.has_value());
		return partial_hash;
	}

	void IdentifierLiteral::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitIdentifierLiteral(*this);
	}
}
