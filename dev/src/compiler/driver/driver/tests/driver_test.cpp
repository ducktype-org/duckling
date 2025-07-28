#include <driver/initialize.hpp>
#include <driver/operations/generic_operations.hpp>
#include <linker/link.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries.hpp>

#include <base/string_id.hpp>

#include <artifacts/artifacts.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

#include <filesystem>

class DriverTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DriverTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(objFileGenerated);
		TESTER_ADD_TEST(assemblyAndLLVMGenerated);
		TESTER_ADD_TEST(dvmBackendRuns);
		TESTER_ADD_TEST(packageCompiles);
		TESTER_ADD_TEST(globalsTest);
		TESTER_ADD_TEST(globalsInitializationTest);
	}

private:

	void objFileGenerated() {
		using namespace compiler;

		driver::initializeTheCompiler(
			driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.compilation_artifacts = {
					.artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath(),
				},
				.debug_options         = {}
			}
		);

		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::File(path("modules/functions")));

		query::utils::withContextDo([&](query::Context& ctx) {
			// This method can fail on module verification
			auto module_o = ctx.query<driver::CompileModule>({module, driver::BackendType::LLVM});

			ASSERT_TRUE(module_o.FILE.exists());

			fs::FileManager::deleteFile(module_o.FILE);
		});
	}

	void assemblyAndLLVMGenerated() {
		using namespace compiler;
		driver::initializeTheCompiler(
			driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.compilation_artifacts = {
					.artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath(),
				},
				.debug_options         = {
					.dump_llvm_ir  = true,
					.dump_llvm_asm = true,
				}
			}
		);

		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::File(path("modules/functions")));
		
		query::utils::withContextDo([&](query::Context& ctx) {
			// This method can fail on module verification
			auto module_o = ctx.query<driver::CompileModule>({module, driver::BackendType::LLVM});

			auto module_name
				= base::StrID(base::strConcat("module_", module.asInt()).c_str());

			auto asm_file = module_name.str() + ".s";
			auto llvm_ir_file = module_name.str() + ".ll";

			assertTrue(std::filesystem::exists(asm_file), "Assembly file does not exist");
			assertTrue(std::filesystem::exists(llvm_ir_file), "LLVM IR file does not exist");

			std::filesystem::remove(asm_file);
			std::filesystem::remove(llvm_ir_file);
		});
	}

	void dvmBackendRuns() {
		using namespace compiler;
		
		driver::initializeTheCompiler(
			driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.compilation_artifacts = {
					.artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath(),
				},
				.debug_options         = {
				}
			}
		);


		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::File(path("modules/functions")));


		query::utils::withContextDo([&](query::Context& ctx) {
			auto run_result = driver::runModuleOnDVM(ctx, module, true);

			ASSERT_TRUE(run_result.has_value());
			ASSERT_EQUAL_PRINT(0, run_result.value().exit_code);
		});
	}

	void packageCompiles() {
		using namespace compiler;
		auto artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		driver::initializeTheCompiler(
			driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.compilation_artifacts = {
					.artifacts_path = artifacts_path,
				},
				.debug_options         = {
				}
			}
		);

		// this also checks if llvm IR lib compile and link into the executable:
		driver::compilerEntirePackage(fs::File(path("modules/functions")), driver::BackendType::LLVM);
		assertTrue(
			std::filesystem::exists(artifacts_path / "package_llvm.exe"),
			"Object file does not exist"
		);
	}

	void globalsTest() {
		using namespace compiler;
		auto artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		driver::initializeTheCompiler(
			driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.compilation_artifacts = {
					.artifacts_path = artifacts_path,
				},
				.debug_options         = {
				}
			}
		);

		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::File(path("modules/globals")));

		query::utils::withContextDo([&](query::Context& ctx) {
			// This method can fail on module verification
			auto module_o = ctx.query<driver::CompileModule>({module, driver::BackendType::LLVM});

			assertTrue(
				module_o.FILE.exists(),
				"Object file does not exist"
			);

			std::filesystem::remove(module_o.FILE.getFilePath().getPath());
		});

	

		query::utils::withContextDo([&](query::Context& ctx) {
			auto module_dbc = ctx.query<driver::CompileModule>({module, driver::BackendType::DVM});
			assertTrue(
				module_dbc.FILE.exists(),
				"Object file does not exist"
			);

			std::filesystem::remove(module_dbc.FILE.getFilePath().getPath());

			auto run_result = driver::runModuleOnDVM(ctx, module, false);
			ASSERT_TRUE(run_result.has_value());
			ASSERT_EQUAL_PRINT(0, run_result.value().exit_code);
		});
	}

	void globalsInitializationTest() {
		using namespace compiler;
		auto artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		driver::initializeTheCompiler(
			driver::CompilerModeOfOperationAndOptions::PackageCompilationMode{
				.compilation_artifacts = {
					.artifacts_path = artifacts_path,
				},
				.debug_options         = {
				}
			}
		);
		auto module = query::entryPoint<frontend::QueryModuleTree>(
			fs::File(path("modules/globals_initialization"))
		);

		query::utils::withContextDo([&](query::Context& ctx) {
	
			auto run_result = driver::runModuleOnDVM(ctx, module, false);;
			ASSERT_TRUE(run_result.has_value());
			ASSERT_EQUAL_PRINT(5, run_result.value().exit_code);
		});
	}
};


TESTER_COMMON_MAIN("/src/compiler/driver/tests/")
