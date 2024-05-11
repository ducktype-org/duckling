#include <printer/stream_printer.hpp>

void testPrinter() {
	printer::StreamPrinter::print({
		{ "Hello, World! This is the first message.\n" },
		{ "R", printer::Color::BRIGHT_RED },
		{ "A", printer::Color::YELLOW },
		{ "I", printer::Color::BRIGHT_YELLOW },
		{ "N", printer::Color::BRIGHT_GREEN },
		{ "B", printer::Color::BRIGHT_BLUE },
		{ "O", printer::Color::BRIGHT_CYAN },
		{ "W", printer::Color::MAGENTA },
		{ "\n" },
		{ "This message uses a default color. " },
		{ "I can still manually change it. ", printer::Color::GREEN },
		{ "But all messages that do not specify color display it.\n" },
		{ "It's also possible now to change the " },
		{ "BACKGROUND", printer::Color::DEFAULT, printer::Color::YELLOW },
		{ ". " },
		{ "How cool is that ?", printer::Color::RED, printer::Color::CYAN },
		{ "\n" },
	});
}

int main() { testPrinter(); }
