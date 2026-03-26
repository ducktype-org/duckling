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
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(pausesExecution);
		TESTER_ADD_TEST(pausesExecutionWithoutMapping);
	}


private:
	vm::PID loadProgram(std::string_view path_name, bool with_mapping = true) {
		auto process_pid_response = vm::api::spawn(with_mapping);
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

		auto run_response = vm::api::run(pid);
		assertTrue(run_response.has_value(), "Run failed (1)");

		// We want to assure that the start function already managed to call main for the test to
		// work correctly.
		auto execution_position = vm::api::waitForBreakpoint(pid);
		assertTrue(execution_position.has_value(), "Wait for breakpoint failed (1)");
		ASSERT_EQUAL_PRINT(1, execution_position.value().instr_number);

		auto resume_response = vm::api::resume(pid);
		assertTrue(resume_response.has_value(), "Resume failed (1)");

		auto position = vm::api::pause(pid);
		assertTrue(position.has_value(), "Pause failed (1)");
		assertTrue(
			4 <= position.value().instr_number && position.value().instr_number <= 9,
			"Line number is not correct (1)"
		);

		auto expected_next_line = [this](u64 x) -> u64 {
			if (x == 4) return 7;  // Mapped -> Not mapped
			if (x == 7) return 5;  // Not mapped -> Mapped
			if (x == 5) return 9;  // Mapped -> Not mapped
			if (x == 9) return 4;  // Not mapped -> Mapped
			this->fail("Unexpected line number: " + std::to_string(x));
			CORE_UNREACHABLE();
		};
		auto line_number2 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(position.value().instr_number), line_number2);

		auto line_number3 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number2), line_number3);

		auto line_number4 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number3), line_number4);

		auto line_number5 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number4), line_number5);

		auto line_number6 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number5), line_number6);

		resume_response = vm::api::resume(pid);
		assertTrue(resume_response.has_value(), "Resume failed (2)");

		auto stop_response = vm::api::stop(pid);
		assertTrue(stop_response.has_value(), "Stop failed");
	}

	/**
	 * @brief Checks if the program will pause on user request.
	 * The program is an infinite loop, so it will never stop.
	 * This test is the same as `pausesExecution` but with disabled mapping to check if the pause
	 * and step work correctly without it.
	 */
	void pausesExecutionWithoutMapping() {
		auto pid = loadProgram("while_true.dbc", false);

		auto run_response = vm::api::run(pid);
		assertTrue(run_response.has_value(), "Run failed");

		// We want to assure that the start function already managed to call main for the test to
		// work correctly.
		auto execution_position = vm::api::waitForBreakpoint(pid);
		assertTrue(execution_position.has_value(), "Wait for breakpoint failed (1)");
		// Because of stepGILs are inserted, there are more instructions.
		ASSERT_EQUAL_PRINT(2, execution_position.value().instr_number);

		auto resume_response = vm::api::resume(pid);
		assertTrue(resume_response.has_value(), "Resume failed (1)");

		auto position = vm::api::pause(pid);
		assertTrue(position.has_value(), "Pause failed (1)");
		ASSERT_TRUE(6 <= position.value().instr_number && position.value().instr_number <= 9);

		auto expected_next_line = [this](u64 x) -> u64 {
			if (x == 6) return 7;
			if (x == 7) return 8;
			if (x == 8) return 9;
			if (x == 9) return 6;
			this->fail("Unexpected line number: " + std::to_string(x));
			CORE_UNREACHABLE();
		};

		auto line_number2 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(position.value().instr_number), line_number2);

		auto line_number3 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number2), line_number3);

		auto line_number4 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number3), line_number4);

		auto line_number5 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number4), line_number5);

		auto line_number6 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number5), line_number6);

		resume_response = vm::api::resume(pid);
		assertTrue(resume_response.has_value(), "Resume failed (2)");

		auto stop_response = vm::api::stop(pid);
		assertTrue(stop_response.has_value(), "Stop failed");
	}

	u64 stepAndGetLine(u64 pid) {
		auto step_response = vm::api::step(base::safeIntConv<vm::PID>(pid));
		assertTrue(step_response.has_value(), "Step failed");

		auto execution_position = vm::api::getCurrentPosition(base::safeIntConv<vm::PID>(pid));
		assertTrue(execution_position.has_value(), "Get current position failed");
		return execution_position.value().instr_number;
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
