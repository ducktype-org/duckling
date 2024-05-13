#include "stream_printer.hpp"

#include <ostream>

namespace printer {
	// All background color escape codes are 10 above foregrounds colors.
	ColorId calculateBackgroundColorId(const ColorId color_id) {
		constexpr int8_t background_font_color_offset = 10;
		return static_cast<ColorId>(
			color_id > 0 ? color_id + background_font_color_offset : color_id
		);
	}

	void StreamPrinter::print(const std::vector<PrinterContent>& contents, std::ostream& out) {
		for (auto& c: contents) print(c, out);
	}

	void StreamPrinter::print(const PrinterContent& content, std::ostream& out) {
		ColorId foreground_color_id{ static_cast<ColorId>(content.foreground_color) };
		ColorId background_color_id{ static_cast<ColorId>(content.background_color) };
		// Background colors have different ids than foreground colors.
		background_color_id = calculateBackgroundColorId(background_color_id);

		if (foreground_color_id > 0) out << "\033[" + std::to_string(foreground_color_id) + "m";
		if (background_color_id > 0) out << "\033[" + std::to_string(background_color_id) + "m";

		out << content.str;
		// Reset color settings after message ends.
		out << "\033[0m";
	}

	void StreamPrinter::newline(int times, std::ostream& out) {
		print(std::string(times, '\n'), out);
	}
}
