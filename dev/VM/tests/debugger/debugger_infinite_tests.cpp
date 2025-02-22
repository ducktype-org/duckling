#include <api/data/status.hpp>
#include <api/vm.hpp>
#include <api/api.hpp>
#include <tester/tester.hpp>

/**
 * @brief This test set is responsible for testing the debugger when the VM is running endlessly.
 * This can cause stack overflow if the tail-call optimization is not working properly 
 * and the test is running slow (e.g. when using Valgrind).
 *
 * It is also advised to use "--fair-sched=yes" option in Valgrind,
 * because the code executing thread is running all the time and the "waitingForResponse" thread
 * runs very slowly because of that - mine took 10-70 seconds to finish in Release mode.
 * (but a just a second in "--fair-sched=yes" mode).
 */
class VmDebugInfiniteTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebugInfiniteTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
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
		vm::api::step(pid).expect("Step failed");
		auto execution_position
			= vm::api::getCurrentPosition(pid).expect("Get current position failed");
		return execution_position.instr_number;
	}
};

TESTER_COMMON_MAIN("/VM/tests/debugger/");
