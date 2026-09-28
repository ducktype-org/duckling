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
