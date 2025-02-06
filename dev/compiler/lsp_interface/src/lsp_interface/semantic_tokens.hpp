/**
 * @file semantic_tokens.hpp
 * @brief Semantic tokens definition
 */

#pragma once

#include <lexer/lexer.hpp>
#include <pst_parser/pst.hpp>
#include <base/ref.hpp>

#include <string>

namespace lsp {
	class SemanticToken;

	class SemanticToken {
	public:
		// https://code.visualstudio.com/api/language-extensions/semantic-highlight-guide#standard-token-types-and-modifiers
		enum class Type {
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
		};

		SemanticToken(lexer::Token);

		static Type translateType(lexer::Token::Type);

		std::string toJSON();

	private:
		lexer::Token sourceToken;
		int          ine;
		int          startCharacter;
		int          length;
		Type         type;
		// @TODO token modifiers
	};

	void getSemanticTokens(MCRef<pst::LangElement>, std::vector<SemanticToken>&);
	std::string getSemanticTokens(MCRef<pst::LangElement>);
}
