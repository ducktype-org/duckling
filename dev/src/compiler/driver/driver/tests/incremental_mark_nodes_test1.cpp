#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/backend_options.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/utils/with_context_do.hpp>
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
		compiler::driver::initializeTheCompiler(
            compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
                .main_package_info = {
                    .package_name = std::string("mark_nodes_test_package"),
                    .package_path = fs::FilePath(path("modules/functions_1")),
                },
                .compilation_artifacts = {.artifacts_path = artifacts_path},
            	.backend_options = {
					.llvm_backend = global_state::BackendOptions::LLVMBackend{},
				},
				.debug_options         = {},
				.incremental           = { .enabled = true }
            }
        );

		// First compilation creates a current graph
		auto module = frontend::createModuleTree(
			fs::File(path("modules/functions_1")), "mark_nodes_test_package"
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			(void) ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM });
		});

		// Save artifacts (writes previous graph blob to artifacts)
		driver::exit();

		// Smoke check: artifacts file exists
		auto artc_path = artifacts_path.getPath() / "query" / "query.artc";
		assertTrue(std::filesystem::exists(artc_path), "query.artc should exist");
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
