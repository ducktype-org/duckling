#include <artifacts/artifacts.hpp>
#include <driver/hout_to_binary_driver.hpp>
#include <driver/link.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries.hpp>
#include <query_framework/query_entry_point.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

#include <base/string_id.hpp>

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
		TESTER_ADD_TEST(globalsTest);
	}

private:
	/**
	 * Creates a mock collection for testing purposes.
	 * The collection is created in a temporary directory.
	 */
	Box<artifacts::ArtifactCollection> createMockCollection() {
		return base::makeBox<artifacts::ArtifactCollection>(
			fs::FileManager::createRandomTempDirectory().nativePath()
		);
	}

	void executableGenerated() {
		using namespace compiler;
		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::File(path("modules/functions")));
		CRef<helios::HOUTUnit> top_level = query::entryPoint<helios::QueryTopLevelEntities>(module);

		driver::HoutToBinaryDriver driver({
			.backend_type         = driver::BackendType::LLVM,
			.compile_to_assembly  = false,
			.dump_llvm_ir         = false,
			.dvm_code_only_memory = false,
			.add_builtin_library  = false,
		});

		query::utils::withContextDo([&](query::Context& ctx) {
			auto collection = createMockCollection();
			auto obj        = collection->fileArtifactNew(base::StrID("test_module.o"));
			auto exe        = collection->fileArtifactNew(base::StrID("test_module_exe"));

			// This method can fail on module verification
			driver.compileHOUTUnit(ctx, top_level, base::StrID("test_module"), obj);
			driver::link(exe, { obj }, {});

			assertTrue(std::filesystem::exists(exe.FILE.getPath()), "Output file does not exist");
			assertTrue(std::filesystem::exists(obj.FILE.getPath()), "Object file does not exist");

			std::filesystem::remove(exe.FILE.getPath());
			std::filesystem::remove(obj.FILE.getPath());
		});
	}

	void assemblyAndLLVMGenerated() {
		using namespace compiler;
		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::File(path("modules/functions")));
		CRef<helios::HOUTUnit> top_level = query::entryPoint<helios::QueryTopLevelEntities>(module);

		driver::HoutToBinaryDriver driver({
			.backend_type         = driver::BackendType::LLVM,
			.compile_to_assembly  = true,
			.dump_llvm_ir         = true,
			.dvm_code_only_memory = false,
			.add_builtin_library  = false,
		});

		query::utils::withContextDo([&](query::Context& ctx) {
			auto collection = createMockCollection();
			auto obj        = collection->fileArtifactNew(base::StrID("test_module.o"));
			auto exe        = collection->fileArtifactNew(base::StrID("test_module_exe"));

			// This method can fail on module verification
			driver.compileHOUTUnit(ctx, top_level, base::StrID("test_module"), obj);
			driver::link(exe, { obj }, {});

			assertTrue(std::filesystem::exists("test_module.s"), "Assembly file does not exist");
			assertTrue(std::filesystem::exists("test_module.ll"), "LLVM IR file does not exist");

			std::filesystem::remove("test_module.s");
			std::filesystem::remove("test_module.ll");
		});
	}

	void dvmBackendRuns() {
		using namespace compiler;
		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::File(path("modules/functions")));
		CRef<helios::HOUTUnit> top_level = query::entryPoint<helios::QueryTopLevelEntities>(module);

		driver::HoutToBinaryDriver driver({
			.backend_type         = driver::BackendType::DVM,
			.compile_to_assembly  = false,
			.dump_llvm_ir         = false,
			.dvm_code_only_memory = false,
			.add_builtin_library  = false,
		});

		query::utils::withContextDo([&](query::Context& ctx) {
			auto collection = createMockCollection();
			auto dbc_obj    = collection->fileArtifactNew(base::StrID("test_module.dbc"));
			driver.compileHOUTUnit(ctx, top_level, base::StrID("test_module"), dbc_obj);

			auto run_result = driver.run();
			ASSERT_TRUE(run_result.has_value());
			ASSERT_EQUAL_PRINT(0, run_result.value().exit_code);
		});
	}

	void builtinCompiles() {
		using namespace compiler;
		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::File(path("modules/functions")));
		CRef<helios::HOUTUnit> top_level = query::entryPoint<helios::QueryTopLevelEntities>(module);

		driver::HoutToBinaryDriver driver({
			.backend_type         = driver::BackendType::LLVM,
			.compile_to_assembly  = false,
			.dump_llvm_ir         = false,
			.dvm_code_only_memory = false,
			.add_builtin_library  = true,
		});
		query::utils::withContextDo([&](query::Context& ctx) {
			auto collection = createMockCollection();
			auto obj        = collection->fileArtifactNew(base::StrID("test_module.o"));
			auto exe        = collection->fileArtifactNew(base::StrID("test_module_exe"));

			// This method can fail on module verification
			driver.compileHOUTUnit(ctx, top_level, base::StrID("test_module"), obj);
			driver::link(exe, { obj }, {});

			assertTrue(std::filesystem::exists(exe.FILE.getPath()), "Output file does not exist");
			assertTrue(std::filesystem::exists(obj.FILE.getPath()), "Object file does not exist");

			std::filesystem::remove(exe.FILE.getPath());
			std::filesystem::remove(obj.FILE.getPath());
		});
	}

	void globalsTest() {
		using namespace compiler;
		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::File(path("modules/globals")));
		CRef<helios::HOUTUnit> top_level = query::entryPoint<helios::QueryTopLevelEntities>(module);

		// Test with LLVM backend
		driver::HoutToBinaryDriver llvm_driver({
			.backend_type         = driver::BackendType::LLVM,
			.compile_to_assembly  = false,
			.dump_llvm_ir         = false,
			.dvm_code_only_memory = false,
			.add_builtin_library  = true,
		});

		query::utils::withContextDo([&](query::Context& ctx) {
			auto collection = createMockCollection();
			auto obj        = collection->fileArtifactNew(base::StrID("test_module.o"));
			auto exe        = collection->fileArtifactNew(base::StrID("test_module_exe"));

			// This method can fail on module verification
			llvm_driver.compileHOUTUnit(ctx, top_level, base::StrID("test_module"), obj);
			driver::link(exe, { obj }, {});

			assertTrue(std::filesystem::exists(exe.FILE.getPath()), "Output file does not exist");
			assertTrue(std::filesystem::exists(obj.FILE.getPath()), "Object file does not exist");

			std::filesystem::remove(exe.FILE.getPath());
			std::filesystem::remove(obj.FILE.getPath());
		});

		// Test with DVM backend
		driver::HoutToBinaryDriver dvm_driver({
			.backend_type         = driver::BackendType::DVM,
			.compile_to_assembly  = false,
			.dump_llvm_ir         = false,
			.dvm_code_only_memory = false,
			.add_builtin_library  = false,
		});

		query::utils::withContextDo([&](query::Context& ctx) {
			auto collection = createMockCollection();
			auto dbc_obj    = collection->fileArtifactNew(base::StrID("test_module.dbc"));
			dvm_driver.compileHOUTUnit(ctx, top_level, base::StrID("test_module"), dbc_obj);

			auto run_result = dvm_driver.run();
			ASSERT_TRUE(run_result.has_value());
			ASSERT_EQUAL_PRINT(0, run_result.value().exit_code);
		});
	}
};


TESTER_COMMON_MAIN("/src/compiler/driver/tests/")
