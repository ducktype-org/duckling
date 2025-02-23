/**
 * @file semantic_tokens.cpp
 * @brief This file defines the SemanticToken class, which provides functionality to export
 * semantic tokens in JSON format.
 */
#include "semantic_tokens.hpp"
#include "utils.hpp"

#include <base/variant.hpp>
#include <base/stringifyable_enum.hpp>

#include <string>
#include <map>

namespace lsp {
    SemanticToken::SemanticToken(lexer::Token& source):
        sourceToken(source),
        line(source.getPosition().getStartLineColumn().first),
        startCharacter(source.getPosition().getStartLineColumn().second),
        length(source.getPosition().getEnd() - source.getPosition().getStart() + 1),
        type(translateType(source.getType())) {}

    Type SemanticToken::translateType(lexer::Token::Type type) {
        using lTT = lexer::Token::Type;
        using sT = Type;
        switch (type) {
            case lTT::Keyword:         return sT::Keyword;
            // case lTT::Identifier: return;
            case lTT::NumLiteral:      return sT::Number;
            case lTT::String:          return sT::String;
            case lTT::FormattedString: return sT::String;
            // case lTT::BracketGroup: return;
            case lTT::Operator:        return sT::Operator;
            case lTT::Comment:         return sT::Comment;
            // case lTT::Special: return;
            // case lTT::Empty: return;
            // case lTT::Sentinel: return;
            // case lTT::Error: return;
            default:                   return sT::Unknown;
        }
    }

    std::string SemanticToken::toJSON() {
        std::map<std::string, std::string> result;

        result["line"]           = std::to_string(this->line);
        result["startCharacter"] = std::to_string(this->startCharacter);
        result["length"]         = std::to_string(this->length);
        result["tokenType"]      = base::enumToStr(this->type).strView();
        result["tokenModifiers"] = "0"; // @TODO Duckling LSP 2.0

        return jsonDict(result);
    }

    void getSemanticTokens(MCRef<pst::LangElement> element, std::vector<SemanticToken>& token_list) {
        for (auto sub: element->viewSubElements()) {
            variant_match(sub) {
                variant_case(pst::LangElement::SubToken, token) {
                    // add token to list
                    auto semantic_token = SemanticToken((lexer::Token&) token);
                    token_list.push_back(semantic_token);
                }
                variant_case(pst::LangElement::ConstChild, child) {
                    // recursive token generation
                    getSemanticTokens(child, token_list);
                }
            }
        }
    }

    std::string getSemanticTokens(MCRef<pst::LangElement> element) {
        std::vector<SemanticToken> tokens;
        getSemanticTokens(element, tokens);

        std::vector<std::string> token_strings;
        for (auto token: tokens) {
            token_strings.push_back(token.toJSON());
        }

        return jsonList(token_strings);
    }
}
