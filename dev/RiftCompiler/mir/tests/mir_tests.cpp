/**
 * @file mir_tests.cpp
 * @brief Tests in this file are very bad right now, because MIR
 * is not yet fully implemented and is hard to properly test. 
 */

#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>
#include <query_framework/test_utils/context_suite.hpp>

#include <tester/tester.hpp>

#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/queries.cpp>

#include <mir/mir_lowering/mir_lowering.hpp>

using namespace ts;
using namespace compiler::helios::test_utils;


class MIRConstructionTest final: public tester::ContextSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MIRConstructionTest

public:
	MIRConstructionTest(tester::TestConfig&& config):
		  tester::ContextSuite(std::move(config), "TypeSystem overload resolution test") {
		TESTER_ADD_TEST(simpleTest);
	}

private:

	void simpleTest() {
		auto [module, scope] = getModule(fs::FilePath(path("modules/mir_simple_test")));
		
		withContextDo([&](query::Context& ctx) {
			auto unit = ctx.query<compiler::helios::QueryTopLevelEntities>(module);

			auto& functions = unit.functions;
			ASSERT_EQUAL(3, functions.size());
			ASSERT_EQUAL(base::StrId("foo1"), functions.at(0).original_name);
			ASSERT_EQUAL(base::StrId("foo2"), functions.at(1).original_name);
			ASSERT_EQUAL(base::StrId("foo3"), functions.at(2).original_name);

			auto& foo1_mir = ctx.query<compiler::mir::LowerToMirFunction>({functions.at(0)});
			auto& foo2_mir = ctx.query<compiler::mir::LowerToMirFunction>({functions.at(1)});
			auto& foo3_mir = ctx.query<compiler::mir::LowerToMirFunction>({functions.at(2)});

			ASSERT_EQUAL(foo1_mir.name, base::StrId("foo1"));
			ASSERT_EQUAL(foo2_mir.name, base::StrId("foo2"));
			ASSERT_EQUAL(foo3_mir.name, base::StrId("foo3"));

			ASSERT_EQUAL(foo1_mir.blocks.size(), 1);
			ASSERT_EQUAL(foo2_mir.blocks.size(), 2);
			ASSERT_EQUAL(foo3_mir.blocks.size(), 5);

			// This doesn't test much other then that the code doesn't crash/throw exceptions.
			// It also make debug_prints covered by tests.
			std::stringstream all_functions;
			foo1_mir.debugPrint(all_functions);
			foo2_mir.debugPrint(all_functions);
			foo3_mir.debugPrint(all_functions);
		});
	}
};

TESTER_COMMON_MAIN("/RiftCompiler/mir/tests/")
