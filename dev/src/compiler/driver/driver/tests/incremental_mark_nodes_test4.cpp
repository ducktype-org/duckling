#include "incremental_metadata_test_common.hpp"  // IWYU pragma: keep
#include "test_utils.hpp"

#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/backend_options.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <tester/tester.hpp>

#include <filesystem>
#include <iostream>

using namespace compiler;

namespace {
	const char* k_artifacts_dir = "incremental_mark_nodes_artifacts";
}

class IncrementalMarkNodesTest4 final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS IncrementalMarkNodesTest4

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(initChangedFunctionsAndCountColors); }

private:
	void initChangedFunctionsAndCountColors() {
		// Place artifacts under the build directory (CTest working dir)
		fs::FilePath artifacts_path
			= fs::FilePath(std::filesystem::current_path() / k_artifacts_dir);

		// Initialize with changed functions path (same package name as previous step)
		auto init_result = compiler::driver::initializeTheCompiler(
            compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.packages_info = { driver_test_utils::emptyRawPackageInfo(
					"mark_nodes_test_package",
					path("modules/incremental/changed_functions_mistake/functions_1")
				) },
                .compilation_artifacts = {.artifacts_path = artifacts_path},
            	.backend_options = {
					.llvm_backend = global_state::BackendOptions::LLVMBackend{},
				},
				.debug_options         = {},
				.incremental           = { .enabled = true },
				.execution_options     = { .worker_count = 1 },
				.stdlib_options		= { },}
        );

		ASSERT_TRUE(init_result.status().isOk());

		auto prev_graph_opt = query::internal::ContextAccess::getState()->getPreviousGraph();
		ASSERT_HAS_VALUE(prev_graph_opt);
		auto prev = prev_graph_opt.value();

		auto prev_colors = query::internal::ContextAccess::getState()->getPreviousNodeColors();
		ASSERT_TRUE(
			prev_colors->size() != 0
		);  // If there are no nodes, there's nothing to mark green, so test is not valid

		u64 green_count = 0;
		u64 red_count   = 0;
		for (const auto& node: prev->getAllNodes()) {
			if (prev_colors->contains(node)) {
				if (*prev_colors->atMaybe(node).value()
				    == query::internal::QueryState::PrevColor::Green)
					green_count++;
				else if (*prev_colors->atMaybe(node).value()
				         == query::internal::QueryState::PrevColor::Red)
					red_count++;
				else
					ASSERT_TRUE(false);
			}
		}
		std::cerr << "Green nodes: " << green_count << ", Red nodes: " << red_count << '\n';
		ASSERT_TRUE(green_count > 0);
		// functions_1/functions_1.dmf there is a change in variable name a -> c in function main()
		// this should result in only one red node in the previous graph
		ASSERT_TRUE(red_count == 1);


		// Check if red green sweep marks only one node as red

		// Compile module to trigger red-green sweep
		auto module = frontend::createModuleTree(
			fs::File(path("modules/incremental/changed_functions_mistake/functions_1")),
			base::StrID("mark_nodes_test_package")
		);


		// Check the location of .o object in artifacts before compilation, it should be present
		// because of previous compilation step
		compiler::driver::KeyOf_CompileModule key{ .module_id = module,
			                                       .backend_type
			                                       = compiler::driver::BackendType::LLVM,
			                                       .build_debug_info = false };

		auto output_name = key.queryStablePerfectHash().toStringHex() + ".o";

		auto collection
			= global_state::getRootCollection()
		          ->subCollectionAtOrNew(base::StrID("query"))
		          ->subCollectionAtOrNew(base::StrID(
					  base::strConcat("query", compiler::driver::CompileModule::getID().asInt())
						  .c_str()
				  ));
		auto output_maybe = collection->fileArtifactAtMaybe(base::StrID(output_name.c_str()));

		// Validate that .o file from previous compilation is present before we run the compilation
		// with changed source code
		assertTrue(
			output_maybe.has_value(), "Output file should be present in artifacts before compilation"
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			(void) ctx.query<driver::CompileModule>(key);
		});

		query::internal::NodeID root_node{
			compiler::driver::CompileModule::getID(),
			query::internal::KeyHash{ key.queryStablePerfectHash() },
		};

		// Check if red-green sweep marks all direct dependencies of the root node as green
		int red_dep_count   = 0;
		int green_dep_count = 0;
		for (const auto& dep_node: prev->getNodeDeps(root_node)) {
			if (!prev_colors->contains(dep_node))
				std::cout << "Node " << dep_node.q_id.getData().name << " missing in prev_colors\n";
			ASSERT_TRUE(prev_colors->contains(dep_node));
			if (*prev_colors->atMaybe(dep_node).value()
			    == query::internal::QueryState::PrevColor::Red)
				red_dep_count++;
			else
				green_dep_count++;
		}
		std::cout << "Red direct dependencies of root node: " << red_dep_count << "\n";
		std::cout << "Green direct dependencies of root node: " << green_dep_count << "\n";
		ASSERT_TRUE(red_dep_count > 1);
		ASSERT_TRUE(green_dep_count > red_dep_count);

		// Save artifacts (writes previous graph blob to artifacts)
		// Because the compilation should fail, the .o from prev compilation should be deleted from
		// disc Check that there is no .o file in artifacts after compilation
		auto output_maybe2 = collection->fileArtifactAtMaybe(base::StrID(output_name.c_str()));

		// Validate that .o file from previous compilation is present before we run the compilation
		// with changed source code
		assertFalse(
			output_maybe2.has_value(),
			"Output file should be deleted from artifacts after failed compilation"
		);
		driver::exit();

		// delete the artifacts directory after test
		std::filesystem::remove_all(artifacts_path.getPath());
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
