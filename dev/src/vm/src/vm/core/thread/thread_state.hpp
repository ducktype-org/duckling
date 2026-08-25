/**
 * @brief All states and events available for a single VMThread and `applyThreadEvent` which is a
 * single source of truth about the legality of state transitions.
 *
 * @note: Transitions are applied to the process's `ProcessStateManager` ONLY by the VMThread's
 * execution thread. To change a VMThread's state from the outside, post a request on its
 * `ThreadSignal`.
 *
 * VMThread transition table:
 * 	State		|	Event		|	End State
 * 	----------------------------------------
 *  NotStarted ->	Spawn     	-> 	Running
 *  NotStarted ->	Kill      	-> 	Stopped
 *  Running    ->	Pause     	-> 	Paused
 *  Running    ->	EnterSleep	-> 	Sleeping 	(IO / Mutex etc.)
 *  Running    ->	Finish    	-> 	Completed
 *  Running    ->	Panic     	-> 	Panicked
 *  Running    ->	Kill      	-> 	Stopped
 *  Sleeping   ->	WakeUp    	-> 	Running
 *  Sleeping   ->	Panic     	-> 	Panicked
 *  Sleeping   ->	Kill      	-> 	Stopped
 *  Paused     ->	Resume    	-> 	Running
 *  Paused     ->	Finish    	-> 	Completed
 *  Paused     ->	Panic     	-> 	Panicked
 *  Paused     ->	Kill      	-> 	Stopped
 *  Completed  ->	Spawn     	-> 	Running     (reuse)
 *  Stopped    ->	Spawn     	-> 	Running     (reuse)
 *  Panicked   ->	Spawn     	-> 	Running     (reuse)
 *
 * All other state transitions are not allowed.
 */

#pragma once


#include <base/extend_cpp/variant_match.hpp>

#include <vm/api/data/status.hpp>

#include <string_view>
#include <variant>

namespace vm::thread_sm {
	namespace thread_state {

		struct NotStarted {};  ///< The thread object exists but its exec-thread was never spawned.

		struct Running {};     ///< The interpreter loop is actively executing.

		struct Sleeping {};    ///< The thread is sleeping i.e. waiting on IO.

		struct Paused {};      ///< Thread is paused by the debugger.

		struct Stopped {};     ///< Terminated by an explicit Stop/Kill before completing normally.

		struct Completed {     ///< Finished executing normally.
			api::ExitValue exit_value;
		};

		struct Panicked {  ///< Finished by an unrecoverable runtime error.
			std::string err;
		};

		using ThreadState
			= std::variant<NotStarted, Running, Sleeping, Paused, Stopped, Completed, Panicked>;

		inline constexpr std::array<std::string_view, std::variant_size_v<ThreadState>>
			THREAD_STATE_NAMES{ "NotStarted", "Running",   "Sleeping", "Paused",
			                    "Stopped",    "Completed", "Panicked" };

		[[nodiscard]] inline bool isTerminal(const ThreadState& state) {
			return v_matches(state, Completed, Stopped, Panicked);
		}

		[[nodiscard]] inline bool hasStarted(const ThreadState& state) {
			return !v_matches(state, NotStarted);
		}

		[[nodiscard]] inline bool isActive(const ThreadState& state) {
			return v_matches(state, Running, Sleeping, Paused);
		}

		[[nodiscard]] inline std::string_view threadStateName(const ThreadState& state) {
			return THREAD_STATE_NAMES.at(state.index());
		}
	}

	namespace thread_event {
		struct Spawn {};

		struct Pause {};

		struct Resume {};

		struct EnterSleep {};

		struct WakeUp {};

		struct Finish {
			api::ExitValue exit_value;
		};

		struct Panic {
			std::string msg;
		};

		struct Kill {};

		using ThreadEvent
			= std::variant<Spawn, Pause, Resume, EnterSleep, WakeUp, Finish, Kill, Panic>;

		inline constexpr std::array<std::string_view, std::variant_size_v<ThreadEvent>>
			THREAD_EVENT_NAMES{ "Spawn",  "Pause",  "Resume", "EnterSleep",
			                    "WakeUp", "Finish", "Kill",   "Panic" };

		[[nodiscard]] inline std::string_view threadEventName(const ThreadEvent& event) {
			return THREAD_EVENT_NAMES.at(event.index());
		}
	}

	/**
	 * @brief The transition function of the VMThread state machine.
	 *
	 * @return The new state if the `(state, event)` transition is allowed, `std::nullopt`
	 * otherwise.
	 */
	[[nodiscard]] inline base::Optional<thread_state::ThreadState> applyThreadEvent(
		const thread_state::ThreadState& state, const thread_event::ThreadEvent& event
	) {
		namespace st = thread_state;
		namespace ev = thread_event;
		using R      = base::Optional<st::ThreadState>;

		variant_match(event) {
			variant_case_novalue(ev::Spawn) {
				if (v_matches(state, st::NotStarted, st::Completed, st::Stopped, st::Panicked))
					return R{ st::Running{} };
			}
			variant_case_novalue(ev::Pause) {
				if (v_matches(state, st::Running)) return R{ st::Paused{} };
			}
			variant_case_novalue(ev::Resume) {
				if (v_matches(state, st::Paused)) return R{ st::Running{} };
			}
			variant_case_novalue(ev::EnterSleep) {
				if (v_matches(state, st::Running)) return R{ st::Sleeping{} };
			}
			variant_case_novalue(ev::WakeUp) {
				if (v_matches(state, st::Sleeping)) return R{ st::Running{} };
			}
			variant_case(ev::Finish, finish) {
				if (v_matches(state, st::Running, st::Paused))
					return R{ st::Completed{ finish.exit_value } };
			}
			variant_case(ev::Panic, panic) {
				if (v_matches(state, st::Running, st::Sleeping, st::Paused))
					return R{ st::Panicked{ panic.msg } };
			}
			variant_case_novalue(ev::Kill) {
				if (v_matches(state, st::NotStarted, st::Running, st::Sleeping, st::Paused))
					return R{ st::Stopped{} };
			}
		}
		return std::nullopt;
	}

}
