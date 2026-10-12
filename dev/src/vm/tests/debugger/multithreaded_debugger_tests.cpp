// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <vm_tester_utils.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <tester/tester.hpp>

#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <expected>
#include <future>
#include <string>
#include <thread>
#include <variant>

namespace api = vm::api;

/**
 * @brief Tests of the DVM debugger endpoints on a process with several VMThreads.
 */
class MultithreadedDebuggerTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS MultithreadedDebuggerTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// @TODO: #3612 JIT-compiled code does not observe stop/pause requests, and the JIT patches
		// the same bytecode as breakpoints. The JIT-build rule that disables `vm_*_debugger_*`
		// tests does not match this name, so the suite spawns its processes with the JIT off
		// instead (ignored in non-JIT builds).
		process_config.enable_jit = false;

		TESTER_ADD_TEST(pauseOneThreadWhileOthersRun);
		TESTER_ADD_TEST(pauseAllParksEveryThread);
		TESTER_ADD_TEST(pauseAllWaitsForASleepingThread);
		TESTER_ADD_TEST(pauseAllWaitIsEndedByStop);
		TESTER_ADD_TEST(gilIsReleasedWhilePaused);
		TESTER_ADD_TEST(gilIsReleasedWhileSleepingOnIo);
		TESTER_ADD_TEST(waitForBreakpointIsPerThread);
		TESTER_ADD_TEST(stepIsPerThread);
		TESTER_ADD_TEST(perThreadEndpointsRejectUnknownThreads);
	}

	~MultithreadedDebuggerTest() override = default;

private:
	/// Instruction index of the breakpoint marked in `worker_breakpoint.dbc`.
	static constexpr u64 WORKER_BREAKPOINT_INDEX = 5;

	/// How many further writes a worker must produce to count as "still making progress".
	static constexpr u64 WRITES_PROVING_PROGRESS = 5;

	void pauseOneThreadWhileOthersRun() {
		// Pausing one thread must neither be refused because another thread is running, nor stop
		// the threads that were not asked. Before the per-thread API these requests were validated
		// against the process aggregate, which refused this outright.

		const vm::PID       pid = runSpinThreads();
		const api::ThreadID target{ 1 };
		const api::ThreadID bystander{ 2 };

		auto position = api::pause(pid, target);
		assertSucceeded(position, "pause of a worker while the other threads run");
		ASSERT_EQUAL_PRINT(std::string("spinner"), position->function_name.str());

		// The other threads are untouched, so the process as a whole is still running.
		auto status = api::getExecutionStatus(pid);
		assertSucceeded(status, "getExecutionStatus");
		assertTrue(
			isRunning(status.value()),
			"One paused thread must not make the whole process report Paused"
		);

		// Only a paused thread accepts a resume, which is how "thread 2 kept running" is asserted.
		assertRefusedWith<api::ResumeError>(
			api::resume(pid, bystander),
			"resume of a thread that was never paused",
			"the thread is not paused"
		);
		assertSucceeded(api::resume(pid, target), "resume of the paused worker");

		assertSucceeded(api::stop(pid), "stop");
		(void) api::kill(pid);
	}

	void pauseAllParksEveryThread() {
		// `pauseAll` must park every thread, which it can only do if a thread that already parked
		// lets go of the GIL. A thread parking with the GIL still held starves the threads that
		// have not reached their pause point yet, and `pauseAll` never finishes.

		const vm::PID pid = runSpinThreads();

		auto pending = std::async(std::launch::async, [pid] { return api::pauseAll(pid); });
		if (pending.wait_for(UNBLOCKED_CALL_BUDGET) != std::future_status::ready) {
			// Let the blocked call finish so the future can be destroyed.
			(void) api::stop(pid);
			(void) pending.get();
			(void) api::kill(pid);
			fail("pauseAll did not park every thread - is a paused thread still holding the GIL?");
			return;
		}

		auto paused = pending.get();
		assertSucceeded(paused, "pauseAll of a fully running multi-threaded process");
		ASSERT_EQUAL_PRINT(SPIN_THREAD_COUNT, paused->thread_ids.size());

		// Every thread parked, so nothing outranks Paused in the aggregate any more.
		waitUntilStatus(pid, isPaused, "Paused after pauseAll");

		for (const api::ThreadID tid: paused->thread_ids)
			assertSucceeded(
				api::resume(pid, tid), base::strConcat("resume of thread ", tid.asInt())
			);
		waitUntilStatus(pid, isRunning, "Running after resuming every thread");

		assertSucceeded(api::stop(pid), "stop");
		(void) api::kill(pid);
	}

	void pauseAllWaitsForASleepingThread() {
		// A thread sleeping on IO cannot park while it sleeps - `waitInterruptible` only looks at
		// the stop flag - so the request stays pending and takes effect once the IO completes.
		// `pauseAll` waits for that, by design: it returns only when every thread it asked has
		// parked.

		const vm::PID       pid = spawnAndLoad("sleeping_main.dbc");
		const api::ThreadID worker{ 1 };

		assertSucceeded(api::run(pid), "run of sleeping_main.dbc");

		// Park the worker first. With the worker paused and `main` blocked on input, `Sleeping` is
		// the top-ranked state left, so the aggregate reporting Sleeping is what proves `main`
		// actually reached the input.
		waitUntilPauseSucceeds(pid, worker);
		waitUntilStatus(pid, isSleeping, "Sleeping, with the worker parked and main on IO");

		auto pending = std::async(std::launch::async, [pid] { return api::pauseAll(pid); });
		assertTrue(
			pending.wait_for(BLOCKED_CALL_PROBE) != std::future_status::ready,
			"pauseAll returned while a thread was still sleeping on IO, instead of waiting for it"
		);

		// The IO completes, so the pending pause request finally takes effect.
		assertSucceeded(api::input(pid, "5 "), "input that wakes the sleeping thread");

		assertTrue(
			pending.wait_for(UNBLOCKED_CALL_BUDGET) == std::future_status::ready,
			"pauseAll never returned after the IO it was waiting for completed"
		);
		auto paused = pending.get();
		assertSucceeded(paused, "pauseAll after the sleeping thread woke up");
		ASSERT_EQUAL_PRINT(usize{ 2 }, paused->thread_ids.size());
		waitUntilStatus(pid, isPaused, "Paused after the woken thread parked");

		assertSucceeded(api::stop(pid), "stop");
		(void) api::kill(pid);
	}

	void pauseAllWaitIsEndedByStop() {
		// An IO read that never completes keeps `pauseAll` waiting forever, so `stop` has to be
		// able to end that wait. No endpoint may hold `api_lock` while it waits on an execution
		// thread, and this is the case that proves `pauseAll` does not.

		const vm::PID       pid = spawnAndLoad("sleeping_main.dbc");
		const api::ThreadID worker{ 1 };

		assertSucceeded(api::run(pid), "run of sleeping_main.dbc");
		waitUntilPauseSucceeds(pid, worker);
		waitUntilStatus(pid, isSleeping, "Sleeping, with the worker parked and main on IO");

		auto pending = std::async(std::launch::async, [pid] { return api::pauseAll(pid); });
		assertTrue(
			pending.wait_for(BLOCKED_CALL_PROBE) != std::future_status::ready,
			"pauseAll returned while a thread was still sleeping on IO"
		);

		// No input is ever sent. `stop` is the only way out, and it must not be blocked by the
		// waiting `pauseAll`.
		assertSucceeded(api::stop(pid), "stop while pauseAll is waiting on a sleeping thread");

		assertTrue(
			pending.wait_for(UNBLOCKED_CALL_BUDGET) == std::future_status::ready,
			"pauseAll kept waiting after the process was stopped"
		);
		auto paused = pending.get();
		assertSucceeded(paused, "pauseAll interrupted by a stop");
		// `main` is the thread the call was waiting for, and it reached a terminal state instead of
		// parking, so it must not be reported. The worker was already parked when the call started,
		// so it is reported even though the stop terminated it afterwards.
		assertTrue(
			std::ranges::find(paused->thread_ids, api::MAIN_THREAD_ID) == paused->thread_ids.end(),
			"pauseAll reported the thread that was stopped instead of paused"
		);
		waitUntilStatus(pid, isTerminal, "Stopped");
		(void) api::kill(pid);
	}

	void gilIsReleasedWhilePaused() {
		// A paused thread must not keep the GIL, otherwise the rest of the program stops making
		// progress the moment the debugger parks one thread.

		const vm::PID    pid = spawnAndLoad("output_worker.dbc");
		std::atomic<u64> writes{ 0 };

		events::Listener<std::string> output_listener([&writes](const std::string&) { writes++; });
		assertSucceeded(api::attachOutputListener(pid, &output_listener), "attachOutputListener");

		assertSucceeded(api::run(pid), "run of output_worker.dbc");
		waitUntilStatus(pid, isRunning, "Running");

		// Wait until the worker is past its first write, so the counter below only measures the
		// progress made after the pause.
		waitForWrites(pid, writes, 1, "the worker never produced any output");

		assertSucceeded(api::pause(pid, api::MAIN_THREAD_ID), "pause of the main thread");

		waitForWrites(
			pid,
			writes,
			writes.load() + WRITES_PROVING_PROGRESS,
			"the worker stopped producing output once the main thread was paused - is the paused "
			"thread still holding the GIL?"
		);

		output_listener.detach();
		assertSucceeded(api::stop(pid), "stop");
		(void) api::kill(pid);
	}

	void gilIsReleasedWhileSleepingOnIo() {
		// Same for a thread blocked on IO. The IO opcodes report the thread as `Sleeping` and then
		// block, which they may only do with the GIL released - otherwise a program waiting for
		// input freezes every other thread of the process.

		const vm::PID       pid = spawnAndLoad("sleeping_main.dbc");
		const api::ThreadID worker{ 1 };
		std::atomic<u64>    writes{ 0 };

		events::Listener<std::string> output_listener([&writes](const std::string&) { writes++; });
		assertSucceeded(api::attachOutputListener(pid, &output_listener), "attachOutputListener");

		assertSucceeded(api::run(pid), "run of sleeping_main.dbc");

		// Parking the worker is what makes the aggregate report Sleeping, which is the only way to
		// tell through the API that `main` really reached the input.
		waitUntilPauseSucceeds(pid, worker);
		waitUntilStatus(pid, isSleeping, "Sleeping, with the worker parked and main on IO");
		assertSucceeded(api::resume(pid, worker), "resume of the worker");

		// No input is ever sent, so `main` stays blocked for the rest of the test.
		waitForWrites(
			pid,
			writes,
			WRITES_PROVING_PROGRESS,
			"the worker produced no output while the main thread was blocked on input - is the "
			"sleeping thread still holding the GIL?"
		);

		output_listener.detach();
		assertSucceeded(api::stop(pid), "stop");
		(void) api::kill(pid);
	}

	void waitForBreakpointIsPerThread() {
		// `waitForBreakpoint` waits on the thread it was asked about, not on the process. The
		// aggregate never reports Paused here, because `main` keeps running, so a wait on the
		// process state would never return.

		const vm::PID       pid = spawnAndLoad("worker_breakpoint.dbc");
		const api::ThreadID worker{ 1 };

		assertSucceeded(
			api::setBreakpoint(pid, base::StrID("worker"), WORKER_BREAKPOINT_INDEX, true),
			"setBreakpoint in `worker`"
		);
		assertSucceeded(api::run(pid), "run of worker_breakpoint.dbc");

		auto hit = waitForBreakpointOfStartingThread(pid, worker);
		assertSucceeded(hit, "waitForBreakpoint on the worker thread");
		ASSERT_EQUAL_PRINT(std::string("worker"), hit->function_name.str());
		ASSERT_EQUAL_PRINT(WORKER_BREAKPOINT_INDEX, hit->instr_number);

		// The breakpoint belongs to `worker`, so `main` never hit it and is still running.
		auto status = api::getExecutionStatus(pid);
		assertSucceeded(status, "getExecutionStatus");
		assertTrue(isRunning(status.value()), "The process must still report Running");
		assertRefusedWith<api::ResumeError>(
			api::resume(pid, api::MAIN_THREAD_ID),
			"resume of the main thread, which never hit the worker's breakpoint",
			"the thread is not paused"
		);

		assertSucceeded(api::resume(pid, worker), "resume of the worker");
		assertSucceeded(api::stop(pid), "stop");
		(void) api::kill(pid);
	}

	void stepIsPerThread() {
		// `step` advances the thread it was asked about. Its new position is read back with
		// `waitForBreakpoint`, which returns immediately for a thread that is already parked.

		const vm::PID       pid = spawnAndLoad("worker_breakpoint.dbc");
		const api::ThreadID worker{ 1 };

		assertSucceeded(
			api::setBreakpoint(pid, base::StrID("worker"), WORKER_BREAKPOINT_INDEX, true),
			"setBreakpoint in `worker`"
		);
		assertSucceeded(api::run(pid), "run of worker_breakpoint.dbc");

		auto hit = waitForBreakpointOfStartingThread(pid, worker);
		assertSucceeded(hit, "waitForBreakpoint on the worker thread");

		// Otherwise the step would land on the breakpoint again.
		assertSucceeded(
			api::setBreakpoint(pid, base::StrID("worker"), WORKER_BREAKPOINT_INDEX, false),
			"clearing the breakpoint"
		);
		assertSucceeded(api::step(pid, worker), "step of the worker thread");

		auto after_step = api::waitForBreakpoint(pid, worker);
		assertSucceeded(after_step, "waitForBreakpoint on the already parked worker");
		ASSERT_EQUAL_PRINT(std::string("worker"), after_step->function_name.str());
		assertTrue(
			after_step->instr_number != hit->instr_number,
			base::strConcat(
				"A step did not move the worker: it is still at instruction ",
				after_step->instr_number
			)
		);

		// Stepping the main thread is refused, since it is running rather than paused.
		assertRefusedWith<api::OtherError>(
			api::step(pid, api::MAIN_THREAD_ID), "step of a running thread", "wrong execution status"
		);

		assertSucceeded(api::resume(pid, worker), "resume of the worker");
		assertSucceeded(api::stop(pid), "stop");
		(void) api::kill(pid);
	}

	void perThreadEndpointsRejectUnknownThreads() {
		// A thread id comes straight from an API client, so every per-thread endpoint has to reject
		// one it does not know instead of aborting the process.

		const vm::PID       pid = runSpinThreads();
		const api::ThreadID unknown{ 4'242 };

		assertRefusedWith<api::OtherError>(
			api::pause(pid, unknown), "pause of an unknown thread", "Thread not found"
		);
		assertRefusedWith<api::OtherError>(
			api::resume(pid, unknown), "resume of an unknown thread", "Thread not found"
		);
		assertRefusedWith<api::OtherError>(
			api::step(pid, unknown), "step of an unknown thread", "Thread not found"
		);
		assertRefusedWith<api::OtherError>(
			api::join(pid, unknown), "join of an unknown thread", "Thread not found"
		);
		assertRefusedWith<api::OtherError>(
			api::waitForBreakpoint(pid, unknown),
			"waitForBreakpoint on an unknown thread",
			"Thread not found"
		);

		// The refusals left the process running.
		auto status = api::getExecutionStatus(pid);
		assertSucceeded(status, "getExecutionStatus after the refused requests");
		assertTrue(isRunning(status.value()), "The process must still report Running");

		// The two stack-frame endpoints read guest memory, so they refuse a running process before
		// they ever look at the thread id. Park everything first, to reach the id check.
		assertSucceeded(api::pauseAll(pid), "pauseAll");
		assertRefusedWith<api::OtherError>(
			api::debuggerGetNumberOfStackFrames(pid, unknown),
			"debuggerGetNumberOfStackFrames of an unknown thread",
			"Thread not found"
		);
		assertRefusedWith<api::OtherError>(
			api::debuggerGetStackFrameData(pid, unknown, 0),
			"debuggerGetStackFrameData of an unknown thread",
			"Thread not found"
		);

		assertSucceeded(api::stop(pid), "stop");
		(void) api::kill(pid);
	}

	/**
	 * @brief Runs `spin_threads.dbc` and waits until every one of its threads is up.
	 */
	vm::PID runSpinThreads() {
		const vm::PID pid = spawnAndLoad("spin_threads.dbc");
		assertSucceeded(vm::api::run(pid), "run of spin_threads.dbc");
		waitUntilEveryThreadRuns(pid, SPIN_THREAD_COUNT);
		return pid;
	}

	/**
	 * @brief Pauses a thread, retrying while the running program has not started it yet.
	 *
	 * A worker is started by the running program, so a request aimed at it can arrive before the
	 * program got that far. That refusal is correct, and retrying is the client's job.
	 */
	void waitUntilPauseSucceeds(vm::PID pid, vm::api::ThreadID thread_id) {
		const auto deadline = std::chrono::steady_clock::now() + STATUS_WAIT_BUDGET;
		while (std::chrono::steady_clock::now() < deadline) {
			if (vm::api::pause(pid, thread_id).has_value()) return;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		fail(base::strConcat("Thread ", thread_id.asInt(), " never became pausable"));
	}

	/**
	 * @brief `waitForBreakpoint` on a thread the running program has not started yet.
	 *
	 * Same retry as `waitUntilPauseSucceeds`. Once the thread exists the wait itself is blocking,
	 * and it returns straight away for a thread that already parked.
	 */
	std::expected<vm::api::response::CodePosition, vm::api::ApiError> waitForBreakpointOfStartingThread(
		vm::PID pid, vm::api::ThreadID thread_id
	) {
		const auto deadline = std::chrono::steady_clock::now() + STATUS_WAIT_BUDGET;
		while (std::chrono::steady_clock::now() < deadline) {
			auto hit = vm::api::waitForBreakpoint(pid, thread_id);
			if (hit.has_value()) return hit;
			if (!std::holds_alternative<vm::api::OtherError>(hit.error())) return hit;
			if (std::get<vm::api::OtherError>(hit.error()).error != "Thread not found") return hit;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return std::unexpected(vm::api::ApiError{ vm::api::OtherError{
			base::strConcat("Thread ", thread_id.asInt(), " was never started by the program") } });
	}

	/**
	 * @brief Waits until the output listener has seen at least @p wanted writes.
	 */
	void waitForWrites(
		vm::PID pid, const std::atomic<u64>& writes, u64 wanted, std::string_view why
	) {
		const auto deadline = std::chrono::steady_clock::now() + STATUS_WAIT_BUDGET;
		while (std::chrono::steady_clock::now() < deadline) {
			if (writes.load() >= wanted) return;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		// Release the program before failing, so the suite can continue.
		(void) vm::api::stop(pid);
		fail(base::strConcat(why, " (saw ", writes.load(), " writes, wanted at least ", wanted, ")")
		);
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/debugger/");
