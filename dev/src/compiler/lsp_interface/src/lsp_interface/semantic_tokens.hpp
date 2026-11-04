// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file semantic_tokens.hpp
 * @brief Classification of the tokens of a source file for the `textDocument/semanticTokens`
 * requests.
 */

#pragma once

#include <frontend/module_tree/source_file.hpp>
#include <lsp/types.h>

#include <base/pointers/ref.hpp>

#include <query_framework/context/context_fd.hpp>

namespace duck_ls {

	/**
	 * @brief The legend the server advertises: the token types in the order the indices of
	 * `lsp::SemanticTokenTypes` refer to, and no modifiers.
	 */
	lsp::SemanticTokensLegend semanticTokensLegend();

	/**
	 * @brief Classifies every token of `file` and encodes them the way the protocol expects:
	 * sorted and relative to the previous token.
	 *
	 * Without `analyze` the tokens are classified by their lexical kind only. With it, names
	 * are classified by the declarations and the semantic analysis of the expressions they
	 * appear in, which is only safe once the whole package parsed.
	 */
	lsp::SemanticTokens collectSemanticTokens(
		query::Context& ctx, base::Ref<compiler::frontend::SourceFile> file, bool analyze
	);
}
