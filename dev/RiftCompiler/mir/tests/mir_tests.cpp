/**
 * @file mir_tests.cpp
 * @brief Tests in this file are very bad right now, because MIR
 * is not yet fully implemented and is hard to properly test. 
 */

#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>
#include <tester/tester.hpp>

#include <helios/test_utils/helios_test_utils.hpp>
#include <query_framework/test_utils/context_suite.hpp>

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
		
		withContextDo([&](query::Context& ctx) {
			auto [module, scope] = getModule(fs::FilePath(path("modules/mir_simple_tests")));

		});
	}

};

TESTER_COMMON_MAIN("/RiftCompiler/mir/tests/")
