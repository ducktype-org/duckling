#include <vm_tester_utils.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <tester/tester.hpp>

#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>

#include <atomic>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

/**
 * @brief Single-process, single-thread tests of the whole DVM API. Note that debugger API is not
 * tested here. It's tested in `debugger_tests.cpp`.
 */
class VmApiTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmApiTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(spawnAndKillEndpoints);
		TESTER_ADD_TEST(loadEndpoints);
		TESTER_ADD_TEST(setExecutionConfigEndpoint);
		TESTER_ADD_TEST(runEndpoint);
		TESTER_ADD_TEST(rerunEndpoint);
		TESTER_ADD_TEST(runFunctionEndpoints);
		TESTER_ADD_TEST(exitValueEndpoint);
		TESTER_ADD_TEST(joinEndpoint);
		TESTER_ADD_TEST(stopEndpoint);
		TESTER_ADD_TEST(killFromEveryState);
		TESTER_ADD_TEST(deinitAndValidateEndpoint);
		TESTER_ADD_TEST(globalDestructorsRunOnDeinit);
		TESTER_ADD_TEST(ioEndpoints);
		TESTER_ADD_TEST(dataEndpoints);
		TESTER_ADD_TEST(listenerEndpoints);
		TESTER_ADD_TEST(deadProcessRejectsEveryEndpoint);
	}

	~VmApiTest() override = default;

private:
	/// `spin_threads.dbc`: `main` plus three workers, all in an endless loop.
	static constexpr usize SPIN_THREAD_COUNT = 4;

	// ==================================================================
	// Process lifecycle
	// ==================================================================

	void spawnAndKillEndpoints() {
		/*
		 * A freshly spawned process is `NotStarted` and answers status calls. A killed one is gone
		 * from the supervisor, so every later request on it is refused.
		 */
		namespace api = vm::api;
		Watchdog watchdog("spawnAndKillEndpoints");

		const vm::PID pid    = spawnProcess();
		auto          status = api::getExecutionStatus(pid);
		assertSucceeded(status, "getExecutionStatus on a fresh process");
		assertTrue(
			v_matches(status.value(), api::NotStarted), "A fresh process must report NotStarted"
		);

		assertSucceeded(api::kill(pid), "kill of a fresh process");
		assertRefusedWith<api::ProcessNotFound>(
			api::getExecutionStatus(pid), "getExecutionStatus after kill"
		);
		assertRefusedWith<api::ProcessNotFound>(api::kill(pid), "a second kill");
	}

	void loadEndpoints() {
		/*
		 * `loadFiles` and `loadCode` accept valid bytecode and refuse anything else, without making
		 * the process unusable.
		 */
		namespace api = vm::api;
		Watchdog watchdog("loadEndpoints");

		const vm::PID pid = spawnProcess();

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
		releaseUntilTerminal(pid);
		assertSucceeded(api::join(pid), "join");
		(void) api::kill(pid);

		// An empty load is a legal no-op.
		const vm::PID code_pid = spawnProcess();
		assertSucceeded(
			api::loadCode(code_pid, vm::code::CodeCollection{}),
			"loadCode of an empty code collection"
		);
		(void) api::kill(code_pid);
	}

	void setExecutionConfigEndpoint() {
		/*
		 * `setExecutionConfig` is accepted at any point of a process's life.
		 */
		namespace api = vm::api;
		Watchdog watchdog("setExecutionConfigEndpoint");

		const vm::PID pid = spawnAndLoad("while_true.dbc");
		assertSucceeded(api::setExecutionConfig(pid, {}), "setExecutionConfig on NotStarted");

		assertSucceeded(api::run(pid), "run");
		waitUntilStatus(pid, isRunning, "Running");
		assertSucceeded(api::setExecutionConfig(pid, {}), "setExecutionConfig on a running process");

		assertSucceeded(api::stop(pid), "stop");
		assertSucceeded(api::setExecutionConfig(pid, {}), "setExecutionConfig on a stopped process");
		(void) api::kill(pid);
	}

	void runEndpoint() {
		/*
		 * `run` needs a `main` and refuses to start a second run of a live process.
		 */
		namespace api = vm::api;
		Watchdog watchdog("runEndpoint");

		{
			// No code loaded at all - there is no `main` to run. The refusal comes from argument
			// validation, which runs before anything touches the process state, so the process is
			// left exactly as it was and stays runnable once a `main` is loaded.
			const vm::PID pid = spawnProcess();
			assertRefusedWith<api::RunError>(
				api::run(pid), "run without any loaded code", "Called function 'main' does not"
			);

			auto status = api::getExecutionStatus(pid);
			assertSucceeded(status, "getExecutionStatus after a refused run");
			assertTrue(
				v_matches(status.value(), api::NotStarted),
				"A refused run must leave the process NotStarted"
			);

			assertSucceeded(api::loadFiles(pid, { fs::File(path("breakpoint.dbc")) }), "loadFiles");
			assertSucceeded(api::run(pid), "run after loading a `main`");
			releaseUntilTerminal(pid);
			assertSucceeded(api::join(pid), "join");
			(void) api::kill(pid);
		}

		{
			const vm::PID pid = spawnAndLoad("while_true.dbc");
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");

			assertRefusedWith<api::StateError>(
				api::run(pid), "a second run of a running process", "must be freshly loaded"
			);

			assertSucceeded(api::stop(pid), "stop");
			(void) api::kill(pid);
		}
	}

	void rerunEndpoint() {
		/*
		 * A rerun is legal only after a run that completed AND whose threads were all joined.
		 */
		namespace api = vm::api;
		Watchdog watchdog("rerunEndpoint");

		{  // Completed and joined - the only case that may run again.
			const vm::PID   pid = spawnAndLoad("breakpoint.dbc");
			ScopedStatusLog scoped(*this, pid);

			assertSucceeded(api::run(pid), "the first run");
			waitUntilStatus(pid, isTerminal, "Completed");

			assertRefusedWith<api::StateError>(
				api::run(pid),
				"a rerun without a join",
				"thread 0 of the previous run was never joined"
			);

			assertSucceeded(api::join(pid), "join of the main thread");
			assertSucceeded(api::run(pid), "a rerun after the join");

			waitUntilStatus(pid, isTerminal, "Completed again");
			assertSucceeded(api::join(pid), "join of the second run");

			validateTransitions(scoped.log, "a rerun of a completed process");

			auto valid = api::deinitAndValidate(pid);
			assertSucceeded(valid, "deinitAndValidate after two clean runs");
			ASSERT_TRUE(valid.value());
		}

		{  // Every thread has to be joined, not just the main one.
			const vm::PID   pid = spawnAndLoad("unjoined_threads.dbc");
			ScopedStatusLog scoped(*this, pid);

			assertSucceeded(api::run(pid), "the first run");
			waitUntilStatus(pid, isTerminal, "Completed");

			// `main` is joined, its two workers are not.
			assertSucceeded(api::join(pid), "join of the main thread");
			assertRefusedWith<api::StateError>(
				api::run(pid),
				"a rerun with two unjoined worker threads",
				"threads 1, 2 of the previous run were never joined"
			);

			assertSucceeded(api::join(pid, api::ThreadID{ 1 }), "join of worker 1");
			assertRefusedWith<api::StateError>(
				api::run(pid),
				"a rerun with one unjoined worker thread",
				"thread 2 of the previous run was never joined"
			);

			assertSucceeded(api::join(pid, api::ThreadID{ 2 }), "join of worker 2");
			assertSucceeded(api::run(pid), "a rerun once every thread was joined");

			waitUntilStatus(pid, isTerminal, "Completed again");
			for (usize tid = 0; tid < 3; tid++)
				assertSucceeded(
					api::join(pid, api::ThreadID{ tid }),
					base::strConcat("join of thread ", tid, " of the second run")
				);

			validateTransitions(scoped.log, "a rerun of a multi-threaded process");

			auto valid = api::deinitAndValidate(pid);
			assertSucceeded(valid, "deinitAndValidate after two multi-threaded runs");
			ASSERT_TRUE(valid.value());
		}

		{  // Stopped: refused right away, and still refused after the process was drained.
			const vm::PID pid = spawnAndLoad("while_true.dbc");
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");
			assertSucceeded(api::stop(pid), "stop");
			waitUntilStatus(pid, isTerminal, "Stopped");

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
			waitUntilStatus(
				pid,
				[](const api::ProcStatus& s) { return v_matches(s, api::ExecutionPanicked); },
				"Panicked"
			);

			assertRefusedWith<api::StateError>(
				api::run(pid), "a rerun right after a panic", "must be freshly loaded"
			);
			assertRefusedWith<api::OtherError>(
				api::join(pid), "join of a panicked thread", "Execution panicked with error"
			);
			assertRefusedWith<api::StateError>(
				api::run(pid), "a rerun after a panic and a join", "must be freshly loaded"
			);
			(void) api::kill(pid);
		}
	}

	void runFunctionEndpoints() {
		/*
		 * `runFunction` starts a named function on a thread of its own, `runFunctionAwait` runs it
		 * on the caller's thread and hands back the return value. Both validate the arguments
		 * against the loaded signature.
		 */
		namespace api = vm::api;
		Watchdog watchdog("runFunctionEndpoints");

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

			wrong_type->vm_value->freeData();
			right_type->vm_value->freeData();
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

			lhs->vm_value->freeData();
			rhs->vm_value->freeData();

			assertRefusedWith<api::RunError>(
				api::runFunctionAwait(pid, "no_such_function"),
				"runFunctionAwait of a missing function",
				"does not exist"
			);
			(void) api::kill(pid);
		}

		{  // A panicking function reports the panic instead of a return value.
			const vm::PID pid = spawnAndLoad("panic.dbc");
			assertRefusedWith<api::StateError>(
				api::runFunctionAwait(pid, "main"), "runFunctionAwait of a panicking function"
			);
			(void) api::kill(pid);
		}

		{  // A function that never returns is torn down by a kill.
			const vm::PID pid = spawnAndLoad("while_true.dbc");
			assertSucceeded(api::runFunction(pid, "main"), "runFunction of an endless function");
			assertSucceeded(api::kill(pid), "kill of a process running a function");
		}
	}

	void exitValueEndpoint() {
		/*
		 * `getExitValue` answers only once a run completed, and says which of the two reasons it
		 * has for refusing.
		 */
		namespace api = vm::api;
		Watchdog watchdog("exitValueEndpoint");

		const vm::PID pid = spawnAndLoad("breakpoint.dbc");
		assertRefusedWith<api::StateError>(
			api::getExitValue(pid), "getExitValue before a run", "Execution did not start"
		);

		assertSucceeded(api::run(pid), "run");
		waitUntilStatus(pid, isTerminal, "Completed");
		assertSucceeded(api::join(pid), "join");

		auto exit_value = api::getExitValue(pid);
		assertSucceeded(exit_value, "getExitValue after completion");
		assertReturnedI64(exit_value.value(), 0, "getExitValue after completion");
		(void) api::kill(pid);

		// A stopped run never completed, so it has no exit value.
		const vm::PID stopped_pid = spawnAndLoad("while_true.dbc");
		assertSucceeded(api::run(stopped_pid), "run");
		waitUntilStatus(stopped_pid, isRunning, "Running");
		assertSucceeded(api::stop(stopped_pid), "stop");
		assertRefusedWith<api::StateError>(
			api::getExitValue(stopped_pid),
			"getExitValue of a stopped process",
			"Execution did not complete"
		);
		(void) api::kill(stopped_pid);
	}

	void joinEndpoint() {
		/*
		 * `join` reaps the execution thread of one VMThread. It refuses an unknown thread and a
		 * thread that never ran.
		 */
		namespace api = vm::api;
		Watchdog watchdog("joinEndpoint");

		const vm::PID pid = spawnAndLoad("breakpoint.dbc");

		assertRefusedWith<api::OtherError>(
			api::join(pid, api::ThreadID{ 77 }), "join of an unknown thread", "Thread not found"
		);
		assertRefusedWith<api::JoinError>(api::join(pid), "join before a run");

		assertSucceeded(api::run(pid), "run");
		releaseUntilTerminal(pid);
		assertSucceeded(api::join(pid, api::MAIN_THREAD_ID), "join of a completed run");
		// The handle is gone now, so a second join has nothing to reap.
		assertRefusedWith<api::JoinError>(api::join(pid), "a second join");
		(void) api::kill(pid);
	}

	void stopEndpoint() {
		/*
		 * `stop` is legal from every state, including the states where it does nothing.
		 */
		namespace api = vm::api;
		Watchdog watchdog("stopEndpoint");

		{
			// NotStarted: there is nothing to stop. The flag is deliberately not raised, because a
			// process that never ran must stay runnable.
			const vm::PID pid = spawnAndLoad("while_true.dbc");
			assertSucceeded(api::stop(pid), "stop of a NotStarted process");
			auto status = api::getExecutionStatus(pid);
			assertSucceeded(status, "getExecutionStatus after a stop of a NotStarted process");
			assertTrue(
				v_matches(status.value(), api::NotStarted),
				"A stop of a process that never ran must leave it NotStarted"
			);
			assertSucceeded(api::run(pid), "run after a stop of a NotStarted process");
			(void) api::kill(pid);
		}

		{
			const vm::PID   pid = spawnAndLoad("while_true.dbc");
			ScopedStatusLog scoped(*this, pid);

			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");
			assertSucceeded(api::stop(pid), "stop of a running process");
			waitUntilStatus(pid, isTerminal, "Stopped");
			assertSucceeded(api::stop(pid), "a second stop");

			assertTrue(
				scoped.log.wasEmitted(
					base::variantTypeIndex<api::ProcStatus, api::ExecutionStopping>()
				),
				"Stopping a Running program never emitted the ExecutionStopping status"
			);
			validateTransitions(scoped.log, "a stop of a running process");
			(void) api::kill(pid);
		}

		{  // A pending pause must not keep a stop from finishing.
			const vm::PID pid = spawnAndLoad("while_true.dbc");
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");
			assertSucceeded(api::pause(pid), "pause");
			assertSucceeded(api::stop(pid), "stop of a paused process");
			waitUntilStatus(pid, isTerminal, "Stopped from Paused");
			(void) api::kill(pid);
		}

		{  // Several threads, some of them parked by the debugger.
			const vm::PID   pid = spawnAndLoad("spin_threads.dbc");
			ScopedStatusLog scoped(*this, pid);

			assertSucceeded(api::run(pid), "run of spin_threads.dbc");
			waitUntilEveryThreadRuns(pid);
			assertSucceeded(api::pause(pid, api::ThreadID{ 1 }), "pause of worker 1");
			assertSucceeded(api::pause(pid, api::ThreadID{ 2 }), "pause of worker 2");

			assertSucceeded(api::stop(pid), "stop of a process with parked and running threads");
			waitUntilStatus(pid, isTerminal, "Stopped");

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
		/*
		 * `kill` must succeed from every reachable state and leave no process behind. After a
		 * successful kill the process is removed from the supervisor, so a later status query
		 * fails.
		 */
		namespace api = vm::api;

		auto kill_from = [&](std::string_view state_name, const std::string& program, auto setup) {
			Watchdog      watchdog(base::strConcat("kill@", state_name));
			const vm::PID pid = spawnAndLoad(program);
			setup(pid);
			assertSucceeded(api::kill(pid), base::strConcat("kill from ", state_name));
			assertRefusedWith<api::ProcessNotFound>(
				api::getExecutionStatus(pid),
				base::strConcat("a status query after a kill from ", state_name)
			);
		};

		// NotStarted: loaded but never run.
		kill_from("NotStarted", "while_true.dbc", [](vm::PID) {});

		// Running: the interpreter loop is actively executing.
		kill_from("Running", "while_true.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");
		});

		// Paused: suspended by the debugger.
		kill_from("Paused", "while_true.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");
			assertSucceeded(api::pause(pid), "pause");
			waitUntilStatus(pid, isPaused, "Paused");
		});

		// Blocked on a bytecode condition variable behind a bytecode mutex.
		kill_from("Blocked(mutex/cv)", "mutex_hang.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running (CV-blocked)");
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		});

		// Sleeping on IO, reading input that never arrives.
		kill_from("Sleeping(IO)", "io_hang.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isSleeping, "Sleeping(IO)");
		});

		// Several live threads at once.
		kill_from("Running(4 threads)", "spin_threads.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilEveryThreadRuns(pid);
		});

		// Several threads, all of them parked by the debugger.
		kill_from("Paused(4 threads)", "spin_threads.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilEveryThreadRuns(pid);
			assertSucceeded(api::pauseAll(pid), "pauseAll");
			waitUntilStatus(pid, isPaused, "Paused");
		});

		// Terminal: Completed.
		kill_from("Completed", "breakpoint.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			releaseUntilTerminal(pid);
			waitUntilStatus(pid, isTerminal, "Terminal");
		});

		// Terminal: Panicked, by an integer division by zero.
		kill_from("Panicked", "panic.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(
				pid,
				[](const api::ProcStatus& s) { return v_matches(s, api::ExecutionPanicked); },
				"Panicked"
			);
		});

		// Terminal: Stopped, the process is kept in the table until it is killed.
		kill_from("Stopped", "while_true.dbc", [&](vm::PID pid) {
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");
			assertSucceeded(api::stop(pid), "stop");
			waitUntilStatus(pid, isTerminal, "Terminal");
		});
	}

	void deinitAndValidateEndpoint() {
		/*
		 * `deinitAndValidate` refuses an executing process, reports `false` for a leaked
		 * allocation, and drops the process once it ran.
		 */
		namespace api = vm::api;
		Watchdog watchdog("deinitAndValidateEndpoint");

		{
			// An executing process must not have its globals torn down under the running program.
			const vm::PID pid = spawnAndLoad("while_true.dbc");
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isRunning, "Running");
			assertRefusedWith<api::StateError>(
				api::deinitAndValidate(pid),
				"deinitAndValidate of a running process",
				"the process is still executing"
			);
			assertSucceeded(api::stop(pid), "stop");
			// The threads were killed mid-instruction, so their locals are still alive and the
			// memory state is legitimately invalid - that is exactly why a stopped process may not
			// be rerun. What matters is that the teardown itself goes through.
			assertSucceeded(api::deinitAndValidate(pid), "deinitAndValidate of a stopped process");
		}

		{  // A clean run leaves a valid memory state, and the process is dropped.
			const vm::PID pid = spawnAndLoad("breakpoint.dbc");
			assertSucceeded(api::run(pid), "run");
			releaseUntilTerminal(pid);
			assertSucceeded(api::join(pid), "join");
			auto valid = api::deinitAndValidate(pid);
			assertSucceeded(valid, "deinitAndValidate after a clean run");
			ASSERT_TRUE(valid.value());
			assertRefusedWith<api::ProcessNotFound>(
				api::getExecutionStatus(pid), "a status query after deinitAndValidate"
			);
		}

		{
			const vm::PID pid = spawnAndLoad("global_leak.dbc");
			assertSucceeded(api::run(pid), "run");
			releaseUntilTerminal(pid);
			assertSucceeded(api::join(pid), "join");
			auto valid = api::deinitAndValidate(pid);
			assertSucceeded(valid, "deinitAndValidate of a leaking program");
			assertFalse(valid.value(), "A leaked allocation must be reported as invalid");
		}
	}

	void globalDestructorsRunOnDeinit() {
		/*
		 * The global destructors run as bytecode during `deinitAndValidate`, after the run has
		 * already ended. They have to run whether that run completed or was stopped mid-instruction
		 * - a `Stop` left over from the stopped run must not kill them.
		 */
		namespace api = vm::api;
		Watchdog watchdog("globalDestructorsRunOnDeinit");

		{
			// `global_destructor.dbc` frees in its destructor what its constructor allocated, so a
			// destructor that did not run shows up as an invalid memory state.
			const vm::PID pid = spawnAndLoad("global_destructor.dbc");
			assertSucceeded(api::run(pid), "run");
			waitUntilStatus(pid, isTerminal, "Completed");
			assertSucceeded(api::join(pid), "join");

			auto valid = api::deinitAndValidate(pid);
			assertSucceeded(valid, "deinitAndValidate after a completed run");
			assertTrue(
				valid.value(),
				"The global destructor did not run, so what the constructor allocated was leaked"
			);
		}

		{
			// After a stop the leaked locals of the killed thread make the memory state invalid
			// either way, so the destructor writes to the output instead and the listener is what
			// proves it ran.
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
			waitUntilStatus(pid, isTerminal, "Stopped");

			assertSucceeded(api::deinitAndValidate(pid), "deinitAndValidate after a stop");
			assertTrue(
				writes.load() > 0,
				"The global destructor did not run after the program was stopped mid-instruction"
			);
			output_listener.detach();
		}
	}

	// ==================================================================
	// IO and data endpoints
	// ==================================================================

	void ioEndpoints() {
		/*
		 * `attach`, `detach`, `input` and `output`.
		 */
		namespace api = vm::api;
		Watchdog watchdog("ioEndpoints");

		{  // `input` reaches a thread that is already sleeping on it, and wakes it.
			const vm::PID pid = spawnAndLoad("io_hang.dbc");
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
		/*
		 * `getType` and `getVMValue` answer for loaded types only, and only while the process can
		 * respond.
		 */
		namespace api = vm::api;
		Watchdog watchdog("dataEndpoints");

		const vm::PID pid = spawnAndLoad("breakpoint.dbc");

		assertSucceeded(api::getType(pid, "i64"), "getType(i64)");
		assertRefusedWith<api::OtherError>(
			api::getType(pid, "DoesNotExist"), "getType of an unknown type", "Type not found"
		);

		auto value = api::getVMValue(pid, "i64");
		assertSucceeded(value, "getVMValue(i64)");
		// The caller owns this one, see `vm::IVMValue::freeData()`.
		value->vm_value->freeData();
		assertRefusedWith<api::OtherError>(
			api::getVMValue(pid, "DoesNotExist"), "getVMValue of an unknown type", "Type not found"
		);

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

		releaseUntilTerminal(pid);
		assertSucceeded(api::join(pid), "join");
		(void) api::kill(pid);
	}

	void listenerEndpoints() {
		/*
		 * A status listener sees every status the process emits. The output listener is covered by
		 * `debugger_tests`, it is only attached here so both endpoints are exercised together.
		 */
		namespace api = vm::api;
		Watchdog watchdog("listenerEndpoints");

		const vm::PID pid = spawnAndLoad("io_hang.dbc");

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
				scoped.log.wasEmitted(base::variantTypeIndex<api::ProcStatus, api::Running>()),
				"A started program never emitted the Running status"
			);
			assertTrue(
				scoped.log.wasEmitted(base::variantTypeIndex<api::ProcStatus, api::Sleeping>()),
				"A thread blocking on IO never emitted the Sleeping status"
			);
			validateTransitions(scoped.log, "a program blocking on IO");
		}

		output_listener.detach();
		(void) api::kill(pid);
	}

	void deadProcessRejectsEveryEndpoint() {
		/*
		 * Every endpoint must answer `ProcessNotFound` for a process that is gone, instead of
		 * reaching into a dangling process.
		 */
		namespace api = vm::api;
		Watchdog watchdog("deadProcessRejectsEveryEndpoint");

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
		check(api::runFunction(pid, "main"), "runFunction");
		check(api::runFunctionAwait(pid, "main"), "runFunctionAwait");
		check(api::getExitValue(pid), "getExitValue");
		check(api::join(pid), "join");
		check(api::kill(pid), "kill");
		check(api::deinitAndValidate(pid), "deinitAndValidate");
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

	// ------------------------------------------------------------------
	// Helpers
	// ------------------------------------------------------------------

	/**
	 * @brief Waits until every thread of a running `spin_threads.dbc` is up, then leaves them all
	 * running.
	 *
	 * `pauseAll` is the only way to count the live threads through the API, so it doubles as the
	 * readiness check. It is retried because the workers are started one by one by the running
	 * program.
	 */
	void waitUntilEveryThreadRuns(vm::PID pid) {
		namespace api = vm::api;

		const auto deadline = std::chrono::steady_clock::now() + STATUS_WAIT_BUDGET;
		while (std::chrono::steady_clock::now() < deadline) {
			auto paused = api::pauseAll(pid);
			assertSucceeded(paused, "pauseAll while waiting for the workers to start");
			const bool all_up = paused->thread_ids.size() == SPIN_THREAD_COUNT;
			for (const api::ThreadID tid: paused->thread_ids) (void) api::resume(pid, tid);
			if (all_up) {
				waitUntilStatus(pid, isRunning, "Running again after the readiness check");
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		fail("spin_threads.dbc never got all of its threads running");
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
				if (values.size() != 1) return;
				ASSERT_EQUAL_PRINT(expected, values.at(0)->readBytes<i64>());
			}
		}
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/api/");
