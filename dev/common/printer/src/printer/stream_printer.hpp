/**
 * @file stream_printer.hpp
 * @brief Module for outputting text, especially coloured, to a stream, like the console or a file.
 *
 * Basic usage: playground/printer_test.hpp
 */

#pragma once

#include "printer_content.hpp"

#include <vector>
#include <iostream>

namespace printer {
	class StreamPrinter {
	public:
		static void print(const PrinterContent& content, std::ostream& out = std::cerr);
		static void
			print(const printer::PrinterContentsSeq& contents, std::ostream& out = std::cerr);
		static void newline(std::ostream& out = std::cerr);
	};
}
