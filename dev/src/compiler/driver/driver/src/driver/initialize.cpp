#include "initialize.hpp"

#include <diagnostic_interactive/logger.hpp>
#include <driver/module_flags/module_flags.hpp>
#include <driver_private/collect_input.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/artifacts_location.hpp>
#include <global_state/packages.hpp>
#include <linker/link.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <artifacts/artifacts.hpp>
#include <lexer/lexer_class.hpp>
#include <logger/logger.hpp>
#include <query_framework/external/api.hpp>

namespace compiler::driver {

	namespace {
		constinit bool is_initialized = false;

		void handleDebugOptions(const options_types::DebugOptions& debug_options) {
			if (not debug_options.dev_log_categories.empty()) logger::enable_dev_logs = true;

			for (const auto& category_name: debug_options.dev_log_categories)
				logger::enableDevCategoryByStringName(category_name);

			dia_int::Logger::configureImmediatePrint(true, std::cerr);

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

		void handlePackageOptions(const options_types::PackageInfo& package_info) {
			// Create the module tree for the main package and add it to global state
			auto root_module = compiler::frontend::createModuleTree(
				package_info.package_path, package_info.package_name
			);
			global_state::setters::addMainPackage(root_module);
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
				auto query_col  = maybe_query_col.value();
				auto maybe_blob = query_col->blobArtifactAtMaybe(base::StrID("query_graph"));
				if (maybe_blob.has_value()) {
					auto                  view = maybe_blob.value()->getDataView();
					std::span<const byte> span(view.getBegin(), view.size());
					auto                  graph  = query::external::deserialize(span);
					auto                  inputs = collectAllPstElementHashesFromGlobalPackages();
					query::external::setPreviousGraph(std::move(graph), std::move(inputs));
				}
			}
		}

		void handleIncrementalOptions(const options_types::IncrementalOptions& inc_options) {
			if (inc_options.enabled) loadPreviousQueryGraphIfExists();
		}
	}

	void initializeTheCompiler(CompilerModeOfOperationAndOptions options) {
		CORE_ASSERT(
			init::wasInitObject(),
			"InitObject should be used before call to the initializeTheCompiler function!"
		);

		CORE_ASSERT(!is_initialized, "Compiler is already initialized!");
		is_initialized = true;

		variant_match(options.mode) {
			variant_case(CompilerModeOfOperationAndOptions::BareMode, options) {
				handleDebugOptions(options.debug_options);
			}
			variant_case(CompilerModeOfOperationAndOptions::PackageCompilationMode, options) {
				handleDebugOptions(options.debug_options);
				handleArtifactsOptions(options.compilation_artifacts);
				handlePackageOptions(options.main_package_info);
				handleIncrementalOptions(options.incremental);
			}
			variant_default { CORE_PANIC("Unknown compiler mode of operation"); }
		}
	}
}
