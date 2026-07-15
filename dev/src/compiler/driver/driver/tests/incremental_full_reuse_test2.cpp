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
 * Second stage of the full-reuse incremental test: re-initialize the compiler from the
 * artifacts saved by incremental_full_reuse_test1 with unchanged sources and compile the same
 * modules again. Every CompileModule call must be served from disk — provide() must never run.
 * Guards against regressions where package-level side inputs are not re-collected on driver
 * init or unstable-hash nodes leak into the serialized graph (both poison the red-green sweep
 * and force full recompilation).
 */
class IncrementalFullReuseTest2 final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS IncrementalFullReuseTest2

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(reuseEverythingOnSecondRun); }

private:
	void reuseEverythingOnSecondRun() {
		fs::FilePath artifacts_path
			= fs::FilePath(std::filesystem::current_path() / k_artifacts_dir);

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

		// With unchanged sources everything must be loaded from disk.
		assertEqual(
			query::getProvideCallCount(driver::CompileModule::getID()),
			u64{ 0 },
			"No module should be recompiled when nothing changed"
		);

		driver::exit();
	}
};

TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
