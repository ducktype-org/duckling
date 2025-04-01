#include "../../hierarchy/not_statements.hpp"
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> Atom::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		if (state[0].isKeyword()) {
			return KeywordLiteral::parse(state, length);
		} else if (state[0].isIdentifier()) {
			return IdentifierLiteral::parse(state, length);
		} else if (state[0].isNumLiteral()) {
			return ExprValue::parse(state, length);
		} else if (state[0].isBracketGroup(lexer::Token::Round)) {
			return RoundExpr::parse(state, length);
		} else if (state[0].isBracketGroup(lexer::Token::Curly)) {
			return BlockExpr::parse(state, length);
		} else {
			state.log(makeBox<tpc::NoIdentifierError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
			fastForward(state, length);
			return nullptr;
		}
	}
}
