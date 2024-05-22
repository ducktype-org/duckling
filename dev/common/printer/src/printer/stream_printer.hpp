/**
 * @file stream_printer.hpp
 * @brief Module for outputting text, especially coloured, to a stream, like the console or a file.
 *
 * Basic usage: playground/printer_test.hpp
 */

#pragma once

#include "printer_content.hpp"

#include <iostream>

namespace printer {
	class StreamPrinter {
	public:
		static void print(const PrinterContent& content, std::ostream& out = std::cerr);
		static void print(const PrinterContentsSeq& contents, std::ostream& out = std::cerr);
		static void newline(int times = 1, std::ostream& out = std::cerr);

		static void printNL(const PrinterContent& content, std::ostream& out = std::cerr) {
			print(content, out);
			newline(1, out);
		}

		static void printNL(const PrinterContentsSeq& contents, std::ostream& out = std::cerr) {
			print(contents, out);
			newline(1, out);
		}
	};
}
