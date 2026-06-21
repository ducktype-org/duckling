#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/api/vm.hpp>

class VmRuntimeExprTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmRuntimeExprTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(pausesOnBreakpointAndResumes); }


private:
	vm::PID loadProgram(std::string_view path_name) {
		auto process_pid_response = vm::api::spawn();
		assertTrue(process_pid_response.has_value(), "Spawn failed (loadProgram)");
		auto pid = process_pid_response.value().pid;

		fs::File file(path(std::string(path_name)));
		auto     loaded_file_response = vm::api::loadFiles(pid, { file });
		assertTrue(loaded_file_response.has_value(), "Load failed (loadProgram)");
		return pid;
	}

	/**
	 * @brief Checks if the program will pause on breakpoint.
	 * Checks if `api::waitForPause` and `api::resume` functions work correctly.
	 */
	void pausesOnBreakpointAndResumes() {
		auto pid = loadProgram("breakpoint.dbc");
		for (auto breakpoint: { 5ULL, 8ULL })
			ASSERT_TRUE(
				vm::api::setBreakpoint(pid, base::StrID("main"), breakpoint, true).has_value()
			);

		vm::api::run(pid).value();  // "Run failed (1)"

		auto execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (1)"
		ASSERT_EQUAL_PRINT(5, execution_position.instr_number);

		vm::api::resume(pid).value();  // "Resume failed (1)"

		execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (1)"
		ASSERT_EQUAL_PRINT(8, execution_position.instr_number);

		vm::api::resume(pid).value();  // "Resume failed (2)"

		vm::api::stop(pid).value();    // "Stop failed (1)"
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
