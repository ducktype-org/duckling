// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "stream_printer.hpp"

namespace printer {
	// All background color escape codes are 10 above foregrounds colors.
	ColorID calculateBackgroundColorID(const ColorID color_id) {
		constexpr int8_t BACKGROUND_FONT_COLOR_OFFSET = 10;
		return static_cast<ColorID>(
			color_id > 0 ? color_id + BACKGROUND_FONT_COLOR_OFFSET : color_id
		);
	}

	void StreamPrinter::print(const std::vector<PrinterContent>& contents, std::ostream& out) {
		for (auto& c: contents) print(c, out);
	}

	void StreamPrinter::print(const PrinterContent& content, std::ostream& out) {
		ColorID foreground_color_id{ static_cast<ColorID>(content.foreground_color) };
		ColorID background_color_id{ static_cast<ColorID>(content.background_color) };
		// Background colors have different ids than foreground colors.
		background_color_id = calculateBackgroundColorID(background_color_id);

		if (foreground_color_id > 0) out << "\033[" + std::to_string(foreground_color_id) + "m";
		if (background_color_id > 0) out << "\033[" + std::to_string(background_color_id) + "m";

		out << content.str;
		// Reset color settings after message ends.
		if (foreground_color_id > 0 || background_color_id > 0) out << "\033[0m";
	}

	void StreamPrinter::newline(usize times, std::ostream& out) {
		print(std::string(times, '\n'), out);
	}
}
