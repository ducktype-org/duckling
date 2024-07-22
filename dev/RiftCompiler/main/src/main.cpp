/**
 * @file main.cpp
 * @brief This file implements logic and main procedure that can be used to
 * convenient run (or add) certain functionalities of the Duckling compiler.
 * It compiles to `duck` binary.
 * @note: the ideas from here might be one day changed to framework
 */

#include <filesystem/file.hpp>
#include <pst_parser/parser.hpp>
#include <pst_parser/pst.hpp>
#include <lexer/lexer.hpp>
#include <base/exceptions.hpp>
#include <iostream>
#include <clap/clap.hpp>
#include <printer/stream_printer.hpp>
#include <config/config.hpp>


/**
 * @brief Runs inits needed by main
 */
void init() {
	lexer::init();
	pst::init();
	// @TODO: more inits?
}


namespace {
	/**
	 * @brief Whether main should (not) catch exceptions.
	 */
	bool throwing_main = true;
}

/**
 * @brief Type of command callback
 */
using CommandRunner = std::function<void()>;

/**
 * @brief Structure representing a single command of "duck main"
 */
struct Command {
	std::string   name;
	std::string   description;
	CommandRunner runner;
};

/**
 * @brief Structure representing all commands of "duck main"
 */
struct CommandList {
	std::vector<Command> commands;

	/**
	 * @brief Adds new command to the list.
	 */
	void add(std::string name, std::string desc, CommandRunner runner) {
		commands.emplace_back(Command{ std::move(name), std::move(desc), std::move(runner) });
	}

	/**
	 * @brief Generate help messages with list of all commands
	 */
	[[nodiscard]]
	std::string generateHelpMessage() const {
		std::string out;
		out.reserve(128);

		out += "Available commands: \n";
		for (auto& cmd: commands) {
			out += "    ";
			out += cmd.name;
			out += std::string(30 - cmd.name.length(), ' ');
			out += cmd.description;
			out += "\n";
		}
		return out;
	}

	/**
	 * @brief Runs command of given name
	 * @param what command name
	 * @return if command of given name was found was run.
	 */
	bool run(std::string_view what) {
		for (auto& cmd: commands) {
			if (cmd.name == what) {
				cmd.runner();
				return true;
			}
		}
		return false;
	}
};

void printHelp(const clap::Clap& clap, const clap::ParsingResult& parsing_result, const CommandList& commands, bool command_mode) {
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
 * @brief Generate Clap instance with all standard "main" parameters.
 * @return clap::Clap
 */
clap::Clap getClap() {
	// standard options:
	auto clap         = config::standardOptions();
	
	// custom options of main:
	clap.add(clap::ParamBuilder::ofFlag()
		.addLongName("let-it-throw")
		.addShortDesc("If set, unhandled exceptions will not be caught by main procedure. "
					"Useful for debugging.")
		.addLongDesc(
			"Note that sometimes exception can happen before logic behind this option "
			"will happen. In that case exception will most likely not be caught."
		)
		.build());

	return clap;
}

/**
 * @brief Parses arguments with @p clap and performs
 * configuration of the program that is independent from any command.
 * @note it assumes that @p clap has parameters
 * added by getClap.
 */
clap::ParsingResult configureDuckMainWith(clap::Clap& clap, clap::CLIArgs args) {
	// standard options:
	auto res = config::configureWith(clap, args);

	// custom options of main:
	if (res.isFlag("let-it-throw"))
		throwing_main = true;
	else
		throwing_main = false;

	return res;
}

CommandList getCommandList(clap::CLIArgs& command_args, clap::Clap& clap) {
	CommandList commands;
	commands.add("lex", "Runs lexer on single file and prints result to cout.", [&]() {
		// modify clap as needed:
		clap.add(clap::ParamBuilder::ofValue(clap::FileParser::make())
		             .addShortName('f')
		             .addLongName("file")
		             .addShortDesc("File to lex")
		             .required()
		             .build());

		auto options = configureDuckMainWith(clap, command_args);

		auto file_to_lex = options.getValue<fs::FilePath>("file").value();

		auto token_file = tokenizer::makeTokenFile(file_to_lex);

		bool tokenize_ok = token_file->tokenize();

		if (not tokenize_ok) {
			std::cout << "Tokenization errors: ";
			token_file->getLogger().dumpLog(true, std::cout);
			std::cout << "\n";
		} else {
			auto& tokens = token_file->getTokenData();
			for (auto& token: tokens.tokens) {
				// @TODO: more detailed printing:
				printer::StreamPrinter::printNL(
					{
						"Token: ",
						std::string(token.getStrValue()),
					},
					std::cout
				);
			}
		}
	});
	commands.add("parse", "Runs parser on single file and prints result in json to cout.", [&]() {
		// modify clap as needed:
		clap.add(clap::ParamBuilder::ofValue(clap::FileParser::make())
		             .addShortName('f')
		             .addLongName("file")
		             .addShortDesc("File to parse")
		             .required()
		             .build());

		auto options = configureDuckMainWith(clap, command_args);

		auto file_to_parse = options.getValue<fs::FilePath>("file").value();

		auto pst = pst::PST(file_to_parse);

		if (pst.getLogger().messageCount() != 0) {
			std::cout << "Errors and messages: \n";
			pst.getLogger().dumpLog(true, std::cout);
			std::cout << "\n\n";
		}

		std::cout << "Parsed tree:\n";
		pst.dprint(std::cout);
		std::cout << "\n";
	});
	commands.add("throw", "Throws exception (testing command).", [&]() {
		configureDuckMainWith(clap, command_args);
		throw base::LogicError("Command `throw` thrown successfully!");
	});
	return commands;
}

/**
 * @brief Wrapper for logic of main function
 */
int mainProcedure(int argc, const char* const* argv) {
	init();

	clap::CLIArgs full_args{ (usize) argc, argv };
	clap::CLIArgs command_args{ (usize) argc - 1, argv + 1 };

	auto clap         = getClap();
	bool command_mode = false;

	auto commands = getCommandList(command_args, clap);

	// our custom commands:
	clap.add(clap::ParamBuilder::ofFlag()
		.addLongName("let-it-throw")
		.addShortDesc("If set, unhandled exceptions will not be caught by main procedure. "
					"Useful for debugging.")
		.addLongDesc(
			"Note that sometimes exception can happen before logic behind this option "
			"will happen. In that case exception will most likely not be caught."
		)
		.build());

	try {
		// @future: improve the way we detect whether there was a command or no and
		// the way we handle command line arguments.
		// It is currently done this way, because clap was not designed for 
		// "interactive" options, and "Conditional parameters" don't serve this role well.

		if (argc >= 2 and argv[1][0] != '-') {
			std::string command = argv[1];

			command_mode         = true;
			auto was_command_run = commands.run(command);

			if (not was_command_run) {
				std::cerr << "Unknown command: " << command << ".\n";
				return 1;
			}
		} else {
			command_mode = false;

			clap.add(clap::ParamBuilder::ofFlag()
			             .addLongName("version")
			             .addShortName('v')
			             .addShortDesc("Print version and don't perform any tasks.")
			             .build());

			auto options = configureDuckMainWith(clap, full_args);

			if (options.isFlag("version")) {
				std::cerr << "Duckling version: 0.0.1 pre-alpha\n";
				return 0;
			}
			
			printHelp(clap, options, commands, command_mode);
			return 0;
		}
	} catch (const clap::exceptions::HelpException& e) {
		printHelp(clap, e.parsing_result, commands, command_mode);
	} catch (const clap::exceptions::ClapException& e) {
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
		if (throwing_main) throw;
		std::cerr << "[ERROR] Compiler Exception was caught with message:\n";
		std::cerr << e.what();
		std::cerr << "\nAborting\n";
		return 1;
	} catch (const std::exception& e) {
		if (throwing_main) throw;
		std::cerr << "[ERROR] Unexpected Exception was caught with message:\n";
		std::cerr << e.what();
		std::cerr << "\nAborting\n";
		return 1;
	}
}
