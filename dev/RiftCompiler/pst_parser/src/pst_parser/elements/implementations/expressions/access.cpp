#include "preamble.hpp"

namespace pst::expr {
	ParserRef<ExprElement> Access::parse(RiftParserState& state, u64 length) {
		std::cerr << "Parsing Access Specifier" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		if (length != 2 && length != 4) {}  // Error

		auto out = base::make_unique<Access>(state.getPosition());

		out->type = state[0].getValue();
		state.parse(out).eatOne();  // `.` or `.?`
		state.parse(out).one(&out->name);

		if (length > 2 && state[0].is(Operator::Colon) && state[1].isBracketGroup(Token::Curly))
			state.parse(out).with(&out->template_specifier, TemplateSpecifier::parse, 2UL);

		return out;
	}
}
