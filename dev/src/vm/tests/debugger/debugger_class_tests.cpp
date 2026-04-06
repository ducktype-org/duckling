#include <poll.h>

#include <tester/tester.hpp>

#include <vm/debugger/debugger.hpp>

template<typename TVariant, typename... Ts>
static constexpr std::vector<usize> _altIndexes() {
	return { base::internal::alternativeIndex<vm::api::ProcStatus, Ts>()... };
}

#define altIndex(t)     base::internal::alternativeIndex<vm::api::ProcStatus, t>()
#define altIndexes(...) _altIndexes<__VA_ARGS__>()

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
	std::vector<usize> expected_statuses;

	int counter;

	events::Listener<vm::api::ProcStatus> listener
		= events::Listener<vm::api::ProcStatus>([&](const vm::api::ProcStatus& status) {
			  ASSERT_TRUE(counter < expected_statuses.size());
			  ASSERT_EQUAL_PRINT(expected_statuses[counter], status.index());
			  counter++;
		  });

	void debuggerEventLoop(vm::debugger::Debugger& debugger, size_t loop_counter) {
		while (loop_counter-- > 0) debugger.updateStatus();
	}

	void runAndGetStatus() {
		auto debugger = vm::debugger::Debugger(fs::File(path("while_true_no_breakpoint.dbc")));
		ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionNotStarted>(debugger.getStatus()));
		expected_statuses = {
			altIndex(vm::api::Running),
		};
		counter = 0;
		debugger.on_vm_status_change.attachListener(listener);
		debugger.runMain();
		debuggerEventLoop(debugger, expected_statuses.size() + 1);
		ASSERT_EQUAL_PRINT(expected_statuses.size(), counter);
	}

	void getStatusWait() {
		auto debugger = vm::debugger::Debugger(fs::File(path("vm_api_tests.dbc")));
		ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionNotStarted>(debugger.getStatus()));
		expected_statuses = {
			altIndex(vm::api::Running),
			altIndex(vm::api::WaitingForInput),
		};
		counter = 0;
		debugger.on_vm_status_change.attachListener(listener);
		debugger.runMain();
		debuggerEventLoop(debugger, expected_statuses.size() + 1);
		ASSERT_EQUAL_PRINT(expected_statuses.size(), counter);
	}

	void getStatusBreakpoint() {
		auto debugger = vm::debugger::Debugger(fs::File(path("breakpoint.dbc")));
		ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionNotStarted>(debugger.getStatus()));
		expected_statuses = {
			altIndex(vm::api::Running),
			altIndex(vm::api::Paused),
		};
		counter = 0;
		debugger.on_vm_status_change.attachListener(listener);
		debugger.runMain();
		debuggerEventLoop(debugger, expected_statuses.size() + 1);
		ASSERT_EQUAL_PRINT(expected_statuses.size(), counter);
	}

	void rerunTest() {
		auto debugger = vm::debugger::Debugger(fs::File(path("debugger_test.dbc")));
		ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionNotStarted>(debugger.getStatus()));
		expected_statuses = {
			altIndex(vm::api::Running),
			altIndex(vm::api::ExecutionCompleted),
		};
		debugger.on_vm_status_change.attachListener(listener);

		int loop = 3;

		while (loop --> 0)
		{
			counter = 0;
			debugger.runMain();
			debuggerEventLoop(debugger, expected_statuses.size() + 1);
			ASSERT_EQUAL_PRINT(expected_statuses.size(), counter);
		}
	}

};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
