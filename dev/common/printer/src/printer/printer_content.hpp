#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace printer {
	using PrinterContentText = std::string;

	// @TODO: Determine the correct type for enum class Color.
	using ColorID = int8_t;
	// @IDEA: Add possibility and functionality for custom colors. Reference ANSI escape code 38.
	/**
	 * Important to note that these colors are inconsistent across terminals and can have
	 * deceiving names. For example WHITE is grayish (BRIGHT_WHITE is closer to real white)
	 * and YELLOW is white in Windows PowerShell. Names were taken from Wikipedia. I recommend
	 * taking a look to consult which colors you should use and how they will be displayed.
	 * https://en.wikipedia.org/wiki/ANSI_escape_code#Colors
	 *
	 * Positive integers indicate foreground ANSI escape code ids. Background ids are achieved
	 * by adding 10 to foreground ids.
	 *
	 * DEFAULT is for PrinterContent, it uses the message's default color. If message color is not
	 * set, it's the defaultMessageColor from printer.cpp. Do not set message's color to DEFAULT.
	 * DEFAULT resets color settings to terminal's default.
	 */
	enum class Color : ColorID {
		DEFAULT = 0,

		BLACK          = 30,
		RED            = 31,
		GREEN          = 32,
		YELLOW         = 33,
		BLUE           = 34,
		MAGENTA        = 35,
		CYAN           = 36,
		WHITE          = 37,
		GRAY           = 90,
		BRIGHT_RED     = 91,
		BRIGHT_GREEN   = 92,
		BRIGHT_YELLOW  = 93,
		BRIGHT_BLUE    = 94,
		BRIGHT_MAGENTA = 95,
		BRIGHT_CYAN    = 96,
		BRIGHT_WHITE   = 97,
	};

	class StreamPrinter;

	class PrinterContent {
		PrinterContentText str;
		Color              foreground_color;
		Color              background_color;
		friend StreamPrinter;

	public:
		PrinterContent()                                 = delete;
		PrinterContent(const PrinterContent&)            = default;
		PrinterContent(PrinterContent&&)                 = default;
		PrinterContent& operator=(const PrinterContent&) = default;
		PrinterContent& operator=(PrinterContent&&)      = default;

		PrinterContent(
			const char* str,
			const Color foreground_color = Color::DEFAULT,
			const Color background_color = Color::DEFAULT
		):
			  str(str),
			  foreground_color(foreground_color),
			  background_color(background_color) {}

		PrinterContent(
			PrinterContentText str,
			const Color        foreground_color = Color::DEFAULT,
			const Color        background_color = Color::DEFAULT
		):
			  str(std::move(str)),
			  foreground_color(foreground_color),
			  background_color(background_color) {}
	};

	using PrinterContentsSeq = std::vector<PrinterContent>;
}
