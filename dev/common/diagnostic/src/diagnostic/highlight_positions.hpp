#pragma once

#include "source_position.hpp"

#include <printer/stream_printer.hpp>

namespace dia {
	/**
	 * @brief Print code snippets to printer with marked positions.
	 *
	 * @param positions - Positions to mark
	 * @param neighborhood - Adjacent lines to include.
	 * @param line_color - Color of line numbers
	 * @param highlight_color - Color of highlighted text
	 */
	void printHighlightedPositions(
		printer::PrinterOStream&,
		const std::vector<SourcePosition>& positions,
		usize                              neighborhood    = (usize) -1,
		printer::Color                     line_color      = printer::Color::BRIGHT_BLUE,
		printer::Color                     highlight_color = printer::Color::BRIGHT_MAGENTA
	);
}
