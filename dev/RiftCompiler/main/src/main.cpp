/**
 * @file main.cpp
 * @note This code is a legacy code, but is left for adaptation
 * to "global-compiler options" and future compiler handler
 */

#include <filesystem/file.hpp>
#include <pst_parser/parser.hpp>
#include <lexer/lexer.hpp>
#include <compiler/compiler_config.hpp>
#include <base/exceptions.hpp>
#include <iostream>
#include <clap/clap.hpp>
#include <printer/stream_printer.hpp>

void init() {
	lexer::init();
	pst::init();
}

int main(int argc, const char* argv[]) {
	clap::CLIArgs args{ (usize) argc, argv };

	try {
		init();

		auto config = compiler::fromArgs(args);

		// @TODO: update CompilerConfig and write proper compiler luncher

	} catch (const clap::exceptions::HelpException& e) {
		printer::StreamPrinter::print(compiler::generateHelpMessage(e));
	} catch (const base::Exception& e) {
		std::cerr << "Compiler Exception was caught with message:\n";
		std::cerr << e.what();
		std::cerr << "\nAborting\n";
	} catch (const std::exception& e) {
		std::cerr << "Unexpected Exception was caught with message:\n";
		std::cerr << e.what();
		std::cerr << "\nAborting\n";
	}
}
