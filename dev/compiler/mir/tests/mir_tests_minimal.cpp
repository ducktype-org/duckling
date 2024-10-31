/**
 * @brief Minimal example of mir/query UB
 */

#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>
#include <query_framework/test_utils/context_suite.hpp>

#include <tester/tester.hpp>

#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/queries.cpp>

#include <mir/mir_lowering/mir_lowering.hpp>

using namespace compiler::helios::test_utils;





class MIRConstructionTest final: public tester::ContextSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MIRConstructionTest

public:
	MIRConstructionTest(tester::TestConfig&& config):
		  tester::ContextSuite(std::move(config), "mir construction test") {
		TESTER_ADD_TEST(testTerminatorSuccessors);
		TESTER_ADD_TEST(mockLifetimeAnalysisTest);
	}

private:

	void testTerminatorSuccessors() {
		auto [module, scope] = getModule(fs::FilePath(path("modules/mir_var_test")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			ctx.query<compiler::mir::LowerToMirFunction>({ unit.functions.at(0) });
		});
	}

	void mockLifetimeAnalysisTest() {
	
		auto [module, scope] = getModule(fs::FilePath(path("modules/mir_var_test")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);

			bool a = unit.functions.at(0) == unit.functions.at(0);
			message(std::to_string(a));
		
		});
	}
};

TESTER_COMMON_MAIN("/compiler/mir/tests/")
