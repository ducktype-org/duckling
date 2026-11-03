// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/not_statements/code_block_or_any_statement.hpp"

#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(CodeBlockOrAnyStmt);

	MBox<CodeBlockOrAnyStmt> CodeBlockOrAnyStmt::parse(LangParserState& state) {
		auto out = makeBox<CodeBlockOrAnyStmt>(state);
		if (state[0].isBracketGroup(Token::BracketType::Curly)) {
			MBox<CodeBlock> block;
			PARSE().one(&block);
			if (!block) return nullptr;
			PARSE().assign(&out->code_block, std::move(block));
		} else {
			MBox<Stmt> stmt;
			PARSE().one(&stmt);
			if (!stmt) return nullptr;
			PARSE().assign(&out->stmt, std::move(stmt));
		}

		PST_RETURN out;
	}
}
