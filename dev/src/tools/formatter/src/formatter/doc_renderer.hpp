// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
	 * their own layout; Fill nodes fill lines greedily. Columns are counted in UTF-8 code
	 * points, with a tab counting as `config.indent_width` columns (see formatter::visualWidth
	 * for what that does and does not cover).
	 */
	[[nodiscard]]
	std::string renderDoc(const Doc& doc, const FormatConfig& config);
}
