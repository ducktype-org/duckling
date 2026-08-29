#include "ivmthread.hpp"

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <vm/core/process/ivmprocess.hpp>
#include <vm/core/process/process_state_manager.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/thread/kill_process_exception.hpp>
#include <vm/core/thread/thread_state.hpp>

#include <exception>
#include <expected>
#include <mutex>

namespace vm {
	namespace ts = thread_state;
	namespace te = thread_event;

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
			variant_default { CORE_UNREACHABLE(); }
		}
		CORE_UNREACHABLE();
	}

	void IVMThread::joinExecutionThread() {
		std::lock_guard lock(exec_thread_mutex);
		if (exec_thread && exec_thread->joinable()) exec_thread->join();
		exec_thread.reset();
	}

	void IVMThread::safeRun(const std::string& func_name, const RunArguments& run_arguments) {
		// Thread event `Spawn` was committed by the `prepareSpawnLocked`.
		try {
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
		// `run` has to be followed by a `join`. Having an active `exec_thread` handle here means
		// the previous run was never joined.
		if (exec_thread.has_value()) return false;
		if (!te::applyThreadEvent(getThreadState(), te::Spawn{}).has_value()) return false;

		// Clear any control requests from a previous run and perform a transition to Running. The
		// `exec_thread` is the only writer of state from here on.
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
		} catch (const std::system_error& e) {
			// OS thread creation failed.
			applyEvent(te::Panic{
				base::strConcat("Failed to create the execution thread: ", e.what()) });
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

	std::expected<void, std::string> IVMThread::validateThreadRequest(
		const ThreadSignal::Request request
	) const {
		const ThreadState state = getThreadState();

		const auto refuse = [&](std::string_view why) {
			return std::unexpected(base::strConcat(
				"Invalid request '",
				ThreadSignal::requestName(request),
				"' for a thread in state '",
				ts::threadStateName(state),
				"': ",
				why
			));
		};

		switch (request) {
		case ThreadSignal::Request::Pause:
			if (ts::isTerminal(state)) return refuse("the thread already terminated");
			if (!ts::hasStarted(state)) return refuse("the thread has not started");
			return {};
		case ThreadSignal::Request::Resume:
		case ThreadSignal::Request::Step:
			if (!v_matches(state, ts::Paused)) return refuse("the thread is not paused");
			return {};
		case ThreadSignal::Request::Stop:
			if (ts::isTerminal(state)) return refuse("the thread already terminated");
			return {};
		}
		CORE_UNREACHABLE();
	}

	std::expected<void, std::string> IVMThread::pause() {
		if (auto valid = validateThreadRequest(ThreadSignal::Request::Pause); !valid.has_value())
			return valid;

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
		if (auto valid = validateThreadRequest(ThreadSignal::Request::Resume); !valid.has_value())
			return valid;

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
		if (auto valid = validateThreadRequest(ThreadSignal::Request::Step); !valid.has_value())
			return valid;

		// The state change counter is needed to distinguish the "current Paused state" we're in and
		// the one we're waiting for. The step flow is as follows:
		// 1) The thread is in state `Paused` when entering the step.
		// 2) We post a `Resume` request through `ThreadSignal`
		// 3) We wait for the step to perform and wait for the next `Paused` state.
		//
		// It is possible, that when being in step 3), the `exec_thread` didn't manage to read the
		// new `Resume` request we posted in step 2) (meaning it didn't change it state from Paused
		// to Running and the state is sill `Paused`). This means the thread is still in the same
		// `Paused` state we started with, with no actual step of instruction happening in between.
		// Waiting for ANY Paused state in step 3) would mean we exit right away in the case
		// described above. For this reason, we use the state change counter and remember that the
		// `Paused` state we started with has some index `Paused{N}`. Than, we wait for any new
		// `Paused` state newer then `N` (Paused{>N})
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
		if (!validateThreadRequest(ThreadSignal::Request::Stop).has_value()) return;
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
