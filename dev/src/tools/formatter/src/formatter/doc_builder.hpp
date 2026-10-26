// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file doc_builder.hpp
 * @brief Builds the layout document (Doc) for a parsed statement list.
 */
#pragma once

#include "config.hpp"
#include "doc.hpp"
#include "statement_tree.hpp"

#include <vector>

namespace formatter {

	/**
	 * @brief Translates @p statements into a Doc.
	 *
	 * All spacing decisions are made here (the renderer only breaks lines): tokens are joined
	 * per the spacing oracle, bracket groups become Group nodes that explode one element per
	 * line when over-long, over-long expressions become Fill nodes that break greedily at
	 * method-chain dots or binary operators, and comments attach to the element or statement
	 * they belong to.
	 */
	[[nodiscard]]
	Doc buildFileDoc(const std::vector<Statement>& statements, const FormatConfig& config);
}
