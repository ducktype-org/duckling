#include "base/except/exceptions.hpp"
#include "base/pointers/ref.hpp"
#include <base/comptime/type_traits.hpp>

#include <expected>
#include <iostream>
#include <tuple>
#include <variant>
#include <numeric>

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

    template<class NextState, class... Args>
    void transitionTo(Args&&... constructor_args) {
        if (!tryTransitionTo<NextState, Args...>(std::forward<Args>(constructor_args)...)) {
            CORE_PANIC("Invalid state transition attempted");
        }
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

template<class T, class... Args>
concept StateMachineEvent = requires(T t, Args&&... args) {
    typename T::RequiredStates;
};


template<class StateTuple, class EventTuple>
requires(base::IsInstantiationOf<StateTuple, std::tuple> && base::IsInstantiationOf<EventTuple, std::tuple>)
struct EventDrivenStateMachine;

// During event handler addition we specify which states are acceptable for this event.
template<class... States, class... Events>
requires((StateMachineEvent<Events> && ...))
struct EventDrivenStateMachine<std::tuple<States...>, std::tuple<Events...>>: public StateMachine<States...> {
    using StateMachine<States...>::StateMachine;

	template<class EvT, class OperatingClass>
	requires(IS_IN<EvT, Events...>) auto processEvent(EvT&& event, OperatingClass& operating_class) -> std::conditional_t<std::is_void_v<decltype(operating_class.handleEvent(event))>, bool, base::Optional<decltype(operating_class.handleEvent(event))>> {
        bool is_state_acceptable = std::visit(
            [&](const auto& state) -> bool {
                using StateT = std::decay_t<decltype(state)>;
                return base::IsTupleMember<StateT, typename EvT::RequiredStates>;
            },
            this->current_state
        );
        if constexpr(std::is_void_v<decltype(operating_class.handleEvent(event))>) {
            if (!is_state_acceptable) return false;
            operating_class.handleEvent(event);
            return true;
        } else {
            if (!is_state_acceptable) return std::nullopt;
            return operating_class.handleEvent(event);
        }
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


    ///////// Events /////////

    struct LoadFilesEvent {
        using RequiredStates = std::tuple<LoadedCode, EmptyProcess>;

        std::string file;
    };
    std::expected<void, std::string> handleEvent(const LoadFilesEvent& event) {
        if(state_machine.isInState<EmptyProcess>())
            CORE_ASSERT(state_machine.tryTransitionTo<LoadedCode>(), "Transition should work, because we are in the right state");

        auto& state = state_machine.getState<LoadedCode>();
        std::cout << "Loading file: " << event.file << '\n';
        state.files.push_back(event.file);
        return {};
    }

    struct RunEvent {
        using RequiredStates = std::tuple<LoadedCode>;
    };
	void handleEvent(const RunEvent&) {
        std::vector<std::string> loaded_files = std::move(state_machine.getState<LoadedCode>().files);
        std::string program = std::accumulate(
            loaded_files.begin(), loaded_files.end(), std::string{}, std::plus<>()
        );
		state_machine.transitionTo<Running>(program);
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

    template<class Ev>
    auto processEvent(Ev&& event) {
        return state_machine.processEvent(std::forward<Ev>(event), *this);
    }

private:
	EventDrivenStateMachine<
            std::tuple<EmptyProcess, LoadedCode, Running, Done>,
            std::tuple<LoadFilesEvent, RunEvent>
    > state_machine{ EmptyProcess{} };
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
    base::Optional<std::expected<void, std::string>> load_result = process.processEvent(Process::LoadFilesEvent{ "good_program" });
    CORE_ASSERT(load_result.has_value(), "Loading did not work");
    CORE_ASSERT(load_result->has_value(), "Loading did not work: ", "Loading failed: ", load_result->error());

    // 3. Load more files
    load_result = process.processEvent(Process::LoadFilesEvent{ "another_file" });
    CORE_ASSERT(load_result.has_value(), "Loading did not work");
    CORE_ASSERT(load_result->has_value(), "Loading did not work: ", "Loading failed: ", load_result->error());

	// 4. State -> Running
	auto ran = process.processEvent(Process::RunEvent{});
    CORE_ASSERT(ran, "Running did not work");

    // 5. State == Running
    // Loading should not work, because we are in the wrong state.
    load_result = process.processEvent(Process::LoadFilesEvent{ "file3" });
    CORE_ASSERT(!load_result.has_value(), "Loading worked");

	// 6. State == Running
	// getProgress should work
	std::expected progress = process.getProgress();
	CORE_ASSERT(progress.has_value(), "getProgress did not work: ", progress.error());
	std::cout << "Progress: " << progress.value() << '\n';
	// run should not work
	bool run_result2 = process.processEvent(Process::RunEvent{});
	CORE_ASSERT(!run_result2, "Running did not work");


	return 0;
}
