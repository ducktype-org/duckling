/**
 * @file state_machine_example.cpp
 *
 * @brief Demonstrates how to model a small VM lifecycle using the
 * `state_machine` library.
 *
 * The VM walks the following states:
 *
 *     NotStarted --LoadCode--> Ready --Run--> Running
 *                                 ^             |
 *                                 |             | Pause
 *                                 |             v
 *                                 +--Run-------Paused
 *
 * In addition, `Kill` is accepted from every state and brings the VM
 * back to `NotStarted`. A faulty payload makes `LoadCode` transition
 * to a `Panicked` state instead of `Ready`.
 */

#include <state_machine/state_machine.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <expected>
#include <iostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace state {
	struct NotStarted {};

	struct Ready {};

	struct Running {};

	struct Paused {};

	struct Panicked {
		std::string msg;
	};

	using VMProcessState = std::variant<NotStarted, Ready, Running, Paused, Panicked>;
}

namespace event {
	struct LoadCode {
		std::vector<i32> code{};
	};

	struct Run {};

	struct Pause {};

	struct Kill {};

	using VMProcessEvent = std::variant<LoadCode, Run, Pause, Kill>;
}

class VMProcess {
public:
	using ErrorT = std::expected<void, std::string>;

	VMProcess(): machine(state::NotStarted{}, getConfig()) {}

	ErrorT load(std::vector<i32> code) {
		return toError(machine.handleEvent(event::LoadCode{ std::move(code) }));
	}

	ErrorT start() { return toError(machine.handleEvent(event::Run{})); }

	ErrorT pause() { return toError(machine.handleEvent(event::Pause{})); }

	ErrorT kill() { return toError(machine.handleEvent(event::Kill{})); }

	void printStatus() const {
		variant_match(machine.getState()) {
			variant_case_novalue(state::NotStarted) { std::cout << "State: NotStarted\n"; }
			variant_case_novalue(state::Ready) { std::cout << "State: Ready\n"; }
			variant_case_novalue(state::Running) { std::cout << "State: Running\n"; }
			variant_case_novalue(state::Paused) { std::cout << "State: Paused\n"; }
			variant_case(state::Panicked, p) { std::cout << "State: Panicked(" << p.msg << ")\n"; }
			variant_default { CORE_UNREACHABLE(); }
		}
	}

private:
	using Definition
		= state_machine::StateMachineDefinition<state::VMProcessState, event::VMProcessEvent>;
	using Machine = state_machine::StateMachine<state::VMProcessState, event::VMProcessEvent>;

	/**
	 * @brief Returns a process-wide, lazily initialized definition.
	 *
	 * The definition is configured on first call and shared by every
	 * `VMProcess` instance afterwards.
	 */
	static CRef<Definition> getConfig() {
		static const Definition config = [] {
			Definition cfg;

			// LoadCode: from NotStarted or Ready, becomes Ready (or Panicked on bad payload).
			cfg.addTransitions<event::LoadCode, state::NotStarted, state::Ready>(
				[](const state::VMProcessState&,
			       const event::LoadCode& load) -> state::VMProcessState {
					std::cout << "Loading code (" << load.code.size() << " words)\n";
					if (!load.code.empty() && load.code[0] == 1)
						return state::Panicked{ "magic byte 0x1 forbidden" };
					return state::Ready{};
				}
			);

			// Run: from Ready or Paused, becomes Running.
			cfg.addTransitions<event::Run, state::Ready, state::Paused>(
				[](const state::VMProcessState&, const event::Run&) -> state::VMProcessState {
					std::cout << "Running VM thread\n";
					return state::Running{};
				}
			);

			// Pause: only valid while Running.
			cfg.addTransition<state::Running, event::Pause>(
				[](const state::Running&, const event::Pause&) -> state::VMProcessState {
					std::cout << "VM thread paused\n";
					return state::Paused{};
				}
			);

			// Kill: always accepted, brings the VM back to NotStarted.
			cfg.addTransitionFromAllStates<event::Kill>(
				[](const state::VMProcessState&, const event::Kill&) -> state::VMProcessState {
					std::cout << "Killing VM\n";
					return state::NotStarted{};
				}
			);

			return cfg;
		}();
		return &config;
	}

	/**
	 * @brief Collapses the three-state `handleEvent` result down to a
	 * simple `std::expected<void, std::string>` for the public API.
	 *
	 * `std::nullopt` (no transition registered) is reported as an error
	 * so that callers cannot silently miss an unhandled event.
	 */
	static ErrorT toError(Machine::ResultT res) {
		if (res.has_value()) return res.value();
		return std::unexpected("No transition available for current state/event");
	}

	Machine machine;
};

int main() {
	VMProcess vm;

	vm.printStatus();  // NotStarted
	CORE_ASSERT(!vm.start().has_value(), "Run on NotStarted should be rejected");

	CORE_ASSERT(vm.load({ 2, 2, 3 }).has_value(), "Load should succeed");
	vm.printStatus();  // Ready

	CORE_ASSERT(vm.start().has_value(), "Start should succeed");
	CORE_ASSERT(!vm.start().has_value(), "Double start should be rejected");
	vm.printStatus();  // Running

	CORE_ASSERT(vm.pause().has_value(), "Pause should succeed");
	CORE_ASSERT(!vm.pause().has_value(), "Double pause should be rejected");
	vm.printStatus();  // Paused

	CORE_ASSERT(vm.start().has_value(), "Run after pause should succeed");
	vm.printStatus();  // Running

	CORE_ASSERT(vm.kill().has_value(), "Kill should always succeed");
	vm.printStatus();  // NotStarted

	CORE_ASSERT(vm.load({ 2, 2, 3 }).has_value(), "Load should succeed");
	CORE_ASSERT(vm.kill().has_value(), "Kill should always succeed");

	CORE_ASSERT(vm.load({ 2, 2, 3 }).has_value(), "Load should succeed");
	CORE_ASSERT(vm.start().has_value(), "Start should succeed");
	CORE_ASSERT(vm.kill().has_value(), "Kill should always succeed");

	CORE_ASSERT(vm.load({ 2, 2, 3 }).has_value(), "Load should succeed");
	CORE_ASSERT(vm.start().has_value(), "Start should succeed");
	CORE_ASSERT(vm.pause().has_value(), "Pause should succeed");
	CORE_ASSERT(vm.kill().has_value(), "Kill should always succeed");

	return 0;
}
