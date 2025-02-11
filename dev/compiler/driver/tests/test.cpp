#include "helios/hout/hout.hpp"
#include <tester/tester.hpp>

#include <query_framework/utils/with_context_do.hpp>
#include <query_framework/query_impl.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/queries.hpp>
#include <driver/driver.hpp>

class DriverTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS DriverTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(simpleFunctionsTest); }

private:
	void simpleFunctionsTest() {
		using namespace compiler;
		helios::HOUTUnit top_level;
		query::utils::withContextDo([&](query::Context& ctx) {
			auto module
				= ctx.query<frontend::QueryModuleTree>(fs::FilePath(path("modules/functions")));
			top_level = ctx.query<helios::QueryTopLevelEntities>(module);
		});

		driver::Driver driver({ .backend_type = driver::BackendType::LLVM,
		                        .output_file  = base::StrID("output") });
		driver.compileHOUTUnit(&top_level, frontend::ModuleID());
	}
};


TESTER_COMMON_MAIN("/compiler/driver/tests/")
