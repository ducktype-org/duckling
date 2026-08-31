#include "process_state_manager.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#if not defined(BUILD_TYPE_DEV)
	#include <cstdlib>
	#include <iostream>
#endif

namespace vm {
	namespace ts = thread_state;
	namespace te = thread_event;
	namespace ps = process_state;

	namespace {
		using ThreadEntry = ProcessStateAggregation::ThreadEntry;

		[[noreturn]] void fatalStateError(const std::string& message) {
#if defined(BUILD_TYPE_DEV)
			CORE_PANIC(message);
#else
			std::cerr << "DVM fatal state error: " << message << "\n"
					  << base::getCurrentStackTrace() << "\n";
			std::abort();
#endif
		}

		const ThreadEntry& threadEntryOrAbort(
			const ProcessStateAggregation& agg_state, api::ThreadID tid
		) {
			const base::Optional<CRef<ThreadEntry>> entry = agg_state.threads.atMaybe(tid);
			if (!entry.has_value())
				fatalStateError(base::strConcat("Unknown ThreadID: ", tid.asInt()));
			return *entry.value();
		}
	}

	const ProcessStateManager::ThreadState& ProcessStateManager::Snapshot::threadState(
		api::ThreadID tid
	) const {
		return threadEntryOrAbort(table.agg_state, tid).state;
	}

	ProcessStateManager::ProcessState ProcessStateManager::Snapshot::processState() const {
		return table.agg_state.aggregateState();
	}

	u64 ProcessStateManager::Snapshot::threadStateChangeCounter(api::ThreadID tid) const {
		return threadEntryOrAbort(table.agg_state, tid).state_change_counter;
	}

	ProcessStateManager::ProcessStateManager(api::ThreadID main_tid) {
		agg_state.main_tid = main_tid;
	}

	void ProcessStateManager::setOnStatusChangedCallback(OnStatusChangedCallback callback) {
		std::lock_guard emit_lock(emit_mutex);
		on_status_changed = std::move(callback);
	}

	void ProcessStateManager::setThreadStateLocked(api::ThreadID tid, ThreadState new_state) {
		if (!agg_state.isRegistered(tid))
			fatalStateError(base::strConcat("Unknown ThreadID: ", tid.asInt()));
		agg_state.setThreadState(tid, std::move(new_state));
	}

	void ProcessStateManager::finalizeStateChangeLocked(
		std::unique_lock<std::mutex>& table_lock, const ProcessState& prev, const ProcessState& next
	) {
		table_lock.unlock();
		// Notify waiters when the state changed.
		state_changed.notify_all();
		if (prev.index() != next.index() && on_status_changed) on_status_changed(next);
	}

	void ProcessStateManager::registerThread(api::ThreadID tid) {
		std::lock_guard  emit_lock(emit_mutex);
		std::unique_lock table_lock(table_mutex);
		if (agg_state.isRegistered(tid))
			fatalStateError(base::strConcat("ThreadID ", tid.asInt(), " already registered"));

		const ProcessState prev = agg_state.aggregateState();
		agg_state.registerThread(tid);
		const ProcessState next = agg_state.aggregateState();
		finalizeStateChangeLocked(table_lock, prev, next);
	}

	std::expected<ProcessStateManager::ApplyResult, std::string> ProcessStateManager::applyThreadEvent(
		api::ThreadID tid, const ThreadEvent& event
	) {
		ApplyResult      result;
		std::lock_guard  emit_lock(emit_mutex);
		std::unique_lock table_lock(table_mutex);

		const base::Optional<Ref<ThreadEntry>> entry = agg_state.threads.atMaybe(tid);
		if (!entry.has_value())
			return std::unexpected(base::strConcat("Unknown ThreadID: ", tid.asInt()));

		const ThreadState&                state     = entry.value()->state;
		const base::Optional<ThreadState> new_state = thread_event::applyThreadEvent(state, event);
		if (!new_state.has_value())
			return std::unexpected(base::strConcat(
				"No transition for thread event '",
				te::threadEventName(event),
				"' in state '",
				ts::threadStateName(state),
				"'"
			));

		const ProcessState prev = agg_state.aggregateState();
		setThreadStateLocked(tid, *new_state);

		// First panic raises the stop flag. The caller must send out Stops to all threads.
		if (v_matches(*new_state, ts::Panicked) && !agg_state.stop_requested) {
			agg_state.stop_requested = true;
			result.stop_all_threads  = true;
		}

		const ProcessState next = agg_state.aggregateState();
		finalizeStateChangeLocked(table_lock, prev, next);
		return result;
	}

	ProcessStateManager::ApplyResult ProcessStateManager::applyThreadEventOrAbort(
		api::ThreadID tid, const ThreadEvent& event
	) {
		const std::expected<ApplyResult, std::string> result = applyThreadEvent(tid, event);
		if (result.has_value()) return *result;
		// A must-succeed transition failed. This is a VM bug.
		fatalStateError(base::strConcat("applyThreadEventOrAbort failed: ", result.error()));
	}

	std::expected<void, std::string> ProcessStateManager::resetForRun() {
		std::lock_guard  emit_lock(emit_mutex);
		std::unique_lock table_lock(table_mutex);

		const ProcessState prev = agg_state.aggregateState();
		// Only a run that completed may be repeated.
		if (!v_matches(prev, ps::Completed))
			return std::unexpected(base::strConcat(
				"A process may only be re-run after it completed, but it is '",
				ps::processStateName(prev),
				"'"
			));
		for (const auto& [tid, _]: agg_state.threads) setThreadStateLocked(tid, ts::NotStarted{});
		agg_state.stop_requested = false;
		agg_state.first_panic_err.reset();
		const ProcessState next = agg_state.aggregateState();
		finalizeStateChangeLocked(table_lock, prev, next);
		return {};
	}

	bool ProcessStateManager::requestStop() {
		std::lock_guard  emit_lock(emit_mutex);
		std::unique_lock table_lock(table_mutex);

		const ProcessState prev = agg_state.aggregateState();
		// A stop must never overwrite a terminal state (e.g. Completed -> Stopped).
		if (ps::isTerminal(prev)) return false;
		// Nothing has ever started, so there is nothing to stop. Raising the flag here would make
		// the process state `Stopped` and make it impossible to rerun.
		if (v_matches(prev, ps::NotStarted)) return false;
		if (agg_state.stop_requested) return true;  // Already requested.

		agg_state.stop_requested = true;
		const ProcessState next  = agg_state.aggregateState();
		finalizeStateChangeLocked(table_lock, prev, next);
		return true;
	}

	ProcessStateManager::ThreadState ProcessStateManager::threadState(api::ThreadID tid) const {
		std::lock_guard lock(table_mutex);
		return threadEntryOrAbort(agg_state, tid).state;
	}

	ProcessStateManager::ProcessState ProcessStateManager::aggregate() const {
		std::lock_guard lock(table_mutex);
		return agg_state.aggregateState();
	}

	u64 ProcessStateManager::threadStateChangeCounter(api::ThreadID tid) const {
		std::lock_guard lock(table_mutex);
		return threadEntryOrAbort(agg_state, tid).state_change_counter;
	}
}
