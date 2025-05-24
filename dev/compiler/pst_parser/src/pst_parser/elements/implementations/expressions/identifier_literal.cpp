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

	void IdentifierLiteral::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitIdentifierLiteral(*this);
	}
}
