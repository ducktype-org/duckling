// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expressions/atom.hpp"

#include "../../hierarchy/expressions/array_literal.hpp"
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
	namespace {
		/**
		 * @brief Whether @p key is a statement/specifier keyword that cannot be an expression
		 * value (e.g. `fun`, `class`). `const` is a type modifier, so it is allowed. Other
		 * non-literal keywords (`sizeof`, `assert`, ...) are left to `KeywordLiteral::parse`
		 * and reported as not-yet-implemented by HELIOS.
		 */
		bool isMisusedStatementKeyword(lang_def::Keyword key) {
			if (key == lang_def::Keyword::Const) return false;
			const auto flags = lang_def::keywordFlags(key);
			return flags.contains(lang_def::KeywordFlagsOptions::IsStmtStart)
			    || flags.contains(lang_def::KeywordFlagsOptions::IsSpecifier);
		}
	}

	MBox<ExprElement> Atom::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		if (state[0].is(Keyword::Match)) {
			return MatchExpr::parse(state);
		} else if (state[0].isKeyword()) {
			if (isMisusedStatementKeyword(state[0].asKeyword())) {
				state.logInt(
					makeBox<NoAtomError>(state.getPosition(), state.ctokens().peek().describe())
				);
				return nullptr;
			}
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
			return ArrayLiteral::parse(state);
		} else {
			i64 length = base::safeIntConv<i64>(state.ctokens().size());

			state.logInt(makeBox<NoAtomError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd()),
				state.ctokens().peek().describe()
			));
			return nullptr;
		}
	}
}
