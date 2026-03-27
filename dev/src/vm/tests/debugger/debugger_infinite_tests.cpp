#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/api/vm.hpp>

/**
 * @brief This test set is responsible for testing the debugger when the VM is running endlessly.
 * This can cause stack overflow if the tail-call optimization is not working properly
 * and the test is running slow (e.g. when using Valgrind).
 *
 * It is also advised to use "--fair-sched=yes" option in Valgrind,
 * because the code executing thread is running all the time and the "waitingForResponse" thread
 * runs very slowly because of that - mine took 10-70 seconds to finish in Release mode.
 * (but a just a second in "--fair-sched=yes" mode).
 * https://stackoverflow.com/questions/8663148/valgrind-stalls-in-multithreaded-socket-program
 */
class VmDebugInfiniteTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebugInfiniteTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(pausesExecution); }


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
	 * @brief Checks if the program will pause on user request.
	 * The program is an infinite loop, so it will never stop.
	 */
	void pausesExecution() {
		auto pid = loadProgram("while_true.dbc");

		vm::api::run(pid).value();  // "Run failed (1)"

		// We want to assure that the start function already managed to call main for the test to
		// work correctly.
		auto execution_position
			= vm::api::waitForBreakpoint(pid).value();  // "Wait for breakpoint failed (1)"
		// Because of stepGILs are inserted, there are more instructions.
		ASSERT_EQUAL_PRINT(2, execution_position.instr_number);

		vm::api::resume(pid).value();                 // "Resume failed (1)"
		auto position = vm::api::pause(pid).value();  // "Pause failed (1)"
		ASSERT_TRUE(6 <= position.instr_number && position.instr_number <= 9);

		auto expected_next_line = [this](u64 x) -> u64 {
			if (x == 6) return 7;
			if (x == 7) return 8;
			if (x == 8) return 9;
			if (x == 9) return 6;
			this->fail("Unexpected line number: " + std::to_string(x));
			CORE_UNREACHABLE();
		};

		auto line_number2 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(position.instr_number), line_number2);

		auto line_number3 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number2), line_number3);

		auto line_number4 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number3), line_number4);

		auto line_number5 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number4), line_number5);

		auto line_number6 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number5), line_number6);

		vm::api::resume(pid).value();  // "Resume failed (1)"

		vm::api::stop(pid).value();    // "Stop failed (1)"
	}

	u64 stepAndGetLine(u64 pid) {
		vm::api::step(base::safeIntConv<vm::PID>(pid)).value();  // "Step failed"
		auto execution_position = vm::api::getCurrentPosition(base::safeIntConv<vm::PID>(pid))
		                              .value();                  // "Get current position failed"
		return execution_position.instr_number;
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
