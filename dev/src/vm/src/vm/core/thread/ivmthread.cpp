#include "ivmthread.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <vm/core/process/ivmprocess.hpp>
#include <vm/core/process/process_state_manager.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/thread/kill_process_exception.hpp>
#include <vm/core/thread/thread_state.hpp>

#include <exception>
#include <expected>
#include <mutex>

namespace vm {
	namespace ts = thread_sm::thread_state;
	namespace te = thread_sm::thread_event;

	IVMThread::IVMThread(api::ThreadID thread_id, IVMProcess& my_process):
		  my_process(my_process),
		  thread_id(thread_id) {
		// Every VMThread is part of its process's state aggregation.
		getProcessStateManager().registerThread(thread_id);
	}

	ProcessStateManager& IVMThread::getProcessStateManager() {
		return my_process.getStateManager();
	}

	const ProcessStateManager& IVMThread::getProcessStateManager() const {
		return my_process.getStateManager();
	}

	IVMThread::ThreadState IVMThread::getThreadState() const {
		return getProcessStateManager().threadState(thread_id);
	}

	void IVMThread::applyEvent(const ThreadEvent& event) {
		my_process.applyThreadEvent(thread_id, event);
	}

	std::expected<api::Response, api::ApiError> IVMThread::join() {
		{
			std::lock_guard lock(exec_thread_mutex);
			if (!exec_thread.has_value()) return std::unexpected(api::ApiError{ api::JoinError{} });
		}

		if (!ts::hasStarted(getThreadState()))
			return std::unexpected(api::ApiError{ api::JoinError{} });

		// Wait for some terminal state.
		const ThreadState terminal
			= getProcessStateManager().waitForThreadState(thread_id, ts::isTerminal);
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

	void IVMThread::joinExecutionThread() {
		std::lock_guard lock(exec_thread_mutex);
		if (exec_thread && exec_thread->joinable()) exec_thread->join();
		exec_thread.reset();
	}

	void IVMThread::safeRun(const std::string& func_name, const RunArguments& run_arguments) {
		// `Spawn` was committed by the `prepareSpawnLocked`). From here on this exec thread is the
		// only writer of this thread's state.
		try {
			if (isTerminateRequested()) throw KillProcessException{};
			run(func_name, run_arguments);
		} catch (const KillProcessException& e) {
			applyEvent(te::Kill{});
		} catch (const exceptions::VMRuntimeException& e) {
			std::cerr << "VMThread has panicked: " << e.what() << "\n";
			applyEvent(te::Panic{ e.what() });
		} catch (const std::exception& e) {
			std::cerr << "VMThread has panicked with an unexpected error: " << e.what() << "\n";
			applyEvent(te::Panic{ std::string{ "Unexpected non-DVM error: " } + e.what() });
		}
	}

	bool IVMThread::prepareSpawnLocked() {
		if (ts::isActive(getThreadState())) return false;

		// The thread is non-active, but a finished run leaves its `exec_thread` joinable until
		// `join` runs. We reset it here since otherwise a second `run` couldn't reuse this
		// VMThread. For the main thread that means the process state never sees `main` as
		// `Completed` again, which downgrades it to `Stopped` and loses the exit value.
		if (exec_thread.has_value()) {
			if (exec_thread->joinable()) exec_thread->join();
			exec_thread.reset();
		}

		// The thread is non-active. Clear any control requests from a previous run and
		// perform a transition to Running. The exec thread is the only writer of state from here on.
		signal.reset();
		applyEvent(te::Spawn{});
		return true;
	}

	bool IVMThread::spawnThreadAndRun(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		std::lock_guard lock(exec_thread_mutex);
		if (!prepareSpawnLocked()) return false;

		try {
			exec_thread = std::thread(&IVMThread::safeRun, this, func_name, run_arguments);
		} catch (const std::system_error&) {
			// OS thread creation failed.
			applyEvent(te::Kill{});
			return false;
		}
		return true;
	}

	bool IVMThread::runNoSpawn(const std::string& func_name, const RunArguments& run_arguments) {
		{
			std::lock_guard lock(exec_thread_mutex);
			if (!prepareSpawnLocked()) return false;
		}
		safeRun(func_name, run_arguments);
		return true;
	}

	std::expected<void, std::string> IVMThread::pause() {
		const ThreadState copy = getThreadState();
		if (ts::isTerminal(copy)) return std::unexpected("Pausing a terminal thread");
		if (!ts::hasStarted(copy)) return std::unexpected("Pausing a thread that has not started");

		if (!signal.post(ThreadSignal::Request::Pause))
			return std::unexpected("Another request is active");

		// Wait for the paused state or a terminal.
		const ThreadState state
			= getProcessStateManager().waitForThreadState(thread_id, [](const ts::ThreadState& s) {
				  return v_matches(s, ts::Paused) || ts::isTerminal(s);
			  });

		if (v_matches(state, ts::Paused)) return {};
		return std::unexpected("Thread terminated before pause.");
	}

	std::expected<void, std::string> IVMThread::resume() {
		const ThreadState copy = getThreadState();
		if (ts::isTerminal(copy)) return std::unexpected("Resuming a terminal thread");
		if (!v_matches(copy, ts::Paused)) return std::unexpected("Resuming a non-paused thread");

		const u64 version = getProcessStateManager().threadStateChangeCounter(thread_id);
		if (!signal.post(ThreadSignal::Request::Resume))
			return std::unexpected("Another control request is active");

		// Wait until this thread commits any fresh state.
		(void) getProcessStateManager().waitForFreshThreadState(
			thread_id, version, [](const ts::ThreadState&) { return true; }
		);
		return {};
	}

	std::expected<void, std::string> IVMThread::step() {
		const ThreadState copy = getThreadState();
		if (ts::isTerminal(copy)) return std::unexpected("Stepping a terminal thread");
		if (!v_matches(copy, ts::Paused)) return std::unexpected("Stepping a non-paused thread");

		// Version is needed to see the new Paused after a fast Paused -> Running -> Paused transition.
		const u64 version = getProcessStateManager().threadStateChangeCounter(thread_id);
		if (!signal.post(ThreadSignal::Request::Step))
			return std::unexpected("Another control request is in flight");

		(void) getProcessStateManager().waitForFreshThreadState(
			thread_id,
			version,
			[](const ts::ThreadState& s) { return v_matches(s, ts::Paused) || ts::isTerminal(s); }
		);
		return {};
	}

	void IVMThread::requestStop() noexcept {
		if (ts::isTerminal(getThreadState())) return;
		(void) signal.post(ThreadSignal::Request::Stop);
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
		const auto req = signal.consume();
		if (!req.has_value()) return;

		switch (*req) {
		case ThreadSignal::Request::Stop:
			throw KillProcessException{};
		case ThreadSignal::Request::Pause:
			// Skip any stale Pause requests of they came to a non-running thread.
			if (v_matches(getThreadState(), ts::Running)) pausedLoop();
			return;
		case ThreadSignal::Request::Resume:
		case ThreadSignal::Request::Step:
			// Resume and Step only mean something to a paused thread, and this thread is running.
			// The API had to post it while the thread was Paused, but the thread resumed before it
			// read the request. We skip it here so it doesn't clutter the request channel.
			return;
		}
	}

	/**
	 * @details Assumes that the instruction in the frame is to be executed before AND after running
	 * this function. Runs with no locks held. A breakpoint inside a `Step` execution cannot
	 * re-enter.
	 */
	void IVMThread::pausedLoop() {
		applyEvent(te::Pause{});

		while (true) {
			switch (signal.waitForRequest()) {
			case ThreadSignal::Request::Resume: {
				applyEvent(te::Resume{});
				return;
			}
			case ThreadSignal::Request::Stop: {
				throw KillProcessException{};
			}
			case ThreadSignal::Request::Step: {
				applyEvent(te::Resume{});
				executeOneStep();
				// A Stop posted while the step ran must win over re-pausing.
				if (isTerminateRequested()) throw KillProcessException{};
				applyEvent(te::Pause{});
				break;
			}
			case ThreadSignal::Request::Pause:
				break;  // Already paused.
			}
		}
	}

	void IVMThread::handleBreakpoint() {
		if (isTerminateRequested()) throw KillProcessException{};
		pausedLoop();
	}

	void IVMThread::notifyWaiters() { signal.notifyWaiters(); }

	void IVMThread::reportAsSleeping() { applyEvent(te::EnterSleep{}); }

	void IVMThread::reportAsRunning() { applyEvent(te::WakeUp{}); }
}
