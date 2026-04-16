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
#include <helios/queries/queries.hpp>
#include <linker/link.hpp>
#include <repl/session.hpp>
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
#include <printer/stream_printer.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/q_stats/q_stats.hpp>

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

	linking_options.linker_path = parsing_result.getValue<std::string>("linker");

	if (auto lib_path = parsing_result.getValue<std::string>("additional-link-options"))
		linking_options.additional_link_options = lib_path.value();

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
					auto init_result = compiler::driver::initializeTheCompiler(
						compiler::driver::CompilerModeOfOperationAndOptions::BareMode{
							.debug_options = getDebugOptionsFromClap(options),
						}
					);

					if (init_result.status().isBad()) {
						compiler::driver::exit();
						return 1;
					}

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
					auto init_result = compiler::driver::initializeTheCompiler(
						compiler::driver::CompilerModeOfOperationAndOptions::BareMode{
							.debug_options = getDebugOptionsFromClap(options),
						}
					);

					if (init_result.status().isBad()) {
						compiler::driver::exit();
						return 1;
					}

					auto file_to_parse = options.getPositional<fs::File>(0);

					// @TODO: #1879 Currently defaults to program
					auto pst = pst::PST(file_to_parse, pst::PSTType::Program);

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
							   auto init_result = compiler::driver::initializeTheCompiler(
								   compiler::driver::CompilerModeOfOperationAndOptions::BareMode{
									   .debug_options = getDebugOptionsFromClap(options),
								   }
							   );
							   if (init_result.status().isBad()) {
								   compiler::driver::exit();
								   return 1;
							   }

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
									   hout_unit->debugPrint(ctx, std::cout);
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

					auto init_result = compiler::driver::initializeTheCompiler(
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
							.execution_options = {
								.worker_count = 1,
							},
						}
					);

					if (init_result.status().isBad()) {
						compiler::driver::exit();
						return 1;
					}

					// @TODO: error handling. This should change in #1112.
					using namespace compiler;

					auto backend_type = options.isFlag("dvm-backend") ? driver::BackendType::DVM
		                                                              : driver::BackendType::LLVM;

					auto root = global_state::getMainPackage().root_module;

					(void) query::entryPoint<driver::CompileModule>({ root, backend_type, false });


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
				.add(clah::ParamBuilder::ofValue(clah::StringParser::make("link-options"))
	                     .addLongName("additional-link-options")
	                     .addShortDesc("Additional link options.")
	                     .optional()
	                     .build())
				.add(clah::ParamBuilder::ofValue(clah::StringParser::make("linker"))
	                     .addLongName("linker")
	                     .addShortDesc("Path to the linker executable.")
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
				.add(clah::ParamBuilder::ofValue(clah::IntParser::make("worker count"))
	                     .addShortName('w')
	                     .addLongName("workers")
	                     .addShortDesc("Worker count.")
	                     .optional()
	                     .build())
				.setHandler([](const clah::ParsingResult& options) -> int {
					auto path_to_compile = options.getPositional<fs::File>(0);
					auto package_name    = options.getValue<std::string>("name").copyValueOr("");
					CORE_ASSERT(package_name != "", "Package name must be specified");

					auto worker_count = options.getValue<i64>("workers").copyValueOr(1);

					auto init_result = compiler::driver::initializeTheCompiler(
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
							.execution_options = {
								.worker_count = base::safeIntConv<u64>(worker_count),
							},
						}
					);

					if (init_result.status().isBad()) {
						compiler::driver::exit();
						return 1;
					}

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

					auto init_result = compiler::driver::initializeTheCompiler(
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
									.execution_options = {
										.worker_count = 1,
									},
						}
					);

					if (init_result.status().isBad()) {
						compiler::driver::exit();
						return 1;
					}

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
	    .addSubcommand(
			clah::Clah("compile_script", "Compile a .ds script file into a .dbc or executable.")
				.addPositional(clah::FileParser::make("script"))
				.add(getLlvmOptLevelParam())
				.add(clah::ParamBuilder::ofValue(clah::FilePathParser::make("filepath"))
	                     .addShortName('a')
	                     .addLongName("artifact-location")
	                     .addShortDesc("Path to the top-level folder with build artifacts")
	                     .optional()
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("dvm-backend")
	                     .addShortDesc(
							 "Compile to DVM bytecode (.dbc) instead of a native executable."
						 )
	                     .build())
				.add(clah::ParamBuilder::ofValue(clah::StringParser::make("linker"))
	                     .addLongName("linker")
	                     .addShortDesc("Path to the linker executable.")
	                     .optional()
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("no-c-standard-library")
	                     .addShortDesc("Don't link the C standard library (LLVM backend only).")
	                     .build())
				.add(clah::ParamBuilder::ofValue(clah::IntParser::make("worker count"))
	                     .addShortName('w')
	                     .addLongName("workers")
	                     .addShortDesc("Worker count.")
	                     .optional()
	                     .build())
				.setHandler([](const clah::ParsingResult& options) -> int {
					using namespace compiler;

					auto script_file  = options.getPositional<fs::File>(0);
					auto backend_type = options.isFlag("dvm-backend") ? driver::BackendType::DVM
		                                                              : driver::BackendType::LLVM;

					auto worker_count = options.getValue<i64>("workers").copyValueOr(1);

					auto mode = compiler::driver::CompilerModeOfOperationAndOptions::ScriptMode{
						.script_file     = script_file,
						.backend_options = getBackendOptionsFromClap(options),
						.compilation_artifacts = {
							.artifacts_path = options.getValue<fs::FilePath>("artifact-location")
							                      .copyValueOr("./duck_build/"),
						},
						.debug_options     = getDebugOptionsFromClap(options),
						.execution_options = {
							.worker_count = base::safeIntConv<u64>(worker_count),
						},
					};

					auto init_result = compiler::driver::initializeTheCompiler(mode);
					if (init_result.status().isBad()) {
						compiler::driver::exit();
						return 1;
					}

					const auto& linking_options = getLinkingOptionsFromClap(options);
					auto        result = driver::compileScript(backend_type, linking_options);

					compiler::driver::exit();
					return result.isOk() ? 0 : 1;
				})
		)
	    // For now run works only for DVM backend.
	    .addSubcommand(clah::Clah("run", "Compile a .ds script file and run it on DVM.")
	                       .addPositional(clah::FileParser::make("script"))
	                       .add(clah::ParamBuilder::ofValue(clah::IntParser::make("worker count"))
	                                .addShortName('w')
	                                .addLongName("workers")
	                                .addShortDesc("Worker count.")
	                                .optional()
	                                .build())
	                       .setHandler([](const clah::ParsingResult& options) -> int {
							   using namespace compiler;

							   auto script_file  = options.getPositional<fs::File>(0);
							   auto worker_count = options.getValue<i64>("workers").copyValueOr(1);
							   // duckc run doesn't produce any artifacts for now, but this may be
		                       // changed later by for example adding option to save compiled
		                       // bytecode. Also, ScriptMode requires artifacts path, maybe this
		                       // will be refactored later.
							   auto run_temp_artifacts_path = fs::FilePath(
								   fs::FilePath::getDefaultTempDirectoryPath().getPath()
								   / "duckling_script_run_artifacts"
							   );

							   auto mode = compiler::driver::CompilerModeOfOperationAndOptions::ScriptMode{
						.script_file     = script_file,
						.backend_options = {}, // only dvm for now.
						.compilation_artifacts = {
							.artifacts_path = run_temp_artifacts_path,
						},
						.debug_options     = getDebugOptionsFromClap(options),
						.execution_options = {
							.worker_count = base::safeIntConv<u64>(worker_count),
						},
					};

							   auto init_result = compiler::driver::initializeTheCompiler(mode);
							   if (init_result.status().isBad()) {
								   compiler::driver::exit();
								   return 1;
							   }

							   auto run_result = driver::runScriptOnDVM();

							   compiler::driver::exit();
							   if (!run_result.has_value()) {
								   std::cerr << "Error: " << run_result.error() << "\n";
								   return 1;
							   }
							   return run_result->exit_code;
						   }))
	    .addSubcommand(clah::Clah("repl", "Start an interactive REPL session")
	                       .setHandler([](const clah::ParsingResult& options) -> int {
							   auto init_result = compiler::driver::initializeTheCompiler(
								   compiler::driver::CompilerModeOfOperationAndOptions::ReplMode{
									   .debug_options = getDebugOptionsFromClap(options),
									   .execution_options = {
										   .worker_count = 1,
									   },
								   }
							   );
							   if (init_result.status().isBad()) {
								   compiler::driver::exit();
								   return 1;
							   }
							   compiler::repl::ReplSession session;
							   int                         result = session.run();
							   compiler::driver::exit();
							   return result;
						   }))
	    .addSubcommand(clah::Clah("dummy", "Dummy command (cli testing command).")
	                       .setHandler([](const clah::ParsingResult& options) -> int {
							   (void) compiler::driver::initializeTheCompiler(
								   compiler::driver::CompilerModeOfOperationAndOptions::BareMode{
									   .debug_options = getDebugOptionsFromClap(options),
								   }
							   )
								   .status();
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
			{ "Unexpected Exception not inheriting from std::exception was "
		      "caught.\n",
		      printer::Color::Default },
		});
		return 1;
	}
}
