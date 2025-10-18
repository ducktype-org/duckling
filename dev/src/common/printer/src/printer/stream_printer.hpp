/**
 * @file stream_printer.hpp
 * @brief Module for outputting text, especially coloured, to a stream, like the console or a file.

 Basic usage:
 @include printer_example.cpp

 First message is standard, second one is "RAINBOW" letters colored in a rainbow pattern, third
 showcases default letter colors, fourth showcases background colors and fifth showcases default
 background colors.

 - First, all messages are printed normally to showcase the output.
 - Then, minimum importance of a message level is set to 4, so that only the fifth message shows up
 when printing console output.
 - Then, amount of hint type messages is limited to 1 so the third
 message which is the second hint is not printed.
 - Then, maximum amount of all messages is set to
 one so only the first message is printed.
 - And lastly, the console is cleared of all messages and
 adding single messages to consoles through initializer lists is showcased.

 Also showcasing that
 console settings are not reset upon clearing messages from it.

 Here is a screenshot of the console output:
 @image html common/printer/examples/exampleoutput.png

 @example printer_example.cpp
 Example creates a `Console` and adds messages of various types and importance levels to it.
 */

#pragma once

#include "printer_content.hpp"

#include <base/types/ints.hpp>

#include <iostream>

namespace printer {
	class StreamPrinter final {
	public:
		static void print(const PrinterContent& content, std::ostream& out = std::cerr);
		static void print(const PrinterContentsSeq& contents, std::ostream& out = std::cerr);
		static void newline(usize times = 1, std::ostream& out = std::cerr);

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
