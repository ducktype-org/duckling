#include "initialize.hpp"

#include <global_state/artifacts_location.hpp>
#include <global_state/options.hpp>
#include <global_state/packages.hpp>
#include <linker/link.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <artifacts/artifacts.hpp>
#include <diagnostic/logger.hpp>
#include <lexer/lexer_class.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>

namespace compiler::driver {

	namespace {
		constinit bool is_initialized = false;

		void handleDebugOptions(const options_types::DebugOptions& debug_options) {
			dia::Logger::setImmediatelyDump(debug_options.logger_cerr);
			lexer::Lexer::setTokenMessages(debug_options.lexer_cerr);
			global_state::getDynamicDebugOptions()->llvm_dump_ir  = debug_options.dump_llvm_ir;
			global_state::getDynamicDebugOptions()->llvm_dump_asm = debug_options.dump_llvm_asm;
		}

		void handleArtifactsOptions(const options_types::ArtifactsOptions& artifacts_options) {
			auto path = artifacts_options.artifacts_path;
			if (not path.exists()) {
				if (path.isPhysical()) {
					auto file = fs::FileManager::createPhysicalFolder(path);
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
			// Add package name and path to global state
			global_state::setters::addPackage(
				package_info.package_name, fs::FilePath(package_info.package_path)
			);
		}

		void loadPreviousQueryGraphIfExists() {
			auto root            = global_state::getRootCollection();
			auto maybe_query_col = root->subCollectionAtMaybe(base::StrID("query"));

			if (maybe_query_col.has_value()) {
				auto query_col  = maybe_query_col.value();
				auto maybe_blob = query_col->blobArtifactAtMaybe(base::StrID("query_graph"));
				if (maybe_blob.has_value()) {
					auto                  view = maybe_blob.value()->getDataView();
					std::span<const byte> span(view.getBegin(), view.size());
					auto                  graph = query::internal::QueryGraph::deserialize(span);
					query::internal::ContextAccess::getState()->setPreviousGraph(std::move(graph));
				}
			}
		}
	}

	void resetInitializationForTests() {
		using query::internal::ContextAccess;
		using query::internal::QueryState;

		// Clear root collection and packages so setRootCollection can be called again
		global_state::setters::clearRootCollectionForTests();
		global_state::setters::clearPackagesForTests();

		// Reset main query state in-place
		QueryState* state_raw = &*ContextAccess::getState();
		state_raw->~QueryState();
		new (state_raw) QueryState();

		// Reset initialization flag so initializeTheCompiler can run again
		is_initialized = false;
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
				loadPreviousQueryGraphIfExists();
			}
			variant_default { CORE_PANIC("Unknown compiler mode of operation"); }
		}
	}
}
