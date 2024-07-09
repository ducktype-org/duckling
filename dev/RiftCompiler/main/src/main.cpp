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
	// @TODO: more inits?
}

clap::Clap baseCompilerOptions() {
	return clap::Clap()
		.addHelpFlag();

		// .setDefaultParser(clap::FileParser::make())
		// .add(clap::ParamBuilder::ofValue(clap::StringParser::make())
		//          .addShortName('o')
		//          .addLongName("output")
		//          .addShortDesc("Set output file")
		//          .build());
}

int main(int argc, const char* argv[]) {

	// Note: the idea from here might be one day changed to framework

	// bool put_help = false;
	if (argc >= 2 and argv[1][0] != '-') {
		std::string command = argv[1];

		// @TODO: set args with mock command:
		clap::CLIArgs args{ (usize) argc - 2, argv };

		if (command == "lex") {
			// ...
			// @TODO
		}
		else if (command == "parse") {
			// ...
			// @TODO
		}
		else {
			// Unknown command error
		}

	}
	else {
		// no command:
		// @TODO: parse no command flags:
		//  --version
		//  --help
		//  --some generic info
		//  etc
		// return
	} 
	
	try {
		init();

		// auto config = compiler::fromArgs(args);

		auto options = baseCompilerOptions().parse(args);

		std::cerr << options.getFilePath() << "\n";
		std::cerr << options.getArgs() << "\n";
		std::cerr << options.getExtraParameterCount() << "\n";

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

	// if (put_help) {
	// 	// ...
	// 	// clap::HelpMessageGenerator
	// }
}
