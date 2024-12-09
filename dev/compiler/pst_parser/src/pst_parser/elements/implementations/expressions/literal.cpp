#include "preamble.hpp"

#include "../../hierarchy/not_statements.hpp"

namespace pst::expr {
	class BadLiteralError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected a literal";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadLiteralError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<ExprElement> Literal::parse(LangParserState& state, i64 length) {
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
			state.log(base::make_unique<BadLiteralError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
			fastForward(state, length);
			return nullptr;
		}
	}
}
