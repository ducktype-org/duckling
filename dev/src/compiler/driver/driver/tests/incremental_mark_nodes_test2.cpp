#include "incremental_metadata_test_common.hpp"
#include "test_utils.hpp"

#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/artifacts_location.hpp>
#include <global_state/backend_options.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <tester/tester.hpp>

#include <filesystem>

using namespace compiler;

namespace {
	const char* k_artifacts_dir = "incremental_mark_nodes_artifacts";
}

class IncrementalMarkNodesTest2 final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS IncrementalMarkNodesTest2

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(reinitAndVerifyGreen); }

private:
	void reinitAndVerifyGreen() {
		// Place artifacts under the build directory (CTest working dir)
		fs::FilePath artifacts_path
			= fs::FilePath(std::filesystem::current_path() / k_artifacts_dir);

		// Re-initialize compiler which will load the previous graph from artifacts
		auto init_result = compiler::driver::initializeTheCompiler(
            compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.packages_info = { driver_test_utils::emptyRawPackageInfo(
					"mark_nodes_test_package",
					path("modules/incremental/org_functions/functions_1")
				) },
                .compilation_artifacts = {.artifacts_path = artifacts_path},
            	.backend_options = {
					.llvm_backend = global_state::BackendOptions::LLVMBackend{},
				},
				.debug_options         = {},
				.incremental           = { .enabled = true },
				.execution_options     = { .worker_count = 1 },
				.stdlib_options		= { },
            }
        );

		ASSERT_TRUE(init_result.status().isOk());

		// After initialization the previous graph (if present) should be loaded
		auto prev_opt = query::internal::ContextAccess::getState()->getPreviousGraph();
		ASSERT_HAS_VALUE(prev_opt, "Previous graph should be present after initialization");
		auto prev = prev_opt.value();

		// Verify node colors: previously-leaf nodes are green and dependency count checks hold
		// This also test that non-existing ChildSideInput that submodules depend on is marked green
		auto prev_colors = query::internal::ContextAccess::getState()->getPreviousNodeColors();
		ASSERT_TRUE(
			prev_colors->size() != 0
		);  // If there are no nodes, there's nothing to mark green, so test is not valid

		for (const auto& node: prev->getAllNodes()) {
			if (prev_colors->contains(node)) {
				ASSERT_TRUE(prev->getNodeDeps(node).size() == 1);
				ASSERT_TRUE(
					*prev_colors->atMaybe(node).value()
					== query::internal::QueryState::PrevColor::Green
				);
			} else {
				ASSERT_TRUE(
					!node.q_id.getData().usesStableHashing() || prev->getNodeDeps(node).size() > 1
				);
			}
		}

		// Probably because of linker optimizations the INTERNAL_QUERY_IMPLEMENTATION_BOILERPLATE
		// won't initialise without actually running a query

		// Compile the module again to trigger loadFromDisk and use the previous graph
		auto module = frontend::createModuleTree(
			fs::File(path("modules/incremental/org_functions/functions_1")),
			base::StrID("mark_nodes_test_package")
		);

		// Build a NodeID for the CompileModule query with the exact key we used
		compiler::driver::KeyOf_CompileModule key{
			.module_id        = module,
			.backend_type     = compiler::driver::BackendType::LLVM,
			.build_debug_info = false,
		};
		query::internal::NodeID root_node{
			compiler::driver::CompileModule::getID(),
			query::internal::KeyHash{ key.queryStablePerfectHash() },
		};

		// Capture dependencies before the graph is merged (merge now consumes prev graph entries)
		auto root_deps = prev->getNodeDeps(root_node);

		// The previous compilation compiled two modules (functions_1 + submodule), so two .o files
		// are on disk. This compilation only demands functions_1's CompileModule, so submodule's is
		// an orphan and saveArtifacts() (via driver::exit() below) must reclaim its .o from disk.
		auto count_object_files = [&] {
			const auto query_dir
				= artifacts_path.getPath() / "query"
			    / ("query" + std::to_string(driver::CompileModule::getID().asInt()));
			u64 count = 0;
			if (std::filesystem::exists(query_dir))
				for (const auto& entry: std::filesystem::directory_iterator(query_dir))
					if (entry.path().extension() == ".o") ++count;
			return count;
		};
		const u64 objects_before = count_object_files();
		ASSERT_TRUE(
			objects_before >= 2
		);  // functions_1 + undemanded submodule from previous compile

		query::utils::withContextDo([&](query::Context& ctx) {
			ctx.query<driver::CompileModule>({ .module_id        = module,
			                                   .backend_type     = driver::BackendType::LLVM,
			                                   .build_debug_info = false });

			// Trigger metadata merge by calling the same queries
			ctx.query<MetadataPersistenceTestQuery>({ 42 });
			ctx.query<MetadataPersistenceTestQuery>({ 100 });
		});

		// ========== Verify metadata persisted from previous compilation ==========
		{
			using namespace metadata_persistence_test;
			const auto& state = *query::internal::ContextAccess::getState();

			// Build NodeIDs for our test queries
			auto node_42  = query::internal::makeNodeID<MetadataPersistenceTestQuery>({ 42 });
			auto node_100 = query::internal::makeNodeID<MetadataPersistenceTestQuery>({ 100 });

			// Verify TestCounter metadata
			auto counter_42 = state.getMetadata<metadata_TestCounter>(node_42);
			ASSERT_EQUAL(counter_42.size(), 1);
			ASSERT_EQUAL(counter_42[0]->value, u64{ 420 });  // 42 * 10

			auto counter_100 = state.getMetadata<metadata_TestCounter>(node_100);
			ASSERT_EQUAL(counter_100.size(), 1);
			ASSERT_EQUAL(counter_100[0]->value, u64{ 1'000 });  // 100 * 10

			// Verify TestSourceFile (StrID) metadata
			auto source_42 = state.getMetadata<metadata_TestSourceFile>(node_42);
			ASSERT_EQUAL(source_42.size(), 1);
			ASSERT_EQUAL(source_42[0]->value.strView(), std::string_view{ "test/source_42.duck" });

			auto source_100 = state.getMetadata<metadata_TestSourceFile>(node_100);
			ASSERT_EQUAL(source_100.size(), 1);
			ASSERT_EQUAL(source_100[0]->value.strView(), std::string_view{ "test/source_100.duck" });
		}

		// Check if red-green sweep marks all direct dependencies of the root node as green
		for (const auto& dep_node: root_deps) {
			if (!prev_colors->contains(dep_node))
				std::cout << "Node " << dep_node.q_id.getData().name << " missing in prev_colors\n";
			ASSERT_TRUE(prev_colors->contains(dep_node));
			ASSERT_TRUE(
				*prev_colors->atMaybe(dep_node).value()
				== query::internal::QueryState::PrevColor::Green
			);
		}

		// Verify that CompileModule artifact exists and is non-empty on disk
		// If name logic changes in implementation, update this test accordingly
		auto stable = key.queryStablePerfectHash().toStringHex();
		auto fname  = stable + std::string(".o");

		// Reconstruct the artifacts collection path used by CompileModule
		auto root      = global_state::getRootCollection();
		auto queries   = root->subCollectionAtOrNew(base::StrID("query"));
		auto query_col = queries->subCollectionAtOrNew(base::StrID(
			base::strConcat("query", compiler::driver::CompileModule::getID().asInt()).c_str()
		));

		auto maybe_art = query_col->fileArtifactAtMaybe(base::StrID(fname.c_str()));
		ASSERT_HAS_VALUE(maybe_art);

		const auto& art  = *maybe_art.value();
		auto        path = art.file.getFilePath().getPath();
		ASSERT_TRUE(std::filesystem::exists(path));
		ASSERT_TRUE(std::filesystem::file_size(path) > 0);

		// Save artifacts (writes previous graph blob to artifacts)
		driver::exit();

		// submodule's CompileModule was not demanded this run, so its orphaned .o must have been
		// reclaimed from disk during saveArtifacts().
		const u64 objects_after = count_object_files();
		ASSERT_TRUE(objects_after == objects_before - 1);
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
