#include <driver/initialize.hpp>
#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

#include <filesystem>

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
		compiler::driver::initializeTheCompiler(
            compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
                .main_package_info = {
                    .package_name = std::string("mark_nodes_test_package2"),
                    .package_path = fs::FilePath(path("modules/incremental/changed_functions/functions_1")),
                },
                .compilation_artifacts = {.artifacts_path = artifacts_path},
				.debug_options         = {},
				.incremental           = {}
            }
        );

		auto prev_graph_opt = query::internal::ContextAccess::getState()->getPreviousGraph();
		ASSERT_TRUE(prev_graph_opt.has_value());
		auto prev = prev_graph_opt.value();

		auto prev_colors = query::internal::ContextAccess::getState()->getPreviousNodeColors();
		ASSERT_TRUE(!prev_colors->empty());

		int green_count = 0;
		int red_count   = 0;
		for (const auto& node: prev->getAllNodes()) {
			if (prev_colors->contains(node)) {
				if (prev_colors->at(node) == query::internal::QueryState::PrevColor::Green)
					green_count++;
				else if (prev_colors->at(node) == query::internal::QueryState::PrevColor::Red)
					red_count++;
				else
					ASSERT_TRUE(false);
			}
		}
		std::cerr << "Green nodes: " << green_count << ", Red nodes: " << red_count << '\n';
		ASSERT_TRUE(green_count > 0);
		ASSERT_TRUE(red_count == 1);

		// Probably because of linker optimizations the INTERNAL_QUERY_IMPLEMENTATION_BOILERPLATE
		// won't initialise without actually running a query
		auto module = frontend::createModuleTree(
			fs::File(path("modules/functions_2")), "mark_nodes_test_packag1e1e"
		);

		query::utils::withContextDo([&](query::Context& ctx) {
			(void) ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM });
		});

		// delete the artifacts directory after test
		std::filesystem::remove_all(artifacts_path.getPath());
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
