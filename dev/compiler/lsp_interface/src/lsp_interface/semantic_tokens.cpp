/**
 * @file semantic_tokens.cpp
 * @brief This file defines the SemanticToken class, which provides functionality to export
 * semantic tokens in JSON format.
 */
#include "semantic_tokens.hpp"

#include <base/variant.hpp>

#include <iostream>

namespace lsp {
    int ExportSemanticTokens::getSemanticTokens(MCRef<pst::LangElement> element) {
        for (auto sub: element->viewSubElements()) {
            variant_match(sub) {
                variant_case(pst::LangElement::SubToken, token) {
                    // add semantic token
                    std::cout << token->getStrValue();
                }

                variant_case(pst::LangElement::ConstChild, child) {
                    // to recursive token generation
                    getSemanticTokens(child);
                }
            }
        }
    }
}