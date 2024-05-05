#include <printer/stream_printer.hpp>
#include <iostream>

void testPrinter() {
	auto console = printer::StreamPrinter();
	console.add(
		{ { { { "Hello, World! This is the first message, " },
	          { "So it will be the only one you see before error, when GeneralMax is set to 1." } },
	        printer::MessageType::HINT,
	        0,
	        printer::Color::RESET,
	        printer::Color::RESET },
	      { { { "R", printer::Color::BRIGHT_RED },
	          { "A", printer::Color::YELLOW },
	          { "I", printer::Color::BRIGHT_YELLOW },
	          { "N", printer::Color::BRIGHT_GREEN },
	          { "B", printer::Color::BRIGHT_BLUE },
	          { "O", printer::Color::BRIGHT_CYAN },
	          { "W", printer::Color::MAGENTA } },
	        printer::MessageType::NOTE,
	        1 },
	      { { { "This message uses a default color. " },
	          { "I can still manually change it. ", printer::Color::GREEN },
	          { "But all messages that do not specify color display it. This is the second hint "
	            "message, it won't be displayed when hints are limited to 1." } },
	        printer::MessageType::HINT,
	        2,
	        printer::Color::BLUE },
	      { { { "It's also possible now to change the " },
	          { "BACKGROUND", printer::Color::DEFAULT, printer::Color::YELLOW },
	          { ". " },
	          { "How cool is that ?", printer::Color::RED, printer::Color::CYAN } },
	        printer::MessageType::ERROR,
	        3 },
	      { { { "How about default message backgrounds? Also this is the only message of level 4 "
	            "or above" },
	          { ", so it's the only one that appears when printing with MinLevel 4 on ALL." } },
	        printer::MessageType::DEBUG,
	        4,
	        printer::Color::RESET,
	        printer::Color::GRAY } }
	);
	std::cout << "Full pass:\n";
	console.print(std::cerr);

	console.setMinLevel(printer::MessageType::ALL, 4);
	std::cout << "\nMinLevel 4:\n";
	console.print(std::cerr);

	console.setMinLevel(printer::MessageType::ALL, 0);
	console.setMaxAmounts(printer::MessageType::HINT, 1);
	std::cout << "\nOnly 1 HINT:\n";
	console.print(std::cerr);

	std::cout << "\nGeneralMax 1:\n";
	console.setGeneralMax(1);
	console.print(std::cerr);

	console.clear();

	console.add(
		{ { { "You can add single messages to console." } }, printer::MessageType::DEBUG, 1 }
	);
	console.add({ { { "You can clear all console messages using clear. Be careful though - it "
	                  "doesn't reset settings." } },
	              printer::MessageType::NOTE,
	              2 });
	console.add({ { { "You need to be careful with curly brackets, because Message constructor "
	                  "takes an initializer list of MessageContents." } },
	              printer::MessageType::HINT,
	              1 });
	std::cout << "\nClearing and adding single messages:\n";
	console.print(std::cerr);

	console.setGeneralMax(SIZE_MAX);
	std::cout << "\nFixing settings after clear.\n";
	console.print(std::cerr);
}

int main() { testPrinter(); }
