/** @brief This file implements main logic of this module and the commands system
 * to conveniently run (or add) certain functionalities of the Duckling compiler.
 */

#include "commands.hpp"

#include <driver/initialize.hpp>
#include <driver/operations/generic_operations.hpp>
#include <driver/operations/dvm_operations.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <lexer/lexer.hpp>
#include <pst_parser/pst.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>

void CommandList::add(std::string name, std::string desc, CommandRunner runner) {
	commands.emplace_back(Command{
		.name        = std::move(name),
		.description = std::move(desc),
		.runner      = std::move(runner),
	});
}

std::string CommandList::generateHelpMessage() const {
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

CommandList::CommandStatus CommandList::run(std::string_view what) {
	for (auto& cmd: commands) {
		if (cmd.name == what) {
			int status = cmd.runner();
			return { .was_command_run = true, .exit_code = status };
		}
	}
	return { .was_command_run = false, .exit_code = 1 };
}

compiler::driver::options_types::DebugOptions getDebugOptionsFromClap(
	const clap::ParsingResult& parsing_result
) {
	return compiler::driver::options_types::DebugOptions{
		.lexer_cerr  = parsing_result.isFlag("lexer-cerr"),
		.logger_cerr = parsing_result.isFlag("logger-cerr"),
		.dump_llvm_ir = parsing_result.isFlag("dump-llvm-ir"),
		.dump_llvm_asm = parsing_result.isFlag("dump-llvm-asm"),
	};
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

		auto options = clap.parse(command_args);

		compiler::driver::initializeTheCompiler(
			compiler::driver::CompilerModeOfOperationAndOptions::BareMode{
				.debug_options = getDebugOptionsFromClap(options),
			}
		);

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

		auto options = clap.parse(command_args);

		compiler::driver::initializeTheCompiler(
			compiler::driver::CompilerModeOfOperationAndOptions::BareMode{
				.debug_options = getDebugOptionsFromClap(options),
			}
		);

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

		auto options = clap.parse(command_args);

		compiler::driver::initializeTheCompiler(
			compiler::driver::CompilerModeOfOperationAndOptions::BareMode{
				.debug_options = getDebugOptionsFromClap(options),
			}
		);

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
		             .addLongName("dump-llvm-asm")
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

		auto options = clap.parse(command_args);

		auto path_to_compile = options.getValue<fs::File>("module").value();
		using namespace compiler;

		auto backend_type
			= options.isFlag("dvm-backend") ? driver::BackendType::DVM : driver::BackendType::LLVM;

		compiler::driver::initializeTheCompiler(
			compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.compilation_artifacts = {
					.artifacts_path = fs::File("./duck_build/"),
				},
				.debug_options = getDebugOptionsFromClap(options),
			}
		);

		auto root = query::entryPoint<frontend::QueryModuleTree>(path_to_compile);

		auto output_artifact = query::entryPoint<driver::CompileModule>({ root, backend_type });

		// TODO PR:
		// .compile_to_assembly  = options.isFlag("dump-llvm-asm"),
		// 		.dump_llvm_ir         = options.isFlag("dump-llvm-ir"),
		// 		.dvm_code_only_memory = options.isFlag("dvm-run"),
		// 		.add_builtin_library  = options.isFlag("add-builtin-library"),


		// int exit_code = 0;
		// query::utils::withContextDo([&](query::Context& ctx) {
		// 	driver.compileHOUTUnit(ctx, &top_level, base::StrID("main_module"), output_artifact);
		// 	if (options.isFlag("dvm-run")) {
		// 		auto run_result = driver.run();
		// 		if (run_result.has_value()) {
		// 			exit_code = run_result.value().exit_code;
		// 		} else {
		// 			std::cerr << "Error: " << run_result.error() << "\n";
		// 			exit_code = 1;
		// 		}
		// 	}
		// });

		return 0;
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

		auto options = clap.parse(command_args);

		compiler::driver::initializeTheCompiler(
			compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.compilation_artifacts = {
					.artifacts_path = options.getValue<fs::File>("artifact-location").value(),
				},
				.debug_options = getDebugOptionsFromClap(options),
			}
		);

		auto path_to_compile = options.getValue<fs::File>("module").value();
		auto backend_type    = options.isFlag("dvm-backend") ? compiler::driver::BackendType::DVM
		                                                     : compiler::driver::BackendType::LLVM;

		defer(printContextErrors());

		compiler::driver::compilerEntirePackageIntoBinary(path_to_compile, backend_type);

		return 0;
	});
	commands.add("throw", "Throws exception (testing command).", [&]() -> int {
		clap.parse(command_args);
		throw base::LogicError("Command `throw` thrown successfully!");
	});
	return commands;
}
