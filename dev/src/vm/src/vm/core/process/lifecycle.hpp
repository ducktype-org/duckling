#pragma once

#include <state_machine/state_machine.hpp>

#include <vm/api/data/status.hpp>

#include <string>
#include <variant>

namespace vm::lifecycle {
	/**
	 * @file lifecycle.hpp
	 *
	 * @brief Execution lifecycle state machine shared by VMProcess and VMThread.
	 *
	 * Both the process and each of its threads are state machines over
	 * `api::ProcStatus`, driven by the events below - similar to how an OS models
	 * process states. The allowed transitions are:
	 *
	 *     NotStarted --Start--> Running --Pause--> Paused --Resume--> Running
	 *                           Running --Sleep--> Sleeping --Wake--> Running
	 *     {Running, Paused} --Complete--> ExecutionCompleted
	 *     {Running, Paused, Sleeping} --Stop--> ExecutionStopped
	 *     {Running, Paused, Sleeping} --Panic--> ExecutionPanicked
	 *     {terminal} --Reset--> NotStarted     (thread joined / slot reused)
	 *     {terminal} --Start--> Running        (process / main thread reuse)
	 *
	 * Terminal states additionally accept their own announcing event again
	 * (e.g. `Panic` while already `ExecutionPanicked`), so a thread can
	 * re-announce a terminal status it inherited from the process.
	 */

	/// Execution begins (thread spawned or rerun on a reused thread/process).
	struct Start {};

	/// The executing thread honored a pause request or hit a breakpoint.
	struct Pause {};

	/// The executing thread honored a resume / single-step request.
	struct Resume {};

	/// The executing thread blocks waiting for input.
	struct Sleep {};

	/// The executing thread obtained input and continues.
	struct Wake {};

	/// Execution finished successfully with the given exit value.
	struct Complete {
		api::ExitValue exit_value;
	};

	/// Execution was stopped on request.
	struct Stop {};

	/// Execution failed with a runtime error.
	struct Panic {
		std::string error_message;
	};

	/// A finished thread is joined and its slot becomes reusable.
	struct Reset {};

	using Event = std::variant<Start, Pause, Resume, Sleep, Wake, Complete, Stop, Panic, Reset>;

	using Definition = state_machine::StateMachineDefinition<api::ProcStatus, Event>;
	using Machine    = state_machine::WaitableStateMachine<api::ProcStatus, Event>;

	/**
	 * @brief The shared transition table described above.
	 * Built once; every process/thread machine references it.
	 */
	const Definition& statusTransitions();

	/**
	 * @brief Maps a terminal status to the event announcing it.
	 * Used when a thread propagates a terminal status the process already holds
	 * (e.g. another thread panicked while this one was still running).
	 * @note Panics when called with a non-terminal status.
	 */
	Event eventForTerminalStatus(const api::ProcStatus& status);
}
