/**
 * @file semantic_tokens.hpp
 * @brief Semantic tokens definition
 */

#pragma once

#include <frontend/module_tree/source_file.hpp>
#include <pst_parser/pst.hpp>

#include <base/ints.hpp>
#include <base/ref.hpp>
#include <base/stringifyable_enum.hpp>

#include <string>

MAKE_STRINGIFYABLE_ENUM(lsp, int8_t, Type,
	Namespace,
	Class,
	Enum,
	Interface,
	Struct,
	TypeParameter,
	Type,
	Parameter,
	Variable,
	Property,
	EnumMember,
	Decorator,
	Event,
	Function,
	Method,
	Macro,
	Label,
	Comment,
	String,
	Keyword,
	Number,
	Regexp,
	Operator,
	Unknown
)

namespace lsp {

	class SemanticToken final {
	public:
		// https://code.visualstudio.com/api/language-extensions/semantic-highlight-guide#standard-token-types-and-modifiers

		SemanticToken(CRef<lexer::Token>);

		static Type translateType(lexer::Token::Type);

		std::string toJSON();

	private:
		CRef<lexer::Token> source_token;
		u64                line;
		u64                start_character;
		u64                length;
		Type               type;
		// @TODO token modifiers (Duckling LSP 2.0)
	};

	std::string getSemanticTokens(base::Ref<compiler::frontend::SourceFile>);
	std::string getSemanticTokens(const std::vector<base::Ref<compiler::frontend::SourceFile>>& files
	);
}
