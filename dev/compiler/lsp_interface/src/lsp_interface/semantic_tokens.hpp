/**
 * @file semantic_tokens.hpp
 * @brief Semantic tokens definition
 */

#pragma once

#include <string>

#include <filesystem/file.hpp>
#include <pst_parser/elements/elements.hpp>  // toplevel only, @TODO: change it to something better

namespace lsp {
	/**
	 * @brief Class to handle semantic tokens for LSP purposes. Especially export them in JSON format.
	 */
	class SemanticToken {
	public:
		SemanticToken();
	};

    [[nodiscard]]
    std::string getSemanticTokens(const fs::FilePath);
}
