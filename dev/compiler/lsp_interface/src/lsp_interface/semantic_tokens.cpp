/**
 * @file semantic_tokens.cpp
 * @brief This file defines the SemanticToken class, which provides functionality to export
 * semantic tokens in JSON format.
 */
#include "semantic_tokens.hpp"

#include "utils.hpp"

#include <base/stringifyable_enum.hpp>
#include <base/variant.hpp>

#include <map>
#include <string>

namespace lsp {
	SemanticToken::SemanticToken(CRef<lexer::Token> source):
		  source_token(source),
		  line(source->getPosition().getStartLineColumn().first),
		  start_character(source->getPosition().getStartLineColumn().second),
		  length(source->getPosition().getEnd() - source->getPosition().getStart() + 1),
		  type(translateType(source->getType())) {}

	Type SemanticToken::translateType(lexer::Token::Type type) {
		using lTT = lexer::Token::Type;
		using sT  = Type;
		switch (type) {
		case lTT::Keyword:
			return sT::Keyword;
		// case lTT::Identifier: return;
		case lTT::NumLiteral:
			return sT::Number;
		case lTT::String:
			return sT::String;
		case lTT::FormattedString:
			return sT::String;
		// case lTT::BracketGroup: return;
		case lTT::Operator:
			return sT::Operator;
		case lTT::Comment:
			return sT::Comment;
		// case lTT::Special: return;
		// case lTT::Empty: return;
		// case lTT::Sentinel: return;
		// case lTT::Error: return;
		default:
			return sT::Unknown;
		}
	}

	std::string SemanticToken::toJSON() {
		std::map<std::string, std::string> result;

		result["line"]           = std::to_string(this->line);
		result["startCharacter"] = std::to_string(this->start_character);
		result["length"]         = std::to_string(this->length);
		result["tokenType"]      = base::enumToStr(this->type).strView();
		result["tokenModifiers"] = "0";  // @TODO Duckling LSP 2.0

		return jsonDict(result);
	}

	void getSemanticTokens(
		pst::AccessLocked<pst::LangElement> element, std::vector<SemanticToken>& token_list
	) {
		auto unlocked = element.illegalAccess().value();
		for (auto sub: unlocked->viewSubElements()) {
			variant_match(sub) {
				variant_case(pst::LangElement::SubToken, token) {
					// add token to list
					auto semantic_token = SemanticToken(token);
					token_list.push_back(semantic_token);
				}
				variant_case(pst::LangElement::Child, child) {
					// recursive token generation
					getSemanticTokens(child, token_list);
				}
			}
		}
	}

	std::string getSemanticTokens(pst::AccessLocked<pst::LangElement> element) {
		std::vector<SemanticToken> tokens;
		getSemanticTokens(element, tokens);

		std::vector<std::string> token_strings(tokens.size());
		for (usize i = 0; i < token_strings.size(); i++) token_strings[i] = tokens[i].toJSON();

		return jsonList(token_strings);
	}
}
