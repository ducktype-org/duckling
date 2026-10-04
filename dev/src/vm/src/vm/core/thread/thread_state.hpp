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

#include <json/type_parse.hpp>

#include <string_view>
#include <type_traits>
#include <variant>

namespace vm {
	namespace thread_state {

		struct NotStarted final {
		};  ///< The thread object exists but its exec-thread was never spawned.

		struct Running final {};   ///< The interpreter loop is actively executing.

		struct Sleeping final {};  ///< The thread is sleeping i.e. waiting on IO.

		struct Paused final {};    ///< Thread is paused by the debugger.

		struct Stopped final {
		};  ///< Terminated by an explicit Stop/Kill before completing normally.

		struct Completed final {  ///< Finished executing normally.
			api::ExitValue exit_value;
		};

		struct Panicked final {  ///< Finished by an unrecoverable runtime error.
			std::string err;
		};

		using ThreadState
			= std::variant<NotStarted, Running, Sleeping, Paused, Stopped, Completed, Panicked>;

		[[nodiscard]] inline bool isTerminal(const ThreadState& state) {
			return v_matches(state, Completed, Stopped, Panicked);
		}

		[[nodiscard]] inline bool hasStarted(const ThreadState& state) {
			return !v_matches(state, NotStarted);
		}

		[[nodiscard]] inline bool isActive(const ThreadState& state) {
			return v_matches(state, Running, Sleeping, Paused);
		}
	}

	namespace thread_event {
		struct Spawn final {};

		struct Pause final {};

		struct Resume final {};

		struct EnterSleep final {};

		struct WakeUp final {};

		struct Finish final {
			api::ExitValue exit_value;
		};

		struct Panic final {
			std::string msg;
		};

		struct Kill final {};

		using ThreadEvent
			= std::variant<Spawn, Pause, Resume, EnterSleep, WakeUp, Finish, Kill, Panic>;

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
					if (v_matches(state, ts::NotStarted, ts::Completed, ts::Stopped, ts::Panicked))
						return R{ ts::Running{} };
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

JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::Spawn, "Spawn")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::Pause, "Pause")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::Resume, "Resume")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::EnterSleep, "EnterSleep")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::WakeUp, "WakeUp")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::Finish, "Finish")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::Kill, "Kill")
JSON_REGISTER_TYPE_WITH_NAME(vm::thread_event::Panic, "Panic")

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
