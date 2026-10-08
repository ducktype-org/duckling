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
 *  Completed  ->	Join      	-> 	Joined
 *  Stopped    ->	Join      	-> 	Joined
 *  Panicked   ->	Join      	-> 	Joined
 *  Joined     ->	Spawn     	-> 	Running     (reuse)
 *
 * All other state transitions are not allowed.
 */

#pragma once


#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <vm/api/data/status.hpp>

#include <json/type_parse.hpp>

#include <string_view>
#include <type_traits>
#include <variant>

namespace vm {
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

		/// How a finished run ended. Used by the `Joined` state.
		using TerminalOutcome = std::variant<Completed, Stopped, Panicked>;

		/**
		 * @brief The thread finished and its execution thread was already joined.
		 *
		 * This is the only state a VMThread may be deinitialized from, and the only state a reused
		 * VMThread may be spawned from.
		 */
		struct Joined {
			/// The terminal state at the moment `join` was called.
			TerminalOutcome outcome;
		};

		using ThreadState
			= std::variant<NotStarted, Running, Sleeping, Paused, Stopped, Completed, Panicked, Joined>;

		[[nodiscard]] inline bool isTerminal(const ThreadState& state) {
			return v_matches(state, Completed, Stopped, Panicked, Joined);
		}

		[[nodiscard]] inline bool isJoined(const ThreadState& state) {
			return v_matches(state, Joined);
		}

		/**
		 * @brief How a finished thread ended.
		 * @return The terminal outcome, or nothing when the thread has not finished yet.
		 */
		[[nodiscard]] inline base::Optional<TerminalOutcome> terminalOutcome(const ThreadState& state
		) {
			variant_match(state) {
				variant_case(Completed, completed) return TerminalOutcome{ completed };
				variant_case(Stopped, stopped) return TerminalOutcome{ stopped };
				variant_case(Panicked, panicked) return TerminalOutcome{ panicked };
				variant_case(Joined, joined) return joined.outcome;
				variant_default {}
			}
			return std::nullopt;
		}

		[[nodiscard]] inline bool hasStarted(const ThreadState& state) {
			return !v_matches(state, NotStarted);
		}

		[[nodiscard]] inline bool isActive(const ThreadState& state) {
			return v_matches(state, Running, Sleeping, Paused);
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

		/// The execution thread of a finished VMThread was joined, see `IVMThread::join`.
		struct Join {};

		using ThreadEvent
			= std::variant<Spawn, Pause, Resume, EnterSleep, WakeUp, Finish, Kill, Panic, Join>;

		/**
		 * @brief The transition function of the VMThread state machine.
		 *
		 * @return The new state if the `(state, event)` transition is allowed, `std::nullopt`
		 * otherwise.
		 */
		[[nodiscard]] inline base::Optional<thread_state::ThreadState> applyThreadEvent(
			const thread_state::ThreadState& state, const ThreadEvent& event
		) {
			namespace ts = thread_state;
			namespace te = thread_event;
			using R      = base::Optional<ts::ThreadState>;

			variant_match(event) {
				variant_case_novalue(te::Spawn) {
					// A VMThread may only be re-spawned when a thread doesn't have an execution
					// thread (i.e. it never started or was joined).
					if (v_matches(state, ts::NotStarted, ts::Joined)) return R{ ts::Running{} };
				}
				variant_case_novalue(te::Pause) {
					if (v_matches(state, ts::Running)) return R{ ts::Paused{} };
				}
				variant_case_novalue(te::Resume) {
					if (v_matches(state, ts::Paused)) return R{ ts::Running{} };
				}
				variant_case_novalue(te::EnterSleep) {
					if (v_matches(state, ts::Running)) return R{ ts::Sleeping{} };
				}
				variant_case_novalue(te::WakeUp) {
					if (v_matches(state, ts::Sleeping)) return R{ ts::Running{} };
				}
				variant_case(te::Finish, finish) {
					if (v_matches(state, ts::Running, ts::Paused))
						return R{ ts::Completed{ finish.exit_value } };
				}
				variant_case(te::Panic, panic) {
					if (v_matches(state, ts::Running, ts::Sleeping, ts::Paused))
						return R{ ts::Panicked{ panic.msg } };
				}
				variant_case_novalue(te::Kill) {
					if (v_matches(state, ts::NotStarted, ts::Running, ts::Sleeping, ts::Paused))
						return R{ ts::Stopped{} };
				}
				variant_case_novalue(te::Join) {
					// Only a finished and not yet `joined` thread can be joined.
					if (!ts::isJoined(state))
						if_opt_some(ts::terminalOutcome(state), outcome) return R{ ts::Joined{
							outcome } };
				}
			}
			return std::nullopt;
		}
	}

}

JSON_REGISTER_TYPE_WITH_NAME(vm::thread_state::NotStarted, "NotStarted")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_state::Running, "Running")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_state::Sleeping, "Sleeping")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_state::Paused, "Paused")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_state::Stopped, "Stopped")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_state::Completed, "Completed")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_state::Panicked, "Panicked")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_state::Joined, "Joined")

JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::Spawn, "Spawn")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::Pause, "Pause")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::Resume, "Resume")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::EnterSleep, "EnterSleep")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::WakeUp, "WakeUp")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::Finish, "Finish")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::Kill, "Kill")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::Panic, "Panic")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::Join, "Join")

namespace vm {
	namespace thread_state {
		[[nodiscard]] inline std::string_view threadStateName(const ThreadState& state) {
			return VISIT(state, held, return js::typeName<std::remove_cvref_t<decltype(held)>>());
		}
	}

	namespace thread_event {
		[[nodiscard]] inline std::string_view threadEventName(const ThreadEvent& event) {
			return VISIT(event, held, return js::typeName<std::remove_cvref_t<decltype(held)>>());
		}
	}
}
