#include "preamble.hpp"

namespace pst::expr {
	ParserRef<ExprElement> Access::parse(LangParserState& state, u64 length) {
		std::cerr << "Parsing Access Specifier" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		if (length != 2 && length != 4) {}  // Error

		auto out = base::make_unique<Access>(state.getPosition());

		out->type = state[0].getValue();
		state.parse(out).eatOne();  // `.` or `.?`
		state.parse(out).one(&out->name);

		if (length > 2 && state[0].is(NamedOperator::Colon)
		    && state[1].isBracketGroup(Token::Curly))
			state.parse(out).with(&out->template_specifier, TemplateSpecifier::parse, 2UL);

		return out;
	}

	void Access::dprint(std::ostream& out) const {
		out << "{";

		out << R"("type": ")" << type.str() << "\"";
		out << R"(, "name": )";
		nullAwareDprint(name, out);
		if (template_specifier) {
			out << R"(, "template specifier": )";
			nullAwareDprint(template_specifier.value(), out);
		}

		out << "}";
	}
}
