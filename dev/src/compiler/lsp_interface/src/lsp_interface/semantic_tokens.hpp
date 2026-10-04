// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file semantic_tokens.hpp
 * @brief Semantic tokens definition
 * @TODO: #3604 bring this back
 */

#pragma once

#include <frontend/module_tree/source_file.hpp>
#include <frontend/pst_parser/pst.hpp>

#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <string>

MAKE_STRINGIFYABLE_ENUM(lsp, int8_t, StandardTokenType,
	Namespace,
	Type,
	Class,
	Enum,
	Interface,
	Struct,
	TypeParameter,
	Parameter,
	Variable,
	Property,
	EnumMember,
	Event,
	Function,
	Method,
	Macro,
	Keyword,
	Modifier,
	Comment,
	String,
	Number,
	Regexp,
	Operator,
	Decorator,
	Unknown
)

namespace lsp {

	class SemanticToken final {
	public:
		// https://code.visualstudio.com/api/language-extensions/semantic-highlight-guide#standard-token-types-and-modifiers

		SemanticToken(CRef<lexer::Token>);
		SemanticToken(CRef<lexer::Token>, StandardTokenType);

		static StandardTokenType translateType(lexer::Token::Type);

		std::string toJSON();

	private:
		CRef<lexer::Token> source_token;
		u64                line;
		u64                start_character;
		u64                length;
		StandardTokenType  type;
	};

	std::string getSemanticTokens(base::Ref<compiler::frontend::SourceFile>);
	std::string getSemanticTokens(const std::vector<base::Ref<compiler::frontend::SourceFile>>& files
	);
}
