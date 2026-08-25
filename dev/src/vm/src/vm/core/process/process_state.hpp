/**
 * @brief States and events for the VMProcess.
 *
 * The VMProcess state is never stored - it is a pure aggregate of its VMThread states, computed
 * by `aggregateState()` from the `ProcessStateAggregation` held by the process's
 * `ProcessStateManager`. Every thread state change goes through the table, which recomputes the
 * aggregate.
 *
 * `ProcessEvent` covers only the two process operations (which actually cause a process state
 * change) -  `Run`, `Stop`. Pause/Resume/Step are per-thread operations.
 */
#pragma once

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

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
		};  ///< No VMThread is active any more and the main thread finished normally.

		struct Stopped {};  ///< No VMThread is active and the main VMThread did not finish normally
		                    ///< (it was stopped, killed or never started).

		struct Panicked {
			std::string err;
		};  ///< At least one VMThread panicked.

		using ProcessState
			= std::variant<NotStarted, Running, Sleeping, Paused, Stopping, Completed, Stopped, Panicked>;

		inline constexpr std::array<std::string_view, std::variant_size_v<ProcessState>>
			PROCESS_STATE_NAMES{ "NotStarted", "Running",   "Sleeping", "Paused",
			                     "Stopping",   "Completed", "Stopped",  "Panicked" };

		/**
		 * @note When changing the behaviour of these function remember to change ones in
		 * `status.hpp` as well.
		 */
		[[nodiscard]] inline bool isTerminal(const ProcessState& state) {
			return v_matches(state, Completed, Stopped, Panicked);
		}

		/// True while any thread is doing (or winding down) work.
		[[nodiscard]] inline bool isExecuting(const ProcessState& state) {
			return v_matches(state, Running, Paused, Sleeping, Stopping);
		}

		/// True when memory-touching read endpoints may run (nothing is mutating memory).
		[[nodiscard]] inline bool canRespond(const ProcessState& state) {
			return v_matches(state, NotStarted, Paused) || isTerminal(state);
		}

		[[nodiscard]] inline std::string_view processStateName(const ProcessState& state) {
			return PROCESS_STATE_NAMES.at(state.index());
		}
	}

	/**
	 * @brief The only non-read-only process-level events. Pause/Resume/Step are NOT process events,
	 * they are handled per-thread, so they don't appear here.
	 */
	namespace process_event {
		struct Run {};   ///< Start or re-run the process.

		struct Stop {};  ///< Request an orderly stop of all threads.

		using ProcessEvent = std::variant<Run, Stop>;

		inline constexpr std::array<std::string_view, std::variant_size_v<ProcessEvent>>
			PROCESS_EVENT_NAMES{ "Run", "Stop" };

		[[nodiscard]] inline std::string_view processEventName(const ProcessEvent& event) {
			return PROCESS_EVENT_NAMES.at(event.index());
		}
	}

	/**
	 * @brief The state of every VMThread of one VMProcess, plus the process-wide stop flag.
	 *
	 * This is the only state a VMProcess has - everything else is derived from it by `aggregate()`.
	 */
	struct ProcessStateAggregation {
		struct ThreadEntry {
			ThreadState state;

			/// Incremented on every state change of this thread. Enables to distinguish two
			/// identical looking states apart (e.g. a fast Paused -> Running -> Paused).
			u64 state_change_counter{ 0 };
		};

		/// State and the change counter of every VMThread registered in the process.
		base::HashMap<api::ThreadID, ThreadEntry> threads;

		/// Set when a stop of the whole process was requested (an API `Stop`, or the first panic
		/// of any thread). Makes the aggregate state `Stopping`/`Stopped` regardless of the states
		/// of the individual threads.
		bool stop_requested{ false };

		/// The main VMThread. The only thread whose `Completed` completes the whole process (if all
		/// other threads are terminal or not started).
		api::ThreadID main_tid{ api::MAIN_THREAD_ID };

		[[nodiscard]] bool isRegistered(api::ThreadID tid) const { return threads.contains(tid); }

		/// Adds a thread in the `NotStarted` state with a zeroed counter.
		void registerThread(api::ThreadID tid) {
			threads.insert_or_assign(
				tid,
				ThreadEntry{
					.state                = thread_sm::thread_state::NotStarted{},
					.state_change_counter = 0,
				}
			);
		}

		/// Overwrites a registered thread's state and bumps its version.
		void setThreadState(api::ThreadID tid, ThreadState state) {
			const auto entry = threads.atMaybe(tid);
			if (!entry.has_value()) CORE_PANIC(base::strConcat("Unknown ThreadID: ", tid.asInt()));
			entry.value()->state = std::move(state);
			entry.value()->state_change_counter++;
		}

		/**
		 * @brief The aggregating function which determines the VMProcess state based on its
		 * VMThread states.
		 *
		 * The VMProcess state is calculated like this (starting from the most important to the
		 * least important):
		 * 		1. Any VMThread Panicked 								-> VMProcess Panicked
		 *   	2. If stop was requested:
		 *			- If any VMThread is still active 					-> VMProcess Stopping
		 *			- If all VMThreads are stopped  					-> VMProcess Stopped
		 *   	3. No VMThread has ever started							-> VMProcess NotStarted
		 *   	4. Any VMThread is Running 								-> Running
		 *   	5. Any VMThread is Sleeping                  			-> VMProcess Sleeping
		 *   	6. Any VMThread is Paused 								-> VMProcess Paused
		 *   	7. Every VMThread is either NotStarted or terminal:
		 *   		- Main VMThread Completed (not Panicked/Stopped) 	-> VMProcess Completed{exit}
		 *   		- Otherwise 										-> VMProcess Stopped
		 */
		[[nodiscard]] process_state::ProcessState aggregateState() const {
			using namespace process_state;
			namespace ts = thread_sm::thread_state;

			// If any VMThread panicked - we panic as well.
			for (const auto& [tid, entry]: threads)
				v_if_matches(entry.state, ts::Panicked, panicked) return Panicked{ panicked->err };

			const bool any_started_active = std::ranges::any_of(threads, [](const auto& p) {
				return ts::isActive(p.second.state);
			});

			// Stop in progress.
			if (stop_requested)
				return any_started_active ? ProcessState{ Stopping{} } : ProcessState{ Stopped{} };

			const bool any_started = std::ranges::any_of(threads, [](const auto& p) {
				return ts::hasStarted(p.second.state);
			});

			// Nothing started.
			if (!any_started) return NotStarted{};

			// If any thread is running then the process is running as well.
			for (const auto& [tid, entry]: threads)
				if (v_matches(entry.state, ts::Running)) return Running{};
			// If any thread is sleeping then the process is sleeping as well.
			for (const auto& [tid, entry]: threads)
				if (v_matches(entry.state, ts::Sleeping)) return Sleeping{};
			// If any thread is paused, then the process is paused as well.
			for (const auto& [tid, entry]: threads)
				if (v_matches(entry.state, ts::Paused)) return Paused{};

			IF_BUILD_TYPE_DEV({
				for (const auto& [tid, entry]: threads)
					CORE_ASSERT(
						ts::isTerminal(entry.state)
							|| v_matches(entry.state, thread_sm::thread_state::NotStarted),
						"Unhandled state: ",
						threadStateName(entry.state)
					);
			});

			// Nothing is active, so every thread is either NotStarted or terminal.
			for (const auto& [tid, entry]: threads)
				// If Main thread completed, VMProcess does as well.
				if (tid == main_tid)
					v_if_matches(
						entry.state, ts::Completed, completed
					) return Completed{ completed->exit_value };
			return Stopped{};
		}
	};

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
