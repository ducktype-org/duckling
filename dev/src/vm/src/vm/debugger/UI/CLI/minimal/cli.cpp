#include "cli.hpp"

#include <diagnostic/highlight_positions.hpp>


namespace vm::debugger::cli {
	void CLIDebugger::implInit() {}
	void CLIDebugger::implExit() {}

	bool CLIDebugger::getline(std::string& line) {
		return !!std::getline(std::cin, line);
	}

	void CLIDebugger::print(const printer::PrinterContentsSeq& content) {
		std::lock_guard lk(output_mutex);
		printer::StreamPrinter::print(content, std::cout);
	}

	void CLIDebugger::printNL(const printer::PrinterContentsSeq& content) {
		std::lock_guard lk(output_mutex);
		printer::StreamPrinter::print(content, std::cout);
		printer::StreamPrinter::newline(1, std::cout);
	}

	void CLIDebugger::printError(const printer::PrinterContentsSeq& content) {
		std::lock_guard lk(output_mutex);
		printer::StreamPrinter::print(
			{ { "[Debug error]: ", printer::Color::BrightRed } }, std::cout
		);
		printer::StreamPrinter::print(content, std::cout);
		printer::StreamPrinter::newline(1, std::cout);
	}
}
