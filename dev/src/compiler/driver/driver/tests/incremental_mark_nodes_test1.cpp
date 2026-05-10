#include "incremental_metadata_test_common.hpp"

#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/backend_options.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <string_id/string_id.hpp>
#include <tester/tester.hpp>

#include <filesystem>

using namespace compiler;

namespace {
	const char* k_artifacts_dir = "incremental_mark_nodes_artifacts";
}

class IncrementalMarkNodesTest1 final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS IncrementalMarkNodesTest1

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(firstCompileAndSave); }

private:
	void firstCompileAndSave() {
		// Place artifacts under the build directory (CTest working dir) to avoid touching sources
		fs::FilePath artifacts_path
			= fs::FilePath(std::filesystem::current_path() / k_artifacts_dir);

		// first clear previous artifact files
		std::filesystem::remove_all(artifacts_path.getPath());

		// Initialize compiler (as in markPreviousLeavesGreenTest, first stage)
		auto init_result= compiler::driver::initializeTheCompiler(
            compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.packages_info = {
					{
						.package_name = base::StrID("mark_nodes_test_package"),
						.version      = base::StrID("not_supported"),
						.package_path  = fs::FilePath(path("modules/incremental/org_functions/functions_1")),
						.features     = {},
						.dependencies = {},
					},
				},
                .compilation_artifacts = {.artifacts_path = artifacts_path},
            	.backend_options = {
					.llvm_backend = global_state::BackendOptions::LLVMBackend{},
				},
				.debug_options         = {},
				.incremental           = { .enabled = true },
				.execution_options     = { .worker_count = 1 },
            }
        );

		ASSERT_TRUE(init_result.status().isOk());

		// First compilation creates a current graph
		auto module = frontend::createModuleTree(
			fs::File(path("modules/incremental/org_functions/functions_1")),
			base::StrID("mark_nodes_test_package")
		);

		// Get the submodule "submodule" id for compilation
		// This module check if ChildSideInput nodes for non-existing submodules are exist in the
		// graph in the second test Thanks to that we know that driver correctly marks non-existing
		// submodules as green
		auto sub_module_locked = frontend::getModuleRef(module)
		                             ->getSubmoduleByName(base::StrID{ "submodule" })
		                             .illegalAccess();

		// First check is sumbodule exists
		assertTrue(sub_module_locked.has_value(), "Submodule should exist");

		auto submodule_id = sub_module_locked.value().illegalAccess().getID();

		query::utils::withContextDo([&](query::Context& ctx) {
			(void) ctx.query<driver::CompileModule>({ .module_id        = module,
			                                          .backend_type     = driver::BackendType::LLVM,
			                                          .build_debug_info = false });
			(void) ctx.query<driver::CompileModule>({ .module_id        = submodule_id,
			                                          .backend_type     = driver::BackendType::LLVM,
			                                          .build_debug_info = false });

			// Add metadata for persistence test
			(void) ctx.query<MetadataPersistenceTestQuery>({ 42 });
			(void) ctx.query<MetadataPersistenceTestQuery>({ 100 });
		});

		// Save artifacts (writes previous graph blob to artifacts)
		driver::exit();

		// Smoke check: artifacts file exists
		auto artc_path = artifacts_path.getPath() / "query" / "query.artc";
		assertTrue(std::filesystem::exists(artc_path), "query.artc should exist");
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
