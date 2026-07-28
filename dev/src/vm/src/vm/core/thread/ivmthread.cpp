#include "ivmthread.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <vm/core/process/ivmprocess.hpp>
#include <vm/core/thread/kill_process_exception.hpp>
#include <vm/core/thread/thread_state.hpp>

#include <exception>
#include <expected>
#include <mutex>

namespace vm {
	namespace ts = thread_sm::thread_state;
	namespace te = thread_sm::thread_event;

	std::expected<vm::api::Response, vm::api::ApiError> IVMThread::join() {
		{
			std::lock_guard lock(exec_thread_mutex);
			if (!exec_thread.has_value()) return std::unexpected(api::ApiError{ api::JoinError{} });
		}

		if (!ts::hasStarted(state_machine.getStateCopy()))
			return std::unexpected(api::ApiError{ api::JoinError{} });

		// Wait for some terminal state.
		const ThreadState terminal = state_machine.waitForState(ts::isTerminal);
		// Now join the OS thread.
		joinExecutionThread();

		variant_match(terminal) {
			variant_case_novalue(ts::Completed, ts::Stopped) {
				return api::Response(api::response::Empty());
			}
			variant_case(ts::Panicked, panicked) {
				return std::unexpected(
					api::ApiError(api::OtherError("Execution panicked with error: " + panicked.err))
				);
			}
			variant_default {
				return std::unexpected(
					api::ApiError(api::OtherError("Unexpected run status after join!"))
				);
			}
		}
		CORE_UNREACHABLE();
	}

	bool IVMThread::isPauseRequested() {
		std::unique_lock lock(execution_request_mutex);
		return execution_request == ExecutionRequest::Pause;
	}

	bool IVMThread::isTerminateRequested() {
		std::unique_lock lock(execution_request_mutex);
		return execution_request == ExecutionRequest::Stop;
	}

	void IVMThread::fireEvent(const te::ThreadEvent& event) {
		const auto res = state_machine.handleEvent(event);
		CORE_ASSERT(
			res.has_value(),
			base::strConcat("Fire event failed! No transition for: ", te::threadEventName(event))
		);
		CORE_ASSERT(
			res->has_value(),
			base::strConcat(
				"Fire event reported error! for: ", te::threadEventName(event), " err: ", res->error()
			)
		);
	}

	void IVMThread::joinExecutionThread() {
		std::lock_guard lock(exec_thread_mutex);
		if (exec_thread && exec_thread->joinable()) exec_thread->join();
		exec_thread.reset();
	}

	void IVMThread::safeRun(const std::string& func_name, const RunArguments& run_arguments) {
		fireEvent(te::Spawn{});
		try {
			if (isTerminateRequested()) throw KillProcessException{};
			run(func_name, run_arguments);
		} catch (const KillProcessException& e) {
			fireEvent(te::Kill{});
		} catch (const exceptions::VMRuntimeException& e) {
			std::cerr << "VMThread has panicked: " << e.what() << "\n";
			fireEvent(te::Panic{ e.what() });
		} catch (const std::exception& e) {
			std::cerr << "VMThread has panicked with an unexpected error: " << e.what() << "\n";
			fireEvent(te::Panic{ std::string{ "Unexpected non-DVM error: " } + e.what() });
		}

		postRequest(ExecutionRequest::NoRequest, false);
	}

	bool IVMThread::spawnThreadAndRun(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		{
			std::lock_guard lock(exec_thread_mutex);
			// There is already a thread running.
			if (exec_thread.has_value() && exec_thread->joinable()) return false;
			exec_thread = std::thread(&IVMThread::safeRun, this, func_name, run_arguments);
		}

		// Block until the thread actually starts running and sets the running state.
		state_machine.waitForState(ts::hasStarted);
		return true;
	}

	bool IVMThread::runNoSpawn(const std::string& func_name, const RunArguments& run_arguments) {
		{
			std::lock_guard lock(exec_thread_mutex);
			const bool      thread_busy = exec_thread.has_value() && exec_thread->joinable();
			if (thread_busy || ts::isActive(state_machine.getStateCopy())) return false;
		}
		safeRun(func_name, run_arguments);
		return true;
	}

	std::expected<void, std::string> IVMThread::pause() {
		const ThreadState copy = state_machine.getStateCopy();
		if (ts::isTerminal(copy)) return std::unexpected("Pausing a terminal thread");
		if (v_matches(copy, ts::Sleeping)) return std::unexpected("Pausing a sleeping thread");

		postRequest(ExecutionRequest::Pause, true);

		// Wait for the paused state or a terminal.
		const ThreadState state = state_machine.waitForState([](const ts::ThreadState& s) {
			return v_matches(s, ts::Paused) || ts::isTerminal(s);
		});

		if (v_matches(state, ts::Paused))
			return {};
		else
			return std::unexpected("Thread terminated before pause.");
	}

	std::expected<void, std::string> IVMThread::resume() {
		const ThreadState copy = state_machine.getStateCopy();
		if (ts::isTerminal(copy)) return std::unexpected("Resuming a terminal thread");
		if (!v_matches(copy, ts::Paused)) return std::unexpected("Resuming a non-paused thread");

		// The thread already waits in the debugger loop, no need to set the
		// `execution_request_pending_flag`.
		const u64 gen = state_machine.getStateChangeCount();
		postRequest(ExecutionRequest::Resume, false);
		state_machine.waitForFreshState(gen, [](const ts::ThreadState&) { return true; });
		return {};
	}

	std::expected<void, std::string> IVMThread::step() {
		const ThreadState copy = state_machine.getStateCopy();
		if (ts::isTerminal(copy)) return std::unexpected("Stepping a terminal thread");
		if (!v_matches(copy, ts::Paused)) return std::unexpected("Stepping a non-paused thread");

		// Save the state change count to detect a fresh Paused after Paused -> Running -> Paused.
		const u64 gen = state_machine.getStateChangeCount();
		// The thread already waits in the debugger loop, no need to set the
		// `execution_request_pending_flag`
		postRequest(ExecutionRequest::ExecuteOneStep, false);
		// Wait for the running state or error.
		state_machine.waitForFreshState(gen, [](const ts::ThreadState& s) {
			return v_matches(s, ts::Paused) || ts::isTerminal(s);
		});
		return {};
	}

	void IVMThread::postRequest(ExecutionRequest req, bool raise_pending_flag) {
		{
			std::unique_lock lock(execution_request_mutex);
			// Stop is terminal. Once requested it must never be overwritten by a weaker
			// control request (pause/resume/step).
			if (execution_request != ExecutionRequest::Stop || req == ExecutionRequest::Stop) {
				execution_request = req;
				if (raise_pending_flag) execution_request_pending_flag = true;
			}
		}
		pause_cv.notify_all();
	}

	bool IVMThread::stop() {
		const ThreadState copy = state_machine.getStateCopy();
		if (ts::isTerminal(copy)) return true;

		// The thread is not started.
		if (!ts::hasStarted(copy)) {
			std::lock_guard lock(exec_thread_mutex);
			if (!exec_thread.has_value()) {
				// This is the only time in which the exec_thread doesn't invoke the `fireEvent`
				// since it doesn't exist.
				fireEvent(te::Kill{});
				return true;
			}
		}

		// Otherwise the thread has started. Post a stop flag.
		postRequest(ExecutionRequest::Stop, true);
		state_machine.waitForState(ts::isTerminal);
		return true;
	}

	bool IVMThread::hasActiveThread() const {
		std::lock_guard lock(exec_thread_mutex);
		return exec_thread.has_value() && exec_thread->joinable();
	}

	/**
	 * @details Assumes that the instruction in the frame is to be executed before AND after running
	 * this function.
	 */
	void IVMThread::breakActiveExecution() {
		std::unique_lock lock(execution_request_mutex);
		switch (execution_request) {
		case ExecutionRequest::Pause:
			fireEvent(te::Pause{});

			runDebuggerLoop(lock);

			if (execution_request != ExecutionRequest::Pause
			    && execution_request != ExecutionRequest::Stop)
				execution_request_pending_flag = false;
			break;
		case ExecutionRequest::Stop:
			throw KillProcessException{};
		default:
			CORE_PANIC("breakActiveExecution called without a pending Stop/Pause request");
		}
	}

	void IVMThread::requestStop() noexcept {
		if (ts::isTerminal(state_machine.getStateCopy())) return;
		postRequest(ExecutionRequest::Stop, true);
	}

	void IVMThread::notifyPaused() { pause_cv.notify_all(); }

	void IVMThread::reportAsSleeping() { fireEvent(te::EnterSleep{}); }

	void IVMThread::reportAsRunning() { fireEvent(te::WakeUp{}); }

	/**
	 * @details Assumes that the instruction in the frame is to be executed before AND after running
	 * this function.
	 */
	void IVMThread::runDebuggerLoop(std::unique_lock<std::recursive_mutex>& lock) {
		while (true) {
			// Loop invariant: the instruction in the frame is to be executed
			pause_cv.wait(lock, [this] { return execution_request != ExecutionRequest::Pause; });

			switch (execution_request) {
			case ExecutionRequest::Resume: {
				execution_request = ExecutionRequest::NoRequest;
				fireEvent(te::Resume{});
				return;
			}
			case ExecutionRequest::Stop: {
				throw KillProcessException{};
			}
			case ExecutionRequest::ExecuteOneStep: {
				fireEvent(te::Resume{});

				executeOneStep();
				// A Stop posted while the step ran must win. Overwriting it with Pause would lose
				// the only Stop notification and get in the the debugger loop waiting for
				// `execution_request != Pause` forever causing a deadlock.
				if (execution_request == ExecutionRequest::Stop) throw KillProcessException{};
				execution_request = ExecutionRequest::Pause;
				fireEvent(te::Pause{});
				break;
			}
			default:
				throw exceptions::VMResumedWithPausedStatusException();
			}
		}
	}

	void IVMThread::handleBreakpoint() {
		{
			std::unique_lock lock(execution_request_mutex);
			// A Stop posted while the step ran must win. Overwriting it with Pause would lose
			// the only Stop notification and get in the the debugger loop waiting for
			// `execution_request != Pause` forever causing a deadlock.
			if (execution_request == ExecutionRequest::Stop) throw KillProcessException{};
			execution_request = ExecutionRequest::Pause;
		}

		fireEvent(te::Pause{});
		{
			std::unique_lock lock(execution_request_mutex);
			runDebuggerLoop(lock);
			if (execution_request != ExecutionRequest::Pause
			    && execution_request != ExecutionRequest::Stop)
				execution_request_pending_flag = false;
		}
	}


}
