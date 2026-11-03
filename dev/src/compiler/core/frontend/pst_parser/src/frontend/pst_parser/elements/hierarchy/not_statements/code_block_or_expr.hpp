// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../meta.hpp"
#include "code_block_or_statement.hpp"

#include <base/extend_cpp/variant_match.hpp>

namespace pst {
	/**
	 * @brief Code Block or Statement that only allows expressions as single statements.
	 */
	class CodeBlockOrExpr final: public CodeBlockOrStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(CodeBlockOrExpr, CodeBlockOrStmt);
		CLONE_SUBELEMENTS();

	public:
		explicit CodeBlockOrExpr(const LangParserState& state): CodeBlockOrStmt(state) {
			this->element_kind = ElementKind::CodeBlockOrStmt;
		}

		static MBox<CodeBlockOrExpr> parse(LangParserState& state);
		~CodeBlockOrExpr() final = default;
	};
}
