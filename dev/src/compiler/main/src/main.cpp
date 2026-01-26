/**
 * @file main.cpp
 * @brief This file implements logic and main procedure that can be used to
 * conveniently run (or add) certain functionalities of the Duckling compiler.
 * It compiles to `duckc` binary.
 * @note: The ideas from here might be one day separated into a framework.
 */

#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <global_state/backend_options.hpp>
#include <global_state/packages.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries.hpp>
#include <linker/link.hpp>
#include <repl/repl_session.hpp>
#include <time_stats/time_stats.hpp>

#include <base/except/exceptions.hpp>
#include <base/misc/int_conv.hpp>
#include <base/str/str_utils.hpp>
#include <base/types/ok_bad.hpp>

#include <clah/clah.hpp>
#include <diagnostic/logger.hpp>
#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>
#include <init/init.hpp>
#include <lexer/lexer.hpp>
#include <printer/stream_printer.hpp>
#include <query_framework/q_stats/q_stats.hpp>
#include <query_frameworkentry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>

#include <iostream>

clah::Clah getStandardDucklingOptions() {
	return clah::Clah("duckc", "The Duckling compiler")
	    .add(clah::ParamBuilder::ofFlag()
	             .addShortName('v')
	             .addLongName("version")
	             .addShortDesc("Print version and exit")
	             .build())
	    // Note that dev-logs options are not handled in pre-handler below,
	    // they should be handled in each command by getDebugOptionsFromClap and passed to
	    // initializeTheCompiler.
	    .add(clah::ParamBuilder::ofValue(clah::StringListParser::make("categories"))
	             .addLongName("dev-logs")
	             .addShortDesc("Enable developer logs for given categories.")
	             .build())
	    .setPreHandler([](const clah::ParsingResult& options) {
			if (options.isFlag("version")) {
				std::cout << "Duckling version: 0.0.1 pre-alpha\n";
				throw clah::exceptions::SuccessExitException(options);
			}
		});
}

/**
 * Helper function for setting optimisation level in relevant subcommands.
 */
clah::Parameter getLlvmOptLevelParam() {
	return clah::ParamBuilder::ofValue(clah::StringParser::make("level"))
	    .addLongName("llvm-opt")
	    .addShortName('O')
	    .addShortDesc("Set optimization level.")
	    .addLongDesc(
			"Possible values are: 0, 1, 2, 3, s, z.\n"
			"See https://llvm.org/doxygen/classllvm_1_1OptimizationLevel.html"
		)
	    .build();
}

global_state::BackendOptions getBackendOptionsFromClap(const clah::ParsingResult& parsing_result) {
	using LLVMOptimizationLevel = global_state::BackendOptions::LLVMBackend::LLVMOptimizationLevel;
	using enum LLVMOptimizationLevel;
	static const base::HashMap<std::string, LLVMOptimizationLevel> str_to_llvm_opt_level{
		{ "0", O0 }, { "1", O1 }, { "2", O2 }, { "3", O3 }, { "s", Os }, { "z", Oz },
	};
	const auto llvm_optimization_level
		= str_to_llvm_opt_level.at(parsing_result.getValue<std::string>("llvm-opt").copyValueOr("0")
	    );

	return global_state::BackendOptions{
		.llvm_backend = global_state::BackendOptions::LLVMBackend{
			.llvm_optimization_level = llvm_optimization_level,
		},
	};
}

/**
 * Helper function to extract linking options from clah parsing result.
 */
compiler::linker::LinkingOptions getLinkingOptionsFromClap(const clah::ParsingResult& parsing_result
) {
	compiler::linker::LinkingOptions linking_options;

	if (auto lib_path = parsing_result.getValue<fs::FilePath>("external-static-library"))
		linking_options.external_static_libraries.push_back(lib_path.value());

	linking_options.link_c_standard_library = not parsing_result.isFlag("no-c-standard-library");

	return linking_options;
}

compiler::driver::options_types::DebugOptions getDebugOptionsFromClap(
	const clah::ParsingResult& parsing_result
) {
	return compiler::driver::options_types::DebugOptions{
		.dev_log_categories = parsing_result.getValue<std::vector<std::string>>("dev-logs")
		                          .copyValueOr(std::vector<std::string>{}),
		.immediate_print_diagnostics = true,
		.dump_llvm_ir                = parsing_result.isFlag("dump-llvm-ir"),
		.dump_llvm_asm               = parsing_result.isFlag("dump-llvm-asm"),
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
						std::cout << "This prints only top-level tokens (will not print tokens "
									 "within parentheses).\n";
						auto& tokens = token_file->getTokenData();
						for (auto& token: tokens.tokens) {
							std::string token_str{ token.getStrValue() };
							printer::StreamPrinter::printNL(
								{
									"Token: ",
									token_str,
									std::string(20 - token_str.length(), ' '),  // alignment
									" at ",
									std::to_string(token.getPosition().getStartLineColumn().first),
									":",
									std::to_string(token.getPosition().getStartLineColumn().second),
									",\t type=",
									std::to_string(static_cast<int>(token.getType())),
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
							   auto root
								   = frontend::createModuleTreeWithRandomPackageID(path_to_compile);
							   auto hout_units
								   = query::entryPoint<helios::QueryModuleHOUTRecursively>(root)
		                                 .valueOrPanicMsg("The hout creation failed");
							   query::utils::withContextDo([&](query::Context& ctx) {
								   for (const auto& hout_unit: hout_units)
									   std::cout << hout_unit.debugPrint(ctx);
							   });

							   return exit_code;
						   }))
	    .addSubcommand(
			clah::Clah("compile_module", "Compile given module into a binary.")
				.addPositional(clah::FileParser::make("module"))
				.add(getLlvmOptLevelParam())
				.add(clah::ParamBuilder::ofValue(clah::StringParser::make("name"))
	                     .addShortName('n')
	                     .addLongName("name")
	                     .addShortDesc("Name of the package the module belongs to.")
	                     .optional()
	                     .build())
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
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("no-incremental")
	                     .addShortDesc(
							 "Disable incremental compilation (do not load previous query graph)."
						 )
	                     .build())
				.setHandler([](const clah::ParsingResult& options) -> int {
					auto path_to_compile = options.getPositional<fs::File>(0);
					auto package_name    = options.getValue<std::string>("name").copyValueOr(
                        base::generateRandomString(32)
                    );

					compiler::driver::initializeTheCompiler(
						compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
							.main_package_info = {
								.package_name = package_name,
								.package_path = path_to_compile.getFilePath(),
							},
							.compilation_artifacts = {
								.artifacts_path = fs::FilePath("./duck_build/"),
							},
							.backend_options = getBackendOptionsFromClap(options),
							.debug_options = getDebugOptionsFromClap(options),
							.incremental   = { .enabled = !options.isFlag("no-incremental") },
						}
					);

					// @TODO: error handling. This should change in #1112.
					using namespace compiler;

					auto backend_type = options.isFlag("dvm-backend") ? driver::BackendType::DVM
		                                                              : driver::BackendType::LLVM;

					auto root = global_state::getMainPackage().root_module;

					auto output_artifact
						= query::entryPoint<driver::CompileModule>({ root, backend_type });


					compiler::driver::exit();

					return 0;
				})
		)
	    .addSubcommand(
			clah::Clah("compile_package", "Compile given package into a binary.")
				.addPositional(clah::FileParser::make("module"))
				.add(getLlvmOptLevelParam())
				.add(clah::ParamBuilder::ofValue(clah::StringParser::make("name"))
	                     .addShortName('n')
	                     .addLongName("name")
	                     .addShortDesc("Name of the package the module belongs to.")
	                     .required()
	                     .build())
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
				.add(clah::ParamBuilder::ofValue(clah::FilePathParser::make("library"))
	                     .addLongName("external-static-library")
	                     .addShortDesc("Path to a static library to link against.")
	                     .optional()
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("no-c-standard-library")
	                     .addShortDesc(
							 "Doesn't link the C standard library into the final executable."
						 )
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("no-incremental")
	                     .addShortDesc(
							 "Disable incremental compilation (do not load previous query graph)."
						 )
	                     .build())
				.setHandler([](const clah::ParsingResult& options) -> int {
					auto path_to_compile = options.getPositional<fs::File>(0);
					auto package_name    = options.getValue<std::string>("name").copyValueOr("");
					CORE_ASSERT(package_name != "", "Package name must be specified");

					compiler::driver::initializeTheCompiler(
						compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
							.main_package_info = {
								.package_name = package_name,
								.package_path = path_to_compile.getFilePath(),
							},
							.compilation_artifacts = {
								.artifacts_path =
									options.getValue<fs::FilePath>("artifact-location").copyValueOr("./duck_build/"),
							},
							.backend_options = getBackendOptionsFromClap(options),
							.debug_options = getDebugOptionsFromClap(options),
							.incremental   = { .enabled = !options.isFlag("no-incremental") },
						}
					);
					const auto& linking_options = getLinkingOptionsFromClap(options);


					time_stats::TrackCategoryTime total_compilation_time(
						time_stats::TimeCategories::TotalCompilationTime
					);

					auto backend_type = options.isFlag("dvm-backend")
		                                  ? compiler::driver::BackendType::DVM
		                                  : compiler::driver::BackendType::LLVM;

					base::OkBad result = compiler::driver::compileEntirePackage(
						global_state::getMainPackage(), backend_type, linking_options
					);

					total_compilation_time.end();

					compiler::driver::exit();

					if (options.isFlag("print-statistics")) {
						if (not query::USE_STATS) {
							std::cerr << "Warning: Query statistics are disabled at compile time. "
										 "No query statistics will be printed.\n";
						}
						query::printStats();
						time_stats::prettyPrintTimeStatistics();
					}

					if (options.isFlag("print-graph"))
						query::Context::getState().getGraph().debugPrintForDrawing(std::cerr);


					return result.isOk() ? 0 : 1;
				})
		)
	    .addSubcommand(
			clah::Clah("dvm_run", "Compile given module to DVM (in-memory) and run it")
				.addPositional(clah::FileParser::make("module"))
				.add(clah::ParamBuilder::ofValue(clah::StringParser::make("name"))
	                     .addShortName('n')
	                     .addLongName("name")
	                     .addShortDesc("Name of the package the module belongs to.")
	                     .optional()
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("add-builtin-library")
	                     .addShortDesc("Links builtin library into the final executable.")
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("no-incremental")
	                     .addShortDesc(
							 "Disable incremental compilation (do not load previous query graph)."
						 )
	                     .build())
				.setHandler([](const clah::ParsingResult& options) -> int {
					auto path_to_compile = options.getPositional<fs::File>(0);
					auto package_name    = options.getValue<std::string>("name").copyValueOr(
                        base::generateRandomString(32)
                    );
					using namespace compiler;

					compiler::driver::initializeTheCompiler(
						compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
									.main_package_info = {
										.package_name = package_name,
										.package_path = path_to_compile.getFilePath(),
									},
									.compilation_artifacts = {
										.artifacts_path = fs::FilePath("./duck_build/"),
									},
									.backend_options = {},
									.debug_options = getDebugOptionsFromClap(options),
									.incremental = {.enabled = !options.isFlag("no-incremental") },
						}
					);

					auto root = frontend::createModuleTree(path_to_compile, package_name);

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


					compiler::driver::exit();
					return exit_code;
				})
		)
	    .addSubcommand(clah::Clah("repl", "Start an interactive REPL session")
	                       .setHandler([](const clah::ParsingResult& options) -> int {
							   compiler::driver::initializeTheCompiler(
								   compiler::driver::CompilerModeOfOperationAndOptions::ReplMode{
									   .debug_options = getDebugOptionsFromClap(options),
								   }
							   );

							   compiler::repl::ReplSession session;
							   int                         result = session.run();
							   compiler::driver::exit();
							   return result;
						   }))
	    .addSubcommand(clah::Clah("dummy", "Dummy command (cli testing command).")
	                       .setHandler([](const clah::ParsingResult& options) -> int {
							   compiler::driver::initializeTheCompiler(
								   compiler::driver::CompilerModeOfOperationAndOptions::BareMode{
									   .debug_options = getDebugOptionsFromClap(options),
								   }
							   );
							   return 0;
						   }));
}

int main(int argc, const char* argv[]) {
	init::InitObject _;
	auto             clah = getClahForMain();

	try {
		return clah.execute(base::safeIntConv<usize>(argc), argv);
	} catch (const base::Exception& e) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::Red },
			{ "Compiler Exception was caught with message:\n", printer::Color::Default },
			{ e.what(), printer::Color::Default },
			{ "\nAborting\n", printer::Color::Default },
		});
		return 1;
	} catch (const std::exception& e) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::Red },
			{ "Unexpected Exception was caught with message:\n", printer::Color::Default },
			{ e.what(), printer::Color::Default },
			{ "\nAborting\n", printer::Color::Default },
		});
		return 1;
	} catch (...) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::Red },
			{ "Unexpected Exception not inheriting from std::exception was caught.\n",
		      printer::Color::Default },
		});
		return 1;
	}
}
