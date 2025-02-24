#include <api/data/status.hpp>
#include <api/vm.hpp>
#include <api/api.hpp>
#include <tester/tester.hpp>
#include <chrono>
#include <thread>
#include <base/int_conv.hpp>

class VmDebugTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebugTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(stopTest);
		TESTER_ADD_TEST(killTest);
		TESTER_ADD_TEST(pausesOnBreakpointAndResumes);
		TESTER_ADD_TEST(executesStepByStep);
		TESTER_ADD_TEST(pausesExecution);
	}


private:
	vm::PID loadProgram(std::string_view path_name) {
		auto process_pid_response = vm::api::spawn();
		assertTrue(process_pid_response.has_value(), "Spawn failed (loadProgram)");
		auto pid = process_pid_response.value().pid;

		fs::FilePath file(path(std::string(path_name)));
		auto         loaded_file_response = vm::api::loadFile(pid, file);
		assertTrue(loaded_file_response.has_value(), "Load failed (loadProgram)");
		return pid;
	}

	/**
	 * @brief Checks if the program can be stopped while waiting for input.
	 */
	void stopTest() {
		auto pid = loadProgram("vm_api_tests.dbc");

		auto run_response = vm::api::run(pid);
		assertTrue(run_response.has_value(), "Run failed (1)");

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		auto stop_response = vm::api::stop(pid);
		assertTrue(stop_response.has_value(), "Stop failed (1)");
	}

	/**
	 * @brief Checks if the program can be killed while waiting for input.
	 */
	void killTest() {
		auto pid = loadProgram("vm_api_tests.dbc");

		auto run_response = vm::api::run(pid);
		assertTrue(run_response.has_value(), "Run failed (1)");

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		auto kill_response = vm::api::kill(pid);
		assertTrue(kill_response.has_value(), "Kill failed (1)");
	}

	/**
	 * @brief Checks if the program will pause on breakpoint.
	 * Checks if `api::waitForPause` and `api::resume` functions work correctly.
	 */
	void pausesOnBreakpointAndResumes() {
		auto pid = loadProgram("breakpoint.dbc");
		vm::api::run(pid).expect("Run failed (1)");

		auto execution_position
			= vm::api::waitForBreakpoint(pid).expect("Wait for breakpoint failed (1)");
		assertEqual(3, execution_position.instr_number, "Line number is not correct");

		vm::api::resume(pid).expect("Resume failed (1)");

		execution_position
			= vm::api::waitForBreakpoint(pid).expect("Wait for breakpoint failed (1)");
		assertEqual(7, execution_position.instr_number, "Line number is not correct");

		vm::api::resume(pid).expect("Resume failed (2)");

		vm::api::stop(pid).expect("Stop failed (1)");
	}

	/**
	 * @brief Checks if the program will execute step by step.
	 */
	void executesStepByStep() {
		auto pid = loadProgram("breakpoint.dbc");

		vm::api::run(pid).expect("Run failed (1)");

		auto execution_position
			= vm::api::waitForBreakpoint(pid).expect("Wait for breakpoint failed (1)");
		assertEqual(3, execution_position.instr_number, "Line number is not correct");

		u64 line = stepAndGetLine(pid);
		assertEqual(4, line, "Line number is not correct (2)");

		line = stepAndGetLine(pid);
		assertEqual(5, line, "Line number is not correct (3)");

		vm::api::resume(pid).expect("Resume failed (1)");

		execution_position
			= vm::api::waitForBreakpoint(pid).expect("Wait for breakpoint failed (2)");
		assertEqual(7, execution_position.instr_number, "Line number is not correct (4)");

		vm::api::resume(pid).expect("Resume failed (2)");

		vm::api::stop(pid).expect("Stop failed (1)");
	}

	/**
	 * @brief Checks if the program will pause on user request.
	 * The program is an infinite loop, so it will never stop.
	 */
	void pausesExecution() {
		auto pid = loadProgram("while_true.dbc");

		vm::api::run(pid).expect("Run failed (1)");

		auto position = vm::api::pause(pid).expect("Pause failed (1)");
		assertTrue(
			1 <= position.instr_number && position.instr_number <= 2, "Line number is not correct"
		);

		auto expected_next_line = [](u64 x) -> u64 {
			if (x == 1) return 2;
			if (x == 2) return 1;
			return -1;
		};

		auto line_number2 = stepAndGetLine(pid);
		assertEqual(
			expected_next_line(position.instr_number),
			line_number2,
			"Line number is not correct (2)"
		);

		auto line_number3 = stepAndGetLine(pid);
		assertEqual(
			expected_next_line(line_number2), line_number3, "Line number is not correct (3)"
		);

		auto line_number4 = stepAndGetLine(pid);
		assertEqual(
			expected_next_line(line_number3), line_number4, "Line number is not correct (4)"
		);

		vm::api::resume(pid).expect("Resume failed (1)");

		vm::api::stop(pid).expect("Stop failed (1)");
	}

	u64 stepAndGetLine(u64 pid) {
		vm::api::step(base::safeIntConv<vm::PID>(pid)).expect("Step failed");
		auto execution_position = vm::api::getCurrentPosition(base::safeIntConv<vm::PID>(pid))
		                              .expect("Get current position failed");
		return execution_position.instr_number;
	}
};

TESTER_COMMON_MAIN("/VM/tests/debugger/");
