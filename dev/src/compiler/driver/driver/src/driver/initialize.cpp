#include "initialize.hpp"

#include "options.hpp"

#include <concurrent/module_flags/worker_count.hpp>
#include <diagnostic_interactive/logger.hpp>
#include <diagnostic_interactive/module_flags/module_flags.hpp>
#include <diagnostic_interactive/placeholder.hpp>
#include <driver/incremental_utils/collect_input.hpp>
#include <driver/module_flags/module_flags.hpp>
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
#include <lexer/lexer_class.hpp>
#include <logger/logger.hpp>
#include <query_framework/external/api.hpp>
#include <query_framework/module_flags/module_flags.hpp>

namespace compiler::driver {


	namespace {
		constinit bool is_initialized = false;

		void handleLoggerInitialization() {
			// We might want to configure it differently in the future:
			dia_int::configureImmediatePrint(&std::cerr);
			dia_int::configureTerminalPrinterColors(true);

			global_state::setters::setGlobalLogger(makeBox<dia_int::Logger>());
		}

		void handleDebugOptions(const options_types::DebugOptions& debug_options) {
			if (not debug_options.dev_log_categories.empty()) logger::enable_dev_logs = true;

			for (const auto& category_name: debug_options.dev_log_categories)
				logger::enableDevCategoryByStringName(category_name);

			driver::llvm_dump_ir  = debug_options.dump_llvm_ir;
			driver::llvm_dump_asm = debug_options.dump_llvm_asm;
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

		base::OkBad handlePackageOptions(const options_types::PackageInfo& package_info) {
			// Create the module tree for the main package and add it to global state
			auto root_module = compiler::frontend::createModuleTree(
				package_info.package_path, package_info.package_name
			);
			if (!getModuleRef(root_module)->hasMainSourceFile()) {
				auto module_name = getModuleRef(root_module)->getName();
				global_state::getGlobalLogger()->log(makeBox<dia_int::PlaceholderHeaderError>(
					"Main package does not have a main source file.",
					base::strConcat(
						"The main source file is required for compilation. Please add a ",
						module_name,
						".dmf file to the main module directory."
					)
				));
				return base::BAD;
			}
			global_state::setters::addMainPackage(root_module);
			return base::OK;
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
				for (auto mid: global_state::getPackages())
					compiler::frontend::parseAllFilesInModuleTree(mid.root_module);

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
				enable_incremental_compilation = true;
				loadPreviousQueryGraphIfExists();
			} else {
				enable_incremental_compilation = false;
				// Disable the query graph, cos its not needed and adds overhead.
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
				handleLoggerInitialization();
				handleDebugOptions(bare_options.debug_options);
			}
			variant_case(
				CompilerModeOfOperationAndOptions::PackageCompilationMode,
				package_compilation_options
			) {
				// Be careful to not abort this function
				// in places where the compiler is left in a state
				// that could result in panics/errors during exit.

				handleLoggerInitialization();

				handleDebugOptions(package_compilation_options.debug_options);
				handleExecutionOptions(package_compilation_options.execution_options);
				handleArtifactsOptions(package_compilation_options.compilation_artifacts);

				auto package_success
					= handlePackageOptions(package_compilation_options.main_package_info);

				if (package_success.isBad()) return base::BAD;

				handleBackendOptions(package_compilation_options.backend_options);
				handleIncrementalOptions(package_compilation_options.incremental);
			}
			variant_case(CompilerModeOfOperationAndOptions::ReplMode, repl_options) {
				handleLoggerInitialization();
				handleDebugOptions(repl_options.debug_options);
				handleExecutionOptions(repl_options.execution_options);
			}
			variant_case(CompilerModeOfOperationAndOptions::ScriptMode, script_options) {
				handleLoggerInitialization();
				handleDebugOptions(script_options.debug_options);
				handleExecutionOptions(script_options.execution_options);
				handleArtifactsOptions(script_options.compilation_artifacts);
				handleBackendOptions(script_options.backend_options);
				handleScriptContext(script_options.script_file);
			}
			variant_default { CORE_PANIC("Unknown compiler mode of operation"); }
		}
		return base::OK;
	}
}
