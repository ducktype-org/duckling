#include "../../hierarchy/expressions/atom.hpp"

#include "../../hierarchy/expressions/array_literal_expr.hpp"
#include "../../hierarchy/expressions/block_expr.hpp"
#include "../../hierarchy/expressions/char_value.hpp"
#include "../../hierarchy/expressions/format_string_value.hpp"
#include "../../hierarchy/expressions/identifier_literal.hpp"
#include "../../hierarchy/expressions/keyword_literal.hpp"
#include "../../hierarchy/expressions/match_expr.hpp"
#include "../../hierarchy/expressions/numeric_value.hpp"
#include "../../hierarchy/expressions/round_expr.hpp"
#include "../../hierarchy/expressions/string_value.hpp"
#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> Atom::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		if (state[0].is(Keyword::Match)) {
			return MatchExpr::parse(state);
		} else if (state[0].isKeyword()) {
			return KeywordLiteral::parse(state);
		} else if (state[0].isIdentifier()) {
			return IdentifierLiteral::parse(state);
		} else if (state[0].isNumLiteralGroup()) {
			return ExprNumericValue::parse(state);
		} else if (state[0].isString()) {
			return ExprStrValue::parse(state);
		} else if (state[0].isFormatString()) {
			return ExprFormatStrValue::parse(state);
		} else if (state[0].isChar()) {
			return ExprCharValue::parse(state);
		} else if (state[0].isBracketGroup(lexer::Token::Round)) {
			return RoundExpr::parse(state);
		} else if (state[0].isBracketGroup(lexer::Token::Curly)) {
			return BlockExpr::parse(state);
		} else if (state[0].isBracketGroup(lexer::Token::Square)) {
			return ArrayLiteralExpr::parse(state);
		} else {
			i64 length = base::safeIntConv<i64>(state.ctokens().size());

			state.logInt(makeBox<NoAtomError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
			return nullptr;
		}
	}
}
