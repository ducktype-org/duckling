#include "preamble.hpp"

#include "../../hierarchy/not_statements.hpp"

namespace pst::expr {
	ParserRef<ExprElement> Literal::parse(RiftParserState& state, u64 length) {
		std::cerr << "Parsing Literal" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		if (state[0].isIdentifier()) {
			return IdentifierLiteral::parse(state, length);
		} else if (state[0].isNumLiteral()) {
			return ExprValue::parse(state, length);
		} else if (state[0].isBracketGroup(lexer::Token::Round)) {
			return RoundExpr::parse(state, length);
		} else if (state[0].isBracketGroup(lexer::Token::Curly)) {
			return BlockExpr::parse(state, length);
		} else {
			// Error
			fastForward(state, length);
			return nullptr;
		}
	}
}
