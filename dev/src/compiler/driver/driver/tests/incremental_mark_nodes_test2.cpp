#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/artifacts_location.hpp>
#include <global_state/options.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/utils/with_context_do.hpp>
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
		compiler::driver::initializeTheCompiler(
            compiler::options::CompilerModeOfOperationAndOptions::PackageCompilationMode{
                .main_package_info = {
                    .package_name = std::string("mark_nodes_test_package"),
                    .package_path = fs::FilePath(path("modules/functions_1")),
                },
                .compilation_artifacts = {.artifacts_path = artifacts_path},
            	.backend_options = {
					.llvm_backend = options::BackendOptions::LLVMBackend{},
				},
				.debug_options         = {},
				.incremental           = { .enabled = true }
            }
        );

		// After initialization the previous graph (if present) should be loaded
		auto prev_opt = query::internal::ContextAccess::getState()->getPreviousGraph();
		assertTrue(prev_opt.has_value(), "Previous graph should be present after initialization");
		auto prev = prev_opt.value();

		// Verify node colors: previously-leaf nodes are green and dependency count checks hold
		auto prev_colors = query::internal::ContextAccess::getState()->getPreviousNodeColors();
		ASSERT_TRUE(!prev_colors->empty());

		for (const auto& node: prev->getAllNodes()) {
			if (prev_colors->contains(node)) {
				ASSERT_TRUE(prev->getNodeDeps(node).size() == 1);
				ASSERT_TRUE(prev_colors->at(node) == query::internal::QueryState::PrevColor::Green);
			} else {
				ASSERT_TRUE(
					!node.q_id.getData().usesStableHashing() || prev->getNodeDeps(node).size() > 1
				);
			}
		}

		// Probably because of linker optimizations the INTERNAL_QUERY_IMPLEMENTATION_BOILERPLATE
		// won't initialise without actually running a query

		// Compile the module again to trigger loadFromDisc and use the previous graph
		auto module = frontend::createModuleTree(
			fs::File(path("modules/functions_1")), "mark_nodes_test_package"
		);

		// Build a NodeID for the CompileModule query with the exact key we used
		compiler::driver::KeyOf_CompileModule key{
			.module_id    = module,
			.backend_type = compiler::driver::BackendType::LLVM,
		};
		query::internal::NodeID root_node{
			compiler::driver::CompileModule::getID(),
			query::internal::KeyHash{ key.queryStablePerfectHash() },
		};

		// Capture dependencies before the graph is merged (merge now consumes prev graph entries)
		auto root_deps = prev->getNodeDeps(root_node);

		query::utils::withContextDo([&](query::Context& ctx) {
			(void) ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM });
		});

		// Check if red-green sweep marks all direct dependencies of the root node as green
		for (const auto& dep_node: root_deps) {
			if (!prev_colors->contains(dep_node))
				std::cout << "Node " << dep_node.q_id.getData().name << " missing in prev_colors\n";
			ASSERT_TRUE(prev_colors->contains(dep_node));
			ASSERT_TRUE(prev_colors->at(dep_node) == query::internal::QueryState::PrevColor::Green);
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
		ASSERT_TRUE(maybe_art.has_value());

		const auto& art  = *maybe_art.value();
		auto        path = art.FILE.getFilePath().getPath();
		ASSERT_TRUE(std::filesystem::exists(path));
		ASSERT_TRUE(std::filesystem::file_size(path) > 0);

		// Save artifacts (writes previous graph blob to artifacts)
		driver::exit();
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
