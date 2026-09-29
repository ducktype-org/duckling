#include "cli.hpp"


namespace vm::debugger::cli {
	void CLIDebugger::implInit() {}
	void CLIDebugger::implExit() {}

	bool CLIDebugger::getline(std::string& line) {
		return !!std::getline(std::cin, line);
	}

	void CLIDebugger::print(const printer::PrinterContentsSeq& content) {
		std::lock_guard lk(spec.output_mutex);
		printer::StreamPrinter::print(content, std::cout);
	}
}
