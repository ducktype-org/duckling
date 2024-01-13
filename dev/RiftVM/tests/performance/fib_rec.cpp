#include <api/api.hpp>
#include <tester/tester.hpp>

class FibRecVmTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS FibRecVmTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Recursive Fibonacci VM Test") { TESTER_ADD_TEST(simpleRun); }

private:
	void simpleRun() {
		auto process_pid_response = vm::api::spawn(false);
		assert(process_pid_response.has_value(), "Spawn failed (1)");
		auto pid = process_pid_response.expect("Spawn failed (2)").pid;

		fs::FilePath file(path("fib_rec.rbc"));
		auto         loaded_file_response = vm::api::loadFile(pid, file);
		assert(loaded_file_response.has_value(), "Load failed (1)");

		auto run_response = vm::api::run(pid);
		assert(run_response.has_value(), "Run failed (1)");

		auto input_response = vm::api::input(pid, "32");
		assert(input_response.has_value(), "Input failed (1)");

		auto join_response = vm::api::join(pid);
		assert(join_response.has_value(), "Join failed (1)");
	}
};

TESTER_COMMON_MAIN("/RiftVM/tests/performance/");
