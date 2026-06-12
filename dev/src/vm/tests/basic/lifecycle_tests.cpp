#include <tester/tester.hpp>

#include <vm/core/process/lifecycle.hpp>

#include <variant>

/**
 * @brief Tests for the VMProcess/VMThread lifecycle transition table.
 */
class VmLifecycleTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmLifecycleTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(allowedTransitionsTest);
		TESTER_ADD_TEST(rejectedTransitionsTest);
		TESTER_ADD_TEST(terminalEventRoundTripTest);
	}

	~VmLifecycleTest() override = default;

private:
	/**
	 * @brief Dispatches `event` on a fresh machine in state `from`.
	 * @return The new state if the transition fired.
	 */
	static base::Optional<vm::api::ProcStatus> dispatch(
		vm::api::ProcStatus from, const vm::lifecycle::Event& event
	) {
		vm::lifecycle::Machine machine(std::move(from), &vm::lifecycle::statusTransitions());
		auto                   result = machine.handleEvent(event);
		if (!result.has_value() || !result.value().has_value()) return {};
		return result.value().value().state;
	}

	template<typename ExpectedState>
	void assertTransition(
		vm::api::ProcStatus from, const vm::lifecycle::Event& event, const std::string& what
	) {
		auto new_state = dispatch(std::move(from), event);
		assertTrue(new_state.has_value(), what + " should be allowed");
		assertTrue(
			std::holds_alternative<ExpectedState>(new_state.value()),
			what + " should produce the expected state"
		);
	}

	void assertRejected(
		vm::api::ProcStatus from, const vm::lifecycle::Event& event, const std::string& what
	) {
		assertTrue(!dispatch(std::move(from), event).has_value(), what + " should be rejected");
	}

	void allowedTransitionsTest() {
		using namespace vm::api;
		using namespace vm::lifecycle;

		assertTransition<Running>(NotStarted{}, Start{}, "NotStarted + Start");
		assertTransition<Running>(ExecutionCompleted{}, Start{}, "Completed + Start (reuse)");
		assertTransition<Running>(ExecutionPanicked{ "e" }, Start{}, "Panicked + Start (reuse)");

		assertTransition<Paused>(Running{}, Pause{}, "Running + Pause");
		assertTransition<Paused>(Paused{}, Pause{}, "Paused + Pause (step re-announce)");
		assertTransition<Running>(Paused{}, Resume{}, "Paused + Resume");

		assertTransition<Sleeping>(Running{}, Sleep{}, "Running + Sleep");
		assertTransition<Running>(Sleeping{}, Wake{}, "Sleeping + Wake");

		assertTransition<ExecutionCompleted>(Running{}, Complete{ {} }, "Running + Complete");
		assertTransition<ExecutionCompleted>(Paused{}, Complete{ {} }, "Paused + Complete");

		assertTransition<ExecutionPanicked>(Running{}, Panic{ "e" }, "Running + Panic");
		assertTransition<ExecutionPanicked>(Paused{}, Panic{ "e" }, "Paused + Panic");
		assertTransition<ExecutionPanicked>(Sleeping{}, Panic{ "e" }, "Sleeping + Panic");
		assertTransition<ExecutionPanicked>(
			ExecutionPanicked{ "e" }, Panic{ "e2" }, "Panicked + Panic (re-announce)"
		);

		assertTransition<ExecutionStopped>(Running{}, Stop{}, "Running + Stop");
		assertTransition<ExecutionStopped>(Sleeping{}, Stop{}, "Sleeping + Stop");

		assertTransition<NotStarted>(ExecutionCompleted{}, Reset{}, "Completed + Reset");
		assertTransition<NotStarted>(ExecutionPanicked{ "e" }, Reset{}, "Panicked + Reset");
		assertTransition<NotStarted>(NotStarted{}, Reset{}, "NotStarted + Reset (no-op)");
	}

	void rejectedTransitionsTest() {
		using namespace vm::api;
		using namespace vm::lifecycle;

		assertRejected(NotStarted{}, Pause{}, "NotStarted + Pause");
		assertRejected(NotStarted{}, Complete{ {} }, "NotStarted + Complete");
		assertRejected(Running{}, Start{}, "Running + Start");
		assertRejected(Running{}, Reset{}, "Running + Reset");
		assertRejected(Sleeping{}, Pause{}, "Sleeping + Pause");
		assertRejected(ExecutionCompleted{}, Pause{}, "Completed + Pause");
		assertRejected(ExecutionCompleted{}, Panic{ "e" }, "Completed + Panic");
		assertRejected(ExecutionPanicked{ "e" }, Complete{ {} }, "Panicked + Complete");
	}

	void terminalEventRoundTripTest() {
		using namespace vm::api;
		using namespace vm::lifecycle;

		auto completed = eventForTerminalStatus(ExecutionCompleted{});
		assertTrue(std::holds_alternative<Complete>(completed), "Completed maps to Complete");

		auto stopped = eventForTerminalStatus(ExecutionStopped{});
		assertTrue(std::holds_alternative<Stop>(stopped), "Stopped maps to Stop");

		auto panicked = eventForTerminalStatus(ExecutionPanicked{ "boom" });
		assertTrue(std::holds_alternative<Panic>(panicked), "Panicked maps to Panic");
		assertTrue(
			std::get<Panic>(panicked).error_message == "boom", "Panic keeps the error message"
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/basic/");
