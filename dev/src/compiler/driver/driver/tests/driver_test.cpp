#include <driver/initialize.hpp>
#include <driver/operations/generic_operations.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <global_state/options.hpp>

#include <base/str/string_id.hpp>

#include <artifacts/artifacts.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

#include <filesystem>

class DriverTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DriverTest

	fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// note: all of those tests have to work on different
		// modules, since otherwise query will cache the results, and tests
		// wont test what they are supposed to:
		TESTER_ADD_TEST(objFileGenerated);
		TESTER_ADD_TEST(assemblyAndLLVMGenerated);
		TESTER_ADD_TEST(dvmBackendRuns);
		TESTER_ADD_TEST(packageCompiles);
		TESTER_ADD_TEST(globalsTest);
		TESTER_ADD_TEST(globalsInitializationTest);

		compiler::driver::initializeTheCompiler(
			compiler::driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.compilation_artifacts = {
					.artifacts_path = artifacts_path,
				},
				.debug_options         = {}
			}
		);
	}

private:
	void objFileGenerated() {
		using namespace compiler;

		auto module = frontend::createModuleTree(fs::File(path("modules/functions_1")));

		query::utils::withContextDo([&](query::Context& ctx) {
			// This method can fail on module verification
			auto module_o = ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM });

			ASSERT_TRUE(module_o.FILE.exists());

			fs::FileManager::deleteFile(module_o.FILE);
		});
	}

	void assemblyAndLLVMGenerated() {
		using namespace compiler;

		global_state::getDynamicDebugOptions()->llvm_dump_ir  = true;
		global_state::getDynamicDebugOptions()->llvm_dump_asm = true;
		defer(global_state::getDynamicDebugOptions()->llvm_dump_ir  = false;
		      global_state::getDynamicDebugOptions()->llvm_dump_asm = false;);

		auto module = frontend::createModuleTree(fs::File(path("modules/functions_2")));

		query::utils::withContextDo([&](query::Context& ctx) {
			// This method can fail on module verification
			auto module_o = ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM });

			auto module_name
				= base::StrID(base::strConcat("module_", module.queryUnstablePerfectHash()).c_str());

			auto asm_file     = module_name.str() + ".s";
			auto llvm_ir_file = module_name.str() + ".ll";

			assertTrue(std::filesystem::exists(asm_file), "Assembly file does not exist");
			assertTrue(std::filesystem::exists(llvm_ir_file), "LLVM IR file does not exist");

			std::filesystem::remove(asm_file);
			std::filesystem::remove(llvm_ir_file);
		});
	}

	void dvmBackendRuns() {
		using namespace compiler;

		auto module = frontend::createModuleTree(fs::File(path("modules/functions_3")));


		query::utils::withContextDo([&](query::Context& ctx) {
			auto run_result = driver::runModuleOnDVM(ctx, module);

			ASSERT_TRUE(run_result.has_value());
			ASSERT_EQUAL_PRINT(0, run_result.value().exit_code);
		});
	}

	void packageCompiles() {
		using namespace compiler;

		// this also checks if llvm IR lib compile and link into the executable:
		driver::compilerEntirePackage(
			fs::File(path("modules/functions_4")),
			driver::BackendType::LLVM,
			{ .external_static_libraries = {}, .link_c_standard_library = true }
		);

		auto exe_path = artifacts_path / "package_llvm.exe";
		assertTrue(
			std::filesystem::exists(exe_path),
			base::strConcat("Executable file does not exist: ", exe_path.native())
		);

		driver::compilerEntirePackage(
			fs::File(path("modules/functions_4")),
			driver::BackendType::DVM,
			{ .external_static_libraries = {}, .link_c_standard_library = true }
		);
	}

	void globalsTest() {
		using namespace compiler;

		auto module = frontend::createModuleTree(fs::File(path("modules/globals")));

		query::utils::withContextDo([&](query::Context& ctx) {
			// This method can fail on module verification
			auto module_o = ctx.query<driver::CompileModule>({ module, driver::BackendType::LLVM });

			assertTrue(module_o.FILE.exists(), "Object file does not exist");

			std::filesystem::remove(module_o.FILE.getFilePath().getPath());
		});


		query::utils::withContextDo([&](query::Context& ctx) {
			auto module_dbc
				= ctx.query<driver::CompileModule>({ module, driver::BackendType::DVM });
			assertTrue(module_dbc.FILE.exists(), "Object file does not exist");

			std::filesystem::remove(module_dbc.FILE.getFilePath().getPath());

			auto run_result = driver::runModuleOnDVM(ctx, module);
			ASSERT_TRUE(run_result.has_value());
			ASSERT_EQUAL_PRINT(0, run_result.value().exit_code);
		});
	}

	void globalsInitializationTest() {
		using namespace compiler;

		auto module = frontend::createModuleTree(fs::File(path("modules/globals_initialization")));

		query::utils::withContextDo([&](query::Context& ctx) {
			auto run_result = driver::runModuleOnDVM(ctx, module);
			ASSERT_TRUE(run_result.has_value());
			ASSERT_EQUAL_PRINT(5, run_result.value().exit_code);
		});
	}
};


TESTER_COMMON_MAIN("/src/compiler/driver/driver/tests/")
