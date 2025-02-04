/**
 * @file semantic_tokens.hpp
 * @brief Semantic tokens definition
 */

#pragma once

#include <string>
#include <vector>

#include <lexer/lexer.hpp>
#include <pst_parser/pst.hpp>

namespace lsp {
	class ExportSemanticTokens{
    public:
		ExportSemanticTokens();
    	int getSemanticTokens(MCRef<pst::LangElement>);
	};
}
