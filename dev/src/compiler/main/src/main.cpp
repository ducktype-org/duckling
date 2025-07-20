/**
 * @file main.cpp
 * @brief This file implements logic and main procedure that can be used to
 * conveniently run (or add) certain functionalities of the Duckling compiler.
 * It compiles to `duck` binary.
 * @note: The ideas from here might be one day separated into a framework.
 */

#include <driver/hout_to_binary_driver.hpp>
#include <driver/package_compilation_driver.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <lexer/lexer.hpp>
#include <lexer/lexer_class.hpp>
#include <pst_parser/pst.hpp>

#include <base/exceptions.hpp>
#include <base/int_conv.hpp>

#include <clap/clap.hpp>
#include <diagnostic/logger.hpp>
#include <filesystem/file.hpp>
#include <init/init.hpp>
#include <printer/stream_printer.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>

#include <iostream>

/**
 * Simple function for showing compilation errors.
 */
void printContextErrors() {
	if (query::Context::logger.messageCount() > 0) {
		std::cerr << "Compilation errors logged in context: \n";
		query::Context::logger.dumpLog(true, std::cerr);
	}
}

clap::Clap getStandardDucklingOptions() {
	return clap::Clap("duck", "The Duckling compiler")
	    .add(clap::ParamBuilder::ofFlag()
	             .addLongName("logger-cerr")
	             .addShortDesc("If set, Logger class will immediately print its messages to cerr.")
	             .build())
	    .add(clap::ParamBuilder::ofFlag()
	             .addLongName("lexer-cerr")
	             .addShortDesc("If set, Lexer class will immediately print parsed tokens to cerr.")
	             .build())
	    .add(clap::ParamBuilder::ofFlag()
	             .addLongName("let-it-throw")
	             .addShortDesc("Disables exception handling in main (debug option)")
	             .addLongDesc("If set, unhandled exceptions will not be caught by main procedure. "
	                          "It should be used for debugging only in order to preserve "
	                          "stack-trace. It can prevent stack-unwinding from happening.")
	             .build())
	    .add(clap::ParamBuilder::ofFlag()
	             .addShortName('v')
	             .addLongName("version")
	             .addShortDesc("Print version and exit")
	             .build())
	    .setPreHandler([](const clap::ParsingResult& options) {
			dia::Logger::setImmediatelyDump(options.isFlag("logger-cerr"));
			lexer::Lexer::setTokenMessages(options.isFlag("lexer-cerr"));
			if (options.isFlag("version")) {
				std::cout << "Duckling version: 0.0.1 pre-alpha\n";
				throw clap::exceptions::SuccessExitException(options);
			}
		});
}

/**
 * @brief Generate Clap instance with all standard "main" parameters.
 * @return clap::Clap
 */
clap::Clap getClapForMain() {
	return getStandardDucklingOptions()
	    .addSubcommand(
			clap::Clap("lex", "Runs lexer on a single file and prints the result to cout.")
				.addPositional(clap::FileParser::make())
				.setHandler([](const clap::ParsingResult& options) -> int {
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
							// @TODO: more detailed printing. This should change in #1111.
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
				})
		)
	    .addSubcommand(
			clap::Clap("parse", "Runs parser on a single file and prints result in json to cout.")
				.addPositional(clap::FileParser::make())
				.setHandler([](const clap::ParsingResult& options) -> int {
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
				})
		)
	    .addSubcommand(clap::Clap("get_hout", "Debug prints hout-unit of a module.")
	                       .addPositional(clap::FileParser::make())
	                       .setHandler([](const clap::ParsingResult& options) -> int {
							   auto path_to_compile = options.getValue<fs::File>("module").value();

							   int exit_code = 0;

							   // @TODO: error handling
							   using namespace compiler;
							   auto root
								   = query::entryPoint<frontend::QueryModuleTree>(path_to_compile);
							   auto top_level
								   = query::entryPoint<helios::QueryTopLevelEntities>(root);
							   std::cout << top_level->debugPrint();

							   return exit_code;
						   }))
	    .addSubcommand(
			clap::Clap("compile_module", "Compile given module into a binary.")
				.addPositional(clap::FileParser::make())
				.add(clap::ParamBuilder::ofFlag()
	                     .addLongName("dump-llvm-ir")
	                     .addShortDesc("Also dumps LLVM IR to a file (alongside main compilation).")
	                     .build())
				.add(clap::ParamBuilder::ofFlag()
	                     .addLongName("dvm-backend")
	                     .addShortDesc("Compile to DVM bytecode.")
	                     .build())
				.add(clap::ParamBuilder::ofFlag()
	                     .addLongName("compile-to-assembly")
	                     .addShortDesc(
							 "Also compiles to assembly file (alongside main compilation)."
						 )
	                     .build())
				.add(clap::ParamBuilder::ofFlag()
	                     .addLongName("add-builtin-library")
	                     .addShortDesc("Links builtin library into the final executable.")
	                     .build())
				.add(clap::ParamBuilder::ofFlag()
	                     .addLongName("dvm-run")
	                     .addShortDesc("After compiling to the Duckling bytecode run it on the DVM.")
	                     .conditional(
							 [](const clap::ParsingResult& result) {
								 return not(
									 result.isFlag("dvm-run") && not result.isFlag("dvm-backend")
								 );
							 },
							 "Cannot run the code on the DVM without the --dvm-backend "
							 "option."
						 )
	                     .build())
				.setHandler([](const clap::ParsingResult& options) -> int {
					auto path_to_compile = options.getValue<fs::File>("module").value();

					// @TODO: error handling. This should change in #1112.
					using namespace compiler;
					auto root = query::entryPoint<frontend::QueryModuleTree>(path_to_compile);

					auto top_level = query::entryPoint<helios::QueryModuleHOUT>(root);

					auto backend_type = options.isFlag("dvm-backend") ? driver::BackendType::DVM
		                                                              : driver::BackendType::LLVM;

					driver::HoutToBinaryDriver driver{
						driver::BackendOptions{
							.backend_type         = backend_type,
							.compile_to_assembly  = options.isFlag("compile-to-assembly"),
							.dump_llvm_ir         = options.isFlag("dump-llvm-ir"),
							.dvm_code_only_memory = options.isFlag("dvm-run"),
							.add_builtin_library  = options.isFlag("add-builtin-library"),
						},
					};

					// @TODO: Mock collection for purpose of compilation of single module. This
		            // should change in #1113.
					artifacts::ArtifactCollection base_artifact_collection{
						"./duck_build/",
					};
					auto output_name
						= backend_type == driver::BackendType::DVM ? "module.qbc" : "module.o";
					auto output_artifact
						= base_artifact_collection.fileArtifactAtOrNew(base::StrID(output_name));

					int exit_code = 0;
					query::utils::withContextDo([&](query::Context& ctx) {
						driver.compileHOUTUnit(
							ctx, &top_level, base::StrID("main_module"), output_artifact
						);
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
				})
		)
	    .addSubcommand(
			clap::Clap("compile_package", "Compile given package into a binary.")
				.add(clap::ParamBuilder::ofValue(clap::FileParser::make())
	                     .addShortName('m')
	                     .addLongName("module")
	                     .addShortDesc("Path to the top-level source module of the package")
	                     .required()
	                     .build())
				.add(clap::ParamBuilder::ofValue(clap::FileParser::make())
	                     .addShortName('a')
	                     .addLongName("artifact-location")
	                     .addShortDesc("Path to the top-level folder with build artifacts")
	                     .required()
	                     .build())
				.add(clap::ParamBuilder::ofFlag()
	                     .addLongName("dvm-backend")
	                     .addShortDesc("Compile to DVM bytecode instead of exe.")
	                     .build())
				.setHandler([](const clap::ParsingResult& options) -> int {
					auto path_to_compile = options.getValue<fs::File>("module").value();
					auto backend_type    = options.isFlag("dvm-backend")
		                                     ? compiler::driver::BackendType::DVM
		                                     : compiler::driver::BackendType::LLVM;
					auto artifact_location
						= options.getValue<fs::File>("artifact-location").value();

					defer(printContextErrors());

					compiler::driver::PackageCompilationDriver driver{
						backend_type,
						path_to_compile,
						artifact_location.getFilePath().native(),
					};
					driver.compilerEntirePackageIntoBinary();

					return 0;
				})
		)
	    .addSubcommand(clap::Clap("throw", "Throws exception (testing command).")
	                       .setHandler([](const clap::ParsingResult&) -> int {
							   throw base::LogicError("Command `throw` thrown successfully!");
						   }));
}

int main(int argc, const char* argv[]) {
	init::InitObject _;
	auto             clap = getClapForMain();

	try {
		return clap.execute(base::safeIntConv<usize>(argc), argv);
	} catch (const base::Exception& e) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::RED },
			{ "Compiler Exception was caught with message:\n", printer::Color::DEFAULT },
			{ e.what(), printer::Color::DEFAULT },
			{ "\nAborting\n", printer::Color::DEFAULT },
		});
		return 1;
	} catch (const std::exception& e) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::RED },
			{ "Unexpected Exception was caught with message:\n", printer::Color::DEFAULT },
			{ e.what(), printer::Color::DEFAULT },
			{ "\nAborting\n", printer::Color::DEFAULT },
		});
		return 1;
	} catch (...) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::RED },
			{ "Unexpected Exception not inheriting from std::exception was caught.\n",
		      printer::Color::DEFAULT },
		});
		return 1;
	}
}
