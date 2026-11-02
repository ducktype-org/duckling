// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once
#include "highlight.hpp"
#include "styles.hpp"

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>

#include <diagnostic/core/term_ui_view.hpp>

namespace term_ui {
	/* Minimal amount of whitespace needed before the line bar `|`. */
	u64 minTabSpace(const dia::term_ui_view::CodeLine& line);

	/**
	 * @brief Print the code line to the output stream and calculate the highlights.
	 * Highligh is a span of the code from start column to end column for one pointer message.
	 *
	 * Returns a list of highlights to be displayed in the following lines.
	 *
	 * @param line The code line to print.
	 * @param tab_space The column where the code should begin.
	 * @param ctx A lookup map for highlight message contents.
	 * @param out The output stream.
	 * @return std::vector<Highlight>
	 */
	std::vector<Highlight> printAndCalculateHighlights(
		const dia::term_ui_view::CodeLine&                           line,
		u64                                                          tab_space,
		const base::HashMap<u64, dia::term_ui_view::PointerMessage>& ctx,
		std::ostream&                                                out
	);
}
