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
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace state_machine {
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
		using RawAction
			= std::function<std::expected<States, std::string>(const States&, const Events&)>;
		using RawGuard = std::function<bool(const States&, const Events&)>;

		struct TransitionEntry {
			RawAction action;
			RawGuard  guard;
		};

		static constexpr usize NUM_STATES = std::variant_size_v<States>;
		static constexpr usize NUM_EVENTS = std::variant_size_v<Events>;

		// (State, Event) -> (Action, Guard)
		std::array<std::array<base::Optional<TransitionEntry>, NUM_EVENTS>, NUM_STATES> transitions;

	public:
		// TypeSafe definition of a transition.
		template<typename FromState, typename OnEvent>
		struct Transition {
			using Action
				= std::function<std::expected<States, std::string>(const FromState&, const OnEvent&)>;
			using Guard = std::function<bool(const FromState&, const OnEvent&)>;
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
			RawAction raw_action = [action = std::move(action)](
									   const States& s, const Events& e
								   ) -> std::expected<States, std::string> {
				return action(std::get<State>(s), std::get<Event>(e));
			};

			// Not so strongly typed wrapper over guard so they can be stored in one map.
			RawGuard raw_guard
				= [guard = std::move(guard)](const States& s, const Events& e) -> bool {
				if (!guard.has_value()) return true;
				return (*guard)(std::get<State>(s), std::get<Event>(e));
			};

			transitions.at(state_idx).at(event_idx)
				= TransitionEntry{ std::move(raw_action), std::move(raw_guard) };
		}

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
	};

	template<typename States, typename Events>
	class StateMachine {
	public:
		StateMachine(States initial_state, const StateMachineDefinition<States, Events>& def):
			  current_state(std::move(initial_state)),
			  definition(def) {}

		// Optional if the transition doesn't exist. std::expected if the transition succeded,
		// std::unexpected if the transition failed.
		base::Optional<std::expected<void, std::string>> handleEvent(const Events& event) {
			return std::visit(
				[&](auto&& inner_state,
			        auto&& inner_event) -> base::Optional<std::expected<void, std::string>> {
					using State           = std::decay_t<decltype(inner_state)>;
					using Event           = std::decay_t<decltype(inner_event)>;
					auto maybe_transition = definition.template getTransition<State, Event>();
					if (!maybe_transition.has_value()) return std::nullopt;

					const auto& transition = maybe_transition.value();

					// Check if the guard is satisfied.
					if (!transition->guard(current_state, event))
						return std::unexpected(std::string("Transition guard check failed."));

					// Perform the state transition.
					auto result = transition->action(current_state, event);
					if (result.has_value()) {
						current_state = std::move(result.value());
						return std::expected<void, std::string>{};
					} else {
						return std::unexpected(std::move(result.error()));
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
	static const state_machine::StateMachineDefinition<state::VMProcessState, event::VMProcessEvent>& getConfig(
	) {
		static state_machine::StateMachineDefinition<state::VMProcessState, event::VMProcessEvent>
			config;

		// TODOP: parameter pack stanów
		config.addTransition<state::NotStarted, event::LoadCode>(
			[](const state::NotStarted&, const event::LoadCode& code) -> state::VMProcessState {
				std::cout << "Load code\n";
				if (code.code[0] == 1) return state::Panicked("error");
				return state::Ready{};
			},
			{}
		);

		config.addTransition<state::Ready, event::Run>(
			[](const state::Ready&, const event::Run&) -> state::VMProcessState {
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

	return 0;
}
