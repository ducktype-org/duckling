#include "base/collections/maps.hpp"
#include "base/collections/optional.hpp"
#include "base/comptime/type_traits.hpp"
#include "base/except/exceptions.hpp"
#include "base/extend_cpp/variant_match.hpp"
#include "base/pointers/ref.hpp"

#include "hashing/hash.hpp"
#include "init/init.hpp"

#include "vm/api/data/thread_id.hpp"

#include <expected>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace state_machine {
	struct PairHash {
		template<class T1, class T2>
		usize operator()(const std::pair<T1, T2>& p) const {
			auto h1 = std::hash<T1>{}(p.first);
			auto h2 = std::hash<T2>{}(p.second);
			return h1 ^ (h2 + 0x9e'37'79'b9 + (h1 << 6) + (h1 >> 2));
		}
	};

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

	template<typename States, typename Events>
	requires IsVariant<States> && IsVariant<Events> class StateMachineDefinition {
	private:
		// Type erased handlers, to store them in one map.
		using RawAction = std::function<States(const States&, const Events&)>;
		using RawGuard  = std::function<bool(const States&, const Events&)>;

		struct TransitionEntry {
			RawAction action;
			RawGuard  guard;
		};

		// Index of state and event in the variant.
		using KEY_T = std::pair<usize, usize>;
		// (State, Event) -> (Action, Guard)
		base::HashMap<KEY_T, TransitionEntry, PairHash> transitions;
		// TODOP: Pozbyc sie hashmapy

	public:
		// TypeSafe definition of a transition.
		template<typename FromState, typename OnEvent>
		struct Transition {
			using Action = std::function<States(const FromState&, const OnEvent&)>;
			using Guard  = std::function<bool(const FromState&, const OnEvent&)>;
		};

		// Adds a allowed transition from the FromState(member of the States variant) and for a
		// given OnEvent(member of the Events variant)
		template<typename State, typename Event>
		requires base::IsVariantMember<State, States> && base::IsVariantMember<Event, Events>
		void addTransition(
			typename Transition<State, Event>::Action                action,
			base::Optional<typename Transition<State, Event>::Guard> guard
		) {
			usize state_idx = variantIndex<States, State>();
			usize event_idx = variantIndex<Events, Event>();

			// Not so strongly typed wrapper over action so they can be stored in one map.
			RawAction raw_action
				= [action = std::move(action)](const States& s, const Events& e) -> States {
				return action(std::get<State>(s), std::get<Event>(e));
			};

			// Not so strongly typed wrapper over guard so they can be stored in one map.
			RawGuard raw_guard
				= [guard = std::move(guard)](const States& s, const Events& e) -> bool {
				if (!guard.has_value()) return true;
				return (*guard)(std::get<State>(s), std::get<Event>(e));
			};

			KEY_T key = std::make_pair(state_idx, event_idx);
			transitions.put(key, { std::move(raw_action), std::move(raw_guard) });
		}

		template<typename State, typename Event>
		requires base::IsVariantMember<State, States> && base::IsVariantMember<Event, Events>
		[[nodiscard]] base::Optional<CRef<TransitionEntry>> getTransition() const {
			usize state_idx = variantIndex<States, State>();
			usize event_idx = variantIndex<Events, Event>();
			return transitions.atMaybe({ state_idx, event_idx });
		}
	};

	template<typename States, typename Events>
	class StateMachine {
	public:
		StateMachine(States initial_state, const StateMachineDefinition<States, Events>& def):
			  current_state(std::move(initial_state)),
			  definition(def) {}

		// TODOP: optional
		std::expected<void, std::string> handleEvent(const Events& event) {
			return std::visit(
				[&](auto&& inner_state, auto&& inner_event) -> std::expected<void, std::string> {
					using State           = std::decay_t<decltype(inner_state)>;
					using Event           = std::decay_t<decltype(inner_event)>;
					auto maybe_transition = definition.template getTransition<State, Event>();
					if (maybe_transition.has_value()) {
						const auto& transition = maybe_transition.value();

						// Is guard is satisfied, then perform the action.
						if (transition->guard(current_state, event)) {
							current_state = transition->action(current_state, event);
							return {};
						} else {
							return std::unexpected(std::string("Transition guard check failed."));
						}

					} else {
						return std::unexpected(std::string("Transition not found"));
					}
				},
				current_state,
				event
			);
		}

		[[nodiscard]] const States& getState() const { return current_state; }


	private:
		States                                        current_state;
		const StateMachineDefinition<States, Events>& definition;
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


	using VMProcessEvent = std::variant<LoadCode, Run, Pause, GetType>;
}

class VMProcess {
private:
	static const state_machine::StateMachineDefinition<state::VMProcessState, event::VMProcessEvent>&
		getConfig() {
		static state_machine::StateMachineDefinition<state::VMProcessState, event::VMProcessEvent>
			config;

		// TODOP: parameter pack stanów
		config.addTransition<state::NotStarted, event::LoadCode>(
			[](const state::NotStarted&, const event::LoadCode& code) -> state::VMProcessState {
				std::cout << "Load code\n";
				if (code.code[0] == 1) return state::Panicked("error");

				return state::Ready{};
			}  // TODOP: Guard  for not loading code during execution
			,
			{}
		);

		config.addTransition<state::Ready, event::Run>(
			[](const state::Ready&, const event::Run&) -> state::VMProcessState {
				std::cout << "Running VMThread\n";
				return state::Running{};
			}  // TODOP: Guard  for not execution on non loaded code.
			,
			{}
		);
		config.addTransition<state::Ready, event::GetType>(
			[](const state::Ready&, const event::Run&) -> state::VMProcessState {
				std::cout << "Running VMThread\n";
				return state::Ready{};
			}  // TODOP: Guard  for not execution on non loaded code.
			,
			{}
		);

		config.addTransition<state::Running, event::Pause>(
			[](const state::Running&, const event::Pause&) -> state::VMProcessState {
				std::cout << "VMThread paused\n";
				return state::Paused{};
			}  // TODOP: Guard  for not execution on non loaded code.
			,
			{}
		);

		return config;
	}

	state_machine::StateMachine<state::VMProcessState, event::VMProcessEvent> state_machine;

public:
	using ErrorT = std::expected<void, std::string>;

	VMProcess(): state_machine(state::NotStarted{}, getConfig()) {}

	ErrorT load(std::vector<i32> code) {

		return state_machine.handleEvent(event::LoadCode{ std::move(code) });
	}

	ErrorT start() { return state_machine.handleEvent(event::Run{}); }

	ErrorT pause() { return state_machine.handleEvent(event::Pause{}); }

	void printStatus() {
		variant_match(state_machine.getState()) {
			variant_case_novalue(state::NotStarted) std::cout << "State: Not started\n";
			variant_case_novalue(state::Ready) std::cout << "State: Ready\n";
			variant_case_novalue(state::Running) std::cout << "State: Running\n";
			variant_case_novalue(state::Paused) std::cout << "State: Paused\n";
			variant_default { CORE_UNREACHABLE(); }
		}
	}
};

int main() {
	init::InitObject _;
	VMProcess        vm;

	vm.printStatus();  // Not started
	CORE_ASSERT(!vm.start().has_value(), "Running empty on not code should fail");

	CORE_ASSERT(vm.load({ 1, 2, 3 }).has_value(), "Load should succeed");
	vm.printStatus();  // Ready

	CORE_ASSERT(vm.start().has_value(), "Start should succeed");
	CORE_ASSERT(!vm.start().has_value(), "Double start should fail");
	vm.printStatus();  // Running

	CORE_ASSERT(vm.pause().has_value(), "Pause should succeed");
	CORE_ASSERT(!vm.pause().has_value(), "Double pause should fail");
	vm.printStatus();  // Paused

	return 0;
}
