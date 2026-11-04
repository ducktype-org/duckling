// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "source_position.hpp"

#include <printer/stream_printer.hpp>

namespace dia {
	/**
	 * @brief Print code snippets to printer with positions highlighted.
	 *
	 * @param positions - Positions to highlight.
	 * @param neighborhood - Adjacent lines to include.
	 * @param line_color - Color of line numbers.
	 * @param highlight_color - Color of highlighted code.
	 */
	void printHighlightedPositions(
		printer::PrinterOStream&,
		const std::vector<SourcePosition>& positions,
		usize                              neighborhood    = (usize) -1,
		printer::Color                     line_color      = printer::Color::BrightBlue,
		printer::Color                     highlight_color = printer::Color::BrightMagenta
	);
}
