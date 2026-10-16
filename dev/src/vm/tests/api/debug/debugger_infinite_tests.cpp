// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

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
class VmDebugInfiniteTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebugInfiniteTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(pausesExecution); }

private:
	/**
	 * @brief Checks if the program will pause on user request.
	 * The program is an infinite loop, so it will never stop.
	 */
	void pausesExecution() {
		const vm::PID pid = spawnAndLoad("../../common/programs/while_true.dbc");

		assertSucceeded(vm::api::setBreakpoint(pid, base::StrID("main"), 0, true), "setBreakpoint");
		assertSucceeded(vm::api::run(pid), "run");

		// We want to assure that the start function already managed to call main for the test to
		// work correctly.
		const auto execution_position = vm::api::waitForBreakpoint(pid);
		assertSucceeded(execution_position, "waitForBreakpoint");
		ASSERT_EQUAL_PRINT(0, execution_position->instr_number);

		assertSucceeded(vm::api::resume(pid), "resume");
		const auto position = vm::api::pause(pid);
		assertSucceeded(position, "pause");
		ASSERT_EQUAL_PRINT(position->instr_number, 4);

		auto expected_next_line = [this](u64 x) -> u64 {
			if (x == 3) return 4;
			if (x == 4) return 3;
			this->fail("Unexpected line number: " + std::to_string(x));
			CORE_UNREACHABLE();
		};

		auto line_number2 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(position->instr_number), line_number2);

		auto line_number3 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number2), line_number3);

		auto line_number4 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number3), line_number4);

		auto line_number5 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number4), line_number5);

		auto line_number6 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number5), line_number6);

		auto line_number7 = stepAndGetLine(pid);
		ASSERT_EQUAL_PRINT(expected_next_line(line_number6), line_number7);

		assertSucceeded(vm::api::resume(pid), "resume");
		assertSucceeded(vm::api::stop(pid), "stop");
	}

	u64 stepAndGetLine(vm::PID pid) {
		assertSucceeded(vm::api::step(pid), "step");
		const auto execution_position = vm::api::getCurrentPosition(pid);
		assertSucceeded(execution_position, "getCurrentPosition");
		return execution_position->instr_number;
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/api/debug/");
