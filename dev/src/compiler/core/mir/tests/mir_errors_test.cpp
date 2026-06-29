#include "utils/test_utils.hpp"

#include <frontend/module_tree/module_id.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <helios/queries/queries.hpp>
#include <helios/tsh/queries/types.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <mir/mir_lowering/mir_unit.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>

#include <filesystem/file.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/query_result.hpp>
#include <tester/tester.hpp>

using namespace compiler;

class HeliosErrorsTests: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HeliosErrorsTests

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testErrorLogging); }

private:
	void testErrorLogging() {
		// ============================ Unused shadowed variable ============================
		compiler::mir::test_utils::checkForErrorOnCompileModule(
			R"(fun shadowedLocal() = {
                var n = 42;
                if (true) {
                    var n = 24;
                }
            })",
			{ "Variable declaration shadows a previous declaration.", "Previous declaration:" },
			1
		);
		compiler::mir::test_utils::checkForErrorOnCompileModule(
			R"(fun shadowedArg(n: i64) = {
                var n = 42;
            })",
			{ "Variable declaration shadows a previous declaration.", "Previous declaration:" },
			1
		);
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
