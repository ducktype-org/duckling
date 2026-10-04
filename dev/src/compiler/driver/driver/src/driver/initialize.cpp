#include "initialize.hpp"

#include "options.hpp"

#include <concurrent/module_flags/worker_count.hpp>
#include <driver/diagnostics/log_helpers.hpp>
#include <driver/incremental_utils/collect_input.hpp>
#include <driver/module_flags/module_flags.hpp>
#include <driver_private/standard_library/standard_library.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/artifacts_location.hpp>
#include <global_state/backend_options.hpp>
#include <global_state/global_logger.hpp>
#include <global_state/packages.hpp>
#include <global_state/script_context.hpp>
#include <linker/link.hpp>
#include <time_stats/time_stats.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <artifacts/artifacts.hpp>
#include <diagnostic/logger.hpp>
#include <diagnostic/module_flags/module_flags.hpp>
#include <diagnostic/placeholder.hpp>
#include <lexer/lexer_class.hpp>
#include <logger/logger.hpp>
#include <query_framework/external/api.hpp>
#include <query_framework/module_flags/module_flags.hpp>

#include <iostream>

namespace compiler::driver {


	namespace {
		constinit bool is_initialized = false;

		void handleDebugOptions(const options_types::DebugOptions& debug_options) {
			if (not debug_options.dev_log_categories.empty()) logger::enable_dev_logs = true;

			for (const auto& category_name: debug_options.dev_log_categories)
				logger::enableDevCategoryByStringName(category_name);

			driver::dump_ir_options.dump_asm  = debug_options.dump_asm;
			driver::dump_ir_options.dump_llvm = debug_options.dump_llvm;
			driver::dump_ir_options.dump_dbc  = debug_options.dump_dbc;
			driver::dump_ir_options.dump_lir  = debug_options.dump_lir;
			driver::dump_ir_options.dump_mir  = debug_options.dump_mir;
			driver::dump_ir_options.dump_hir  = debug_options.dump_hir;

			driver::print_ir_options.print_dbc = debug_options.print_dbc;
			driver::print_ir_options.print_lir = debug_options.print_lir;
			driver::print_ir_options.print_mir = debug_options.print_mir;
			driver::print_ir_options.print_hir = debug_options.print_hir;
		}

		void handleArtifactsOptions(const options_types::ArtifactsOptions& artifacts_options) {
			auto path = artifacts_options.artifacts_path;
			if (not path.exists()) {
				if (path.isPhysical() || path.isRelative()) {
					auto file = fs::FileManager::createPhysicalFolder(path.absolute());
					CORE_ASSERT(
						file.exists(), "Failed to create artifacts folder: " + path.string()
					);
				} else if (path.isTemporary()) {
					auto file = fs::FileManager::createTempFolder(path);
					CORE_ASSERT(
						file.exists(), "Failed to create artifacts folder: " + path.string()
					);
				} else {
					throw base::LogicError(
						"Artifacts path must be either physical or temporary, but got: "
						+ path.string()
					);
				}
			}
			global_state::setters::setRootCollection(
				makeBox<artifacts::ArtifactCollection>(artifacts_options.artifacts_path.getPath())
			);
		}

		base::OkBad handlePackageOptions(
			std::vector<compiler::frontend::packages::RawPackageInfo>& packages_info,
			const options_types::StdLibOptions&                        stdlib_options
		) {
			auto report      = diagnostics::makeGlobalLoggerReporter();
			bool had_failure = false;

			compiler::frontend::packages::filterUndeclaredDependencies(packages_info, report);

			// Adding standard library packages
			if (auto path = resolveStdPath(stdlib_options)) {
				if (addStandardLibraryPackages(packages_info, *path, report).isBad())
					return base::BAD;
			}

			for (const auto& package_info: packages_info) {
				auto pkg = compiler::frontend::packages::createPackageInfo(
					package_info, packages_info, report
				);
				if (pkg)
					global_state::setters::addPackage(*pkg);
				else
					had_failure = true;
			}

			if_opt_some(stdlib_options.std_artifacts_path, path) {
				if (not path.exists()) {
					if (path.isPhysical() || path.isRelative()) {
						auto file = fs::FileManager::createPhysicalFolder(path.absolute());
						CORE_ASSERT(
							file.exists(), "Failed to create artifacts folder: " + path.string()
						);
					} else if (path.isTemporary()) {
						auto file = fs::FileManager::createTempFolder(path);
						CORE_ASSERT(
							file.exists(), "Failed to create artifacts folder: " + path.string()
						);
					} else {
						throw base::LogicError(
							"Artifacts path must be either physical or temporary, but got: "
							+ path.string()
						);
					}
				}
				global_state::setters::setCustomStdArtifactsCollection(
					makeBox<artifacts::ArtifactCollection>(path)
				);
			}

			return had_failure ? base::BAD : base::OK;
		}

		/**
		 * Checks if a previous query graph exists in Artifacts,
		 * and if so, loads it into the query framework for incremental compilation.
		 * This function is using query framework external API.
		 */
		void loadPreviousQueryGraphIfExists() {
			auto root            = global_state::getRootCollection();
			auto maybe_query_col = root->subCollectionAtMaybe(base::StrID("query"));

			if (maybe_query_col.has_value()) {
				// Get the "query" collection from artifacts, which should contain the previous
				// query graph and metadata.
				auto query_col = maybe_query_col.value();

				// Load previous query graph
				auto maybe_blob = query_col->blobArtifactAtMaybe(base::StrID("query_graph"));
				if (maybe_blob.has_value()) {
					auto                  view = maybe_blob.value()->getDataView();
					std::span<const byte> span(view.getBegin(), view.size());
					query::external::setPreviousGraphFromRawBytes(span);
				} else {
					// No previous graph found. This can happen if this is the first compilation or
					// if artifacts from previous compilation were deleted. This is not an error.
					return;
				}

				// Load previous metadata (must be called after graph)
				auto maybe_metadata_blob
					= query_col->blobArtifactAtMaybe(base::StrID("query_metadata"));

				// Metadata should exist if graph exists, even if it's empty.
				CORE_ASSERT(
					maybe_metadata_blob.has_value(),
					"Previous query graph found but no metadata blob found in artifacts. "
					"This indicates a corrupted state in artifacts."
				);

				auto                  view = maybe_metadata_blob.value()->getDataView();
				std::span<const byte> span(view.getBegin(), view.size());
				query::external::setPreviousMetadataFromRawBytes(span);

				// We need to parse all files before compilation to collect all PST elements.
				for (const auto& package_info: global_state::getPackages())
					compiler::frontend::parseAllFilesInModuleTree(
						package_info.getRootModule().illegalAccess().getID()
					);

				// Collect all Inputs and Side inputs and perform red-green sweep.
				// This must be called after loading both the graph and metadata, as metadata
				// contains information about which nodes are inputs and their associated hashes.
				query::external::markPreviousGraphNodesInputs(
					collectInputDataFromGlobalPackagesFromPrevMetadata()
				);
			}
		}

		void handleIncrementalOptions(const options_types::IncrementalOptions& inc_options) {
			if (inc_options.enabled) {
				CORE_ASSERT(
					!global_state::getPackages().empty(),
					"Main package must be set before handling incremental compilation"
				);
				driver::enable_incremental_compilation = true;
				loadPreviousQueryGraphIfExists();
			} else {
				driver::enable_incremental_compilation = false;
				// Disable the query graph because it's not needed and adds overhead.
				query::enable_query_graph = false;
			}
		}

		void handleBackendOptions(const global_state::BackendOptions& backend_options) {
			global_state::setters::setBackendOptions(backend_options);
		}

		void handleExecutionOptions(const options_types::ExecutionOptions& execution_options) {
			concurrent::worker::setWorkerCount(execution_options.worker_count);
		}

		void handleScriptContext(const fs::File& script_file) {
			global_state::setters::setScriptContext(script_file);
		}
	}

	/**
	 * Creates a dummy "repl_session" package and root module
	 * This is a hack to make the import from different packages work in the REPL,
	 * as the module lookup relies on the global package registry.
	 * The module is otherwise unused.
	 * @TODO: #2762 probably remove this
	 */
	std::vector<compiler::frontend::packages::RawPackageInfo> getScriptStubPackage() {
		auto package_root_file
			= fs::FileManager::createRandomVirtualFile("", compiler::frontend::LANG_MODULE_FILE);
		std::vector<compiler::frontend::packages::RawPackageInfo> repl_packages_info{
			compiler::frontend::packages::RawPackageInfo{
				.package_id   = base::StrID("repl_session"),
				.package_name = base::StrID("repl_session"),
				.version      = base::StrID("0.1.0"),
				.package_path = package_root_file.getFilePath(),
				.features     = {},
				.dependencies = {},
			},
		};
		return repl_packages_info;
	}

	base::CheckedOkBad initializeTheCompiler(CompilerModeOfOperationAndOptions options) {
		time_stats::TrackCategoryTime driver_initialization_time(
			time_stats::TimeCategories::DriverInitialization
		);

		CORE_ASSERT(
			init::wasInitObject(),
			"InitObject should be used before call to the initializeTheCompiler function!"
		);

		CORE_ASSERT(!is_initialized, "Compiler is already initialized!");
		is_initialized = true;

		variant_match(options.mode) {
			variant_case(CompilerModeOfOperationAndOptions::BareMode, bare_options) {
				handleDebugOptions(bare_options.debug_options);
			}
			variant_case(
				CompilerModeOfOperationAndOptions::PackageCompilationMode,
				package_compilation_options
			) {
				// Be careful to not abort this function
				// in places where the compiler is left in a state
				// that could result in panics/errors during exit.

				handleDebugOptions(package_compilation_options.debug_options);
				handleExecutionOptions(package_compilation_options.execution_options);
				handleArtifactsOptions(package_compilation_options.compilation_artifacts);

				auto package_success = handlePackageOptions(
					package_compilation_options.packages_info,
					package_compilation_options.stdlib_options
				);

				if (package_success.isBad()) return base::BAD;

				handleBackendOptions(package_compilation_options.backend_options);
				handleIncrementalOptions(package_compilation_options.incremental);
			}
			variant_case(CompilerModeOfOperationAndOptions::ReplMode, repl_options) {
				auto repl_packages_info = getScriptStubPackage();

				auto package_success
					= handlePackageOptions(repl_packages_info, repl_options.stdlib_options);
				if (package_success.isBad()) return base::BAD;

				handleDebugOptions(repl_options.debug_options);
				handleExecutionOptions(repl_options.execution_options);
			}
			variant_case(CompilerModeOfOperationAndOptions::ScriptMode, script_options) {
				auto repl_packages_info = getScriptStubPackage();

				handleDebugOptions(script_options.debug_options);
				handleExecutionOptions(script_options.execution_options);
				handleArtifactsOptions(script_options.compilation_artifacts);
				auto package_success
					= handlePackageOptions(repl_packages_info, script_options.stdlib_options);
				if (package_success.isBad()) return base::BAD;
				handleBackendOptions(script_options.backend_options);
				handleScriptContext(script_options.script_file);
			}
			variant_default { CORE_PANIC("Unknown compiler mode of operation"); }
		}
		return base::OK;
	}

	void initializeGlobalLogger() {
		// We might want to configure it differently in the future:
		dia::configureImmediatePrint(&std::cerr);
		dia::configureTerminalPrinterColors(true);

		global_state::setters::setGlobalLogger(makeBox<dia::Logger>());
	}
}
