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

#include <events/emitter.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>

#include <vm/api/data/status.hpp>
#include <vm/api/data/thread_id.hpp>
#include <vm/core/thread/thread_state.hpp>

#include <json/type_parse.hpp>

#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace vm {
	using ThreadState = thread_state::ThreadState;

	namespace process_state {
		struct NotStarted final {};  ///< All VMThreads are in the `NotStarted` state.

		struct Running final {};     ///< At least one VMThread is actively executing.

		struct Paused final {};  ///< All active VMThreads are paused, none are running or sleeping.

		struct Sleeping final {
		};  ///< No VMThread is running - at least one is sleeping, none are running.

		struct Stopping final {};  ///< A stop has been requested and at least one VMThread
		                           ///< has not yet reached a terminal state.

		struct Completed final {
			api::ExitValue exit_value;
		};  ///< No VMThread is active any more and the main thread finished normally.

		/**
		 * @brief No VMThread is active and the process did not complete normally. Either a stop was
		 * requested, or the main VMThread never finished (it was stopped, killed or never
		 * started).
		 */
		struct Stopped final {};

		struct Panicked final {
			std::string err;
		};  ///< At least one VMThread panicked.

		using ProcessState
			= std::variant<NotStarted, Running, Paused, Sleeping, Stopping, Completed, Stopped, Panicked>;

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

		/// True when `deinitAndValidate` is legal in the current state (the process is NotStarted
		/// or Completed nicely). Panicked or Stopped states forbid the deinit.
		[[nodiscard]] inline bool canDeinit(const ProcessState& state) {
			return v_matches(state, NotStarted, Completed);
		}
	}

	/**
	 * @brief The only non-read-only process-level events. Pause/Resume/Step are NOT process events,
	 * they are handled per-thread, so they don't appear here.
	 */
	namespace process_event {
		struct Run final {};   ///< Start or re-run the process.

		struct Stop final {};  ///< Request an orderly stop of all threads.

		struct DeinitAndValidate final {
		};  ///< Deinitialize the process down and validate its memory state.

		using ProcessEvent = std::variant<Run, Stop, DeinitAndValidate>;
	}

	/**
	 * @brief The state of every VMThread of one VMProcess, plus the process-wide stop flag.
	 *
	 * This is the only state a VMProcess has - everything else is derived from it by `aggregate()`.
	 */
	struct ProcessStateAggregation final {
		struct ThreadEntry final {
			ThreadState state;

			/// Incremented on every state change of this thread. Enables to distinguish two
			/// identical looking states apart (e.g. a fast Paused -> Running -> Paused).
			u64 state_change_counter{ 0 };

			/// Emits the thread's status on every state change of this thread.
			Box<events::Emitter<ThreadState>> status_emitter;
		};

		/// State and the change counter of every VMThread registered in the process.
		base::HashMap<api::ThreadID, ThreadEntry> threads;

		/// Set when a stop of the whole process was requested (an API `Stop`, or the first panic
		/// of any thread). Makes the aggregate state `Stopping`/`Stopped` regardless of the states
		/// of the individual threads.
		bool stop_requested{ false };

		/// Error message of the VMThread which panicked first.
		base::Optional<std::string> first_panic_err;

		/// The main VMThread. The only thread whose `Completed` completes the whole process (if all
		/// other threads are terminal or not started).
		api::ThreadID main_tid{ api::MAIN_THREAD_ID };

		[[nodiscard]] bool isRegistered(api::ThreadID tid) const { return threads.contains(tid); }

		/// Adds a thread in the `NotStarted` state with a zeroed counter.
		void registerThread(api::ThreadID tid) {
			threads.insert_or_assign(
				tid,
				ThreadEntry{
					.state                = thread_state::NotStarted{},
					.state_change_counter = 0,
					.status_emitter       = Box<events::Emitter<ThreadState>>::fromPointer(
                        new events::Emitter<ThreadState>{}
                    ),
				}
			);
		}

		/**
		 * @brief Overwrites a registered thread's state and bumps its version.
		 * Saves the panic message on the first panic of the process and emits the thread-level
		 * status event.
		 */
		void setThreadState(api::ThreadID tid, ThreadState state) {
			// The first panic is the one the aggregate reports, so we save it here.
			if (!first_panic_err.has_value())
				v_if_matches(state, thread_state::Panicked, panicked) first_panic_err
					= panicked->err;

			const auto entry = threads.atMaybe(tid);
			if (!entry.has_value()) CORE_PANIC(base::strConcat("Unknown ThreadID: ", tid.asInt()));
			bool new_state       = entry.value()->state.index() != state.index();
			entry.value()->state = std::move(state);
			entry.value()->state_change_counter++;

			// Translate the new thread state into the API status and emit it on the thread's
			// emitter. Callers interested in this thread's state changes subscribe to it.
			//
			// @warning This runs under `ProcessStateManager`'s table mutex (via
			// `setThreadStateLocked`), so a listener that reads the manager back would deadlock.
			if (new_state) entry.value()->status_emitter->emitEvent(entry.value()->state);
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
			namespace ts = thread_state;

			// If any VMThread panicked - we panic as well, and report the first panic.
			if (first_panic_err.has_value()) return Panicked{ *first_panic_err };

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
							|| v_matches(entry.state, thread_state::NotStarted),
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

JSON_REGISTER_TYPE_WITH_NAME(vm::process_state::NotStarted, "NotStarted")
JSON_REGISTER_TYPE_WITH_NAME(vm::process_state::Running, "Running")
JSON_REGISTER_TYPE_WITH_NAME(vm::process_state::Sleeping, "Sleeping")
JSON_REGISTER_TYPE_WITH_NAME(vm::process_state::Paused, "Paused")
JSON_REGISTER_TYPE_WITH_NAME(vm::process_state::Stopping, "Stopping")
JSON_REGISTER_TYPE_WITH_NAME(vm::process_state::Completed, "Completed")
JSON_REGISTER_TYPE_WITH_NAME(vm::process_state::Stopped, "Stopped")
JSON_REGISTER_TYPE_WITH_NAME(vm::process_state::Panicked, "Panicked")

JSON_REGISTER_TYPE_WITH_NAME(vm::process_event::Run, "Run")
JSON_REGISTER_TYPE_WITH_NAME(vm::process_event::Stop, "Stop")
JSON_REGISTER_TYPE_WITH_NAME(vm::process_event::DeinitAndValidate, "DeinitAndValidate")

namespace vm {
	namespace process_state {
		[[nodiscard]] inline std::string_view processStateName(const ProcessState& state) {
			return VISIT(state, held, return js::typeName<std::remove_cvref_t<decltype(held)>>());
		}
	}

	namespace process_event {
		[[nodiscard]] inline std::string_view processEventName(const ProcessEvent& event) {
			return VISIT(event, held, return js::typeName<std::remove_cvref_t<decltype(held)>>());
		}
	}
}
