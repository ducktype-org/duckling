#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/operations/generic_operations.hpp>
#include <global_state/packages.hpp>

#include <filesystem/file_path.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

#include <filesystem>

using namespace compiler;

namespace {
	const char* k_artifacts_dir = "incremental_mark_nodes_artifacts";
}

class IncrementalMarkNodesTest3 final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS IncrementalMarkNodesTest3

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(initOrgFunctionsAndCompile); }

private:
	void initOrgFunctionsAndCompile() {
		// Place artifacts under the build directory (CTest working dir)
		fs::FilePath artifacts_path
			= fs::FilePath(std::filesystem::current_path() / k_artifacts_dir);

		// Initialize with changed package path (org_functions) and verify all marked red
		compiler::driver::initializeTheCompiler(
            compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
                .main_package_info = {
                    .package_name = std::string("mark_nodes_test_package2"),
                    .package_path = fs::FilePath(path("modules/incremental/org_functions/functions_1")),
                },
                .compilation_artifacts = {.artifacts_path = artifacts_path},
				.debug_options         = {},
				.incremental           = {}
            }
        );

		ASSERT_TRUE(query::internal::ContextAccess::getState()->getPreviousGraph().has_value());
		auto prev        = query::internal::ContextAccess::getState()->getPreviousGraph().value();
		auto prev_colors = query::internal::ContextAccess::getState()->getPreviousNodeColors();
		ASSERT_TRUE(!prev_colors->empty());

		// All node should be red since package name changed
		for (const auto& node: prev->getAllNodes())
			if (prev_colors->contains(node))
				ASSERT_TRUE(prev_colors->at(node) == query::internal::QueryState::PrevColor::Red);

		// Compile entire package and save artifacts
		compiler::driver::compilerEntirePackage(
			global_state::getMainPackage(),
			driver::BackendType::LLVM,
			{ .external_static_libraries = {}, .link_c_standard_library = true }
		);

		// we need to do this to Registering query: DoWithContext with
		query::utils::withContextDo([&](query::Context&) {});

		driver::exit();
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
