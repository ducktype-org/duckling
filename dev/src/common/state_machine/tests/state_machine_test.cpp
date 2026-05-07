#include <state_machine/state_machine.hpp>

#include <tester/tester.hpp>

#include <expected>
#include <string>
#include <variant>

class StateMachineTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS StateMachineTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(basicTransitionTest);
		TESTER_ADD_TEST(noTransitionTest);
		TESTER_ADD_TEST(multipleSourceStatesTest);
		TESTER_ADD_TEST(allStatesTransitionTest);
		TESTER_ADD_TEST(transitionToDifferentStateTest);
		TESTER_ADD_TEST(guardAllowedTest);
		TESTER_ADD_TEST(guardNotAllowedTest);
		TESTER_ADD_TEST(failedActionTest);
		TESTER_ADD_TEST(stateWithDataTest);
		TESTER_ADD_TEST(eventWithDataTest);
		TESTER_ADD_TEST(getStateReturnsCurrentTest);
		TESTER_ADD_TEST(definitionSharingTest);
	}

	~StateMachineTest() override = default;

private:
	struct Red {};

	struct Yellow {};

	struct Green {};

	using LightState = std::variant<Red, Yellow, Green>;

	struct Tick {};

	struct Reset {};

	using LightEvent = std::variant<Tick, Reset>;

	using LightDefinition = state_machine::StateMachineDefinition<LightState, LightEvent>;
	using LightMachine    = state_machine::StateMachine<LightState, LightEvent>;

	using LightGuardFn = std::function<std::expected<void, std::string>(const Red&, const Tick&)>;
	using LightActionFn
		= std::function<std::expected<LightState, std::string>(const Red&, const Tick&)>;

	void basicTransitionTest() {
		LightDefinition def;
		def.addTransition<Red, Tick>([](const Red&, const Tick&) -> LightState { return Green{}; });

		LightMachine m(Red{}, &def);
		assertTrue(std::holds_alternative<Red>(m.getState()), "Initial state should be Red");

		auto res = m.handleEvent(Tick{});
		assertTrue(res.has_value(), "Transition should be configured");
		assertTrue(res.value().has_value(), "Transition should succeed");
		assertTrue(std::holds_alternative<Green>(m.getState()), "After Tick should be Green");
	}

	void noTransitionTest() {
		LightDefinition def;
		def.addTransition<Red, Tick>([](const Red&, const Tick&) -> LightState { return Green{}; });

		// (Green, Tick) doesn't exist.
		LightMachine m(Green{}, &def);
		auto         res = m.handleEvent(Tick{});
		assertTrue(!res.has_value(), "Should return nullopt when no transition is configured");
		assertTrue(std::holds_alternative<Green>(m.getState()), "State should be unchanged");
	}

	void multipleSourceStatesTest() {
		LightDefinition def;
		def.addTransitions<Reset, Yellow, Green>([](const LightState&, const Reset&) -> LightState {
			return Red{};
		});

		LightMachine m1(Yellow{}, &def);
		auto         r1 = m1.handleEvent(Reset{});
		assertTrue(r1.has_value() && r1.value().has_value(), "Yellow + Reset should succeed");
		assertTrue(std::holds_alternative<Red>(m1.getState()), "Yellow + Reset -> Red");

		LightMachine m2(Green{}, &def);
		auto         r2 = m2.handleEvent(Reset{});
		assertTrue(r2.has_value() && r2.value().has_value(), "Green + Reset should succeed");
		assertTrue(std::holds_alternative<Red>(m2.getState()), "Green + Reset -> Red");

		// Red was not in FromStates, so Reset is undefined for it.
		LightMachine m3(Red{}, &def);
		auto         r3 = m3.handleEvent(Reset{});
		assertTrue(!r3.has_value(), "No transition for (Red, Reset)");
		assertTrue(std::holds_alternative<Red>(m3.getState()), "Red unchanged");
	}

	void allStatesTransitionTest() {
		LightDefinition def;
		def.addTransitionFromAllStates<Reset>([](const LightState&, const Reset&) -> LightState {
			return Red{};
		});

		LightMachine m1(Yellow{}, &def);
		m1.handleEvent(Reset{});
		assertTrue(std::holds_alternative<Red>(m1.getState()), "Yellow -> Red");

		LightMachine m2(Green{}, &def);
		m2.handleEvent(Reset{});
		assertTrue(std::holds_alternative<Red>(m2.getState()), "Green -> Red");

		LightMachine m3(Red{}, &def);
		m3.handleEvent(Reset{});
		assertTrue(std::holds_alternative<Red>(m3.getState()), "Red -> Red still works");
	}

	void transitionToDifferentStateTest() {
		// Red -> Green -> Yellow -> Red on successive Ticks.
		LightDefinition def;
		def.addTransition<Red, Tick>([](const Red&, const Tick&) -> LightState { return Green{}; });
		def.addTransition<Green, Tick>([](const Green&, const Tick&) -> LightState {
			return Yellow{};
		});
		def.addTransition<Yellow, Tick>([](const Yellow&, const Tick&) -> LightState {
			return Red{};
		});

		LightMachine m(Red{}, &def);
		m.handleEvent(Tick{});
		assertTrue(std::holds_alternative<Green>(m.getState()), "Red -> Green");
		m.handleEvent(Tick{});
		assertTrue(std::holds_alternative<Yellow>(m.getState()), "Green -> Yellow");
		m.handleEvent(Tick{});
		assertTrue(std::holds_alternative<Red>(m.getState()), "Yellow -> Red");
	}

	void guardAllowedTest() {
		LightDefinition def;
		def.addTransition<Red, Tick>(
			[](const Red&, const Tick&) -> LightState { return Green{}; },
			LightGuardFn{
				[](const Red&, const Tick&) -> std::expected<void, std::string> { return {}; } }
		);

		LightMachine m(Red{}, &def);
		auto         res = m.handleEvent(Tick{});
		assertTrue(res.has_value(), "Transition is configured");
		assertTrue(res.value().has_value(), "Guard should allow transition");
		assertTrue(std::holds_alternative<Green>(m.getState()), "State should be Green");
	}

	void guardNotAllowedTest() {
		LightDefinition def;
		def.addTransition<Red, Tick>(
			[](const Red&, const Tick&) -> LightState { return Green{}; },
			LightGuardFn{ [](const Red&, const Tick&) -> std::expected<void, std::string> {
				return std::unexpected("guard says no");
			} }
		);

		LightMachine m(Red{}, &def);
		auto         res = m.handleEvent(Tick{});
		assertTrue(res.has_value(), "Transition is configured");
		assertTrue(!res.value().has_value(), "Guard should veto");
		assertTrue(res.value().error() == "guard says no", "Error message should be propagated");
		assertTrue(std::holds_alternative<Red>(m.getState()), "State should be unchanged");
	}

	void failedActionTest() {
		LightDefinition def;
		def.addTransition<Red, Tick>(LightActionFn{
			[](const Red&, const Tick&) -> std::expected<LightState, std::string> {
				return std::unexpected("action failed");
			} });

		LightMachine m(Red{}, &def);
		auto         res = m.handleEvent(Tick{});
		assertTrue(res.has_value(), "Transition is configured");
		assertTrue(!res.value().has_value(), "Action should fail");
		assertTrue(res.value().error() == "action failed", "Error message should be propagated");
		assertTrue(std::holds_alternative<Red>(m.getState()), "State should be unchanged");
	}

	struct Empty {};

	struct Counter {
		i32 value;
	};

	using CounterState = std::variant<Empty, Counter>;

	struct Init {
		i32 start;
	};

	struct Increment {
		i32 by;
	};

	using CounterEvent = std::variant<Init, Increment>;

	using CounterDefinition = state_machine::StateMachineDefinition<CounterState, CounterEvent>;
	using CounterMachine    = state_machine::StateMachine<CounterState, CounterEvent>;

	void stateWithDataTest() {
		CounterDefinition def;
		def.addTransition<Empty, Init>([](const Empty&, const Init& init) -> CounterState {
			return Counter{ init.start };
		});
		def.addTransition<Counter, Increment>(
			[](const Counter& c, const Increment& inc) -> CounterState {
				return Counter{ c.value + inc.by };
			}
		);

		CounterMachine m(Empty{}, &def);
		m.handleEvent(Init{ 10 });
		assertTrue(std::holds_alternative<Counter>(m.getState()), "Should be Counter after Init");
		assertTrue(std::get<Counter>(m.getState()).value == 10, "Counter should be 10");

		m.handleEvent(Increment{ 5 });
		assertTrue(std::get<Counter>(m.getState()).value == 15, "Counter should be 15");

		m.handleEvent(Increment{ 7 });
		assertTrue(std::get<Counter>(m.getState()).value == 22, "Counter should be 22");
	}

	void eventWithDataTest() {
		CounterDefinition def;
		i32               witnessed = 0;
		def.addTransition<Empty, Init>([&witnessed](const Empty&, const Init& init) -> CounterState {
			witnessed = init.start;
			return Counter{ init.start };
		});

		CounterMachine m(Empty{}, &def);
		m.handleEvent(Init{ 42 });
		assertTrue(witnessed == 42, "Action should receive the event payload");
	}

	void getStateReturnsCurrentTest() {
		CounterDefinition def;
		def.addTransition<Empty, Init>([](const Empty&, const Init& init) -> CounterState {
			return Counter{ init.start };
		});

		CounterMachine m(Empty{}, &def);
		assertTrue(std::holds_alternative<Empty>(m.getState()), "Initial state");

		// getState should reflect updates after a transition fires.
		m.handleEvent(Init{ 99 });
		assertTrue(std::holds_alternative<Counter>(m.getState()), "Updated state");
		assertTrue(std::get<Counter>(m.getState()).value == 99, "Updated value");
	}

	void definitionSharingTest() {
		// One definition can drive multiple independent state machines.
		LightDefinition def;
		def.addTransition<Red, Tick>([](const Red&, const Tick&) -> LightState { return Green{}; });

		LightMachine m1(Red{}, &def);
		LightMachine m2(Red{}, &def);

		m1.handleEvent(Tick{});
		assertTrue(std::holds_alternative<Green>(m1.getState()), "m1 should advance");
		assertTrue(std::holds_alternative<Red>(m2.getState()), "m2 should be unaffected");

		m2.handleEvent(Tick{});
		assertTrue(std::holds_alternative<Green>(m2.getState()), "m2 should advance independently");
	}
};

TESTER_COMMON_MAIN("/src/common/state_machine/tests/");
