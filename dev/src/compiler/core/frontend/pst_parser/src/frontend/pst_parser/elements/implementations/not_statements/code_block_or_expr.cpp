// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/not_statements/code_block_or_expr.hpp"

#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(CodeBlockOrExpr);

	MBox<CodeBlockOrExpr> CodeBlockOrExpr::parse(LangParserState& state) {
		auto out = makeBox<CodeBlockOrExpr>(state);
		if (state[0].isBracketGroup(Token::BracketType::Curly)) {
			MBox<CodeBlock> block;
			PARSE().one(&block);
			if (!block) return nullptr;
			PARSE().assign(&out->code_block, std::move(block));
		} else {
			if (state[0].is(Special::AtSign)) {
				state.logInt(makeBox<SimpleExpressionNotAttributeAllowed>(state.getPosition()));
			} else if (state[0].isKeyword()) {
				Keyword key = state[0].asKeyword();
				if (lang_def::keywordFlags(key).contains(lang_def::KeywordFlagsOptions::IsSpecifier)) {
					state.logInt(makeBox<SimpleExpressionNotSpecifierAllowed>(state.getPosition()));
				} else if (lang_def::keywordFlags(key).contains(lang_def::KeywordFlagsOptions::IsStmtStart)) {
					state.logInt(makeBox<SimpleExpressionNotStatementAllowed>(state.getPosition()));
				}
			}
			MBox<Stmt> stmt;
			PARSE().one(&stmt);
			if (!stmt) return nullptr;
			PARSE().assign(&out->stmt, std::move(stmt));
		}

		PST_RETURN out;
	}
}
