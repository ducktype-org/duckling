#include "ivmthread.hpp"

#include <vm/core/process/vmprocess.hpp>
#include <vm/core/thread/kill_process_exception.hpp>

std::expected<vm::api::Response, vm::api::ApiError> vm::IVMThread::join() {
	if (!exec_thread || !exec_thread->joinable())
		return std::unexpected(api::ApiError{ api::JoinError{} });

	exec_thread->join();
	exec_thread.reset();

	// The thread function has returned, so (by the happens-before of thread::join)
	// the terminal status it announced is visible here. Read it out, then recycle
	// the thread slot. The reset is thread-local: the process keeps its terminal
	// status, e.g. for `getExitCode`.
	const api::ProcStatus execution_status = getStatus();
	applyThreadEvent(lifecycle::Reset{});

	variant_match(execution_status) {
		variant_case(api::ExecutionCompleted, completed) {
			return api::Response(api::response::Empty());
		}
		variant_case(api::ExecutionStopped, stopped) {
			return api::Response(api::response::Empty());
		}
		variant_case(api::ExecutionPanicked, panicked) {
			return std::unexpected(api::ApiError(
				api::OtherError("Execution panicked with error: " + panicked.error_message)
			));
		}
		variant_default {
			return std::unexpected(api::ApiError(api::OtherError("Unexpected run status!")));
		}
	}
	CORE_UNREACHABLE();
}

bool vm::IVMThread::isPauseRequested() {
	std::unique_lock lock(execution_request_mutex);
	return execution_request == ExecutionRequest::Pause;
}

bool vm::IVMThread::isTerminateRequested() {
	{
		std::unique_lock lock(execution_request_mutex);
		if (execution_request == ExecutionRequest::Stop) return true;
	}
	return my_process.isExecutionPanicked();
}

void vm::IVMThread::dispatchEvent(const lifecycle::Event& event) {
	// The order matters:
	// 1. Update this thread's machine first, without waking its waiters, so status
	//    listeners fired by the process observe an up-to-date thread state.
	// 2. Announce the event to the process (which updates its status and emits).
	// 3. Wake the thread machine's waiters last, so API callers blocked in
	//    pause/resume/step/stop observe an up-to-date process status as well.
	assertLifecycleEventApplied(status_machine.handleEventDeferNotify(event), event);
	my_process.applyEvent(event, thread_id);
	status_machine.notifyWaiters();
}

void vm::IVMThread::applyThreadEvent(const lifecycle::Event& event) {
	assertLifecycleEventApplied(status_machine.handleEvent(event), event);
}

void vm::IVMThread::assertLifecycleEventApplied(
	const lifecycle::Machine::ResultT& result, const lifecycle::Event& event
) const {
	CORE_ASSERT(
		result.has_value() && result.value().has_value(),
		"VMThread lifecycle violation: event (index: ",
		event.index(),
		") is not allowed in the current state (index: ",
		status_machine.getStateCopy().index(),
		", thread: ",
		thread_id.asInt(),
		")"
	);
}

bool vm::IVMThread::joinExecutionThread() {
	if (!exec_thread || !exec_thread->joinable()) return false;

	exec_thread->join();
	exec_thread.reset();
	applyThreadEvent(lifecycle::Reset{});
	return true;
}

void vm::IVMThread::safeRun(const std::string& func_name, const RunArguments& run_arguments) {
	try {
		run(func_name, run_arguments);
	} catch (const exceptions::VMRuntimeException& e) {
		std::cerr << "VMThread has panicked: " << e.what() << "\n";
		dispatchEvent(lifecycle::Panic{ e.what() });
	}
}

bool vm::IVMThread::spawnThreadAndRun(
	const std::string& func_name, const RunArguments& run_arguments
) {
	if (exec_thread)  // There is already a thread running.
		return false;

	const u64 spawn_version = status_machine.getSnapshot().version;
	exec_thread             = std::thread(&IVMThread::safeRun, this, func_name, run_arguments);

	// Wait until the new thread announces its `Start` (every run announces at
	// least one transition, even if it finishes before we get here).
	status_machine.waitUntil([spawn_version](const api::ProcStatus&, u64 version) {
		return version > spawn_version;
	});
	return true;
}

void vm::IVMThread::runNoSpawn(const std::string& func_name, const RunArguments& run_arguments) {
	// Runs synchronously in the caller's thread; when this returns, the status
	// machine already holds the terminal status of the run.
	safeRun(func_name, run_arguments);
}

bool vm::IVMThread::pause() {
	std::unique_lock api_lock(api_request_mutex);
	{
		std::unique_lock lock(execution_request_mutex);

		execution_request              = ExecutionRequest::Pause;
		execution_request_pending_flag = true;
	}

	// The thread either honors the pause or terminates first.
	return std::holds_alternative<api::Paused>(waitUntilPausedOrTerminated());
}

bool vm::IVMThread::resume() {
	std::unique_lock api_lock(api_request_mutex);

	const u64 request_version = status_machine.getSnapshot().version;
	{
		std::unique_lock lock(execution_request_mutex);

		// Fail fast: only a thread parked in the debugger loop consumes a Resume
		// request. Resuming a running thread would wait forever, and overwriting
		// a pending Pause/Stop request would panic the executing thread.
		if (!std::holds_alternative<api::Paused>(getStatus())) return false;

		execution_request = ExecutionRequest::Resume;
	}
	pause_cv.notify_all();

	// Wait until the thread acknowledges with any transition (it may have resumed
	// and e.g. hit the next breakpoint or completed before we observe the state),
	// or bail out if it is already terminal and can no longer respond.
	const auto snapshot
		= status_machine.waitUntil([request_version](const api::ProcStatus& status, u64 version) {
			  return version > request_version || api::isStatusTerminal(status);
		  });
	return snapshot.version > request_version
	    && !std::holds_alternative<api::ExecutionPanicked>(snapshot.state)
	    && !std::holds_alternative<api::ExecutionStopped>(snapshot.state);
}

bool vm::IVMThread::step() {
	std::unique_lock api_lock(api_request_mutex);

	const u64 request_version = status_machine.getSnapshot().version;
	{
		std::unique_lock lock(execution_request_mutex);

		// Fail fast: stepping is only valid while the thread is paused. See resume().
		if (!std::holds_alternative<api::Paused>(getStatus())) return false;

		execution_request = ExecutionRequest::ExecuteOneStep;
		pause_cv.notify_all();
	}

	// The step is acknowledged with `Resume` and finishes with `Pause` (or a
	// terminal status if the thread dies while stepping).
	const auto snapshot
		= status_machine.waitUntil([request_version](const api::ProcStatus& status, u64 version) {
			  return version > request_version
		          && (std::holds_alternative<api::Paused>(status) || api::isStatusTerminal(status));
		  });
	return std::holds_alternative<api::Paused>(snapshot.state);
}

bool vm::IVMThread::stop() {
	std::unique_lock api_lock(api_request_mutex);

	// A thread that never started executing will never respond to the request.
	if (!api::hasExecutionStarted(getStatus())) return false;

	{
		std::unique_lock lock(execution_request_mutex);

		execution_request              = ExecutionRequest::Stop;
		execution_request_pending_flag = true;
	}
	pause_cv.notify_all();

	status_machine.waitUntil([](const api::ProcStatus& status, u64) {
		return api::isStatusTerminal(status);
	});
	return true;
}

bool vm::IVMThread::hasActiveThread() const { return exec_thread && exec_thread->joinable(); }

/**
 * @details Assumes that the instruction in the frame is to be executed before AND after running
 * this function.
 */
void vm::IVMThread::breakActiveExecution() {
	std::unique_lock lock(execution_request_mutex);
	switch (execution_request) {
	case ExecutionRequest::Pause:
		dispatchEvent(lifecycle::Pause{});
		runDebuggerLoop(lock);
		execution_request_pending_flag = false;
		break;

	case ExecutionRequest::Stop:
		throw KillProcessException{};

	default:
		CORE_PANIC("Unexpected execution status");
	}
}

void vm::IVMThread::notifyPaused() { pause_cv.notify_all(); }

/**
 * @details Assumes that the instruction in the frame is to be executed before AND after running
 * this function.
 */
void vm::IVMThread::runDebuggerLoop(std::unique_lock<std::mutex>& lock) {
	while (true) {
		// Loop invariant: the instruction in the frame is to be executed
		pause_cv.wait(lock, [this] { return execution_request != ExecutionRequest::Pause; });

		switch (execution_request) {
		case ExecutionRequest::Resume: {
			execution_request = ExecutionRequest::NoRequest;
			// The pause that led into this loop may have been requested via the
			// pending flag (pause request) or set by a breakpoint while the flag
			// was raised. Clear it on resume, or the execution loop would call
			// breakActiveExecution with no request pending and panic.
			execution_request_pending_flag = false;
			dispatchEvent(lifecycle::Resume{});
			return;
		}
		case ExecutionRequest::Stop: {
			throw KillProcessException{};
		}
		case ExecutionRequest::ExecuteOneStep: {
			dispatchEvent(lifecycle::Resume{});

			executeOneStep();

			execution_request = ExecutionRequest::Pause;
			dispatchEvent(lifecycle::Pause{});
			break;
		}
		default:
			throw exceptions::VMResumedWithPausedStatusException();
		}
	}
}

void vm::IVMThread::handleBreakpoint() {
	std::unique_lock lock(execution_request_mutex);

	// A Stop that raced in before the breakpoint must win: overwriting it with
	// Pause below would lose the request and strand the stopper forever.
	if (execution_request == ExecutionRequest::Stop) throw KillProcessException{};

	dispatchEvent(lifecycle::Pause{});
	execution_request = ExecutionRequest::Pause;
	runDebuggerLoop(lock);
}
