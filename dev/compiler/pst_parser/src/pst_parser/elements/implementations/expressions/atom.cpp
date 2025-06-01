#include "../../hierarchy/expressions/atom.hpp"

#include "../../hierarchy/expressions/block_expr.hpp"
#include "../../hierarchy/not_statements/code_block.hpp" // IWYU pragma: keep
#include "../../hierarchy/expressions/char_value.hpp"
#include "../../hierarchy/expressions/identifier_literal.hpp"
#include "../../hierarchy/expressions/keyword_literal.hpp"
#include "../../hierarchy/expressions/round_expr.hpp"
#include "../../hierarchy/expressions/string_value.hpp"
#include "../../hierarchy/expressions/value.hpp"
#include "preamble.hpp"

namespace pst::expr {
	class NoAtomError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected an atom here (singular expression value).";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		NoAtomError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<ExprElement> Atom::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		if (state[0].isKeyword()) {
			return KeywordLiteral::parse(state, length);
		} else if (state[0].isIdentifier()) {
			return IdentifierLiteral::parse(state, length);
		} else if (state[0].isNumLiteral()) {
			return ExprValue::parse(state, length);
		} else if (state[0].isString()) {
			return ExprStrValue::parse(state, length);
		} else if (state[0].isChar()) {
			return ExprCharValue::parse(state, length);
		} else if (state[0].isBracketGroup(lexer::Token::Round)) {
			return RoundExpr::parse(state, length);
		} else if (state[0].isBracketGroup(lexer::Token::Curly)) {
			return BlockExpr::parse(state, length);
		} else {
			state.log(makeBox<NoAtomError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
			fastForward(state, length);
			return nullptr;
		}
	}
}
