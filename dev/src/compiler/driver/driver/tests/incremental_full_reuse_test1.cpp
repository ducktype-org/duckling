#include "test_utils.hpp"

#include <driver/exit.hpp>
#include <driver/initialize.hpp>
#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/backend_options.hpp>

#include <filesystem/file_path.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/q_stats/q_stats.hpp>
#include <string_id/string_id.hpp>
#include <tester/tester.hpp>

#include <filesystem>

using namespace compiler;

namespace {
	const char* k_artifacts_dir = "incremental_full_reuse_artifacts";
}

/**
 * First stage of the full-reuse incremental test: compile a package from scratch and save
 * artifacts. incremental_full_reuse_test2 then re-initializes from the same artifacts and
 * asserts that no module is recompiled.
 */
class IncrementalFullReuseTest1 final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS IncrementalFullReuseTest1

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(firstCompileAndSave); }

private:
	void firstCompileAndSave() {
		// Place artifacts under the build directory (CTest working dir) to avoid touching sources
		fs::FilePath artifacts_path
			= fs::FilePath(std::filesystem::current_path() / k_artifacts_dir);

		// first clear previous artifact files
		std::filesystem::remove_all(artifacts_path.getPath());

		auto init_result = compiler::driver::initializeTheCompiler(
            compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.packages_info = { driver_test_utils::emptyRawPackageInfo(
					"full_reuse_test_package",
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

		auto module = frontend::createModuleTree(
			fs::File(path("modules/incremental/org_functions/functions_1")),
			base::StrID("full_reuse_test_package")
		);

		auto sub_module_locked = frontend::getModuleRef(module)
		                             ->getSubmoduleByName(base::StrID{ "submodule" })
		                             .illegalAccess();
		assertTrue(sub_module_locked.has_value(), "Submodule should exist");
		auto submodule_id = sub_module_locked.value().illegalAccess().getID();

		query::utils::withContextDo([&](query::Context& ctx) {
			(void) ctx.query<driver::CompileModule>({ .module_id        = module,
			                                          .backend_type     = driver::BackendType::LLVM,
			                                          .build_debug_info = false });
			(void) ctx.query<driver::CompileModule>({ .module_id        = submodule_id,
			                                          .backend_type     = driver::BackendType::LLVM,
			                                          .build_debug_info = false });
		});

		// A cold compilation must have executed provide() for both modules.
		assertEqual(
			query::getProvideCallCount(driver::CompileModule::getID()),
			u64{ 2 },
			"Both modules should be compiled by provide() on a cold run"
		);

		// Save artifacts (writes previous graph blob to artifacts)
		driver::exit();

		auto artc_path = artifacts_path.getPath() / "query" / "query.artc";
		assertTrue(std::filesystem::exists(artc_path), "query.artc should exist");
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
