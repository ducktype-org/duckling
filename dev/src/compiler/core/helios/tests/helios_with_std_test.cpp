/**
 * @file helios_with_std_test.cpp
 * @brief HELIOS tests that need the standard library available (e.g. to resolve
 * `import core.builtins.*`). They initialize the compiler via the driver test utils, unlike
 * helios_test.cpp which builds standalone module trees without a std.
 */

#include <driver/test_utils.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/queries.hpp>

#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <string_id/string_id.hpp>
#include <tester/tester.hpp>

class HeliosWithStdTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosWithStdTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testBuiltinDefinitionInModuleHOUT); }

protected:
	void beforeAll() override {
		fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		std::vector<compiler::driver::test_utils::PackagePathAndName> packages{
			{ fs::FilePath(path("test_modules/builtins")), "builtins" }
		};
		auto init_result
			= compiler::driver::test_utils::initializeCompilerForTests(packages, artifacts_path);
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");
	}

private:
	// A `@builtin(...)` fundecl (here `ptr_from_slice` from core.builtins) has no body in source;
	// the compiler synthesizes its implementation (getBuiltinImpl). This checks that the
	// synthesized definition is actually emitted into the module HOUT when the builtin is used.
	void testBuiltinDefinitionInModuleHOUT() {
		auto module_id = compiler::driver::test_utils::getModuleIdFromPath("builtins");

		auto houts = query::entryPoint<compiler::helios::QueryModuleHOUTRecursively>(module_id)
		                 .valueOrPanic();

		bool found_ptr_from_slice = false;
		for (const auto& hout: houts)
			for (const auto& fun: hout->functions)
				if (fun->declaration->original_name == base::StrID("ptr_from_slice"))
					found_ptr_from_slice = true;

		assertTrue(
			found_ptr_from_slice,
			"ptr_from_slice builtin definition should be emitted into the module HOUT"
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
