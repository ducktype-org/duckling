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
		TESTER_ADD_TEST(getStatusWhileRunning);
		TESTER_ADD_TEST(getStatusAfterBreakpoint);
	}

private:
	std::vector<usize> expected_statuses;

	int                                   counter;
	events::Listener<vm::api::ProcStatus> listener
		= events::Listener<vm::api::ProcStatus>([&](vm::api::ProcStatus status) {
			  std::visit(
				  [](auto&& arg) -> void {
					  using T = std::decay_t<decltype(arg)>;
					  std::cerr << "vm status: " << TypeParseTraits<T>::NAME.data() << "\n";
				  },
				  status
			  );
			  ASSERT_TRUE(counter < indexes.size());
			  ASSERT_EQUAL_PRINT(indexes[counter], status.index());
			  counter++;
		  });

	void debuggerEventLoop(vm::debugger::Debugger& debugger, size_t loop_counter) {
		while (loop_counter-- > 0) debugger.updateStatus();
	}

	void runAndGetStatus() {
		auto debugger = vm::debugger::Debugger(fs::File(path("vm_api_tests.dbc")));
		ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionNotStarted>(debugger.getStatus()));
		expected_statuses = altIndexes(vm::api::Running, vm::api::ExecutionCompleted);
		debugger.on_vm_status_change.attachListener(listener);
		debugger.runMain();
		debuggerEventLoop(debugger, expected_statuses.size() + 1);
		ASSERT_EQUAL_PRINT(expected_statuses.size(), counter);
	}

	void getStatusWhileRunning() {
		auto debugger = vm::debugger::Debugger(fs::File(path("while_true_no_breakpoint.dbc")));
		ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionNotStarted>(debugger.getStatus()));
		expected_statuses = {
			altIndex(vm::api::Running),
		};
		debugger.on_vm_status_change.attachListener(listener);
		debugger.runMain();
		debuggerEventLoop(debugger, expected_statuses.size() + 1);
		ASSERT_EQUAL_PRINT(expected_statuses.size(), counter);
	}

	void getStatusAfterBreakpoint() {
		auto debugger = vm::debugger::Debugger(fs::File(path("breakpoint.dbc")));
		ASSERT_TRUE(std::holds_alternative<vm::api::ExecutionNotStarted>(debugger.getStatus()));
		expected_statuses = {
			altIndex(vm::api::Running),
			altIndex(vm::api::Paused),
		};
		debugger.on_vm_status_change.attachListener(listener);
		debugger.runMain();
		debuggerEventLoop(debugger, expected_statuses.size() + 1);
		ASSERT_EQUAL_PRINT(expected_statuses.size(), counter);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
