#include "preamble.hpp"

namespace pst::expr {
	ParserRef<ExprElement> Call::parse(LangParserState& state, u64 length) {
		// std::cerr << "Parsing Call Expression" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		if (not(length == 1
		        && (state[0].isBracketGroup(lexer::Token::Round)
		            || state[0].isBracketGroup(lexer::Token::Square)))) {}  // Error

		auto out = base::make_unique<Call>(state.getPosition());

		state.parse(out).goDown();
		// This is a little wrong but calls will be changed to fix that
		if (state.ctokens().size() > 0)
			state.parse(out).with(&out->args, Comma::parse, state.ctokens().size());
		state.parse(out).goUpAndSkip();

		return out;
	}

	void Call::dprint(std::ostream& out) const {
		out << "{";

		if (type == lexer::Token::Round)
			out << R"--("type": "()")--";
		else
			out << R"--("type": "[]")--";
		out << R"(, "arguments": )";
		nullAwareDprint(args, out);

		out << "}";
	}

	void Call::acceptVisitor(PstExprVisitor& visitor) const { visitor.visitCall(*this); }
}
