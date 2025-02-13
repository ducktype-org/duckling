#include <query_framework/query_entry_point.hpp>
#include <helios/hout/hout.hpp>
#include <tester/tester.hpp>

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
		auto module
			= query::entryPoint<frontend::QueryModuleTree>(fs::FilePath(path("modules/functions")));
		helios::HOUTUnit top_level = query::entryPoint<helios::QueryTopLevelEntities>(module);

		driver::Driver driver({ .backend_type = driver::BackendType::LLVM,
		                        .output_file  = base::StrID("output") });

		// This method can fail on module verification
		driver.compileHOUTUnit(&top_level, base::StrID("test_module"));
	}
};


TESTER_COMMON_MAIN("/compiler/driver/tests/")
