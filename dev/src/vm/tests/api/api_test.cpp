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
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace api = vm::api;

/**
 * @brief What belongs here: the contract of every `vm::api` endpoint on a single process. That
 * covers what each endpoint does in every process state and when it refuses a call.
 * Multi-threaded programs are used where a state needs several threads.
 *
 * The debug endpoints are only used as setup here, they go to `api/debug/`. Running one function
 * with its arguments and return values goes to `api/run_function/`, and loading more code into a
 * process goes to `api/injection/`.
 */
class VmApiTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmApiTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// @TODO: #3612 JIT-compiled code does not observe stop/pause requests. This suite
		// tests the API, not the JIT, so it spawns all of its processes with the JIT off
		// (ignored in non-JIT builds) instead of being disabled wholesale in JIT builds.
		process_config.enable_jit = false;

		TESTER_ADD_TEST(spawnAndKillEndpoints);
		TESTER_ADD_TEST(loadEndpoints);
		TESTER_ADD_TEST(setExecutionConfigEndpoint);
		TESTER_ADD_TEST(runEndpoint);
		TESTER_ADD_TEST(runAwaitEndpoint);
		TESTER_ADD_TEST(rerunEndpoint);
		TESTER_ADD_TEST(runFunctionEndpoints);
		TESTER_ADD_TEST(joinEndpoint);
		TESTER_ADD_TEST(stopEndpoint);
		TESTER_ADD_TEST(killFromEveryState);
		TESTER_ADD_TEST(deinitAndValidateEndpoint);
		TESTER_ADD_TEST(deinitReportsUnjoinedProgramThreads);
		TESTER_ADD_TEST(globalDestructorsRunOnDeinit);
		TESTER_ADD_TEST(deinitAfterPanicIsRefused);
		TESTER_ADD_TEST(deinitKillsTheProcessWhenAGlobalDestructorPanics);
		TESTER_ADD_TEST(deinitOnNotStartedProcessRunsNoGlobalDestructors);
		TESTER_ADD_TEST(deinitOrKillEndpoint);
		TESTER_ADD_TEST(ioEndpoints);
		TESTER_ADD_TEST(dataEndpoints);
		TESTER_ADD_TEST(listenerEndpoints);
		TESTER_ADD_TEST(outputListenerEndpoint);
		TESTER_ADD_TEST(deadProcessRejectsEveryEndpoint);
	}

	~VmApiTest() override = default;

private:
	/// The exit value of `breakpoint.dbc`.
	static constexpr i64 BREAKPOINT_EXIT_VALUE = 7;
	/// How many times `joinEndpoint` makes two `join`s race for the same thread.
	static constexpr usize CONCURRENT_JOIN_ATTEMPTS = 20;

	void spawnAndKillEndpoints() {
		// A freshly spawned process is `NotStarted` and answers status calls. A killed one is gone
		// from the supervisor, so every later request on it is refused.

		const vm::PID pid    = initProcess();
		auto          status = api::getExecutionStatus(pid);
		assertSucceeded(status, "getExecutionStatus on a fresh process");
		ASSERT_MATCHES_MSG(
			status.value(), "A fresh process must report NotStarted", api::NotStarted
		);

		assertSucceeded(api::kill(pid), "kill of a fresh process");
		assertRefusedWith<api::ProcessNotFound>(
			api::getExecutionStatus(pid), "getExecutionStatus after kill"
		);
		assertRefusedWith<api::ProcessNotFound>(api::kill(pid), "a second kill");
	}

	void loadEndpoints() {
		// `loadFiles` and `loadCode` accept valid bytecode and refuse anything else, without making
		// the process unusable.

		const vm::PID pid = initProcess();

		assertRefusedWith<api::LoadProgramError>(
			api::loadFiles(pid, { fs::File(path("api_test.cpp")) }),
			"loadFiles of a file that is not bytecode"
		);

		assertSucceeded(
			api::loadFiles(pid, { fs::File(path("breakpoint.dbc")) }), "loadFiles of valid bytecode"
		);
		// Loading the same program twice defines `main` twice.
		assertRefusedWith<api::LoadProgramError>(
			api::loadFiles(pid, { fs::File(path("breakpoint.dbc")) }), "a duplicate load"
		);

		// The refused loads left the process runnable.
		assertSucceeded(api::run(pid), "run after a refused load");
		waitUntilStatus(pid, isCompleted, "Completed");
		assertSucceeded(api::join(pid), "join");
		(void) api::kill(pid);

		// An empty load is a legal no-op.
		const vm::PID code_pid = initProcess();
		assertSucceeded(
			api::loadCode(code_pid, vm::code::CodeCollection{}),
			"loadCode of an empty code collection"
		);
		(void) api::kill(code_pid);
	}

	void setExecutionConfigEndpoint() {
		// `setExecutionConfig` is accepted at any point of a process's life.

		const vm::PID pid = spawnAndLoad("../common/programs/while_true.dbc");
		assertSucceeded(api::setExecutionConfig(pid, {}), "setExecutionConfig on NotStarted");

		assertSucceeded(api::run(pid), "run");
		waitUntilStatus(pid, isRunning, "Running");
		assertSucceeded(api::setExecutionConfig(pid, {}), "setExecutionConfig on a running process");

		assertSucceeded(api::stop(pid), "stop");
		assertSucceeded(api::setExecutionConfig(pid, {}), "setExecutionConfig on a stopped process");
		(void) api::kill(pid);

		// The config is only read by the next load, which is where it has to take effect.
		const vm::PID single_thread_pid = initProcess();
		assertSucceeded(
			api::setExecutionConfig(single_thread_pid, { .single_thread = true }),
			"setExecutionConfig with single_thread"
		);
		assertRefusedWith<api::LoadProgramError>(
			api::loadFiles(
				single_thread_pid, { fs::File(path("../common/programs/spin_threads.dbc")) }
			),
			"a load of a multi-threaded program into a single-threaded process",
			"single_thread flag is set, but function uses multithreading"
		);
		(void) api::kill(single_thread_pid);
	}

	void runEndpoint() {
		// `run` needs a `main` and refuses to start a second run of a live process.

		{
			// No code loaded at all - there is no `main` to run. The refusal comes from argument
			// validation, which runs before anything touches the process state, so the process is
			// left exactly as it was and stays runnable once a `main` is loaded.
			const vm::PID pid = initProcess();
			assertRefusedWith<api::RunError>(
				api::run(pid), "run without any loaded code", "Called function 'main' does not"
			);

			auto status = api::getExecutionStatus(pid);
			assertSucceeded(status, "getExecutionStatus after a refused run");
			ASSERT_MATCHES_MSG(
				status.value(), "A refused run must leave the process NotStarted", api::NotStarted
			);

			assertSucceeded(api::loadFiles(pid, { fs::File(path("breakpoint.dbc")) }), "loadFiles");
			assertSucceeded(api::run(pid), "run after loading a `main`");
			waitUntilStatus(pid, isCompleted, "Completed");
			assertSucceeded(api::join(pid), "join");
			(void) api::kill(pid);
		}

		{
			const vm::PID pid = spawnAndLoad("../common/programs/while_true.dbc");
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");

			assertRefusedWith<api::StateError>(
				api::run(pid), "a second run of a running process", "the process is still executing"
			);

			assertSucceeded(api::stop(pid), "stop");
			(void) api::kill(pid);
		}
	}

	void runAwaitEndpoint() {
		// `runAwait` runs the program on the caller's thread and hands back its exit value, so it
		// needs neither a wait nor a join.

		const vm::PID pid    = spawnAndLoad("breakpoint.dbc");
		auto          result = api::runAwait(pid);
		assertSucceeded(result, "runAwait");
		assertReturnedI64(result.value(), BREAKPOINT_EXIT_VALUE, "runAwait");

		auto status = api::getExecutionStatus(pid);
		assertSucceeded(status, "getExecutionStatus after runAwait");
		ASSERT_MATCHES_MSG(
			status.value(), "runAwait must leave the process completed", api::ExecutionCompleted
		);
		// `runAwait` already handed the exit value back, and it never spawned an execution thread,
		// so a `join` has nothing left to reap.
		assertRefusedWith<api::JoinError>(api::join(pid), "join after runAwait");

		// `runAwait` joins the thread it ran on, so it may be called again without a `join`.
		for (usize call = 1; call <= 2; call++) {
			auto repeated = api::runAwait(pid);
			assertSucceeded(repeated, base::strConcat("runAwait number ", call + 1));
			assertReturnedI64(
				repeated.value(),
				BREAKPOINT_EXIT_VALUE,
				base::strConcat("runAwait number ", call + 1)
			);
		}
		(void) api::kill(pid);

		// `runAwait` reports the state of the whole process, not of the thread it ran on, so a
		// worker that is still alive when `main` returns hides the exit value.
		const vm::PID worker_pid = spawnAndLoad("worker_outlives_main.dbc");
		assertRefusedWith<api::OtherError>(
			api::runAwait(worker_pid),
			"runAwait of a program whose worker outlives `main`",
			"Execution did not complete"
		);
		(void) api::kill(worker_pid);

		// A program that panics reports the panic instead of an exit value.
		const vm::PID panicking_pid = spawnAndLoad("panic.dbc");
		assertRefusedWith<api::Panicked>(
			api::runAwait(panicking_pid), "runAwait of a panicking program", "Tried dividing by zero"
		);
		(void) api::kill(panicking_pid);

		// There is no `main` to run without a loaded program.
		const vm::PID empty_pid = initProcess();
		assertRefusedWith<api::RunError>(
			api::runAwait(empty_pid), "runAwait without any loaded code", "Called function 'main'"
		);
		(void) api::kill(empty_pid);
	}

	void rerunEndpoint() {
		// A rerun is legal only after a run that completed AND whose threads were all joined.

		{  // Completed and joined - the only case that may run again.
			const vm::PID   pid = spawnAndLoad("breakpoint.dbc");
			ScopedStatusLog scoped(*this, pid);

			assertSucceeded(api::run(pid), "the first run");
			waitUntilStatus(pid, isCompleted, "Completed");

			assertRefusedWith<api::StateError>(
				api::run(pid),
				"a rerun without a join",
				"thread 0 of the previous run was never joined"
			);

			assertSucceeded(api::join(pid), "join of the main thread");
			assertSucceeded(api::run(pid), "a rerun after the join");

			waitUntilStatus(pid, isCompleted, "Completed again");
			assertSucceeded(api::join(pid), "join of the second run");

			validateTransitions(scoped.log, "a rerun of a completed process");

			auto valid = api::deinitAndValidate(pid);
			assertSucceeded(valid, "deinitAndValidate after two clean runs");
			ASSERT_TRUE(valid.value().valid());
		}

		{
			// Only the threads the caller started have to be joined. The two workers were started
			// by the program itself, and the caller never even learns their IDs, so the DVM reaps
			// them before the rerun instead of refusing it.
			const vm::PID   pid = spawnAndLoad("unjoined_threads.dbc");
			ScopedStatusLog scoped(*this, pid);

			for (usize run = 1; run <= 3; run++) {
				assertSucceeded(api::run(pid), base::strConcat("run number ", run));
				// The process is `Completed` only once the workers finished as well.
				waitUntilStatus(pid, isCompleted, "Completed");
				assertSucceeded(api::join(pid), base::strConcat("join of run number ", run));
			}

			validateTransitions(scoped.log, "a rerun of a multi-threaded process");

			auto valid = api::deinitAndValidate(pid);
			assertSucceeded(valid, "deinitAndValidate after three multi-threaded runs");
			ASSERT_TRUE(valid.value().memory_valid);
			// Every run leaked both of its workers. The VMThreads are recycled, so the run number
			// is what tells the six of them apart.
			ASSERT_EQUAL(valid.value().unjoined_program_threads.size(), 6);
			ASSERT_EQUAL_PRINT(
				std::string(
					"The program started `bump` (thread 1), `bump` (thread 2) in run 1 and never "
					"joined them.\n"
					"The program started `bump` (thread 1), `bump` (thread 2) in run 2 and never "
					"joined them.\n"
					"The program started `bump` (thread 1), `bump` (thread 2) in run 3 and never "
					"joined them.\n"
				),
				valid.value().toString()
			);
		}

		{  // Stopped: refused right away, and still refused after the process was drained.
			const vm::PID pid = spawnAndLoad("../common/programs/while_true.dbc");
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");
			assertSucceeded(api::stop(pid), "stop");
			waitUntilStatus(pid, isStopped, "Stopped");

			assertRefusedWith<api::StateError>(
				api::run(pid), "a rerun right after a stop", "must be freshly loaded"
			);
			// `stop` reaps the execution threads itself, so there is no handle left for `join`.
			assertRefusedWith<api::JoinError>(api::join(pid), "join after a stop");
			assertRefusedWith<api::StateError>(
				api::run(pid), "a rerun after a stop and a join attempt", "must be freshly loaded"
			);
			(void) api::kill(pid);
		}

		{  // Panicked: same as `join`
			const vm::PID pid = spawnAndLoad("panic.dbc");
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isPanicked, "Panicked");

			assertRefusedWith<api::StateError>(
				api::run(pid), "a rerun right after a panic", "must be freshly loaded"
			);
			assertRefusedWith<api::Panicked>(
				api::join(pid), "join of a panicked thread", "Tried dividing by zero"
			);
			assertRefusedWith<api::StateError>(
				api::run(pid), "a rerun after a panic and a join", "must be freshly loaded"
			);
			(void) api::kill(pid);
		}
	}

	void runFunctionEndpoints() {
		// `runFunction` starts a named function on a thread of its own, `runFunctionAwait` runs it
		// on the caller's thread and hands back the return value. Both validate the arguments
		// against the loaded signature.

		{
			const vm::PID pid = spawnAndLoad("add.dbc");

			assertRefusedWith<api::RunError>(
				api::runFunction(pid, "no_such_function"),
				"runFunction of a missing function",
				"does not exist"
			);
			assertRefusedWith<api::RunError>(
				api::runFunction(pid, "add"),
				"runFunction of `add` without arguments",
				"expects 2 arguments, but 0 were provided"
			);

			auto wrong_type = api::getVMValue(pid, "i32");
			auto right_type = api::getVMValue(pid, "i64");
			assertSucceeded(wrong_type, "getVMValue(i32)");
			assertSucceeded(right_type, "getVMValue(i64)");
			assertRefusedWith<api::RunError>(
				api::runFunction(
					pid, "add", { wrong_type->vm_value.refMut(), right_type->vm_value.refMut() }
				),
				"runFunction of `add` with a mistyped argument",
				"Type mismatch for argument 0"
			);

			auto started = api::runFunction(
				pid, "add", { right_type->vm_value.refMut(), right_type->vm_value.refMut() }
			);
			assertSucceeded(started, "runFunction of `add`");
			assertTrue(
				started.value() == api::MAIN_THREAD_ID,
				"The first `runFunction` of a process must use the main thread"
			);
			assertSucceeded(api::join(pid, started.value()), "join of the `add` thread");

			assertSucceeded(api::freeVMValue(pid, wrong_type->vm_value.refMut()), "freeVMValue");
			assertSucceeded(api::freeVMValue(pid, right_type->vm_value.refMut()), "freeVMValue");
			(void) api::kill(pid);
		}

		{
			const vm::PID pid = spawnAndLoad("add.dbc");

			auto lhs = api::getVMValue(pid, "i64");
			auto rhs = api::getVMValue(pid, "i64");
			assertSucceeded(lhs, "getVMValue(i64)");
			assertSucceeded(rhs, "getVMValue(i64)");
			lhs->vm_value->writeBytes<i64>(17);
			rhs->vm_value->writeBytes<i64>(25);

			auto result = api::runFunctionAwait(
				pid, "add", { lhs->vm_value.refMut(), rhs->vm_value.refMut() }
			);
			assertSucceeded(result, "runFunctionAwait of `add`");
			assertReturnedI64(result.value(), 42, "runFunctionAwait of `add`");

			assertSucceeded(api::freeVMValue(pid, lhs->vm_value.refMut()), "freeVMValue");
			assertSucceeded(api::freeVMValue(pid, rhs->vm_value.refMut()), "freeVMValue");

			assertRefusedWith<api::RunError>(
				api::runFunctionAwait(pid, "no_such_function"),
				"runFunctionAwait of a missing function",
				"does not exist"
			);
			(void) api::kill(pid);
		}

		{  // A panicking function reports the panic instead of a return value.
			const vm::PID pid = spawnAndLoad("panic.dbc");
			assertRefusedWith<api::Panicked>(
				api::runFunctionAwait(pid, "main"),
				"runFunctionAwait of a panicking function",
				"Tried dividing by zero"
			);
			(void) api::kill(pid);
		}

		{  // A function that never returns is torn down by a kill.
			const vm::PID pid = spawnAndLoad("../common/programs/spin_threads.dbc");
			assertSucceeded(api::runFunction(pid, "spinner"), "runFunction of an endless function");
			assertSucceeded(api::kill(pid), "kill of a process running a function");
		}
	}

	void joinEndpoint() {
		// `join` reaps the execution thread of one VMThread. It refuses an unknown thread and a
		// thread that never ran.

		const vm::PID pid = spawnAndLoad("breakpoint.dbc");

		assertRefusedWith<api::OtherError>(
			api::join(pid, api::ThreadID{ 77 }), "join of an unknown thread", "Thread not found"
		);
		assertRefusedWith<api::JoinError>(api::join(pid), "join before a run");

		assertSucceeded(api::run(pid), "run");
		waitUntilStatus(pid, isCompleted, "Completed");
		assertSucceeded(api::join(pid, api::MAIN_THREAD_ID), "join of a completed run");
		// The handle is gone now, so a second join has nothing to reap.
		assertRefusedWith<api::JoinError>(api::join(pid), "a second join");
		// The refused join left the process joined, so it can still be deinitialized.
		assertSucceeded(api::deinitAndValidate(pid), "deinitAndValidate after a second join");

		{
			// Two callers join the same finished thread at the same time. Only the one which
			// reaps the execution thread may get the exit value, the other one gets `JoinError`.
			const vm::PID racy_pid = spawnAndLoad("breakpoint.dbc");
			for (usize attempt = 1; attempt <= CONCURRENT_JOIN_ATTEMPTS; attempt++) {
				assertSucceeded(api::run(racy_pid), base::strConcat("run number ", attempt));
				waitUntilStatus(racy_pid, isCompleted, "Completed");

				auto first  = std::async(std::launch::async, [&] { return api::join(racy_pid); });
				auto second = std::async(std::launch::async, [&] { return api::join(racy_pid); });
				const auto results = std::array{ first.get(), second.get() };

				ASSERT_EQUAL_PRINT(
					usize{ 1 },
					static_cast<usize>(
						std::ranges::count_if(results, [](const auto& r) { return r.has_value(); })
					)
				);
				for (const auto& result: results) {
					if (result.has_value()) continue;
					assertRefusedWith<api::JoinError>(result, "the join which lost the race");
				}
			}
			assertSucceeded(api::deinitAndValidate(racy_pid), "deinitAndValidate after the races");
		}
	}

	void stopEndpoint() {
		// `stop` is legal from every state, including the states where it does nothing.

		{
			// NotStarted: there is nothing to stop. The flag is deliberately not raised, because a
			// process that never ran must stay runnable.
			const vm::PID pid = spawnAndLoad("../common/programs/while_true.dbc");
			assertSucceeded(api::stop(pid), "stop of a NotStarted process");
			auto status = api::getExecutionStatus(pid);
			assertSucceeded(status, "getExecutionStatus after a stop of a NotStarted process");
			ASSERT_MATCHES_MSG(
				status.value(),
				"A stop of a process that never ran must leave it NotStarted",
				api::NotStarted
			);
			assertSucceeded(api::run(pid), "run after a stop of a NotStarted process");
			(void) api::kill(pid);
		}

		{
			const vm::PID   pid = spawnAndLoad("../common/programs/while_true.dbc");
			ScopedStatusLog scoped(*this, pid);

			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");
			assertSucceeded(api::stop(pid), "stop of a running process");
			waitUntilStatus(pid, isStopped, "Stopped");
			assertSucceeded(api::stop(pid), "a second stop");

			assertTrue(
				scoped.log.wasEmitted<api::ExecutionStopping>(),
				"Stopping a Running program never emitted the ExecutionStopping status"
			);
			validateTransitions(scoped.log, "a stop of a running process");
			(void) api::kill(pid);
		}

		{  // A pending pause must not keep a stop from finishing.
			const vm::PID pid = spawnAndLoad("../common/programs/while_true.dbc");
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");
			assertSucceeded(api::pause(pid), "pause");
			assertSucceeded(api::stop(pid), "stop of a paused process");
			waitUntilStatus(pid, isStopped, "Stopped from Paused");
			(void) api::kill(pid);
		}

		{  // Several threads, some of them parked by the debugger.
			const vm::PID   pid = spawnAndLoad("../common/programs/spin_threads.dbc");
			ScopedStatusLog scoped(*this, pid);

			assertSucceeded(api::run(pid), "run of spin_threads.dbc");
			waitUntilEveryThreadRuns(pid, SPIN_THREAD_COUNT);
			assertSucceeded(api::pause(pid, api::ThreadID{ 1 }), "pause of worker 1");
			assertSucceeded(api::pause(pid, api::ThreadID{ 2 }), "pause of worker 2");

			assertSucceeded(api::stop(pid), "stop of a process with parked and running threads");
			waitUntilStatus(pid, isStopped, "Stopped");

			for (usize tid = 0; tid < SPIN_THREAD_COUNT; tid++)
				assertRefusedWith<api::JoinError>(
					api::join(pid, api::ThreadID{ tid }),
					base::strConcat("join of thread ", tid, " after a stop")
				);

			validateTransitions(scoped.log, "a multi-threaded stop");
			(void) api::kill(pid);
		}
	}

	void killFromEveryState() {
		// `kill` must succeed from every reachable state and leave no process behind. After a
		// successful kill the process is removed from the supervisor, so a later status query
		// fails.

		auto kill_from = [&](std::string_view state_name, const std::string& program, auto setup) {
			const vm::PID pid = spawnAndLoad(program);
			setup(pid);
			assertSucceeded(api::kill(pid), base::strConcat("kill from ", state_name));
			assertRefusedWith<api::ProcessNotFound>(
				api::getExecutionStatus(pid),
				base::strConcat("a status query after a kill from ", state_name)
			);
		};

		kill_from("NotStarted", "../common/programs/while_true.dbc", [](vm::PID) {});

		kill_from("Running", "../common/programs/while_true.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");
		});

		kill_from("Paused", "../common/programs/while_true.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");
			assertSucceeded(api::pause(pid), "pause");
			waitUntilStatus(pid, isPaused, "Paused");
		});

		// Blocked on a bytecode condition variable. `builtinWaitCV` brackets the wait with
		// `ScopedBlockingWait`, so the thread reports itself as sleeping for its duration, which is
		// the readiness signal this case waits for.
		kill_from("Blocked(mutex/cv)", "mutex_hang.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isSleeping, "Sleeping (CV-blocked)");
		});

		kill_from("Sleeping(IO)", "../common/programs/io_hang.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isSleeping, "Sleeping(IO)");
		});

		kill_from("Running(4 threads)", "../common/programs/spin_threads.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilEveryThreadRuns(pid, SPIN_THREAD_COUNT);
		});

		kill_from("Paused(4 threads)", "../common/programs/spin_threads.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilEveryThreadRuns(pid, SPIN_THREAD_COUNT);
			assertSucceeded(api::pauseAll(pid), "pauseAll");
			waitUntilStatus(pid, isPaused, "Paused");
		});

		kill_from("Completed", "breakpoint.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isTerminal, "Terminal");
		});

		// Panicked by an integer division by zero.
		kill_from("Panicked", "panic.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isPanicked, "Panicked");
		});

		// A stopped process is kept in the table until it is killed.
		kill_from("Stopped", "../common/programs/while_true.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");
			assertSucceeded(api::stop(pid), "stop");
			waitUntilStatus(pid, isTerminal, "Terminal");
		});
	}

	void deinitAndValidateEndpoint() {
		// `deinitAndValidate` accepts only a process that never ran or completed cleanly and was
		// joined. It refuses an executing one, a stopped one and an unjoined one, reports a leaked
		// allocation, and drops the process once it ran.

		{
			// An executing process must not have its globals torn down under the running program.
			const vm::PID pid = spawnAndLoad("../common/programs/while_true.dbc");
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");

			assertRefusedWith<api::StateError>(
				api::deinitAndValidate(pid),
				"deinitAndValidate of a running process",
				"the process is still executing"
			);
			assertSucceeded(api::stop(pid), "stop");

			assertRefusedWith<api::StateError>(
				api::deinitAndValidate(pid),
				"deinitAndValidate of a stopped process",
				"Use `api::kill` instead"
			);

			assertSucceeded(api::getExecutionStatus(pid), "getExecutionStatus after the refusal");
			assertSucceeded(api::kill(pid), "kill of a stopped process");
		}

		{  // A clean run leaves a valid memory state, and the process is dropped.
			const vm::PID pid = spawnAndLoad("breakpoint.dbc");
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isCompleted, "Completed");

			// The run completed, but nobody joined its execution thread yet.
			assertRefusedWith<api::StateError>(
				api::deinitAndValidate(pid),
				"deinitAndValidate of a completed but unjoined run",
				"thread 0 of the previous run was never joined"
			);

			assertSucceeded(api::join(pid), "join");
			auto valid = api::deinitAndValidate(pid);
			assertSucceeded(valid, "deinitAndValidate after a clean run");
			ASSERT_TRUE(valid.value().memory_valid);
			// A program which started no threads of its own has nothing to report.
			ASSERT_TRUE(valid.value().unjoined_program_threads.empty());
			assertRefusedWith<api::ProcessNotFound>(
				api::getExecutionStatus(pid), "a status query after deinitAndValidate"
			);
		}

		{
			const vm::PID pid = spawnAndLoad("../common/programs/global_leak.dbc");
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isCompleted, "Completed");
			assertSucceeded(api::join(pid), "join");
			auto valid = api::deinitAndValidate(pid);
			assertSucceeded(valid, "deinitAndValidate of a leaking program");
			assertFalse(
				valid.value().memory_valid, "A leaked allocation must be reported as invalid"
			);
		}
	}

	void deinitReportsUnjoinedProgramThreads() {
		// The program starts two workers and never joins them. That is not something the caller
		// can fix, so the deinit does not refuse. It joins the workers itself and reports them.

		const vm::PID pid = spawnAndLoad("unjoined_threads.dbc");
		assertSucceeded(api::run(pid), "run");
		// The process is `Completed` only once the workers finished as well.
		waitUntilStatus(pid, isCompleted, "Completed");
		// Only the main thread was started through the API, so only it has to be joined.
		assertSucceeded(api::join(pid), "join of the main thread");

		auto valid = api::deinitAndValidate(pid);
		assertSucceeded(valid, "deinitAndValidate with two unjoined worker threads");
		ASSERT_TRUE(valid.value().memory_valid);
		assertFalse(valid.value().valid(), "Unjoined program threads must fail the validation");

		const auto& unjoined = valid.value().unjoined_program_threads;
		ASSERT_EQUAL(unjoined.size(), 2);
		assertTrue(
			std::ranges::all_of(
				unjoined,
				[](const api::response::UnjoinedThread& thread) {
					return thread.function_name.has_value()
			            && thread.function_name.value() == "bump";
				}
			),
			"Every reported thread must name the function it ran"
		);
		assertTrue(
			unjoined.at(0).run_number == 1 && unjoined.at(1).run_number == 1,
			"Both workers were started by the first run"
		);
		// The report names the function, the thread IDs are only there to tell two threads of one
		// run apart. The main thread is API owned, so it is not in the report.
		ASSERT_EQUAL_PRINT(
			std::string(
				"The program started `bump` (thread 1), `bump` (thread 2) and never joined them.\n"
			),
			valid.value().toString()
		);
		assertRefusedWith<api::ProcessNotFound>(
			api::getExecutionStatus(pid), "a status query after deinitAndValidate"
		);

		{
			// The workers leak in the first run only. The rerun resets every thread, but the final
			// deinit must still report what the first run left behind.
			const vm::PID rerun_pid = spawnAndLoad("unjoined_threads.dbc");
			assertSucceeded(api::run(rerun_pid), "run which leaks two workers");
			waitUntilStatus(rerun_pid, isCompleted, "Completed");
			assertSucceeded(api::join(rerun_pid), "join of the leaking run");

			auto bump = api::runFunction(rerun_pid, "bump");
			assertSucceeded(bump, "a clean rerun of `bump`");
			waitUntilStatus(rerun_pid, isCompleted, "Completed again");
			assertSucceeded(api::join(rerun_pid, bump.value()), "join of the clean rerun");

			auto rerun_valid = api::deinitAndValidate(rerun_pid);
			assertSucceeded(rerun_valid, "deinitAndValidate after a leaking run and a clean one");
			// Both workers belong to the first run, and the report still says so after the rerun
			// recycled their VMThreads.
			const auto& leaked = rerun_valid.value().unjoined_program_threads;
			ASSERT_EQUAL(leaked.size(), 2);
			assertTrue(
				leaked.at(0).run_number == 1 && leaked.at(1).run_number == 1,
				"The workers of the first run must stay attributed to it"
			);
			ASSERT_EQUAL_PRINT(
				std::string(
					"The program started `bump` (thread 1), `bump` (thread 2) and never joined "
					"them.\n"
				),
				rerun_valid.value().toString()
			);
		}
	}

	void globalDestructorsRunOnDeinit() {
		// The global destructors run as bytecode during `deinitAndValidate`, after the run has
		// already ended.

		{
			// `global_destructor.dbc` frees in its destructor what its constructor allocated, so a
			// destructor that did not run shows up as an invalid memory state.
			const vm::PID pid = spawnAndLoad("global_destructor.dbc");
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isCompleted, "Completed");
			assertSucceeded(api::join(pid), "join");

			auto valid = api::deinitAndValidate(pid);
			assertSucceeded(valid, "deinitAndValidate after a completed run");
			assertTrue(
				valid.value().memory_valid,
				"The global destructor did not run, so what the constructor allocated was leaked"
			);
		}

		{
			// The same program stopped mid-run: the destructor must NOT be executed.
			const vm::PID pid = spawnAndLoad("global_destructor_loop.dbc");

			std::atomic<u64>              writes{ 0 };
			events::Listener<std::string> output_listener([&writes](const std::string&) {
				writes++;
			});
			assertSucceeded(
				api::attachOutputListener(pid, &output_listener), "attachOutputListener"
			);

			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");
			assertSucceeded(api::stop(pid), "stop");
			waitUntilStatus(pid, isStopped, "Stopped");

			assertRefusedWith<api::StateError>(
				api::deinitAndValidate(pid),
				"deinitAndValidate after a stop",
				"Use `api::kill` instead"
			);
			assertSucceeded(api::kill(pid), "kill of a stopped process");
			assertEqual(
				u64(0),
				writes.load(),
				"The global destructor ran after a stop, but a stopped run must not execute any "
				"more bytecode"
			);
			output_listener.detach();
		}
	}

	void deinitAfterPanicIsRefused() {
		// A refused deinit leaves the process alive, so it can still be killed.
		const auto result = runTestOnVmGetResult("../common/programs/local_leak.dbc", "", "");
		ASSERT_NO_VALUE(result.run_result);

		assertRefusedWith<api::StateError>(
			api::deinitAndValidate(result.pid), "deinitAndValidate of a panicked process"
		);

		auto status = api::getExecutionStatus(result.pid);
		assertSucceeded(status, "getExecutionStatus after the refused deinit");
		ASSERT_MATCHES_MSG(
			status.value(),
			"A refused deinit must leave the process panicked",
			api::ExecutionPanicked
		);

		assertSucceeded(api::kill(result.pid), "kill after the refused deinit");
		assertRefusedWith<api::ProcessNotFound>(
			api::getExecutionStatus(result.pid), "getExecutionStatus after the kill"
		);
	}

	void deinitKillsTheProcessWhenAGlobalDestructorPanics() {
		// The run completes and the deinit is legal, but a global destructor panics during it.
		const auto result = runTestOnVmGetResult("panicking_global_destructor.dbc", "", "");
		ASSERT_HAS_VALUE(result.run_result);

		auto status = api::getExecutionStatus(result.pid);
		assertSucceeded(status, "getExecutionStatus after the run");
		ASSERT_TRUE(api::canDeinit(status.value()));

		assertRefusedWith<api::Panicked>(
			api::deinitAndValidate(result.pid), "deinitAndValidate with a panicking destructor"
		);

		// The deinit ran, so the process is gone and a second one has nothing to deinitialize.
		assertRefusedWith<api::ProcessNotFound>(
			api::getExecutionStatus(result.pid), "getExecutionStatus after the failed deinit"
		);
		assertRefusedWith<api::ProcessNotFound>(
			api::deinitAndValidate(result.pid), "a second deinitAndValidate"
		);
	}

	void deinitOnNotStartedProcessRunsNoGlobalDestructors() {
		// No constructor ran on a process which was only loaded, so the deinit must not run any
		// destructor either.
		const vm::PID pid = spawnAndLoad("two_globals_second_destructor_aborts.dbc");

		std::ostringstream captured;
		assertSucceeded(api::attach(pid, std::cin, captured), "attach");

		auto deinit = api::deinitAndValidate(pid);
		assertSucceeded(deinit, "deinitAndValidate of a process that never ran");
		ASSERT_TRUE(deinit.value().valid());
		ASSERT_EQUAL_PRINT(std::string(""), captured.str());
		assertRefusedWith<api::ProcessNotFound>(
			api::getExecutionStatus(pid), "getExecutionStatus after the deinit"
		);
	}

	void deinitOrKillEndpoint() {
		// `deinitOrKill` deinitializes a completed process and kills one that cannot be
		// deinitialized. Either way the process is gone afterwards.
		const auto completed
			= runTestOnVmGetResult("../common/programs/no_double_destructor.dbc", "", "");
		ASSERT_HAS_VALUE(completed.run_result);
		auto clean_teardown = api::deinitOrKill(completed.pid);
		assertSucceeded(clean_teardown, "deinitOrKill of a completed process");
		ASSERT_HAS_VALUE(
			clean_teardown.value(), "A completed process must be deinitialized, not killed"
		);
		ASSERT_TRUE(clean_teardown->value().valid());

		const auto panicked = runTestOnVmGetResult("../common/programs/local_leak.dbc", "", "");
		ASSERT_NO_VALUE(panicked.run_result);
		auto forced_teardown = api::deinitOrKill(panicked.pid);
		assertSucceeded(forced_teardown, "deinitOrKill of a panicked process");
		ASSERT_NO_VALUE(
			forced_teardown.value(), "A panicked process must be killed, not deinitialized"
		);

		assertRefusedWith<api::ProcessNotFound>(
			api::getExecutionStatus(completed.pid), "getExecutionStatus of the deinitialized process"
		);
		assertRefusedWith<api::ProcessNotFound>(
			api::getExecutionStatus(panicked.pid), "getExecutionStatus of the killed process"
		);

		// A process whose deinit fails half-way is killed, and the destructors that already ran are
		// not undone.
		const auto aborting = runTestOnVmGetResult("two_globals_second_destructor_aborts.dbc");
		ASSERT_HAS_VALUE(aborting.run_result);

		std::ostringstream captured;
		assertSucceeded(api::attach(aborting.pid, std::cin, captured), "attach");

		auto teardown = api::deinitOrKill(aborting.pid);
		assertSucceeded(teardown, "deinitOrKill of a process with an aborting destructor");
		ASSERT_NO_VALUE(teardown.value(), "A failed deinit must be reported as a kill");
		assertRefusedWith<api::ProcessNotFound>(
			api::getExecutionStatus(aborting.pid), "getExecutionStatus after the failed deinit"
		);
		// `b_dtor` ran once, before `a_dtor` aborted.
		ASSERT_EQUAL_PRINT(std::string("2"), captured.str());
	}

	void ioEndpoints() {
		// `attach`, `detach`, `input` and `output`.

		{  // `input` reaches a thread that is already sleeping on it, and wakes it.
			const vm::PID pid = spawnAndLoad("../common/programs/io_hang.dbc");
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isSleeping, "Sleeping(IO)");
			assertSucceeded(api::input(pid, "42 "), "input to a sleeping thread");
			waitUntilStatus(pid, isTerminal, "Terminal after the input");
			assertSucceeded(api::join(pid), "join");
			// `output` blocks while the process executes, so it is only read once it is terminal.
			assertSucceeded(api::output(pid), "output after the program finished");
			(void) api::kill(pid);
		}

		{
			const vm::PID pid = spawnAndLoad("breakpoint.dbc");

			// `output` of a process that is not executing returns whatever is buffered, without
			// blocking.
			auto empty = api::output(pid);
			assertSucceeded(empty, "output of a NotStarted process");
			ASSERT_EQUAL_PRINT(std::string(""), empty->output);

			// `detach` without a previous `attach` has nothing to undo.
			assertRefusedWith<api::AttachDetachError>(api::detach(pid), "detach without an attach");

			std::istringstream in("7 ");
			std::ostringstream out;
			assertSucceeded(api::attach(pid, in, out), "attach");
			assertRefusedWith<api::AttachDetachError>(api::attach(pid, in, out), "a second attach");
			// While the streams are redirected the output cannot be read back through the API.
			assertRefusedWith<api::IOError>(
				api::output(pid), "output while the IO is redirected", "IO is being redirected"
			);
			assertSucceeded(api::detach(pid), "detach");
			assertSucceeded(api::output(pid), "output after the detach");
			(void) api::kill(pid);
		}
	}

	void dataEndpoints() {
		// `getType`, `getVMValue` and `freeVMValue` answer for loaded types only, and only while
		// the process can respond.

		const vm::PID pid = spawnAndLoad("../common/programs/while_true.dbc");

		assertSucceeded(api::getType(pid, "i64"), "getType(i64)");
		assertRefusedWith<api::OtherError>(
			api::getType(pid, "DoesNotExist"), "getType of an unknown type", "Type not found"
		);

		auto value = api::getVMValue(pid, "i64");
		assertSucceeded(value, "getVMValue(i64)");
		// The caller owns this one, so it has to be freed through the API.
		assertSucceeded(api::freeVMValue(pid, value->vm_value.refMut()), "freeVMValue");
		assertRefusedWith<api::OtherError>(
			api::freeVMValue(pid, value->vm_value.refMut()),
			"freeVMValue of an already freed value",
			"already freed"
		);
		assertRefusedWith<api::OtherError>(
			api::getVMValue(pid, "DoesNotExist"), "getVMValue of an unknown type", "Type not found"
		);

		{
			const vm::PID other_pid   = spawnAndLoad("../common/programs/while_true.dbc");
			auto          other_value = api::getVMValue(other_pid, "i64");
			assertSucceeded(other_value, "getVMValue(i64) of the other process");
			assertRefusedWith<api::OtherError>(
				api::freeVMValue(pid, other_value->vm_value.refMut()),
				"freeVMValue of a value from a different process",
				"different process"
			);
			assertSucceeded(
				api::freeVMValue(other_pid, other_value->vm_value.refMut()),
				"freeVMValue in the owning process"
			);
			(void) api::kill(other_pid);
		}

		{
			// The exit values are owned by the process and can't be freed by the user.
			const vm::PID add_pid  = spawnAndLoad("add.dbc");
			auto          argument = api::getVMValue(add_pid, "i64");
			assertSucceeded(argument, "getVMValue(i64) of the `add` argument");
			auto result = api::runFunctionAwait(
				add_pid, "add", { argument->vm_value.refMut(), argument->vm_value.refMut() }
			);
			assertSucceeded(result, "runFunctionAwait of `add`");
			ASSERT_MATCHES(result.value(), std::vector<Ref<vm::IVMValue>>);
			assertRefusedWith<api::OtherError>(
				api::freeVMValue(
					add_pid, v_get(result.value(), std::vector<Ref<vm::IVMValue>>).at(0)
				),
				"freeVMValue of an exit value",
				"owned by the process"
			);
			assertSucceeded(
				api::freeVMValue(add_pid, argument->vm_value.refMut()),
				"freeVMValue of the `add` argument"
			);
			auto valid = api::deinitAndValidate(add_pid);
			assertSucceeded(valid, "deinitAndValidate after a refused free of an exit value");
			ASSERT_TRUE(valid.value().valid());
		}

		auto kept_value = api::getVMValue(pid, "i64");
		assertSucceeded(kept_value, "getVMValue(i64) kept over the run");

		assertSucceeded(api::run(pid), "run");
		waitUntilStatus(pid, isRunning, "Running");
		// Reading guest memory while the interpreter mutates it is refused.
		assertRefusedWith<api::OtherError>(
			api::getType(pid, "i64"), "getType of a running process", "while program is running"
		);
		assertRefusedWith<api::OtherError>(
			api::getVMValue(pid, "i64"),
			"getVMValue of a running process",
			"while program is running"
		);
		assertRefusedWith<api::OtherError>(
			api::freeVMValue(pid, kept_value->vm_value.refMut()),
			"freeVMValue of a running process",
			"while program is running"
		);

		assertSucceeded(api::stop(pid), "stop");
		assertSucceeded(
			api::freeVMValue(pid, kept_value->vm_value.refMut()), "freeVMValue after the stop"
		);
		(void) api::kill(pid);
	}

	void listenerEndpoints() {
		// A status listener sees every status the process emits. The output listener is covered by
		// `outputListenerEndpoint`, it is only attached here so both endpoints are exercised
		// together.

		const vm::PID pid = spawnAndLoad("../common/programs/io_hang.dbc");

		events::Listener<std::string> output_listener([](const std::string&) {});
		assertSucceeded(api::attachOutputListener(pid, &output_listener), "attachOutputListener");

		{
			ScopedStatusLog scoped(*this, pid);

			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isSleeping, "Sleeping(IO)");
			assertSucceeded(api::input(pid, "5 "), "input");
			waitUntilStatus(pid, isTerminal, "Terminal");
			assertSucceeded(api::join(pid), "join");

			assertTrue(
				scoped.log.wasEmitted<api::Running>(),
				"A started program never emitted the Running status"
			);
			assertTrue(
				scoped.log.wasEmitted<api::Sleeping>(),
				"A thread blocking on IO never emitted the Sleeping status"
			);
			validateTransitions(scoped.log, "a program blocking on IO");
		}

		output_listener.detach();
		(void) api::kill(pid);
	}

	void outputListenerEndpoint() {
		// An output listener receives what the program prints.

		std::atomic_bool        output = false;
		std::condition_variable cv;
		std::mutex              m;

		auto                          pid = spawnAndLoad("../common/programs/vm_api_tests.dbc");
		events::Listener<std::string> output_listener([&](const std::string& str) {
			ASSERT_EQUAL_PRINT("7", str);
			output.store(true);
			cv.notify_all();
		});

		ASSERT_HAS_VALUE(vm::api::attachOutputListener(pid, &output_listener));

		ASSERT_HAS_VALUE(vm::api::run(pid));

		std::unique_lock lk(m);
		// Test timeout
		ASSERT_TRUE(cv.wait_for(lk, std::chrono::milliseconds(200), [&] { return output.load(); }));

		ASSERT_HAS_VALUE(vm::api::stop(pid));
	}

	void deadProcessRejectsEveryEndpoint() {
		// Every endpoint must answer `ProcessNotFound` for a process that is gone, instead of
		// reaching into a dangling process.

		// A killed PID is never reused, so this one is guaranteed to be unknown.
		const vm::PID pid = spawnAndLoad("breakpoint.dbc");
		assertSucceeded(api::kill(pid), "kill");

		const auto check = [this](const auto& result, std::string_view endpoint) {
			assertRefusedWith<api::ProcessNotFound>(result, endpoint);
		};

		std::istringstream                in;
		std::ostringstream                out;
		events::Listener<api::ProcStatus> status_listener([](const api::ProcStatus&) {});
		events::Listener<std::string>     output_listener([](const std::string&) {});

		check(api::getExecutionStatus(pid), "getExecutionStatus");
		check(api::setExecutionConfig(pid, {}), "setExecutionConfig");
		check(api::loadFiles(pid, { fs::File(path("breakpoint.dbc")) }), "loadFiles");
		check(api::loadCode(pid, vm::code::CodeCollection{}), "loadCode");
		check(api::run(pid), "run");
		check(api::runAwait(pid), "runAwait");
		check(api::runFunction(pid, "main"), "runFunction");
		check(api::runFunctionAwait(pid, "main"), "runFunctionAwait");
		check(api::join(pid), "join");
		check(api::kill(pid), "kill");
		check(api::deinitAndValidate(pid), "deinitAndValidate");
		check(api::deinitOrKill(pid), "deinitOrKill");
		check(api::pause(pid), "pause");
		check(api::pauseAll(pid), "pauseAll");
		check(api::resume(pid), "resume");
		check(api::step(pid), "step");
		check(api::stop(pid), "stop");
		check(api::waitForBreakpoint(pid), "waitForBreakpoint");
		check(api::getCurrentPosition(pid), "getCurrentPosition");
		check(api::attach(pid, in, out), "attach");
		check(api::detach(pid), "detach");
		check(api::input(pid, "1 "), "input");
		check(api::output(pid), "output");
		check(api::getType(pid, "i64"), "getType");
		check(api::getVMValue(pid, "i64"), "getVMValue");
		check(
			api::debuggerGetNumberOfStackFrames(pid, api::MAIN_THREAD_ID),
			"debuggerGetNumberOfStackFrames"
		);
		check(
			api::debuggerGetStackFrameData(pid, api::MAIN_THREAD_ID, 0), "debuggerGetStackFrameData"
		);
		check(api::attachStatusListener(pid, &status_listener), "attachStatusListener");
		check(api::attachOutputListener(pid, &output_listener), "attachOutputListener");
		check(api::setBreakpoint(pid, base::StrID("main"), 0, true), "setBreakpoint");
		check(
			api::mapFileLineToCodeCollectionPosition(pid, fs::File(path("breakpoint.dbc")), 2),
			"mapFileLineToCodeCollectionPosition"
		);
	}

	/**
	 * @brief Asserts an `ExitValue` carries exactly one `i64` equal to @p expected.
	 */
	void assertReturnedI64(
		const vm::api::ExitValue& exit_value, i64 expected, std::string_view what
	) {
		variant_match(exit_value) {
			variant_case(i64, code) { ASSERT_EQUAL_PRINT(expected, code); }
			variant_case(std::vector<Ref<vm::IVMValue>>, values) {
				assertTrue(
					values.size() == 1,
					base::strConcat(what, " returned ", values.size(), " values instead of one")
				);
				ASSERT_EQUAL_PRINT(expected, values.at(0)->readBytes<i64>());
			}
		}
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/api/");
