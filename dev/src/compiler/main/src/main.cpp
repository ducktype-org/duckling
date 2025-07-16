/**
 * @file main.cpp
 * @brief This file implements logic and main procedure that can be used to
 * conveniently run (or add) certain functionalities of the Duckling compiler.
 * It compiles to `duck` binary.
 * @note: The ideas from here might be one day separated into a framework.
 */

#include <clap/clap.hpp>
#include <config/config.hpp>
#include <driver/hout_to_binary_driver.hpp>
#include <driver/package_compilation_driver.hpp>
#include <filesystem/file.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <init/init.hpp>
#include <lexer/lexer.hpp>
#include <printer/stream_printer.hpp>
#include <pst_parser/pst.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>

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

/**
 * Simple function for showing compilation errors.
 */
void printContextErrors() {
	if (query::Context::logger.messageCount() > 0) {
		std::cerr << "Compilation errors logged in context: \n";
		query::Context::logger.dumpLog(true, std::cerr);
	}
}

/**
 * @brief Type of command callback. The returned int value is the value
 * that will be returned by hole application (i.e. exit status).
 */
using CommandRunner = std::function<int()>;

/**
 * @brief Structure representing a single command of "duck main"
 */
struct Command final {
	std::string   name;
	std::string   description;
	CommandRunner runner;
};

/**
 * @brief Structure representing all commands of "duck main"
 */
struct CommandList final {
	std::vector<Command> commands;

	/**
	 * @brief Adds new command to the list.
	 */
	void add(std::string name, std::string desc, CommandRunner runner) {
		commands.emplace_back(Command{
			.name        = std::move(name),
			.description = std::move(desc),
			.runner      = std::move(runner),
		});
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

	struct CommandStatus final {
		bool was_command_run;
		int  exit_code;
	};

	/**
	 * @brief Runs a command.
	 * @param what Command to run.
	 * @return Whether the command was run.
	 */
	CommandStatus run(std::string_view what) {
		for (auto& cmd: commands) {
			if (cmd.name == what) {
				int status = cmd.runner();
				return { .was_command_run = true, .exit_code = status };
			}
		}
		return { .was_command_run = false, .exit_code = 1 };
	}
};

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
 * @brief Generate Clap instance with all standard "main" parameters.
 * @return clap::Clap
 */
clap::Clap getClapForMain() {
	// standard options:
	auto clap = config::standardOptions();

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
 * @brief Parses arguments with @p clap and performs
 * configuration of the program that is independent from any command.
 * @note it assumes that @p clap has parameters
 * added by getClapForMain.
 */
clap::ParsingResult configureDuckMainWith(clap::Clap& clap, clap::CLIArgs args) {
	// standard options:
	auto res = config::configureWith(clap, args);

	CORE_ASSERT(
		throwing_main == res.isFlag("let-it-throw"),
		"Internal error: let-it-throw flag was not parsed correctly."
	);

	return res;
}

/**
 * @brief Generated command list filled with duck-main commands.
 *
 * @param command_args
 * @param clap
 * @return CommandList
 */
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

		auto file_to_lex = options.getValue<fs::File>("file").value();

		auto token_file = tokenizer::makeTokenSource(file_to_lex);

		bool tokenize_ok = token_file->tokenize();

		if (not tokenize_ok) {
			std::cout << "Tokenization errors: ";
			token_file->getLogger()->dumpLog(true, std::cout);
			std::cout << "\n";
			return 1;
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
			return 0;
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

		auto file_to_parse = options.getValue<fs::File>("file").value();

		auto pst = pst::PST(file_to_parse);

		int exit_code = 0;

		if (pst.getLogger()->messageCount() != 0) {
			std::cout << "Errors and messages: \n";
			pst.getLogger()->dumpLog(true, std::cout);
			std::cout << "\n\n";
			exit_code = 1;
		}

		std::cout << "Parsed tree:\n";
		pst.dprint(std::cout);
		std::cout << "\n";

		return exit_code;
	});
	commands.add("get_hout", "Debug prints hout-unit of a module.", [&]() {
		// modify clap as needed:
		clap.add(clap::ParamBuilder::ofValue(clap::FileParser::make())
		             .addShortName('m')
		             .addLongName("module")
		             .addShortDesc("Path to the module")
		             .required()
		             .build());

		auto options = configureDuckMainWith(clap, command_args);

		auto path_to_compile = options.getValue<fs::File>("module").value();

		int exit_code = 0;

		// @TODO: error handling
		using namespace compiler;
		auto root      = query::entryPoint<frontend::QueryModuleTree>(path_to_compile);
		auto top_level = query::entryPoint<helios::QueryTopLevelEntities>(root);
		std::cout << top_level->debugPrint();

		return exit_code;
	});
	commands.add("compile_module", "compile given module into a binary.", [&]() {
		// modify clap as needed:
		clap.add(clap::ParamBuilder::ofValue(clap::FileParser::make())
		             .addShortName('m')
		             .addLongName("module")
		             .addShortDesc("Path to the module")
		             .required()
		             .build());
		clap.add(clap::ParamBuilder::ofFlag()
		             .addLongName("dump-llvm-ir")
		             .addShortDesc("Also dumps LLVM IR to a file (alongside main compilation).")
		             .build());
		clap.add(clap::ParamBuilder::ofFlag()
		             .addLongName("dvm-backend")
		             .addShortDesc("Compile to DVM bytecode.")
		             .build());
		clap.add(clap::ParamBuilder::ofFlag()
		             .addLongName("compile-to-assembly")
		             .addShortDesc("Also compiles to assembly file (alongside main compilation).")
		             .build());

		clap.add(clap::ParamBuilder::ofFlag()
		             .addLongName("add-builtin-library")
		             .addShortDesc("Links builtin library into the final executable.")
		             .build());
		clap.add(
			clap::ParamBuilder::ofFlag()
				.addLongName("dvm-run")
				.addShortDesc("After compiling to the Duckling bytecode run it on the DVM.")
				.conditional(
					[](const clap::ParsingResult& result) {
						return not(result.isFlag("dvm-run") && not result.isFlag("dvm-backend"));
					},
					"Cannot run the code on the DVM without the --dvm-backend option."
				)
				.build()
		);

		auto options = configureDuckMainWith(clap, command_args);

		auto path_to_compile = options.getValue<fs::File>("module").value();

		// @TODO: error handling
		using namespace compiler;
		auto root = query::entryPoint<frontend::QueryModuleTree>(path_to_compile);

		auto top_level = query::entryPoint<helios::QueryModuleHOUT>(root);

		auto backend_type
			= options.isFlag("dvm-backend") ? driver::BackendType::DVM : driver::BackendType::LLVM;

		driver::HoutToBinaryDriver driver{
			driver::BackendOptions{
				.backend_type         = backend_type,
				.compile_to_assembly  = options.isFlag("compile-to-assembly"),
				.dump_llvm_ir         = options.isFlag("dump-llvm-ir"),
				.dvm_code_only_memory = options.isFlag("dvm-run"),
				.add_builtin_library  = options.isFlag("add-builtin-library"),
			},
		};

		// mock collection for purpose of compilation of single module:
		artifacts::ArtifactCollection base_artifact_collection{
			"./duck_build/",
		};
		auto output_name = backend_type == driver::BackendType::DVM ? "module.dbc" : "module.o";
		auto output_artifact
			= base_artifact_collection.fileArtifactAtOrNew(base::StrID(output_name));

		int exit_code = 0;
		query::utils::withContextDo([&](query::Context& ctx) {
			driver.compileHOUTUnit(ctx, &top_level, base::StrID("main_module"), output_artifact);
			if (options.isFlag("dvm-run")) {
				auto run_result = driver.run();
				if (run_result.has_value()) {
					exit_code = run_result.value().exit_code;
				} else {
					std::cerr << "Error: " << run_result.error() << "\n";
					exit_code = 1;
				}
			}
		});

		return exit_code;
	});
	commands.add("compile_package", "compile given package into a binary.", [&]() {
		// modify clap as needed:
		clap.add(clap::ParamBuilder::ofValue(clap::FileParser::make())
		             .addShortName('m')
		             .addLongName("module")
		             .addShortDesc("Path to the top-level source module of the package")
		             .required()
		             .build());

		clap.add(clap::ParamBuilder::ofValue(clap::FileParser::make())
		             .addShortName('a')
		             .addLongName("artifact-location")
		             .addShortDesc("Path to the top-level folder with build artifacts")
		             .required()
		             .build());

		clap.add(clap::ParamBuilder::ofFlag()
		             .addLongName("dvm-backend")
		             .addShortDesc("Compile to DVM bytecode instead of exe.")
		             .build());

		auto options = configureDuckMainWith(clap, command_args);

		auto path_to_compile   = options.getValue<fs::File>("module").value();
		auto backend_type      = options.isFlag("dvm-backend") ? compiler::driver::BackendType::DVM
		                                                       : compiler::driver::BackendType::LLVM;
		auto artifact_location = options.getValue<fs::File>("artifact-location").value();

		defer(printContextErrors());

		compiler::driver::PackageCompilationDriver driver{
			backend_type,
			path_to_compile,
			artifact_location.nativePath(),
		};
		driver.compilerEntirePackageIntoBinary();

		return 0;
	});
	commands.add("throw", "Throws exception (testing command).", [&]() -> int {
		configureDuckMainWith(clap, command_args);
		throw base::LogicError("Command `throw` thrown successfully!");
	});
	return commands;
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
