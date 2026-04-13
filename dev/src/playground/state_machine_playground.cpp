#include "base/except/exceptions.hpp"
#include "base/pointers/ref.hpp"
#include <base/comptime/type_traits.hpp>

#include <expected>
#include <iostream>
#include <tuple>
#include <variant>

template<class T>
concept IsState = requires(T t) {
	requires base::IsInstantiationOf<typename T::NextStates, std::tuple>;
};

template<class T, class... Ts>
constexpr bool IS_IN = std::disjunction_v<std::is_same<std::decay_t<T>, Ts>...>;

template<class... States>
requires(IsState<States> && ...) struct StateMachine {
	template<class DefaultState, class... Args>
	requires(IS_IN<DefaultState, States...>)
	StateMachine(DefaultState&& state): current_state(std::forward<DefaultState>(state)) {}

	std::variant<States...> current_state;

	template<class NextState, class... Args>
	requires(IS_IN<NextState, States...>) bool tryTransitionTo(Args&&... constructor_args) {
		return std::visit(
			[&](const auto& state) {
				using StateT = std::decay_t<decltype(state)>;
				if constexpr (base::IsTupleMember<NextState, typename StateT::NextStates>) {
					current_state = NextState(std::forward<Args>(constructor_args)...);
					return true;
				} else {
					return false;
                }
			},
			current_state
		);
	}

	template<class... GoodStates>
	requires(IS_IN<GoodStates, States...> && ...) bool isInState() {
		return (std::holds_alternative<std::decay_t<GoodStates>>(current_state) || ...);
	}

	template<class State>
	State& getState() {
		return std::get<State>(current_state);
	}

	template<class State>
	base::Optional<Ref<State>> getStateOpt() {
		if (std::holds_alternative<State>(current_state))
			return Ref<State>(&std::get<State>(current_state));
		return {};
	}
};

template<class T, class... States>
concept StateMachineEvent = requires(T t, StateMachine<States...> state_machine) {
    {t.handleEvent(state_machine) } -> base::IsInstantiationOf<std::expected>;
    typename T::RequiredStates;
};


template<class StateTuple, class EventTuple>
requires(base::IsInstantiationOf<StateTuple, std::tuple> && base::IsInstantiationOf<EventTuple, std::tuple>)
struct EventDrivenStateMachine;

template<class... States, class... Events>
requires((StateMachineEvent<Events> && ...))
struct EventDrivenStateMachine<std::tuple<States...>, std::tuple<Events...>>: public StateMachine<States...> {
    using StateMachine<States...>::StateMachine;

	template<class EvT>
	requires(IS_IN<EvT, Events...>) auto processEvent(EvT&& event) -> base::Optional<
			decltype(std::declval<EvT>().handleEvent(std::declval<StateMachine<States...>&>()))
		> {
        bool is_state_acceptable = std::visit(
            [&](const auto& state) -> bool {
                using StateT = std::decay_t<decltype(state)>;
                return base::IsTupleMember<StateT, typename EvT::RequiredStates>;
            },
            this->current_state
        );
        if (!is_state_acceptable) return std::nullopt;
        return event.handleEvent(*this);
    }
};

struct Process {
	struct EmptyProcess;
    struct LoadedCode;
	struct Running;
	struct Done;

    ///////// States /////////
	struct EmptyProcess {
		using NextStates = std::tuple<LoadedCode, Done>;
	};

    struct LoadedCode {
		std::vector<std::string> files;
        using NextStates = std::tuple<Running, Done>;
    };


	struct Running {
		Running(std::string program): program(std::move(program)) {}

		std::string program;
		int         progress{ 2'137 };

		using NextStates = std::tuple<Done>;
	};

	struct Done {
		using NextStates = std::tuple<>;
	};
    static_assert(IsState<EmptyProcess>);
    static_assert(IsState<LoadedCode>);
    static_assert(IsState<Running>);
    static_assert(IsState<Done>);


    ///////// Events /////////

    struct LoadFilesEvent {
        using RequiredStates = std::tuple<LoadedCode, EmptyProcess>;

        std::string file;

        template<class... States>
        std::expected<void, std::string> handleEvent(
            StateMachine<States...>& state_machine
        ) {
            // Handle event assumes we are in one of the required states,
            // so we don't check that here. We just perform the event logic.
            if(file == "bad_program")
                return std::unexpected("Failed to load file: " + file);

            if(!state_machine.template isInState<LoadedCode>())
                state_machine.template tryTransitionTo<LoadedCode>();

            auto& loaded_state = state_machine.template getState<LoadedCode>();
            loaded_state.files.push_back(file);
            std::cout << "Loaded file: " << file << '\n';

            return {};
        }
    };
    static_assert(StateMachineEvent<LoadFilesEvent>);

	EventDrivenStateMachine<
        std::tuple<EmptyProcess, LoadedCode, Running, Done>,
        std::tuple<LoadFilesEvent>
    > state_machine{ EmptyProcess{} };

	std::expected<void, std::string> run() {
		if (!state_machine.tryTransitionTo<Running>("good_program"))
			return std::unexpected("Failed to switch state to running");

		return {};
	}

	bool step() {
		if (!state_machine.isInState<Running>()) {
			std::cout << "Process is not running, cannot step.\n";
			return false;
		}
		state_machine.getState<Running>().progress += 1;
		return true;
	}

	std::expected<int, std::string> getProgress() {
		match_optional(state_machine.getStateOpt<Running>()) {
			opt_some(running_state) return running_state->progress;
			opt_none return std::unexpected("Process is not running, cannot get progress");
		}
		CORE_UNREACHABLE();
	}
};

int main() {
	Process process;

	// 1. State == EmptyProcess
	// getProgress should fail.
	std::expected result = process.getProgress();
	CORE_ASSERT(!result.has_value(), "Should not work, because in wrong state");
	std::cout << "Error: " << result.error() << '\n';

    // 2. State -> LoadedCode
    // Load a file
    base::Optional<std::expected<void, std::string>> load_result = process.state_machine.processEvent(Process::LoadFilesEvent{ "good_program" });
    CORE_ASSERT(load_result.has_value(), "Loading did not work");
    CORE_ASSERT(load_result->has_value(), "Loading did not work: ", "Loading failed: ", load_result->error());

    // 3. Load more files
    load_result = process.state_machine.processEvent(Process::LoadFilesEvent{ "another_file" });
    CORE_ASSERT(load_result.has_value(), "Loading did not work");
    CORE_ASSERT(load_result->has_value(), "Loading did not work: ", "Loading failed: ", load_result->error());

	// 4. State -> Running
	std::expected run_result = process.run();
	CORE_ASSERT(run_result.has_value(), "Running did not work: ", run_result.error());

    // 5. State == Running
    // Loading should not work, because we are in the wrong state.
    load_result = process.state_machine.processEvent(Process::LoadFilesEvent{ "file3" });
    CORE_ASSERT(!load_result.has_value(), "Loading worked");

	// 6. State == Running
	// getProgress should work
	std::expected progress = process.getProgress();
	CORE_ASSERT(progress.has_value(), "getProgress did not work: ", progress.error());
	std::cout << "Progress: " << progress.value() << '\n';
	// run should not work
	std::expected run_result2 = process.run();
	CORE_ASSERT(!run_result2.has_value(), "Running did not work");
	std::cout << "Error: " << run_result2.error() << '\n';


	return 0;
}
