#include <poll.h>

#include <tester/tester.hpp>

#include <vm/debugger/debugger.hpp>

#define altIndex(t) base::internal::alternativeIndex<vm::api::ProcStatus, t>()

class VmDebuggerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmDebuggerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(runAndGetStatus);
		TESTER_ADD_TEST(getStatusWait);
		TESTER_ADD_TEST(getStatusBreakpoint);
		TESTER_ADD_TEST(rerunTest);
	}

private:
	void debuggerEventLoop(vm::debugger::Debugger& debugger, size_t loop_counter) {
		while (loop_counter-- > 0) debugger.updateStatus();
	}

	void testTemplate(std::string_view path_name, const std::vector<usize>& expected_statuses) {
		size_t counter = 0;

		events::Listener<vm::api::ProcStatus> listener
			= events::Listener<vm::api::ProcStatus>([&](const vm::api::ProcStatus& status) {
				  ASSERT_TRUE(counter < expected_statuses.size());
				  ASSERT_EQUAL_PRINT(expected_statuses[counter], status.index());
				  counter++;
			  });

		auto debugger = vm::debugger::Debugger(fs::File(path(std::string(path_name))));
		ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionNotStarted>(debugger.getStatus()));
		debugger.on_vm_status_change.attachListener(listener);
		debugger.runMain();
		debuggerEventLoop(debugger, expected_statuses.size() + 1);
		ASSERT_EQUAL_PRINT(expected_statuses.size(), counter);
	}

	void runAndGetStatus() {
		testTemplate("while_true_no_breakpoint.dbc", { altIndex(vm::api::Running) });
	}

	void getStatusWait() {
		testTemplate(
			"vm_api_tests.dbc",
			{
				altIndex(vm::api::Running),
				altIndex(vm::api::WaitingForInput),
			}
		);
	}

	void getStatusBreakpoint() {
		testTemplate(
			"breakpoint.dbc",
			{
				altIndex(vm::api::Running),
				altIndex(vm::api::Paused),
			}
		);
	}

	void rerunTest() {
		size_t counter = 0;

		const std::vector<usize> expected_statuses = {
			altIndex(vm::api::Running),
			altIndex(vm::api::ExecutionCompleted),
		};

		events::Listener<vm::api::ProcStatus> listener
			= events::Listener<vm::api::ProcStatus>([&](const vm::api::ProcStatus& status) {
				  ASSERT_TRUE(counter < expected_statuses.size());
				  ASSERT_EQUAL_PRINT(expected_statuses[counter], status.index());
				  counter++;
			  });

		auto debugger = vm::debugger::Debugger(fs::File(path("debugger_test.dbc")));
		ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionNotStarted>(debugger.getStatus()));

		debugger.on_vm_status_change.attachListener(listener);

		int loop = 3;

		while (loop-- > 0) {
			counter = 0;
			debugger.runMain();
			debuggerEventLoop(debugger, expected_statuses.size() + 1);
			ASSERT_EQUAL_PRINT(expected_statuses.size(), counter);
		}
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
