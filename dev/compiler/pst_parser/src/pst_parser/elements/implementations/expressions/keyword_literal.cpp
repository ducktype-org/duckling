#include "preamble.hpp"

#include "../../hierarchy/not_statements.hpp"

namespace pst::expr {
	MBox<ExprElement> KeywordLiteral::parse(LangParserState& state, i64 length) {
		// std::cerr << "Parsing IdentifierLiteral Specifier" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		auto out = makeBox<KeywordLiteral>(state.getPosition());

		state.parse(out).one(&out->keyword);

		if (length > 2 && state[0].is(NamedOperator::Colon)
		    && state[1].isBracketGroup(Token::Curly))
			state.parse(out).with(&out->template_specifier, TemplateSpecifier::parse, 2L);

		return out;
	}

	void KeywordLiteral::dprint(std::ostream& out) const {
		out << "{";

		out << R"("keyword": )";
		tpc::nullAwareDprint(keyword, out);
		if (template_specifier) {
			out << R"(, "template": )";
			nullAwareDprint(template_specifier.value(), out);
		}

		out << "}";
	}

	void KeywordLiteral::acceptVisitor(PstExprVisitor& visitor) const {
		visitor.visitKeywordLiteral(*this);
	}
}
