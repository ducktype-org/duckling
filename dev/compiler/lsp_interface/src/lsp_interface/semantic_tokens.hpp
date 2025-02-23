/**
 * @file semantic_tokens.hpp
 * @brief Semantic tokens definition
 */

#pragma once

#include <lexer/lexer.hpp>
#include <pst_parser/pst.hpp>
#include <base/ref.hpp>
#include <base/ints.hpp>

#include <string>

// clang-format off
MAKE_STRINGIFYABLE_ENUM(lsp, i32, Type,
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
// clang-format on

namespace lsp {
	class SemanticToken;

	class SemanticToken {
	public:
		// https://code.visualstudio.com/api/language-extensions/semantic-highlight-guide#standard-token-types-and-modifiers

		SemanticToken(lexer::Token&);

		static Type translateType(lexer::Token::Type);

		std::string toJSON();

	private:
		lexer::Token& sourceToken;
		i64           line;
		i64           startCharacter;
		i64           length;
		Type          type;
		// @TODO token modifiers (Duckling LSP 2.0)
	};

	void getSemanticTokens(MCRef<pst::LangElement>, std::vector<SemanticToken>&);
	std::string getSemanticTokens(MCRef<pst::LangElement>);
}
