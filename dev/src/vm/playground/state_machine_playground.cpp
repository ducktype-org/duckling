#include "base/collections/optional.hpp"
#include "base/comptime/type_traits.hpp"
#include "base/except/exceptions.hpp"
#include "base/extend_cpp/variant_match.hpp"
#include "base/pointers/ref.hpp"

#include "init/init.hpp"

#include "vm/api/data/thread_id.hpp"

#include <expected>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace {
	template<typename T>
	concept IsVariant = requires(T t) { std::visit([](auto&&) {}, t); };

	// Helper to get the variant index at compile time.
	template<typename Variant, typename T, usize Index = 0>
	static constexpr usize variantIndex() {
		if constexpr (Index >= std::variant_size_v<Variant>)
			return Index;
		else if constexpr (std::is_same_v<std::variant_alternative_t<Index, Variant>, T>)
			return Index;
		else
			return variantIndex<Variant, T, Index + 1>();
	}

}

namespace state_machine {

	template<typename States, typename Events, typename ErrorT = std::string>
	requires IsVariant<States> && IsVariant<Events> class StateMachineDefinition {
	private:
		using ActionResultT = std::expected<States, ErrorT>;
		using GuardResultT  = std::expected<void, ErrorT>;

		// Type erased handlers, to store them in one map.
		using RawAction = std::function<ActionResultT(const States&, const Events&)>;
		using RawGuard  = std::function<GuardResultT(const States&, const Events&)>;

		template<typename Event>
		using GenericAction = std::function<ActionResultT(const States&, const Event&)>;
		template<typename Event>
		using GenericGuard = std::function<GuardResultT(const States&, const Event&)>;

		struct TransitionEntry {
			RawAction                action;
			base::Optional<RawGuard> guard;
		};

		static constexpr usize NUM_STATES = std::variant_size_v<States>;
		static constexpr usize NUM_EVENTS = std::variant_size_v<Events>;
		// (State, Event) -> (Action, Guard)
		std::array<std::array<base::Optional<TransitionEntry>, NUM_EVENTS>, NUM_STATES> transitions;

		[[nodiscard]] const base::Optional<TransitionEntry>& getEntry(
			usize state_idx, usize event_idx
		) const {
			CORE_ASSERT(
				state_idx < NUM_STATES && event_idx < NUM_EVENTS,
				"State or Event index out of bounds"
			);
			return transitions[state_idx][event_idx];
		}

	public:
		// Adds a allowed transition from one State (each of them being a member of the
		// States variant) and for a given Event(member of the Events variant)
		template<typename State, typename Event>
		requires base::IsVariantMember<State, States> && base::IsVariantMember<Event, Events>
		void addTransition(
			std::function<ActionResultT(const State&, const Event&)>                action,
			base::Optional<std::function<GuardResultT(const State&, const Event&)>> guard
		) {
			usize state_idx = variantIndex<States, State>();
			usize event_idx = variantIndex<Events, Event>();

			RawAction raw_action
				= [action = std::move(action)](const States& s, const Events& e) -> ActionResultT {
				return action(std::get<State>(s), std::get<Event>(e));
			};

			base::Optional<RawGuard> raw_guard = std::nullopt;
			if (guard.has_value()) {
				raw_guard =
					[guard = std::move(*guard)](const States& s, const Events& e) -> GuardResultT {
					return (guard) (std::get<State>(s), std::get<Event>(e));
				};
			}

			transitions.at(state_idx).at(event_idx)
				= TransitionEntry{ std::move(raw_action), std::move(raw_guard) };
		}

		// Adds a allowed transition from all the FromStates(each of them being a member of the
		// States variant) and for a given Event(member of the Events variant)
		template<typename Event, typename... FromStates>
		requires(base::IsVariantMember<FromStates, States> && ...)
		     && base::IsVariantMember<Event, Events>
		void addTransitions(
			const GenericAction<Event>& action, const base::Optional<GenericGuard<Event>>& guard = {}
		) {
			(addTransitionInternal<FromStates, Event>(action, guard), ...);
		}

		// Util which adds a transition from all states. Usefull for actions which can be performed
		// from every state.
		template<typename Event>
		requires base::IsVariantMember<Event, Events> void addTransitionFromAllStates(
			const GenericAction<Event>& action, const base::Optional<GenericGuard<Event>>& guard = {}
		) {
			[this, &action, &guard]<usize... Is>(std::index_sequence<Is...>) {
				(this->addTransitionInternal<std::variant_alternative_t<Is, States>, Event>(
					 action, guard
				 ),
				 ...);
			}(std::make_index_sequence<NUM_STATES>{});
		}

		// Returns a transition bound to the (state, event) pair.
		template<typename State, typename Event>
		requires base::IsVariantMember<State, States> && base::IsVariantMember<Event, Events>
		[[nodiscard]] base::Optional<CRef<TransitionEntry>> getTransition() const {
			const usize state_idx = variantIndex<States, State>();
			const usize event_idx = variantIndex<Events, Event>();

			CORE_ASSERT(state_idx < NUM_STATES && event_idx < NUM_EVENTS, "Index out of bounds");
			const auto& entry = transitions.at(state_idx).at(event_idx);
			if (entry.has_value()) return CRef(&entry.value());
			return {};
		}

	private:
		template<typename State, typename Event>
		void addTransitionInternal(
			const GenericAction<Event>& action, const base::Optional<GenericGuard<Event>> guard
		) {
			const usize state_idx = variantIndex<States, State>();
			const usize event_idx = variantIndex<Events, Event>();

			// Not so strongly typed wrapper over action so they can be stored in one array.
			RawAction raw_action = [action = std::move(action)](const States& s, const Events& e) {
				return action(s, std::get<Event>(e));
			};

			// Not so strongly typed wrapper over guard so they can be stored in one array.
			base::Optional<RawGuard> raw_guard = std::nullopt;
			if (guard.has_value()) {
				raw_guard =
					[guard = std::move(*guard)](const States& s, const Events& e) -> GuardResultT {
					return (guard) (std::get<State>(s), std::get<Event>(e));
				};
			}

			transitions.at(state_idx).at(event_idx)
				= TransitionEntry{ std::move(raw_action), std::move(raw_guard) };
		}
	};

	template<typename States, typename Events, typename ErrorT = std::string>
	class StateMachine {
	public:
		using ResultT         = base::Optional<std::expected<void, ErrorT>>;
		using StateMachineDef = StateMachineDefinition<States, Events, ErrorT>;

		StateMachine(States initial_state, const StateMachineDef& def):
			  current_state(std::move(initial_state)),
			  definition(def) {}

		// Optional if the transition doesn't exist. std::expected if the transition succeeded,
		// std::unexpected if the transition failed.
		ResultT handleEvent(const Events& event) {
			return std::visit(
				[&](auto&& inner_state, auto&& inner_event) -> ResultT {
					using State           = std::decay_t<decltype(inner_state)>;
					using Event           = std::decay_t<decltype(inner_event)>;
					auto maybe_transition = definition.template getTransition<State, Event>();
					if (!maybe_transition.has_value()) return std::nullopt;

					const auto& transition = maybe_transition.value();

					// Check if the guard exists and it's satisfied.
					if (transition->guard.has_value()) {
						auto guard_result = (*transition->guard)(current_state, event);
						if (!guard_result.has_value())
							return std::unexpected(std::move(guard_result.error()));
					}

					// Perform the state transition.
					auto action_result = (transition->action)(current_state, event);
					if (!action_result.has_value())
						return std::unexpected(std::move(action_result.error()));

					current_state = std::move(action_result.value());
					return std::expected<void, ErrorT>{};
				},
				current_state,
				event
			);
		}

		[[nodiscard]] const States& getState() const { return current_state; }

	private:
		States                 current_state;
		const StateMachineDef& definition;
	};
}

namespace state {
	struct NotStarted {};

	struct Ready {};

	struct Running {};

	struct Paused {
		vm::api::ThreadID thread_id;
	};

	struct Panicked {
		std::string msg;
	};

	using VMProcessState = std::variant<NotStarted, Ready, Running, Paused, Panicked>;
}

namespace event {
	struct LoadCode {
		std::vector<i32> code;
	};

	struct Run {};

	struct Pause {};

	struct GetType {};

	struct Kill {};

	using VMProcessEvent = std::variant<LoadCode, Run, Pause, GetType, Kill>;
}

class VMProcess {
private:
	static const state_machine::StateMachineDefinition<state::VMProcessState, event::VMProcessEvent>& getConfig(
	) {
		static state_machine::StateMachineDefinition<state::VMProcessState, event::VMProcessEvent>
			config;

		// TODOP: parameter pack stanów
		config.addTransitions<event::LoadCode, state::NotStarted, state::Ready>(
			[](const state::VMProcessState&, const event::LoadCode& code) -> state::VMProcessState {
				std::cout << "Load code\n";
				if (code.code[0] == 1) return state::Panicked("error");
				return state::Ready{};
			},
			{}
		);

		config.addTransitions<event::Run, state::Ready, state::Paused>(
			[](const state::VMProcessState&, const event::Run&) -> state::VMProcessState {
				std::cout << "Running VMThread\n";
				return state::Running{};
			},
			{}
		);
		config.addTransition<state::Ready, event::GetType>(
			[](const state::Ready&, const event::GetType&) -> state::VMProcessState {
				std::cout << "Running VMThread\n";
				return state::Ready{};
			},
			{}
		);

		config.addTransition<state::Running, event::Pause>(
			[](const state::Running&, const event::Pause&) -> state::VMProcessState {
				std::cout << "VMThread paused\n";
				return state::Paused{};
			},
			{}
		);

		config.addTransitionFromAllStates<event::Kill>([](const state::VMProcessState&,
		                                                  const event::Kill&) {
			std::cout << "Killing process\n";
			return state::NotStarted{};
		});

		return config;
	}

	state_machine::StateMachine<state::VMProcessState, event::VMProcessEvent> state_machine;

public:
	using ErrorT = std::expected<void, std::string>;

	VMProcess(): state_machine(state::NotStarted{}, getConfig()) {}

	ErrorT load(std::vector<i32> code) {
		auto res = state_machine.handleEvent(event::LoadCode{ std::move(code) });
		if (res.has_value()) return res.value();
		return std::unexpected("No transition found");
	}

	ErrorT start() {
		auto res = state_machine.handleEvent(event::Run{});
		if (res.has_value()) return res.value();
		return std::unexpected("No transition found");
	}

	ErrorT pause() {
		auto res = state_machine.handleEvent(event::Pause{});
		if (res.has_value()) return res.value();
		return std::unexpected("No transition found");
	}

	ErrorT kill() {
		auto res = state_machine.handleEvent(event::Kill{});
		if (res.has_value()) return res.value();
		return std::unexpected("No transition found");
	}

	void printStatus() {
		variant_match(state_machine.getState()) {
			variant_case_novalue(state::NotStarted) std::cout << "State: Not started\n";
			variant_case_novalue(state::Ready) std::cout << "State: Ready\n";
			variant_case_novalue(state::Running) std::cout << "State: Running\n";
			variant_case_novalue(state::Paused) std::cout << "State: Paused\n";
			variant_case_novalue(state::Panicked) std::cout << "State: Panicked\n";
			variant_default { CORE_UNREACHABLE(); }
		}
	}
};

int main() {
	init::InitObject _;
	VMProcess        vm;

	vm.printStatus();  // Not started
	CORE_ASSERT(!vm.start().has_value(), "Running empty on not code should fail");

	CORE_ASSERT(vm.load({ 2, 2, 3 }).has_value(), "Load should succeed");
	vm.printStatus();  // Ready

	CORE_ASSERT(vm.start().has_value(), "Start should succeed");
	CORE_ASSERT(!vm.start().has_value(), "Double start should fail");
	vm.printStatus();  // Running

	CORE_ASSERT(vm.pause().has_value(), "Pause should succeed");
	CORE_ASSERT(!vm.pause().has_value(), "Double pause should fail");
	vm.printStatus();  // Paused
	CORE_ASSERT(vm.start().has_value(), "Run after start should succeed");
	vm.printStatus();  // Running

	CORE_ASSERT(vm.kill().has_value(), "Kill should always succeed");
	vm.printStatus();  // Running

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
