#include <api/api.hpp>
#include <tester/tester.hpp>

class SimpleVmTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleVmTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(simpleRun); }


private:
	void simpleRun() {
		auto process_pid_response = vm::api::spawn(false);
		assertTrue(process_pid_response.has_value(), "Spawn failed (1)");
		auto pid = process_pid_response.expect("Spawn failed (2)").pid;

		fs::FilePath file(path("working_rbc.dbc"));
		auto         loaded_file_response = vm::api::loadFile(pid, file);
		assertTrue(loaded_file_response.has_value(), "Load failed (1)");

		auto run_response = vm::api::run(pid);
		assertTrue(run_response.has_value(), "Run failed (1)");

		auto join_response = vm::api::join(pid);
		assertTrue(join_response.has_value(), "Join failed (1)");
	}
};

TESTER_COMMON_MAIN("/VM/tests/basic/");
