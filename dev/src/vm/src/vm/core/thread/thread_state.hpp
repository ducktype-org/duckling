/**
 * @brief All states and events available for a single VMThread
 *
 * @note: All events on the VMThread state machine, should be performed ONLY by the VMThread
 * execution thread. When wanting to change the VMThread state from the VMProcess use a
 * `ControlRequest`
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


#include <state_machine/state_machine.hpp>

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

	using ThreadStateMachineDefinition
		= state_machine::StateMachineDefinition<thread_state::ThreadState, thread_event::ThreadEvent>;

	using ThreadStateMachine
		= state_machine::WaitableStateMachine<thread_state::ThreadState, thread_event::ThreadEvent>;

	inline ThreadStateMachineDefinition buildThreadStateMachineDefinition() {
		namespace st = thread_state;
		namespace ev = thread_event;
		using R      = ThreadStateMachineDefinition::ActionResultT;
		using A      = ThreadStateMachineDefinition::ActionResultT;

		ThreadStateMachineDefinition def;

		def.addTransitions<ev::Spawn, st::NotStarted, st::Completed, st::Stopped, st::Panicked>(
			[](const st::ThreadState&, const ev::Spawn&) -> A { return R{ st::Running{} }; }
		);

		def.addTransition<st::Running, ev::Pause>([](const st::Running&, const ev::Pause&) -> A {
			return R{ st::Paused{} };
		});

		def.addTransition<st::Paused, ev::Resume>([](const st::Paused&, const ev::Resume&) -> A {
			return R{ st::Running{} };
		});

		def.addTransition<st::Running, ev::EnterSleep>(
			[](const st::Running&, const ev::EnterSleep&) -> A { return R{ st::Sleeping{} }; }
		);

		def.addTransition<st::Sleeping, ev::WakeUp>(
			[](const st::Sleeping&, const ev::WakeUp&) -> A { return R{ st::Running{} }; }
		);

		def.addTransitions<ev::Finish, st::Running, st::Paused>(
			[](const st::ThreadState&, const ev::Finish& e) -> A {
				return R{ st::Completed{ e.exit_value } };
			}
		);

		def.addTransitions<ev::Panic, st::Running, st::Sleeping, st::Paused>(
			[](const st::ThreadState&, const ev::Panic& e) -> A {
				return R{ st::Panicked{ e.msg } };
			}
		);

		def.addTransitions<ev::Kill, st::NotStarted, st::Running, st::Sleeping, st::Paused>(
			[](const st::ThreadState&, const ev::Kill&) -> A { return R{ st::Stopped{} }; }
		);

		return def;
	}

	inline CRef<ThreadStateMachineDefinition> getThreadStateMachineDefinition() {
		static const ThreadStateMachineDefinition def = buildThreadStateMachineDefinition();
		return { &def };
	}
}
