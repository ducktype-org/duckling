/**
 * @brief States and events for the VMProcess.
 *
 * The process state is an aggregate of it's threads state. On each thread state change `aggregate`
 * is called via the `ThreadState` machine listener, which updates the aggregate state if the process.
 */
#pragma once

#include <base/collections/maps.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <vm/api/data/status.hpp>
#include <vm/api/data/thread_id.hpp>
#include <vm/core/thread/thread_state.hpp>

#include <string_view>
#include <variant>

namespace vm::process_sm {
	using ThreadState = thread_sm::thread_state::ThreadState;

	namespace process_state {
		struct NotStarted {};  ///< All VMThreads are in the `NotStarted` state.

		struct Running {};     ///< At least one VMThread is actively executing.

		struct Sleeping {
		};  ///< No VMThread is running - at least one is sleeping, none are running.

		struct Paused {};    ///< All active VMThreads are paused, none are running or sleeping.

		struct Stopping {};  ///< A stop has been requested and at least one VMThread has not yet
		                     ///< reached a terminal state.

		struct Completed {
			api::ExitValue exit_value;
		};  ///< The main thread finished executing normally and returned an exit value.

		struct Stopped {
		};  ///< All VMThreads have been stopped explicitly (via a stop/kill request) or the process
		    ///< ended without the main thread completing normally.

		struct Panicked {
			std::string err;
		};  ///< At least one VMThread panicked.

		using ProcessState
			= std::variant<NotStarted, Running, Sleeping, Paused, Stopping, Completed, Stopped, Panicked>;

		inline constexpr std::array<std::string_view, std::variant_size_v<ProcessState>>
			PROCESS_STATE_NAMES{ "NotStarted", "Running",   "Sleeping", "Paused",
			                     "Stopping",   "Completed", "Stopped",  "Panicked" };

		[[nodiscard]] inline bool isTerminal(const ProcessState& state) {
			variant_match(state) {
				variant_case_novalue(Completed, Stopped, Panicked) return true;
				variant_default return false;
			}
		}

		[[nodiscard]] inline std::string_view processStateName(const ProcessState& state) {
			return PROCESS_STATE_NAMES.at(state.index());
		}
	}

	namespace process_event {
		struct Run {};     ///< Start or re-run the process.

		struct Pause {};   ///< Request all threads to pause.

		struct Resume {};  ///< Resume after a pause.

		struct Step {};    ///< Single-step in the debugger (process must be Paused).

		struct Stop {};    ///< Request an orderly stop of all threads.

		using ProcessEvent = std::variant<Run, Pause, Resume, Step, Stop>;

		inline constexpr std::array<std::string_view, std::variant_size_v<ProcessEvent>>
			PROCESS_EVENT_NAMES{ "Run", "Pause", "Resume", "Step", "Stop" };

		[[nodiscard]] inline std::string_view processEventName(const ProcessEvent& event) {
			return PROCESS_EVENT_NAMES.at(event.index());
		}
	}

	struct ProcessAggregationState {
		base::HashMap<api::ThreadID, ThreadState> threads;
		bool                                      stop_requested{ false };
		api::ThreadID                             main_tid{ api::ThreadID{ 0 } };

		void setThreadState(api::ThreadID tid, ThreadState state) {
			threads.insert_or_assign(tid, std::move(state));
		}
	};

	/**
	 * @brief The aggregating function which determines the VMProcess state based on its VMThread
	 * states.
	 *
	 * The VMProcess state is calculated like this (starting from the most important to the least
	 * important):
	 * 		1. Any VMThread Panicked 							-> VMProcess Panicked
	 *   	2. If stop was requested:
	 *			- If any VMThread is still active 				-> VMProcess Stopping
	 *			- If all VMThreads are stopped  				-> VMProcess Stopped
	 *   	3. No VMThreads are active(Running/Sleeping/Paused) -> VMProcess NotStarted
	 *   	4. Any VMThread is Running 							-> Running
	 *   	5. Any VMThread is Sleeping                  		-> VMProcess Sleeping
	 *   	6. Any VMThread is Paused 							-> VMProcess Paused
	 *   	7. All VMThreads are terminal:
	 *   		- Main VMThread Completed                       -> VMProcess Completed{exit}
	 *   		- Otherwise 									-> VMProcess Stopped
	 */
	[[nodiscard]] inline process_state::ProcessState aggregate(const ProcessAggregationState& agg) {
		using namespace process_state;
		namespace ts = thread_sm::thread_state;

		// If any VMThread panicked - we panic as well.
		for (const auto& [tid, state]: agg.threads)
			v_if_matches(state, ts::Panicked, panicked) return Panicked{ panicked->err };

		const bool any_started_active = std::ranges::any_of(agg.threads, [](const auto& p) {
			return ts::isActive(p.second);
		});

		// Stop in progress.
		if (agg.stop_requested)
			return any_started_active ? ProcessState{ Stopping{} } : ProcessState{ Stopped{} };

		const bool any_started = std::ranges::any_of(agg.threads, [](const auto& p) {
			return ts::hasStarted(p.second);
		});

		// Nothing started.
		if (!any_started) return NotStarted{};

		// If any is thread is running than the process is running as well.
		for (const auto& [tid, state]: agg.threads)
			if (v_matches(state, ts::Running)) return Running{};
		// If any is thread is sleeping than the process is sleeping as well.
		for (const auto& [tid, state]: agg.threads)
			if (v_matches(state, ts::Sleeping)) return Sleeping{};
		// If any thread is paused, than the process is paused as well.
		for (const auto& [tid, state]: agg.threads)
			if (v_matches(state, ts::Paused)) return Paused{};

		// All started are terminal.
		for (const auto& [tid, state]: agg.threads)
			// If Main thread completed, VMProcess does as well.
			if (tid == agg.main_tid)
				v_if_matches(
					state, ts::Completed, completed
				) return Completed{ completed->exit_value };
		return Stopped{};
	}

	[[nodiscard]] inline api::ProcStatus toApiStatus(const process_state::ProcessState& state) {
		using namespace process_state;
		variant_match(state) {
			variant_case_novalue(NotStarted) return api::NotStarted{};
			variant_case_novalue(Running) return api::Running{};
			variant_case_novalue(Sleeping) return api::Sleeping{};
			variant_case_novalue(Paused) return api::Paused{};
			variant_case_novalue(Stopping) return api::ExecutionStopping{};
			variant_case(Completed, c) return api::ExecutionCompleted{ c.exit_value };
			variant_case_novalue(Stopped) return api::ExecutionStopped{};
			variant_case(Panicked, p) return api::ExecutionPanicked{ p.err };
		}
		CORE_UNREACHABLE();
	}
}
