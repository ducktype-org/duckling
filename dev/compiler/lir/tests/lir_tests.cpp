/**
 * @file lir_tests.cpp
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
#include <lir/lir_lowering/lir_lowering.hpp>

using namespace tsh;
using namespace compiler::helios::test_utils;

class LIRConstructionTest final: public tester::ContextSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LIRConstructionTest

public:
	LIRConstructionTest(tester::TestConfig&& config):
		  tester::ContextSuite(std::move(config), "mir construction test") {
		
		// @TODO 
		// tests here don't tests much apart from the fact that code compiles
		// and does not throw.
		// This is due to the fact that LIR is in a very early stage of development
		// and will likely change a lot in near future.
		// Add more tests with future LIR changes.

		TESTER_ADD_TEST(noTest);
	}

private:

	void noTest() {
		auto [module, scope] = getModule(fs::FilePath(path("modules/simple")));

		withContextDo([&](query::Context& ctx) {
			auto  unit      = ctx.query<compiler::helios::QueryTopLevelEntities>(module);
			auto& functions = unit.functions;
			ASSERT_EQUAL(1, functions.size());
			ASSERT_EQUAL(base::StrID("foo"), functions.at(0).original_name);

			auto& foo_mir = ctx.query<compiler::mir::LowerToMirFunction>({ functions.at(0) });
			ASSERT_EQUAL(foo_mir.name, base::StrID("foo"));

			auto& foo_lir = ctx.query<compiler::lir::LowerToLirFunction>({ foo_mir });

			// @TODO: add some proper tests here
		
			// Test debug print:
			// Note that doesn't test much other then that the code doesn't crash/throw exceptions.
			std::stringstream foo_str;
			foo_lir.debugPrint(foo_str);
		});
	}

};

TESTER_COMMON_MAIN("/compiler/lir/tests/")
