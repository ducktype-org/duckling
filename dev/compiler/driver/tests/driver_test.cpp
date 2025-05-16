#include <driver/driver.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries.hpp>
#include <query_framework/query_entry_point.hpp>
#include <tester/tester.hpp>

#include <filesystem>

class DriverTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DriverTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(executableGenerated);
		TESTER_ADD_TEST(assemblyAndLLVMGenerated);
		TESTER_ADD_TEST(dvmBackendRuns);
		TESTER_ADD_TEST(builtinCompiles);
	}

private:
	void executableGenerated() {
		using namespace compiler;
		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::FilePath(path("modules/functions")));
		CRef<helios::HOUTUnit> top_level = query::entryPoint<helios::QueryTopLevelEntities>(module);

		driver::Driver driver({ .backend_type           = driver::BackendType::LLVM,
		                        .output_file            = base::StrID("test_module_exe"),
		                        .compile_to_assembly    = false,
		                        .dump_llvm_ir           = false,
		                        .dvm_code_only_memory   = false,
		                        .add_builtin_library    = false,
		                        .external_objects_files = {},
		                        .external_libs          = {} });

		// This method can fail on module verification
		driver.compileHOUTUnit(top_level, base::StrID("test_module"));
		driver.link();

		assertTrue(std::filesystem::exists("test_module_exe"), "Output file does not exist");
		assertTrue(std::filesystem::exists("test_module.o"), "Object file does not exist");

		std::filesystem::remove("test_module_exe");
		std::filesystem::remove("test_module.o");
	}

	void assemblyAndLLVMGenerated() {
		using namespace compiler;
		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::FilePath(path("modules/functions")));
		CRef<helios::HOUTUnit> top_level = query::entryPoint<helios::QueryTopLevelEntities>(module);

		driver::Driver driver({ .backend_type           = driver::BackendType::LLVM,
		                        .output_file            = base::StrID("test_module_exe"),
		                        .compile_to_assembly    = true,
		                        .dump_llvm_ir           = true,
		                        .dvm_code_only_memory   = false,
		                        .add_builtin_library    = false,
		                        .external_objects_files = {},
		                        .external_libs          = {} });

		// This method can fail on module verification
		driver.compileHOUTUnit(top_level, base::StrID("test_module"));
		driver.link();

		assertTrue(std::filesystem::exists("test_module.s"), "Assembly file does not exist");
		assertTrue(std::filesystem::exists("test_module.ll"), "LLVM IR file does not exist");

		std::filesystem::remove("test_module.s");
		std::filesystem::remove("test_module.ll");
	}

	void dvmBackendRuns() {
		using namespace compiler;
		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::FilePath(path("modules/functions")));
		CRef<helios::HOUTUnit> top_level = query::entryPoint<helios::QueryTopLevelEntities>(module);

		driver::Driver driver({ .backend_type           = driver::BackendType::DVM,
		                        .output_file            = base::StrID("test_module_exe"),
		                        .compile_to_assembly    = false,
		                        .dump_llvm_ir           = false,
		                        .dvm_code_only_memory   = false,
		                        .add_builtin_library    = false,
		                        .external_objects_files = {},
		                        .external_libs          = {} });

		driver.compileHOUTUnit(top_level, base::StrID("test_module"));

		auto run_result = driver.run();
		ASSERT_TRUE(run_result.has_value());
		ASSERT_EQUAL_PRINT(0, run_result.value().exit_code);
	}

	void builtinCompiles() {
		using namespace compiler;
		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::FilePath(path("modules/functions")));
		CRef<helios::HOUTUnit> top_level = query::entryPoint<helios::QueryTopLevelEntities>(module);

		driver::Driver driver({ .backend_type           = driver::BackendType::LLVM,
		                        .output_file            = base::StrID("test_module_exe"),
		                        .compile_to_assembly    = false,
		                        .dump_llvm_ir           = false,
		                        .dvm_code_only_memory   = false,
		                        .add_builtin_library    = true,
		                        .external_objects_files = {},
		                        .external_libs          = {} });

		// This method can fail on module verification
		driver.compileHOUTUnit(top_level, base::StrID("test_module"));
		driver.link();

		assertTrue(std::filesystem::exists("test_module_exe"), "Output file does not exist");
		assertTrue(std::filesystem::exists("test_module.o"), "Object file does not exist");

		std::filesystem::remove("test_module_exe");
		std::filesystem::remove("test_module.o");
	}
};


TESTER_COMMON_MAIN("/compiler/driver/tests/")
