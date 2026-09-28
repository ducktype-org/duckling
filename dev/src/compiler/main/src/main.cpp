/**
 * @file main.cpp
 * @brief This file implements logic and main procedure that can be used to
 * conveniently run (or add) certain functionalities of the Duckling compiler.
 * It compiles to `duckc` binary.
 * @note: The ideas from here might be one day separated into a framework.
 */

#include <archiver/archive.hpp>
#include <driver/diagnostics/log_helpers.hpp>
#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/manifest/manifest.hpp>
#include <driver/operations/generic_operations.hpp>
#include <driver/standard_library/standard_library.hpp>
#include <driver/task/task.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <global_state/artifacts_location.hpp>
#include <global_state/backend_options.hpp>
#include <global_state/packages.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/queries.hpp>
#include <linker/link.hpp>
#include <repl/session.hpp>
#include <time_stats/time_stats.hpp>
#include <version/version.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/ranges_utils.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/extend_cpp/vector_utils.hpp>
#include <base/misc/int_conv.hpp>
#include <base/str/str_utils.hpp>
#include <base/types/ok_bad.hpp>

#include <clah/clah.hpp>
#include <diagnostic/logger.hpp>
#include <diagnostic/module_flags/module_flags.hpp>
#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>
#include <init/init.hpp>
#include <logger/logger.hpp>
#include <os_utils/exec_self.hpp>
#include <printer/stream_printer.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/external/api.hpp>
#include <query_framework/q_stats/q_stats.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <ranges>
#include <string>
#include <vector>

namespace {
	/**
	 * Global variable storing arguments passed to the program.
	 */
	std::vector<std::string> g_argv;

	/**
	 * @brief Update the stored argv so the REPL can restart with adjusted history options.
	 *
	 * Preserves the original executable and subcommand, removes any existing --history-entries and
	 * --silent arguments, which control reset behavior, and appends the requested
	 * replay count and silence flag.
	 *
	 * @param replay_count Number of history entries to replay after restart.
	 * @param silent Whether history replay should be silent.
	 */
	void setReplRestartArgs(usize replay_count, bool silent) {
		if (g_argv.empty()) return;

		std::vector<std::string> new_args;
		new_args.reserve(g_argv.size() + 3);
		new_args.push_back(g_argv[0]);

		bool seen_repl = false;
		for (usize i = 1; i < g_argv.size(); ++i) {
			const auto& arg = g_argv[i];
			if (!seen_repl) {
				new_args.push_back(arg);
				if (arg == "repl") seen_repl = true;
				continue;
			}

			if (arg == "-n" || arg == "--history-entries") {
				if (i + 1 < g_argv.size()) ++i;
				continue;
			}
			if (arg == "--silent") continue;

			new_args.push_back(arg);
		}

		if (!seen_repl) return;
		new_args.emplace_back("-n");
		new_args.push_back(std::to_string(replay_count));
		if (silent) new_args.emplace_back("--silent");

		g_argv = std::move(new_args);
	}
}

/**
 * @brief The version facts only duckc can report.
 */
constexpr std::array<version::ExtraField, 2> DUCKC_VERSION_FIELDS{
	version::ExtraField{ "LLVM", DUCKC_LLVM_VERSION },
#ifdef ENABLE_JIT
	version::ExtraField{ "JIT", "enabled" },
#else
	version::ExtraField{ "JIT", "disabled" },
#endif
};

clah::Clah getStandardDucklingOptions() {
	return clah::Clah("duckc", "The Duckling compiler")
	    .add(clah::ParamBuilder::ofFlag()
	             .addShortName('v')
	             .addLongName("version")
	             .addShortDesc("Print version and exit")
	             .build())
	    .add(clah::ParamBuilder::ofFlag()
	             .addLongName("version-verbose")
	             .addShortDesc("Print version together with build information and exit")
	             .build())
	    // Note that dev-logs options are not handled in pre-handler below,
	    // they should be handled in each command by debug_options::getDebugOptionsFromClah and
	    // passed to initializeTheCompiler.
	    .add(clah::ParamBuilder::ofValue(
				 clah::StringListParser::make("categories", clah::StringParser::make())
		)
	             .addLongName("dev-logs")
	             .addShortDesc("Enable developer logs for given categories.")
	             .build())
	    .setPreHandler([](const clah::ParsingResult& options) {
			if (options.isFlag("version-verbose")) {
				std::cout << version::renderVerbose("duckc", DUCKC_VERSION_FIELDS) << '\n';
				throw clah::exceptions::SuccessExitException(options);
			}
			if (options.isFlag("version")) {
				std::cout << version::renderShort("duckc") << '\n';
				throw clah::exceptions::SuccessExitException(options);
			}
		});
}

/**
 * Helper function for setting optimisation level in relevant subcommands.
 */
clah::Parameter getLlvmOptLevelParam() {
	std::vector<std::string> llvm_opt_level_values{ "0", "1", "2", "3", "s", "z" };
	return clah::ParamBuilder::ofValue(clah::CategoryParser::make("level", llvm_opt_level_values))
	    .addLongName("llvm-opt")
	    .addShortName('O')
	    .addShortDesc("Set optimization level.")
	    .addLongDesc(
			"Possible values are: 0, 1, 2, 3, s, z.\n"
			"See https://llvm.org/doxygen/classllvm_1_1OptimizationLevel.html"
		)
	    .build();
}

global_state::BackendOptions getBackendOptionsFromClah(const clah::ParsingResult& parsing_result) {
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

auto getClahStdLibOptions() {
	return std::array{
		clah::ParamBuilder::ofFlag()
			.addLongName("no-std")
			.addShortDesc("Do not use the standard library.")
			.build(),
		clah::ParamBuilder::ofValue(clah::FilePathParser::make("path"))
			.addLongName("custom-std-path")
			.addShortDesc("Path to a custom standard library.")
			.optional()
			.build(),
		clah::ParamBuilder::ofValue(clah::FilePathParser::make("path"))
			.addLongName("custom-std-artifacts-path")
			.addShortDesc("Path to the standard library artifacts.")
			.addLongDesc("Uses existing compiled standard library artifacts.\n"
		                 "If the compiled binaries are available in the provided directory,\n"
		                 "standard library compilation is skipped.\n"
		                 "This can produce errors if the standard library\n"
		                 "source code changed after the artifacts in the provided directory\n"
		                 "were compiled.\n"
		                 "Use with caution.\n")
			.build(),
	};
}

/**
 * Helper function to extract standard library options from clah parsing result.
 */
compiler::driver::options_types::StdLibOptions getStdLibOptionsFromClah(
	const clah::ParsingResult& parsing_result
) {
	using compiler::driver::options_types::StdLibOptions;
	compiler::driver::options_types::StdLibOptions std_lib_options;


	if (parsing_result.isFlag("no-std"))
		std_lib_options.std_lib_type = StdLibOptions::NoStd{};

	else if (auto custom_std_path = parsing_result.getValue<fs::FilePath>("custom-std-path"))
		std_lib_options.std_lib_type = StdLibOptions::CustomStd{ .std_path = *custom_std_path };
	else
		std_lib_options.std_lib_type = StdLibOptions::DefaultStd{};

	if (auto custom_std_art_path
	    = parsing_result.getValue<fs::FilePath>("custom-std-artifacts-path"))
		std_lib_options.std_artifacts_path = custom_std_art_path.value();

	return std_lib_options;
}

clah::VerificationResult verifyStdLibOptions(const clah::ParsingResult& parsing_result) {
	if (parsing_result.isFlag("no-std") && parsing_result.isParam("custom-std-path")) {
		return std::unexpected<std::string>(
			"--no-std and --custom-std-path cannot be used together."
		);
	}

	return clah::VerificationPassed{};
}

auto getClahArchivingOptions() {
	return std::array{
		clah::ParamBuilder::ofValue(clah::FilePathParser::make("archiver path"))
			.addLongName("archiver")
			.addShortDesc("Path to the archiver to use when creating static libraries.")
			.optional()
			.build(),
	};
}

/**
 * Helper function to extract archiving options from clah parsing result.
 */
compiler::archiver::ArchivingOptions getArchivingOptionsFromClah(
	const clah::ParsingResult& parsing_result
) {
	compiler::archiver::ArchivingOptions archiving_options;
	archiving_options.archiver_path = parsing_result.getValue<std::string>("archiver");
	return archiving_options;
}

auto getClahLinkingOptions() {
	return std::array{
		clah::ParamBuilder::ofValue(clah::FilePathParser::make())
			.addLongName("linker")
			.addShortDesc("Path to the linker to use when creating executables.")
			.optional()
			.build(),
		clah::ParamBuilder::ofValue(clah::StringParser::make("options"))
			.addLongName("additional-link-options")
			.addShortDesc("Additional options to pass to the linker.")
			.optional()
			.build(),
		clah::ParamBuilder::ofValue(
			clah::StringListParser::make("shared-libs", clah::StringParser::make())
		)
			.addLongName("dvm-shared-libs")
			.addShortDesc("Shared libraries that will be loaded by the VM.")
			.optional()
			.build(),
		clah::ParamBuilder::ofValue(
			clah::FilePathListParser::make("paths", clah::FilePathParser::make())
		)
			.addLongName("dvm-link-libraries")
			.addShortDesc("Paths to the .dbc libraries to link into the output.")
			.optional()
			.build(),
	};
}

/**
 * Helper function to extract local linking options from clah parsing result.
 */
compiler::driver::options_types::LinkingOptions getLinkingOptionsFromClah(
	const clah::ParsingResult& parsing_result
) {
	compiler::driver::options_types::LinkingOptions linking_options;

	linking_options.native_linker_path = parsing_result.getValue<std::string>("linker");

	if (auto lib_path = parsing_result.getValue<std::string>("additional-link-options"))
		linking_options.native_additional_link_options = lib_path.value();

	if (auto lib_paths = parsing_result.getValue<std::vector<std::string>>("dvm-shared-libs"))
		linking_options.dvm_shared_libraries = lib_paths.value();

	if (auto lib_paths = parsing_result.getValue<std::vector<fs::FilePath>>("dvm-link-libraries"))
		linking_options.dvm_link_libraries = lib_paths.value();

	linking_options.native_link_c_standard_lib = not parsing_result.isFlag("no-c-standard-library");

	return linking_options;
}

/**
 * @brief The CLI interface for the DebugOptions part of the driver.
 */
namespace debug_options {
	using compiler::driver::options_types::DebugOptions;

	auto getDebugDumpIROptions() -> const base::HashMap<std::string, bool DebugOptions::*>& {
		static base::HashMap<std::string, bool DebugOptions::*> dump_field_mapping{
			{ "asm", &DebugOptions::dump_asm }, { "llvm", &DebugOptions::dump_llvm },
			{ "dbc", &DebugOptions::dump_dbc }, { "lir", &DebugOptions::dump_lir },
			{ "mir", &DebugOptions::dump_mir }, { "hir", &DebugOptions::dump_hir },
		};
		return dump_field_mapping;
	}

	auto getDebugPrintIROptions() -> const base::HashMap<std::string, bool DebugOptions::*>& {
		static base::HashMap<std::string, bool DebugOptions::*> print_field_mapping{
			{ "dbc", &DebugOptions::print_dbc },
			{ "lir", &DebugOptions::print_lir },
			{ "mir", &DebugOptions::print_mir },
			{ "hir", &DebugOptions::print_hir },
		};
		return print_field_mapping;
	}

	/**
	 * @brief Return the array of clah::Parameter for the debug options of the driver, which can be
	 * added to a Clah instance.
	 */
	auto getClahDebugParameters() {
		std::vector<std::string> dump_categories = getDebugDumpIROptions() | std::views::keys
		                                         | std::ranges::to<std::vector<std::string>>();
		std::vector<std::string> print_categories = getDebugPrintIROptions() | std::views::keys
		                                          | std::ranges::to<std::vector<std::string>>();

		return std::array{
			clah::ParamBuilder::ofValue(clah::CategoryListParser::make(
											"categories", clah::CategoryParser::make(dump_categories)
										))
				.addLongName("dump-ir")
				.addShortDesc("Dump to file the comma separated intermediate representations.")
				.addLongDesc("Possible values are: asm, llvm, dbc, lir, mir, hir.\nNote: dbc "
			                 "requires --dvm-backend.")
				.build(),
			clah::ParamBuilder::ofValue(clah::CategoryListParser::make(
											"categories",
											clah::CategoryParser::make(print_categories)
										))
				.addLongName("print-ir")
				.addShortDesc("Print to stdout the comma separated intermediate representations.")
				.addLongDesc("Possible values are: dbc, lir, mir, hir.")
				.build(),
		};
	}

	/**
	 * @brief Given parsing result from clah, extract the debug options for the driver.
	 */
	DebugOptions getDebugOptionsFromClah(const clah::ParsingResult& parsing_result) {
		DebugOptions debug_options{
			.dev_log_categories = parsing_result.getValue<std::vector<std::string>>("dev-logs")
			                          .copyValueOr(std::vector<std::string>{}),
			.immediate_print_diagnostics = true,
		};

		if (auto dump_categories = parsing_result.getValue<std::vector<std::string>>("dump-ir"))
			for (const auto& category: dump_categories.value())
				debug_options.*(getDebugDumpIROptions().at(category)) = true;
		if (auto print_categories = parsing_result.getValue<std::vector<std::string>>("print-ir"))
			for (const auto& category: print_categories.value())
				debug_options.*(getDebugPrintIROptions().at(category)) = true;

		return debug_options;
	}
}

/**
 * @brief Generate the `compile_package` subcommand.
 * @param dump_query_graph If true, the command additionally takes a required `--graph-output`
 * directory and writes the query graph before and after optimization into it. The optional
 * `--rename-pass` / `--simplify-pass` flags make those graphs smaller before they are written.
 */
clah::Clah getClahForCompilePackage(
	std::string name, std::string description, bool dump_query_graph
) {
	auto command
		= clah::Clah(std::move(name), std::move(description))
	          .addPositional(
				  clah::FileParser::make("path", true),
				  "Path to the root module of the package or the root module file."
			  )
	          .add(getLlvmOptLevelParam())
	          .add(debug_options::getClahDebugParameters())
	          .add(getClahStdLibOptions())
	          .addCustomVerification(verifyStdLibOptions)
	          .add(getClahLinkingOptions())
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
	          .addCustomVerification(
				  [](const clah::ParsingResult& options) -> clah::VerificationResult {
					  if (options.isFlag("print-graph") && options.isFlag("no-incremental")) {
						  return std::unexpected<std::string>(
							  "--print-graph requires the query graph, which is disabled by "
							  "--no-incremental. These flags cannot be used together."
						  );
					  }
					  return clah::VerificationPassed{};
				  }
			  )
	          .add(clah::ParamBuilder::ofValue(clah::StringParser::make("archiver"))
	                   .addLongName("archiver")
	                   .addShortDesc("Path to the archiver executable.")
	                   .optional()
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
	          .add(clah::ParamBuilder::ofValue(clah::StringParser::make("output-file-name"))
	                   .addShortName('o')
	                   .addLongName("output-file-name")
	                   .addShortDesc("Output artifact file name (without extension).")
	                   .optional()
	                   .build())
	          .add(clah::ParamBuilder::ofFlag()
	                   .addLongName("emit-static-lib")
	                   .addShortDesc("Emit a static library (.a) instead of an executable.")
	                   .build());

	if (dump_query_graph) {
		command
			.add(clah::ParamBuilder::ofValue(clah::FilePathParser::make("directory"))
		             .addLongName("graph-output")
		             .addShortDesc(
						 "Directory to write the pre- and post-optimization query graphs to."
					 )
		             .required()
		             .build())
			.add(clah::ParamBuilder::ofFlag()
		             .addLongName("rename-pass")
		             .addShortDesc("Give input nodes readable names and call every unstable node "
		                           "'Unstable Node' in the written graphs.")
		             .build())
			.add(clah::ParamBuilder::ofFlag()
		             .addLongName("simplify-pass")
		             .addShortDesc(
						 "Remove duplicated edges and module tree bookkeeping nodes, and merge "
						 "source code inputs used by only one node, in the written graphs. "
						 "Implies --rename-pass."
					 )
		             .build())
			.addCustomVerification(
				[](const clah::ParsingResult& options) -> clah::VerificationResult {
					if (options.isFlag("no-incremental")) {
						return std::unexpected<std::string>(
							"The query graph is not recorded with --no-incremental, so there is "
							"nothing to dump."
						);
					}
					return clah::VerificationPassed{};
				}
			);
	}

	auto handler = [dump_query_graph](const clah::ParsingResult& options) -> int {
		if (options.isFlag("no-incremental") && options.isFlag("print-graph")) {
			CORE_USER_LOG(
				"Error: --print-graph requires the query graph, which is "
				"disabled by --no-incremental. These flags cannot be used "
				"together.\n"
			);
			return 1;
		}

		auto path_to_compile = options.getPositional<fs::File>(0);
		auto package_name    = options.getValue<std::string>("name").copyValueOr("");
		CORE_ASSERT(package_name != "", "Package name must be specified");

		auto worker_count   = options.getValue<i64>("workers").copyValueOr(1);
		auto stdlib_options = getStdLibOptionsFromClah(options);
		auto artifacts_path
			= options.getValue<fs::FilePath>("artifact-location").copyValueOr("./duck_build/");
		auto init_result = compiler::driver::initializeTheCompiler(
						compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
							.packages_info = {
								compiler::frontend::packages::RawPackageInfo{
									.package_id   = base::StrID(package_name),
									.package_name = base::StrID(package_name),
									.version      = base::StrID("not_supported"),
									.package_path = path_to_compile.getFilePath(),
									.features     = {},
									.dependencies = {},
								},
							},
							.compilation_artifacts = {
								.artifacts_path =
									artifacts_path,
							},
							.backend_options   = getBackendOptionsFromClah(options),
							.debug_options     = debug_options::getDebugOptionsFromClah(options),
							.incremental       = { .enabled = !options.isFlag("no-incremental") },
							.execution_options = {
								.worker_count = base::safeIntConv<u64>(worker_count),
							},
							.stdlib_options = stdlib_options
						}
					);
		if (init_result.status().isBad()) {
			compiler::driver::exit();
			return 1;
		}

		// For now we always compile the standard library on demand,
		// note that it will be always cached.
		auto std_compilation_result = compiler::driver::compilePackages(
			compiler::driver::getRequiredStdLibCompilationTasks()
		);
		if (std_compilation_result.isBad()) {
			compiler::driver::exit();
			return 1;
		}


		compiler::driver::BuildTarget build_target;
		if (options.isFlag("dvm-backend")) {
			auto output_file_name
				= options.getValue<std::string>("output-file-name").copyValueOr("package_dvm.dbc");
			auto is_lib              = options.isFlag("emit-static-lib");
			auto dvm_linking_options = compiler::driver::constructDVMLinkingOptions(
				getLinkingOptionsFromClah(options), stdlib_options, is_lib
			);
			if (is_lib) {
				build_target = compiler::driver::BuildTargetDVMLibrary{
					.output_file_name    = base::StrID(output_file_name),
					.dvm_linking_options = std::move(dvm_linking_options)
				};
			} else {
				build_target = compiler::driver::BuildTargetDVMExecutable{
					.output_file_name    = base::StrID(output_file_name),
					.dvm_linking_options = std::move(dvm_linking_options)
				};
			}

		} else {
			auto output_file_name
				= options.getValue<std::string>("output-file-name").copyValueOr("package_llvm.exe");
			if (options.isFlag("emit-static-lib")) {
				auto archiving_options = getArchivingOptionsFromClah(options);
				build_target           = compiler::driver::BuildTargetLLVMStaticLibrary{
							  .output_file_name  = base::StrID(output_file_name),
							  .archiving_options = archiving_options,
				};
			} else {
				auto native_linking_options = compiler::driver::constructNativeLinkerOptions(
					getLinkingOptionsFromClah(options), stdlib_options
				);
				build_target = compiler::driver::BuildTargetLLVMExecutable{
					.output_file_name = base::StrID(output_file_name),
					.linking_options  = native_linking_options,
				};
			}
		}

		time_stats::TrackCategoryTime total_compilation_time(
			time_stats::TimeCategories::TotalCompilationTime
		);

		CORE_ASSERT(!global_state::getPackages().empty(), "No packages registered");
		base::OkBad result = compiler::driver::compilePackages({
			compiler::driver::PackageCompilationTask{
				.root_module
				= global_state::getPackages().front().getRootModule().illegalAccess().getID(),
				.build_target = build_target,
			},
		});

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

		if (dump_query_graph) {
			auto dumped = query::external::dumpQueryGraphsToDirectory(
				options.getValue<fs::FilePath>("graph-output").value().strView(),
				{ .rename   = options.isFlag("rename-pass"),
			      .simplify = options.isFlag("simplify-pass") }
			);
			if (!dumped) {
				CORE_USER_LOG("Error: ", dumped.error(), "\n");
				return 1;
			}
			for (const auto& path: *dumped)
				CORE_USER_LOG("Query graph written to '", path.string(), "'\n");
		}


		return result.isOk() ? 0 : 1;
	};

	return std::move(command).setHandler(std::move(handler));
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
							.debug_options = debug_options::getDebugOptionsFromClah(options),
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
						std::cout << "Tokenization errors.\n";
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
							.debug_options = debug_options::getDebugOptionsFromClah(options),
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

					if (pst.hasErrors()) exit_code = 1;

					std::cout << "Parsed tree:\n";
					pst.dprint(std::cout);
					std::cout << "\n";

					return exit_code;
				})
		)
	    .addSubcommand(
			clah::Clah("compile_module", "Compile given module into a binary.")
				.addPositional(clah::FileParser::make("module", true))
				.add(getLlvmOptLevelParam())
				.add(debug_options::getClahDebugParameters())
				.add(getClahStdLibOptions())
				.addCustomVerification(verifyStdLibOptions)
				.add(clah::ParamBuilder::ofValue(clah::StringParser::make("name"))
	                     .addShortName('n')
	                     .addLongName("name")
	                     .addShortDesc("Name of the package the module belongs to.")
	                     .optional()
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("dvm-backend")
	                     .addShortDesc("Compile to DVM bytecode.")
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
							.packages_info = {
								compiler::frontend::packages::RawPackageInfo{
									.package_id   = base::StrID(package_name),
									.package_name = base::StrID(package_name),
									.version      = base::StrID("not_supported"),
									.package_path = fs::FilePath(path_to_compile.getFilePath()),
									.features     = {},
									.dependencies  = {},
								},
							},
							.compilation_artifacts = {
								.artifacts_path = fs::FilePath("./duck_build/"),
							},
							.backend_options   = getBackendOptionsFromClah(options),
							.debug_options     = debug_options::getDebugOptionsFromClah(options),
							.incremental       = { .enabled = !options.isFlag("no-incremental") },
							.execution_options = {
								.worker_count = 1,
							},
							.stdlib_options = getStdLibOptionsFromClah(options),
						}
					);

					if (init_result.status().isBad()) {
						compiler::driver::exit();
						return 1;
					}

					using namespace compiler;

					auto backend_type = options.isFlag("dvm-backend") ? driver::BackendType::DVM
		                                                              : driver::BackendType::LLVM;

					CORE_ASSERT(!global_state::getPackages().empty(), "No packages registered");
					auto root
						= global_state::getPackages().front().getRootModule().illegalAccess().getID(
						);

					query::entryPoint<driver::CompileModule>({ root, backend_type, false });


					compiler::driver::exit();

					return 0;
				})
		)
	    .addSubcommand(
			clah::Clah(
				"compile_modules",
				"Compile given modules into binaries. Every module is placed in its own package "
				"and every package depends on all the other ones. "
				"This mimics the simple case of multi file compilation with GCC/clang which is "
				"sometimes useful. Note that the package based entry points should be preferred "
				"when possible."
			)
				.addPositional(clah::FileParser::make("module"))
				.setDefaultValueParser(clah::FileParser::make("module"))
				.add(getLlvmOptLevelParam())
				.add(debug_options::getClahDebugParameters())
				.add(getClahStdLibOptions())
				.addCustomVerification(verifyStdLibOptions)
				.add(getClahLinkingOptions())
				.add(clah::ParamBuilder::ofValue(clah::FilePathParser::make("filepath"))
	                     .addShortName('a')
	                     .addLongName("artifact-location")
	                     .addShortDesc("Path to the top-level folder with build artifacts")
	                     .optional()
	                     .build())
				.add(clah::ParamBuilder::ofValue(clah::StringParser::make("output-file-name"))
	                     .addShortName('o')
	                     .addLongName("output-file-name")
	                     .addShortDesc("Output artifact file name of the executable.")
	                     .optional()
	                     .build())
				.add(clah::ParamBuilder::ofValue(clah::StringParser::make("archiver"))
	                     .addLongName("archiver")
	                     .addShortDesc("Path to the archiver executable.")
	                     .optional()
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("dvm-backend")
	                     .addShortDesc("Compile to DVM bytecode.")
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("no-incremental")
	                     .addShortDesc(
							 "Disable incremental compilation (do not load previous query graph)."
						 )
	                     .build())
				.setHandler([](const clah::ParsingResult& options) -> int {
					std::vector<fs::File> modules_to_compile{ options.getPositional<fs::File>(0) };
					for (usize i = 0; i < options.getExtraParameterCount(); ++i)
						modules_to_compile.push_back(options.getExtra<fs::File>(i).value());

					const bool dvm_backend = options.isFlag("dvm-backend");

					std::set<base::StrID>               aliases_duplicate_check;
					base::Map<base::StrID, base::StrID> package_id_aliases;
					std::vector<compiler::frontend::packages::RawPackageInfo> packages_info;
					packages_info.reserve(modules_to_compile.size());
					for (const auto& module: modules_to_compile) {
						auto package_name = base::StrID(base::generateRandomString(32));

						auto inserted
							= aliases_duplicate_check.emplace(module.getFilePath().stem());
						if (!inserted.second) {
							CORE_USER_LOG(
								"ERROR: Duplicate module names: ",
								module.getFilePath().string(),
								"\n"
							);
							return 1;
						}

						package_id_aliases.emplace(package_name, module.getFilePath().stem());

						packages_info.push_back(compiler::frontend::packages::RawPackageInfo{
							.package_id   = package_name,
							.package_name = package_name,
							.version      = base::StrID("not_supported"),
							.package_path = module.getFilePath(),
							.features     = {},
							.dependencies = {},
						});
					}

					// Every package depends on every other one.
					const usize package_count = packages_info.size();
					for (usize dependent = 0; dependent < package_count; ++dependent)
						for (usize dependency = 0; dependency < package_count; ++dependency) {
							if (dependent == dependency) continue;

							packages_info.at(dependent).dependencies.push_back(
								compiler::frontend::packages::RawDependencyInfo{
									.package_id = packages_info.at(dependency).package_id,
									.alias
									= package_id_aliases.at(packages_info.at(dependency).package_id),
								}
							);
						}

					// Package ids of the modules given in the command line, in the same order.
					std::vector<base::StrID> package_ids;
					package_ids.reserve(packages_info.size());
					for (const auto& package_info: packages_info)
						package_ids.push_back(package_info.package_id);

					auto stdlib_options = getStdLibOptionsFromClah(options);
					auto init_result    = compiler::driver::initializeTheCompiler(
                        compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
							.packages_info         = std::move(packages_info),
							.compilation_artifacts = {
								.artifacts_path = options.getValue<fs::FilePath>("artifact-location").copyValueOr("./duck_build/"),
							},
							.backend_options   = getBackendOptionsFromClah(options),
							.debug_options     = debug_options::getDebugOptionsFromClah(options),
							.incremental       = { .enabled = !options.isFlag("no-incremental") },
							.execution_options = {
								.worker_count = 1,
							},
							.stdlib_options = stdlib_options,
                        }
                    );

					if (init_result.status().isBad()) {
						compiler::driver::exit();
						return 1;
					}

					using namespace compiler;

					// For now we always compile the standard library on demand,
		            // note that it will be always cached.
					auto std_compilation_result = compiler::driver::compilePackages(
						compiler::driver::getRequiredStdLibCompilationTasks()
					);
					if (std_compilation_result.isBad()) {
						compiler::driver::exit();
						return 1;
					}

					// Every module but the first one is built into a library, which is then linked
		            // into the artifact of the first module. The first module is the last task, so
		            // that all the libraries it links are already built.
					std::vector<driver::PackageCompilationTask> compilation_tasks;
					base::Optional<frontend::ModuleID>          main_root_module;
					std::vector<fs::FilePath>                   libraries_to_link;

					CORE_ASSERT(!global_state::getPackages().empty(), "No packages registered");
					for (const auto& package: global_state::getPackages()) {
						auto package_id = package.getPackageID();
						// Skip the packages that were not given in the command line, like the
			            // standard library ones.
						if (std::ranges::find(package_ids, package_id) == package_ids.end())
							continue;

						auto root_module = package.getRootModule().illegalAccess().getID();
						if (package_id == package_ids.front()) {
							main_root_module = root_module;
							continue;
						}

						auto library_name = base::StrID(
							base::strConcat(package_id.strView(), dvm_backend ? ".dbc" : ".a")
						);
						if (dvm_backend) {
							compilation_tasks.push_back(driver::PackageCompilationTask{
								.root_module  = root_module,
								.build_target = driver::BuildTargetDVMLibrary{
									.output_file_name = library_name,
									.dvm_linking_options   = driver::constructDVMLinkingOptions(
									getLinkingOptionsFromClah(options), stdlib_options, true
								),
								},
							});
						} else {
							compilation_tasks.push_back(driver::PackageCompilationTask{
								.root_module  = root_module,
								.build_target = driver::BuildTargetLLVMStaticLibrary{
									.output_file_name  = library_name,
									.archiving_options = getArchivingOptionsFromClah(options),
								},
							});
						}
						auto artifact_file = global_state::getRootCollection()
			                                     ->fileArtifactAtOrNew(library_name)
			                                     .file;
						libraries_to_link.push_back(artifact_file.getFilePath());
					}

					CORE_ASSERT(main_root_module.has_value(), "First package is not registered");

					if (dvm_backend) {
						auto output_file_name = options.getValue<std::string>("output-file-name")
			                                        .copyValueOr("package_dvm.dbc");
						auto linking_options = getLinkingOptionsFromClah(options);
						base::appendToVector(linking_options.dvm_link_libraries, libraries_to_link);
						compilation_tasks.push_back(driver::PackageCompilationTask{
							.root_module  = main_root_module.value(),
							.build_target = driver::BuildTargetDVMExecutable{
								.output_file_name  = base::StrID(output_file_name),
								.dvm_linking_options    = driver::constructDVMLinkingOptions(
									getLinkingOptionsFromClah(options), stdlib_options, false
								),
							},
						});
					} else {
						auto output_file_name = options.getValue<std::string>("output-file-name")
			                                        .copyValueOr("package_llvm.exe");
						auto linking_options = getLinkingOptionsFromClah(options);
						linking_options.native_additional_link_options = base::strConcat(
							linking_options.native_additional_link_options.copyValueOr(""),
							libraries_to_link | std::views::transform([](fs::FilePath& path) {
								return path.native();
							}) | base::rangesIntersperse(std::string(", "))
								| std::views::join | std::ranges::to<std::string>()
						);
						compilation_tasks.push_back(driver::PackageCompilationTask{
							.root_module  = main_root_module.value(),
							.build_target = driver::BuildTargetLLVMExecutable{
								.output_file_name = base::StrID(output_file_name),
								.linking_options  = driver::constructNativeLinkerOptions(
									linking_options, stdlib_options
								),
							},
						});
					}

					base::OkBad result = driver::compilePackages(compilation_tasks);

					compiler::driver::exit();

					return result.isOk() ? 0 : 1;
				})
		)
	    .addSubcommand(getClahForCompilePackage(
			"compile_package", "Compile given package into a binary.", false
		))
	    .addSubcommand(getClahForCompilePackage(
			"experimental_compile_package_dump_graph",
			"[Experimental] Like compile_package, and also write the query graph before and after "
			"its optimization as JSON.",
			true
		))
	    .addSubcommand(
			clah::Clah("compile_packages", "Compile package(s) described by a JSON manifest.")
				.addPositional(clah::FileParser::make("manifest"))
				.add(getLlvmOptLevelParam())
				.add(getClahStdLibOptions())
				.addCustomVerification(verifyStdLibOptions)
				.add(clah::ParamBuilder::ofValue(clah::FilePathParser::make("filepath"))
	                     .addShortName('a')
	                     .addLongName("artifact-location")
	                     .addShortDesc("Path to the top-level folder with build artifacts")
	                     .optional()
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("print-statistics")
	                     .addShortDesc("Print execution time statistics.")
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("print-graph")
	                     .addShortDesc("Print the query graph after the compilation.")
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
					auto manifest_file = options.getPositional<fs::File>(0);
					auto worker_count  = options.getValue<i64>("workers").copyValueOr(1);

					auto manifest_content = manifest_file.getContent();
					auto file_content     = manifest_content.view();

					nlohmann::json manifest_json;
					try {
						manifest_json = nlohmann::json::parse(file_content.stringView());
					} catch (const nlohmann::json::parse_error& e) {
						CORE_USER_LOG(base::strConcat(
							"Error: failed to parse manifest JSON: ", e.what(), "\n"
						));
						compiler::driver::exit();
						return 1;
					}

					auto report   = compiler::driver::diagnostics::makeGlobalLoggerReporter();
					auto manifest = compiler::driver::PackageCompilationManifest::fromJson(
						manifest_json, report
					);

					if (!manifest.has_value()) {
						compiler::driver::exit();
						return 1;
					}

					std::ignore         = manifest->verify(report);
					auto stdlib_options = getStdLibOptionsFromClah(options);
					auto init_result = compiler::driver::initializeTheCompiler(
						compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
							.packages_info = manifest->packages,
							.compilation_artifacts = {
								.artifacts_path =
									options.getValue<fs::FilePath>("artifact-location").copyValueOr("./duck_build/"),
							},
							.backend_options   = getBackendOptionsFromClah(options),
							.debug_options     = debug_options::getDebugOptionsFromClah(options),
							.incremental       = { .enabled = !options.isFlag("no-incremental") },
							.execution_options = {
								.worker_count = base::safeIntConv<u64>(worker_count),
							},
							.stdlib_options = stdlib_options,
						}
					);

					if (init_result.status().isBad()) {
						compiler::driver::exit();
						return 1;
					}

					auto std_compilation_result = compiler::driver::compilePackages(
						compiler::driver::getRequiredStdLibCompilationTasks()
					);
					if (std_compilation_result.isBad()) {
						compiler::driver::exit();
						return 1;
					}

					std::vector<compiler::driver::PackageCompilationTask> compilation_tasks;
					compilation_tasks.reserve(manifest->tasks.size());
					for (const auto& raw_task: manifest->tasks) {
						auto converted = compiler::driver::convertRawTaskToTask(
							raw_task, stdlib_options, report
						);
						if (!converted.has_value()) {
							compiler::driver::exit();
							return 1;
						}
						variant_match(converted->task_data) {
							variant_case(compiler::driver::PackageCompilationTask, package_task) {
								compilation_tasks.push_back(package_task);
							}
							variant_default { CORE_PANIC("Unsupported task type in manifest"); }
						}
					}

					time_stats::TrackCategoryTime total_compilation_time(
						time_stats::TimeCategories::TotalCompilationTime
					);

					CORE_ASSERT(!global_state::getPackages().empty(), "No packages registered");
					base::OkBad result = compiler::driver::compilePackages(compilation_tasks);

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
			clah::Clah("compile_script", "Compile a .dks script file into a .dbc or executable.")
				.addPositional(clah::FileParser::make("script"))
				.add(getLlvmOptLevelParam())
				.add(getClahLinkingOptions())
				.add(getClahStdLibOptions())
				.addCustomVerification(verifyStdLibOptions)
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

					auto stdlib_options = getStdLibOptionsFromClah(options);
					auto mode = compiler::driver::CompilerModeOfOperationAndOptions::ScriptMode{
						.script_file     = script_file,
						.backend_options = getBackendOptionsFromClah(options),
						.compilation_artifacts = {
							.artifacts_path = options.getValue<fs::FilePath>("artifact-location")
							                      .copyValueOr("./duck_build/"),
						},
						.debug_options     = debug_options::getDebugOptionsFromClah(options),
						.execution_options = {
							.worker_count = base::safeIntConv<u64>(worker_count),
						},
						.stdlib_options = stdlib_options
					};

					auto init_result = compiler::driver::initializeTheCompiler(mode);
					if (init_result.status().isBad()) {
						compiler::driver::exit();
						return 1;
					}

					auto std_compilation_result = compiler::driver::compilePackages(
						compiler::driver::getRequiredStdLibCompilationTasks()
					);
					if (std_compilation_result.isBad()) {
						compiler::driver::exit();
						return 1;
					}

					auto result = driver::compileScript(
						backend_type, stdlib_options, getLinkingOptionsFromClah(options)
					);

					compiler::driver::exit();
					return result.isOk() ? 0 : 1;
				})
		)
	    // Scripts can only be "run" on DVM for now, since compiling with LLVM would produce
	    // artifacts. To compile to native executable, the compile_script command can be used.
	    // This may change in the future.
	    .addSubcommand(clah::Clah("run", "Compile a .dks script file and run it on DVM.")
	                       .addPositional(clah::FileParser::make("script"))
	                       .add(clah::ParamBuilder::ofValue(clah::IntParser::make("worker count"))
	                                .addShortName('w')
	                                .addLongName("workers")
	                                .addShortDesc("Worker count.")
	                                .optional()
	                                .build())
	                       .add(getClahStdLibOptions())
	                       .addCustomVerification(verifyStdLibOptions)
	                       .setHandler([](const clah::ParsingResult& options) -> int {
							   using namespace compiler;

							   auto script_file  = options.getPositional<fs::File>(0);
							   auto worker_count = options.getValue<i64>("workers").copyValueOr(1);
							   // duckc run doesn't produce any artifacts for now, but this may be
		                       // changed later by for example adding option to save compiled
		                       // bytecode. Also, ScriptMode requires artifacts path, maybe this
		                       // will be refactored later.
							   auto run_temp_artifacts_path
								   = fs::FileManager::createRandomTempDirectory().getFilePath();

							   auto stdlib_options = getStdLibOptionsFromClah(options);
							   auto mode = compiler::driver::CompilerModeOfOperationAndOptions::ScriptMode{
						.script_file     = script_file,
						.backend_options = {}, // only dvm for now.
						.compilation_artifacts = {
							.artifacts_path = run_temp_artifacts_path,
						},
						.debug_options     = debug_options::getDebugOptionsFromClah(options),
						.execution_options = {
							.worker_count = base::safeIntConv<u64>(worker_count),
						},
						.stdlib_options = stdlib_options,
					};

							   auto init_result = compiler::driver::initializeTheCompiler(mode);
							   if (init_result.status().isBad()) {
								   compiler::driver::exit();
								   return 1;
							   }

							   {
								   // `run` shares its stdout with the script being executed, so
			                       // build progress (standard library compilation, archiving, ...)
			                       // must not be printed there for now.
								   auto prev_user_logs      = logger::enable_user_logs;
								   logger::enable_user_logs = false;
								   defer(logger::enable_user_logs = prev_user_logs);
								   auto std_compilation_result = compiler::driver::compilePackages(
									   compiler::driver::getRequiredStdLibCompilationTasks()
								   );
								   if (std_compilation_result.isBad()) {
									   compiler::driver::exit();
									   return 1;
								   }
							   }

							   auto run_result = driver::runScriptOnDVM(stdlib_options.stdActive());

							   compiler::driver::exit();
							   if (!run_result.has_value()) {
								   std::cerr << "Error: " << run_result.error() << "\n";
								   return 1;
							   }
							   return run_result->exit_code;
						   }))
	    .addSubcommand(
			clah::Clah("repl", "Start an interactive REPL session")
				.add(getClahStdLibOptions())
				.addCustomVerification(verifyStdLibOptions)
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("no-completions")
	                     .addShortDesc("Disable REPL autocompletions and hints.")
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("disable-bracketed-paste")
	                     .addShortDesc("Disable bracketed paste in REPL.")
	                     .build())
				.setDefaultValueParser(clah::FileParser::make("script")
	            )  // for optional script path.
				.add(clah::ParamBuilder::ofValue(clah::IntParser::make("count"))
	                     .addShortName('n')
	                     .addLongName("history-entries")
	                     .addShortDesc("Replay first N entries from session history.")
	                     .optional()
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("silent")
	                     .addShortDesc("Replay history without output (internal).")
	                     .build())
				.add(clah::ParamBuilder::ofFlag()
	                     .addLongName("plain-output")
	                     .addShortDesc("Print REPL results without interactive decorations.")
	                     .build())
				.setHandler([](const clah::ParsingResult& options) -> int {
					auto stdlib_opts = getStdLibOptionsFromClah(options);
					auto init_result = compiler::driver::initializeTheCompiler(
								   compiler::driver::CompilerModeOfOperationAndOptions::ReplMode{
									   .debug_options = debug_options::getDebugOptionsFromClah(options),
									   .execution_options = {
										   .worker_count = 1,
									   },
									   .stdlib_options = stdlib_opts
								   }
							   );
					if (init_result.status().isBad()) {
						compiler::driver::exit();
						return 1;
					}
					bool completions = compiler::repl::FRONTEND_DEFAULT_COMPLETIONS_ENABLED;
					if (options.isFlag("no-completions")) completions = false;

					bool bracketed = compiler::repl::FRONTEND_DEFAULT_BRACKETED_PASTE_ENABLED;
					bool decorative_output = !options.isFlag("plain-output");

					// Bracketed paste emits terminal-control sequences, which would violate plain
		            // output.
					if (options.isFlag("disable-bracketed-paste") || !decorative_output)
						bracketed = false;

					base::Optional<usize>      reset_replay_count;
					bool                       reset_replay_silent = false;
					compiler::repl::ReplResult repl_result = compiler::repl::ReplResult::success();
					{
						compiler::repl::ReplSession session(
							completions, bracketed, decorative_output
						);
						if (stdlib_opts.stdActive()) session.preloadStandardLibrary();

						auto replay_count_opt = options.getValue<i64>("history-entries");
						i64  replay_count     = replay_count_opt.copyValueOr(0);
						bool replay_silent    = options.isFlag("silent");
						if (replay_count_opt && replay_count < 0) {
							std::cerr << "Error: history replay count must be non-negative.\n";
							compiler::driver::exit();
							return 1;
						}
						if (replay_count > 0)
							session.replayHistoryEntries(
								static_cast<usize>(replay_count), replay_silent
							);
						if (options.getExtraParameterCount() > 1) {
							std::cerr << "Error: repl accepts at most one script path. "
										 "Usage: duckc repl [script.dks]\n";
							compiler::driver::exit();
							return 1;
						}

						if (options.getExtraParameterCount() == 1) {
							// Preload mode currently treats load failure as fatal: if the
				            // script fails to load/compile, we print the error and exit
				            // before entering the interactive REPL loop.
							auto script_file = options.getExtra<fs::File>(0).value();
							auto load_result
								= session.loadScriptFile(script_file.getFilePath().string());
							if (load_result.status == compiler::repl::ReplResult::Status::Error) {
								std::cerr << load_result.message << "\n";
								compiler::driver::exit();
								return 1;
							}
						}
						repl_result         = session.run(replay_count_opt.has_value());
						reset_replay_count  = session.getResetReplayCount();
						reset_replay_silent = session.getResetReplaySilent();
					}
					compiler::driver::exit();
					if (repl_result.status == compiler::repl::ReplResult::Status::Reset) {
						// We need to flush stdout before execSelf to avoid losing any buffered output.
						std::cout.flush();
						setReplRestartArgs(reset_replay_count.copyValueOr(0), reset_replay_silent);
						auto exec_result = os_utils::execSelf(g_argv);
						if (exec_result.status == os_utils::ExecSelfStatus::Error) return 1;
						return exec_result.exit_code;
					}
					return 0;
				})
		)
	    .addSubcommand(
			clah::Clah("dummy", "Dummy command (cli testing command).")
				.setHandler([](const clah::ParsingResult& options) -> int {
					std::ignore
						= compiler::driver::initializeTheCompiler(
							  compiler::driver::CompilerModeOfOperationAndOptions::BareMode{
								  .debug_options = debug_options::getDebugOptionsFromClah(options),
							  }
						)
		                      .status();
					return 0;
				})
		);
}

int main(int argc, const char* argv[]) {
	g_argv.clear();
	g_argv.reserve(static_cast<usize>(argc));
	for (int i = 0; i < argc; ++i) g_argv.emplace_back(argv[i]);

	init::InitObject _;

	compiler::driver::initializeGlobalLogger();
	auto clah = getClahForMain();

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
