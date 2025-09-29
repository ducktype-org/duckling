/**
 * @file main.cpp
 * @brief This file implements logic and main procedure that can be used to
 * conveniently run (or add) certain functionalities of the Duckling compiler.
 * It compiles to `duck` binary.
 * @note: The ideas from here might be one day separated into a framework.
 */

#include <driver/initialize.hpp>
#include <driver/operations/generic_operations.hpp>
#include <driver/statistics/statistics.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <pst_parser/pst.hpp>
#include <timer/timer.hpp>

#include <base/exceptions.hpp>
#include <base/int_conv.hpp>

#include <clah/clah.hpp>
#include <diagnostic/logger.hpp>
#include <filesystem/file.hpp>
#include <init/init.hpp>
#include <lexer/lexer.hpp>
#include <lexer/lexer_class.hpp>
#include <printer/stream_printer.hpp>
#include <query_framework/q_stats/q_stats.hpp>
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

clah::Clah getStandardDucklingOptions() {
	return clah::Clah("duck", "The Duckling compiler")
	    .add(clah::ParamBuilder::ofFlag()
	             .addLongName("logger-cerr")
	             .addShortDesc("If set, Logger class will immediately print its messages to cerr.")
	             .build())
	    .add(clah::ParamBuilder::ofFlag()
	             .addLongName("lexer-cerr")
	             .addShortDesc("If set, Lexer class will immediately print parsed tokens to cerr.")
	             .build())
	    .add(clah::ParamBuilder::ofFlag()
	             .addLongName("let-it-throw")
	             .addShortDesc("Disables exception handling in main (debug option)")
	             .addLongDesc("If set, unhandled exceptions will not be caught by main procedure. "
	                          "It should be used for debugging only in order to preserve "
	                          "stack-trace. It can prevent stack-unwinding from happening.")
	             .build())
	    .add(clah::ParamBuilder::ofFlag()
	             .addShortName('v')
	             .addLongName("version")
	             .addShortDesc("Print version and exit")
	             .build())
	    .setPreHandler([](const clah::ParsingResult& options) {
			if (options.isFlag("version")) {
				std::cout << "Duckling version: 0.0.1 pre-alpha\n";
				throw clah::exceptions::SuccessExitException(options);
			}
		});
}

compiler::driver::options_types::DebugOptions getDebugOptionsFromClap(
	const clah::ParsingResult& parsing_result
) {
	return compiler::driver::options_types::DebugOptions{
		.lexer_cerr    = parsing_result.isFlag("lexer-cerr"),
		.logger_cerr   = parsing_result.isFlag("logger-cerr"),
		.dump_llvm_ir  = parsing_result.isFlag("dump-llvm-ir"),
		.dump_llvm_asm = parsing_result.isFlag("dump-llvm-asm"),
	};
}

/**
 * @brief Generate Clah instance with all standard "main" parameters.
 * @return clah::Clah
 */
clah::Clah getClahForMain() {
	return getStandardDucklingOptions()
	    .addSubcommand(
			clah::Clah("lex", "Runs lexer on a single file and prints the result to cout.")
				.addPositional(clah::FileParser::make("file"))
				.setHandler([](const clah::ParsingResult& options) -> int {
					compiler::driver::initializeTheCompiler(
						compiler::driver::CompilerModeOfOperationAndOptions::BareMode{
							.debug_options = getDebugOptionsFromClap(options),
						}
					);

					auto file_to_lex = options.getPositional<fs::File>(0);

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
			clah::Clah("parse", "Runs parser on a single file and prints result in json to cout.")
				.addPositional(clah::FileParser::make("file"))
				.setHandler([](const clah::ParsingResult& options) -> int {
					compiler::driver::initializeTheCompiler(
						compiler::driver::CompilerModeOfOperationAndOptions::BareMode{
							.debug_options = getDebugOptionsFromClap(options),
						}
					);

					auto file_to_parse = options.getPositional<fs::File>(0);

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
	    .addSubcommand(clah::Clah("get_hout", "Debug prints hout-unit of a module.")
	                       .addPositional(clah::FileParser::make("module"))
	                       .setHandler([](const clah::ParsingResult& options) -> int {
							   compiler::driver::initializeTheCompiler(
								   compiler::driver::CompilerModeOfOperationAndOptions::BareMode{
									   .debug_options = getDebugOptionsFromClap(options),
								   }
							   );

							   auto path_to_compile = options.getPositional<fs::File>(0);

							   int exit_code = 0;

							   // @TODO: error handling
							   using namespace compiler;
							   auto root = frontend::createModuleTree(path_to_compile);
							   auto hout_units
								   = query::entryPoint<helios::QueryModuleHOUTRecursively>(root);
							   for (const auto& hout_unit: hout_units)
								   std::cout << hout_unit.debugPrint();

							   return exit_code;
						   }))
	    .addSubcommand(
			clah::Clah("compile_module", "Compile given module into a binary.")
				.addPositional(clah::FileParser::make("module"))
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("dump-llvm-ir")
	                     .addShortDesc("Also dumps LLVM IR to a file (alongside main compilation).")
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("dvm-backend")
	                     .addShortDesc("Compile to DVM bytecode.")
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("dump-llvm-asm")
	                     .addShortDesc(
							 "Also compiles to assembly file (alongside main compilation)."
						 )
	                     .build())
				.setHandler([](const clah::ParsingResult& options) -> int {
					compiler::driver::initializeTheCompiler(
						compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
							.compilation_artifacts = {
								.artifacts_path = fs::FilePath("./duck_build/"),
							},
							.debug_options = getDebugOptionsFromClap(options),
						}
					);

					auto path_to_compile = options.getPositional<fs::File>(0);

					// @TODO: error handling. This should change in #1112.
					using namespace compiler;

					auto backend_type = options.isFlag("dvm-backend") ? driver::BackendType::DVM
		                                                              : driver::BackendType::LLVM;

					auto root = frontend::createModuleTree(path_to_compile);

					auto output_artifact
						= query::entryPoint<driver::CompileModule>({ root, backend_type });

					return 0;
				})
		)
	    .addSubcommand(
			clah::Clah("compile_package", "Compile given package into a binary.")
				.addPositional(clah::FileParser::make("module"))
				.add(clah::ParamBuilder::ofValue(clah::FilePathParser::make("filepath"))
	                     .addShortName('a')
	                     .addLongName("artifact-location")
	                     .addShortDesc("Path to the top-level folder with build artifacts")
	                     .optional()
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("dvm-backend")
	                     .addShortDesc("Compile to DVM bytecode instead of exe.")
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("print-statistics")
	                     .addShortDesc("Print execution time statistics.")
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("print-graph")
	                     .addShortDesc("Print the query graph after the compilation.")
	                     .build())
				.setHandler([](const clah::ParsingResult& options) -> int {
					compiler::driver::initializeTheCompiler(
						compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
							.compilation_artifacts = {
								.artifacts_path = 
									options.getValue<fs::FilePath>("artifact-location").copyValueOr("./duck_build/"),
							},
							.debug_options = getDebugOptionsFromClap(options),
						}
					);

					// @TODO #1058: make graph/statistics printing configuration better.

					timer::TimeMeasurement total_compilation_time;
					total_compilation_time.startMeasurement();


					auto path_to_compile = options.getPositional<fs::File>(0);
					auto backend_type    = options.isFlag("dvm-backend")
		                                     ? compiler::driver::BackendType::DVM
		                                     : compiler::driver::BackendType::LLVM;

					defer(printContextErrors());

					compiler::driver::compilerEntirePackage(path_to_compile, backend_type);

					total_compilation_time.endMeasurement();

					if (options.isFlag("print-statistics")) {
						if (not query::USE_STATS) {
							std::cerr << "Warning: Query statistics are disabled at compile time. "
										 "No query statistics will be printed.\n";
						}
						query::printStats();

						std::cerr << "\nTotal compilation time: ";
						timer::printAs(
							std::cerr,
							total_compilation_time.duration(),
							timer::TimeUnit::Milliseconds
						);
						std::cerr << "\n";
						std::cerr << " - Backend compilation time: ";
						timer::printAs(
							std::cerr,
							compiler::driver::getBackendCompilationTime(),
							timer::TimeUnit::Milliseconds
						);
						std::cerr << "\n\n";
					}

					if (options.isFlag("print-graph"))
						query::Context::getState().getGraph().debugPrintForDrawing(std::cerr);

					return 0;
				})
		)
	    .addSubcommand(clah::Clah("dvm_run", "Compile given module to DVM (in-memory) and run it")
	                       .addPositional(clah::FileParser::make("module"))
	                       .add(clah::ParamBuilder::ofFlag()
	                                .addLongName("add-builtin-library")
	                                .addShortDesc("Links builtin library into the final executable.")
	                                .build())
	                       .setHandler([](const clah::ParsingResult& options) -> int {
							   auto path_to_compile = options.getPositional<fs::File>(0);
							   using namespace compiler;

							   compiler::driver::initializeTheCompiler(
			compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.compilation_artifacts = {
					.artifacts_path = fs::FilePath("./duck_build/"),
				},
				.debug_options = getDebugOptionsFromClap(options),
			}
		);

							   auto root = frontend::createModuleTree(path_to_compile);

							   int exit_code = 0;
							   query::utils::withContextDo([&](query::Context& ctx) {
								   auto run_result = driver::runModuleOnDVM(ctx, root);
								   if (run_result.has_value()) {
									   exit_code = run_result.value().exit_code;
								   } else {
									   std::cerr << "Error: " << run_result.error() << "\n";
									   exit_code = 1;
								   }
							   });

							   return exit_code;
						   }))
	    .addSubcommand(clah::Clah("throw", "Throws exception (testing command).")
	                       .setHandler([](const clah::ParsingResult&) -> int {
							   throw base::LogicError("Command `throw` thrown successfully!");
						   }));
}

int main(int argc, const char* argv[]) {
	init::InitObject _;
	auto             clah = getClahForMain();

	try {
		return clah.execute(base::safeIntConv<usize>(argc), argv);
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
