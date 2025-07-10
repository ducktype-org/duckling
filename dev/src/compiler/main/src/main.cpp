/**
 * @file main.cpp
 */

#include <clap/clap.hpp>
#include "cli_options.hpp"
#include "commands/commands.hpp"
#include <init/init.hpp>

#include <base/exceptions.hpp>
#include <base/int_conv.hpp>

#include <iostream>


constexpr auto LET_IT_THROW_NAME   = "let-it-throw";
constexpr auto LET_IT_THROW_OPTION = "--let-it-throw";

namespace {
	/**
	 * @brief Whether main should throw compiler exceptions.
	 * Used by let-it-throw option.
	 */
	constinit bool throwing_main = false;
}


void printHelp(
	const clap::Clap&          clap,
	const clap::ParsingResult& parsing_result,
	const CommandList&         commands,
	bool                       command_mode
) {
	if (command_mode) {
		std::cerr << clap::HelpMessageGenerator::generate(clap, parsing_result);
		std::cerr << "\nFor list of available commands use: ./duck --help\n";
	} else {
		std::cerr << commands.generateHelpMessage();
		std::cerr << "\nFor help with given command use: ./duck [command] --help\n\n";
		std::cerr << "General options and usage:\n";
		std::cerr << clap::HelpMessageGenerator::generate(clap, parsing_result);
	}
}

/**
 * @brief Generate Clap instance with all standard "main" parameters and the let-it-throw parameter.
 * @return clap::Clap
 */
clap::Clap getClapForMain() {
	// standard options:
	auto clap = standardOptions();

	// custom options of main:
	clap.add(clap::ParamBuilder::ofFlag()
	             .addLongName(LET_IT_THROW_NAME)
	             .addShortDesc("Disables exception handling in main (debug option).")
	             .addLongDesc("If set, unhandled exceptions will not be caught by main procedure. "
	                          "It should be used for debugging only in order to preserve "
	                          "stack-trace. It can prevent stack-unwinding from happening.")
	             .build());

	return clap;
}





/**
 * @brief Wrapper for logic of main function
 */
int mainProcedure(int argc, const char* const* argv) {
	init::InitObject _;

	clap::CLIArgs full_args{
		.argc = base::safeIntConv<usize>(argc),
		.argv = argv,
	};
	clap::CLIArgs command_args{
		.argc = base::safeIntConv<usize>(argc - 1),
		.argv = argv + 1,
	};

	auto clap         = getClapForMain();
	bool command_mode = false;

	auto commands = getCommandList(command_args, clap);

	try {
		// @future: improve the way we detect whether there was a command or no and
		// the way we handle command line arguments.
		// It is currently done this way, because clap was not designed for
		// "interactive" options, and "Conditional parameters" don't serve this role well.

		if (argc >= 2 and argv[1][0] != '-') {
			std::string command = argv[1];

			command_mode        = true;
			auto command_status = commands.run(command);

			if (not command_status.was_command_run)
				std::cerr << "Unknown command: " << command << ".\n";

			return command_status.exit_code;
		} else {
			command_mode = false;

			clap.add(clap::ParamBuilder::ofFlag()
			             .addLongName("version")
			             .addShortName('v')
			             .addShortDesc("Print version and don't perform any tasks.")
			             .build());

			auto options = clap.parse(full_args);

			if (options.isFlag("version")) {
				std::cerr << "Duckling version: 0.0.1 pre-alpha\n";
				return 0;
			}

			printHelp(clap, options, commands, command_mode);
			return 0;
		}
	} catch (const clap::exceptions::HelpException& e) {
		printHelp(clap, e.parsing_result, commands, command_mode);
		return 0;
	} catch (const clap::exceptions::ClapException& e) {
		std::cerr << "Incorrect option: " << e.what() << '\n';
		std::cerr << "Use --help for available options.\n";
		return 1;
	}
}

int main(int argc, const char* argv[]) {
	// We have to see if --let-it-throw was passed
	// before anything else happens.
	// Thats why we do it here, bypassing typical clap usage.
	// --let-it-throw is still included in clap options.
	// for showing help.

	throwing_main = false;
	for (int i = 1; i < argc; i++) {
		if (std::string_view(argv[i]) == LET_IT_THROW_OPTION) {
			throwing_main = true;
			break;
		}
	}

	if (throwing_main) return mainProcedure(argc, argv);

	// else we just catch exceptions and print them:

	try {
		return mainProcedure(argc, argv);
	} catch (const base::Exception& e) {
		std::cerr << "[ERROR] Compiler Exception was caught with message:\n";
		std::cerr << e.what();
		std::cerr << "\nAborting\n";
		return 1;
	} catch (const std::exception& e) {
		std::cerr << "[ERROR] Unexpected Exception was caught with message:\n";
		std::cerr << e.what();
		std::cerr << "\nAborting\n";
		return 1;
	} catch (...) {
		std::cerr
			<< "[ERROR] Unexpected Exception not inheriting from std::exception was caught.\n";
		return 1;
	}
}
