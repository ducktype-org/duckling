/**
 * @file main.cpp
 * @note This code is a legacy code, but is left for adaptation
 * to "global-compiler options" and future compiler handler
 */

#include <filesystem/file.hpp>
#include <pst_parser/parser.hpp>
#include <lexer/lexer.hpp>
#include <lexer/lexer_class.hpp>
#include <compiler/compiler_config.hpp>
#include <base/exceptions.hpp>
#include <iostream>
#include <clap/clap.hpp>
#include <printer/stream_printer.hpp>
#include <diagnostic/logger.hpp>

void init() {
	lexer::init();
	pst::init();
	// @TODO: more inits?
}

clap::Clap baseCompilerOptions() {
	return clap::Clap()
		.addHelpFlag()
		.add(clap::ParamBuilder::ofFlag()
			.addLongName("logger-cerr")
			.addShortDesc("todo")
			.addLongDesc("If set, Logger class will immediately print its messages to cerr. Useful for debugging.")
			.build())
		.add(clap::ParamBuilder::ofFlag()
			.addLongName("lexer-cerr")
			.addShortDesc("todo")
			.addLongDesc("If set, Lexer class will immediately print parsed tokens to cerr. Useful for debugging.")
			.build())
		.add(clap::ParamBuilder::ofFlag()
			.addLongName("let-it-throw")
			.addShortDesc("todo")
			.addLongDesc("If set, program will let unhandled exceptions to be thrown . Useful for debugging.")
			.build());
}

namespace {
	bool throwing_main = false;
}

/**
 * @note it assumes that @p clap has parameters
 * added by baseCompilerOptions.
 */
clap::ParsingResult configureWith(clap::Clap& clap, clap::CLIArgs args) {

	auto res = clap.parse(args);

	if (res.isFlag("logger-cerr")) {
		dia::Logger::setImmediatelyDump(true);
	}
	if (res.isFlag("lexer-cerr")) {
		lexer::Lexer::setTokenMessages(true);
	}
	if (res.isFlag("let-it-throw")) {
		throwing_main = true;
	}

	return res;
}

int mainProcedure(int argc, const char* argv[]) {
	init();

	auto clap = baseCompilerOptions();

	// Note: the ideas from here might be one day changed to framework

	try {

		if (argc >= 2 and argv[1][0] != '-') {
			std::string command = argv[1];

			clap::CLIArgs mock_args{ (usize) argc - 1, argv + 1};

			if (command == "lex") {
				// modify clap as needed
				auto options = configureWith(clap, mock_args);

			}
			else if (command == "parse") {
				// modify clap as needed
			}
			else {
				std::cerr << "Unknown command: " << command << ".\n";
				return 1;
			}

		}
		else {
			clap.add(clap::ParamBuilder::ofFlag()
				.addLongName("version")
				.addShortDesc("todo")
				.addLongDesc("Ignore everything and print version")
				.build());

			auto options = configureWith(clap, clap::CLIArgs{usize(argc), argv});

			if (options.isFlag("version")) {
				std::cerr << "Duckling version: 0.0.1 pre-alpha\n";
			}
		}
	}
	catch (const clap::exceptions::HelpException& e) {
		std::cerr << "tralalala\n";
		std::cerr << clap::HelpMessageGenerator::generate(clap, e.parsing_result);
	}
	catch (const clap::exceptions::ClapException& e) {
		std::cerr << "Incorrect option: " << e.what() << '\n';
		std::cerr << "Use --help for available options.\n";
		return 1;
	}
	return 0;
}


int main(int argc, const char* argv[]) {
	try {
		return mainProcedure(argc, argv);
	} catch (const base::Exception& e) {
		std::cerr << "Compiler Exception was caught with message:\n";
		std::cerr << e.what();
		std::cerr << "\nAborting\n";
		return 1;
	} catch (const std::exception& e) {
		std::cerr << "Unexpected Exception was caught with message:\n";
		std::cerr << e.what();
		std::cerr << "\nAborting\n";
		return 1;
	}
}
