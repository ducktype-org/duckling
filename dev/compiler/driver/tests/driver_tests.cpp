#include <filesystem>
#include <query_framework/query_entry_point.hpp>
#include <helios/hout/hout.hpp>
#include <tester/tester.hpp>

#include <helios/queries.hpp>
#include <driver/driver.hpp>

class DriverTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DriverTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(executableGenerated);
		TESTER_ADD_TEST(assemblyAndLLVMGenerated);
	}

private:
	void executableGenerated() {
		using namespace compiler;
		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::FilePath(path("modules/functions")));
		helios::HOUTUnit top_level = query::entryPoint<helios::QueryTopLevelEntities>(module);

		driver::Driver driver({ .backend_type        = driver::BackendType::LLVM,
		                        .output_file         = base::StrID("test_module_exe"),
		                        .compile_to_assembly = false,
		                        .dump_llvm_ir        = false });

		// This method can fail on module verification
		driver.compileHOUTUnit(&top_level, base::StrID("test_module"));

		assertTrue(std::filesystem::exists("test_module_exe"), "Output file does not exist");
		assertTrue(std::filesystem::exists("test_module.o"), "Object file does not exist");
	}

	void assemblyAndLLVMGenerated() {
		using namespace compiler;
		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::FilePath(path("modules/functions")));
		helios::HOUTUnit top_level = query::entryPoint<helios::QueryTopLevelEntities>(module);

		driver::Driver driver({ .backend_type        = driver::BackendType::LLVM,
		                        .output_file         = base::StrID("test_module_exe"),
		                        .compile_to_assembly = true,
		                        .dump_llvm_ir        = true });

		// This method can fail on module verification
		driver.compileHOUTUnit(&top_level, base::StrID("test_module"));

		assertTrue(std::filesystem::exists("test_module.s"), "Assembly file does not exist");
		assertTrue(std::filesystem::exists("test_module.ll"), "LLVM IR file does not exist");
	}
};


TESTER_COMMON_MAIN("/compiler/driver/tests/")
