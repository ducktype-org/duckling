/**
 * @file mir_tests.cpp
 */

#include <ctv/ctv.hpp>
#include <driver/test_utils.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/queries.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <mir/mir_lowering/mir_unit.hpp>
#include <mir/mir_lowering/mir_validation.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <filesystem/file.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler::tsh;
using namespace compiler::helios::test_utils;
using compiler::mir::BlockID;
using query::utils::withContextDo;

class MIRConstructionTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MIRConstructionTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(sliceTest); }

protected:
	void beforeAll() override {
		fs::FilePath artifacts_path = fs::FileManager::createRandomTempDirectory().getFilePath();
		std::vector<compiler::driver::test_utils::PackagePathAndName> packages{
			{ fs::FilePath(path("modules/slices")), "slices" }
		};
		auto init_result
			= compiler::driver::test_utils::initializeCompilerForTests(packages, artifacts_path);
		assertTrue(init_result.status().isOk(), "Compiler initialization failed");
	}

private:
	using enum compiler::tsh::IntegralAbstractType::Signedness;

	void sliceTest() {
		auto module_id = compiler::driver::test_utils::getModuleIdFromPath("slices");
		withContextDo([&](query::Context& ctx) {
			auto& unit
				= ctx.query<compiler::helios::QueryTopLevelEntities>(module_id)->valueOrPanic();
			auto mir_unit = compiler::mir::lowerToMIRUnit(ctx, &unit).valueOrPanic();
			// This 4 blocks are from the conditions for the slice access bounds check
			ASSERT_EQUAL_PRINT(mir_unit.mir_functions[0]->block_order.size(), 5);
		});
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/mir/tests/")
