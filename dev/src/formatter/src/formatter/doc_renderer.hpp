/**
 * @file doc_renderer.hpp
 * @brief Renders a layout document (Doc) to formatted source text.
 */
#pragma once

#include "config.hpp"
#include "doc.hpp"

#include <string>

namespace formatter {

	/**
	 * @brief Renders @p doc against the configured line length.
	 *
	 * The top level renders in broken mode: every Line node starts a new line. Groups decide
	 * their own layout; Fill nodes fill lines greedily. Columns are measured visually — a tab
	 * counts as `config.indent_width` columns — so wrapping matches what an editor shows.
	 */
	[[nodiscard]]
	std::string renderDoc(const Doc& doc, const FormatConfig& config);
}
