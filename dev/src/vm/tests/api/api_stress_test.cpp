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

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace api = vm::api;

/**
 * @brief Randomized API stress tests for the DVM.
 *
 * Clients may call ANY endpoint at ANY time, from any number of threads. Individual results are not
 * asserted - racing the program's progress may legally fail - the harness asserts the global
 * properties instead: nothing crashes or hangs, every status the process emits is a legal edge of
 * the process-state model, and a program that was allowed to complete cleanly still leaves a valid
 * memory state. The seed of every scenario is part of the assertion message, so a failure is
 * reproducible.
 *
 * What each endpoint is supposed to do on its own is covered by `api/api_test.cpp` and
 * `debugger/debugger_tests.cpp`.
 */
class VmApiStressTest: public VmTestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmApiStressTest

public:
	VM_TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		// @TODO: #3612 JIT-compiled code does not observe stop/pause requests. Like `api_test`,
		// this suite tests the API, not the JIT, so it spawns its processes with the JIT off
		// (ignored in non-JIT builds).
		process_config.enable_jit = false;

		TESTER_ADD_TEST(stressTerminatingProgram);
		TESTER_ADD_TEST(stressInfiniteProgram);
		TESTER_ADD_TEST(stressConcurrentClients);
		TESTER_ADD_TEST(stressConcurrentPerThreadClients);
	}

	~VmApiStressTest() override = default;

private:
	/**
	 * @brief Kills the process when the scope ends, however it ends.
	 *
	 * A failed assertion unwinds out of the test method, so without this a scenario that kills its
	 * process on its last line leaves it running, and the `Supervisor` teardown then prints its own
	 * warnings on top of the real failure. Killing an already dead process is a no-op.
	 */
	struct ScopedKill final {
		vm::PID pid;

		explicit ScopedKill(vm::PID pid): pid(pid) {}

		~ScopedKill() { (void) vm::api::kill(pid); }

		ScopedKill(const ScopedKill&)            = delete;
		ScopedKill& operator=(const ScopedKill&) = delete;
		ScopedKill(ScopedKill&&)                 = delete;
		ScopedKill& operator=(ScopedKill&&)      = delete;
	};

	/**
	 * @brief Aborts the binary (printing the scenario name) if a scenario does not finish in time.
	 *
	 * A client thread wedged in a blocking endpoint cannot be recovered from inside the process, so
	 * failing hard with the seed of the scenario is the only useful outcome.
	 */
	class Watchdog final {
	public:
		explicit Watchdog(std::string what): what(std::move(what)) {
			thread = std::thread([this] {
				std::unique_lock lock(mutex);
				if (!cv.wait_for(lock, TIME_BUDGET, [this] { return done; })) {
					std::cerr << "DVM API WATCHDOG: '" << this->what
							  << "' exceeded its time budget\n";
					std::abort();
				}
			});
		}

		~Watchdog() {
			{
				std::lock_guard lock(mutex);
				done = true;
			}
			cv.notify_all();
			thread.join();
		}

		Watchdog(const Watchdog&)            = delete;
		Watchdog& operator=(const Watchdog&) = delete;

	private:
		/// Generous, since this is a "something hung" detector and not a performance assertion.
		static constexpr auto TIME_BUDGET = std::chrono::seconds(60);

		std::string             what;
		std::thread             thread;
		std::mutex              mutex;
		std::condition_variable cv;
		bool                    done = false;
	};

	/// Fixed so a failure is reproducible without an environment variable. Bump it to shake out
	/// new interleavings.
	static constexpr u64 MASTER_SEED = 0x5D'EE'CE'66'D1'CE'20'40ULL;

	static constexpr u64 PARK_INSTRUCTION = 5;

	void stressTerminatingProgram() {
		// Runs a fast-terminating program to completion while hammering it with random requests,
		// then joins and validates the memory state.

		for (usize iter = 0; iter < 15; iter++) {
			const u64       seed = MASTER_SEED + iter;
			std::mt19937_64 rng(seed);
			Watchdog        watchdog(base::strConcat("stressTerminatingProgram seed=", seed));

			const vm::PID   pid = spawnAndLoad("breakpoint.dbc");
			ScopedKill      kill_guard(pid);
			ScopedStatusLog scoped(*this, pid);

			assertSucceeded(
				api::setBreakpoint(pid, base::StrID("main"), PARK_INSTRUCTION, true),
				"setBreakpoint before the random ops"
			);
			assertSucceeded(api::run(pid), "run");
			assertSucceeded(api::waitForBreakpoint(pid), "waitForBreakpoint");

			// Debugger-only ops, no stop/run and no step: the program is allowed to complete
			// cleanly so guest-memory validity is a meaningful post-condition.
			std::atomic<bool> status_ok{ true };
			const usize       op_count = std::uniform_int_distribution<usize>(0, 30)(rng);
			for (usize op = 0; op < op_count; op++) randomOp(pid, rng, false, true, status_ok);
			assertTrue(
				status_ok.load(),
				base::strConcat("getExecutionStatus must succeed for a live process, seed=", seed)
			);

			// The program may be parked by racing pauses or breakpoints - release it fully so the
			// join can finish.
			releaseUntilTerminal(pid);
			(void) api::join(pid);

			auto final_status = api::getExecutionStatus(pid);
			assertSucceeded(final_status, base::strConcat("the final status, seed=", seed));
			assertTrue(
				v_matches(final_status.value(), api::ExecutionCompleted),
				base::strConcat("the program must complete, not stop, seed=", seed)
			);
			assertSucceeded(
				api::getExitValue(pid), base::strConcat("getExitValue after completion, seed=", seed)
			);

			validateTransitions(scoped.log, base::strConcat("seed=", seed));

			// The program completed cleanly, so a leak here is a regression.
			auto valid = api::deinitAndValidate(pid);
			assertSucceeded(valid, base::strConcat("deinitAndValidate, seed=", seed));
			ASSERT_TRUE(valid.value());
		}
	}

	void stressInfiniteProgram() {
		// Random mix against a program that never terminates on its own, then stop and kill. A
		// pending pause must not prevent stopping.

		for (usize iter = 0; iter < 15; iter++) {
			const u64       seed = MASTER_SEED + iter;
			std::mt19937_64 rng(seed);
			Watchdog        watchdog(base::strConcat("stressInfiniteProgram seed=", seed));

			const vm::PID   pid = spawnAndLoad("../debugger/while_true.dbc");
			ScopedKill      kill_guard(pid);
			ScopedStatusLog scoped(*this, pid);

			assertSucceeded(api::run(pid), "run");

			std::atomic<bool> status_ok{ true };
			const usize       op_count = std::uniform_int_distribution<usize>(1, 40)(rng);
			for (usize op = 0; op < op_count; op++) randomOp(pid, rng, true, true, status_ok);
			assertTrue(
				status_ok.load(),
				base::strConcat("getExecutionStatus must succeed for a live process, seed=", seed)
			);

			// No `resume` before the `stop`: a pause left pending by the random mix is exactly what
			// must not prevent stopping, and the terminal state afterwards is what proves it.
			assertSucceeded(
				api::stop(pid), base::strConcat("stop of an infinite program, seed=", seed)
			);
			waitUntilStatus(pid, isTerminal, "terminal after the stop");

			validateTransitions(scoped.log, base::strConcat("seed=", seed));
			assertSucceeded(api::kill(pid), "kill");
		}
	}

	void stressConcurrentClients() {
		// Several client threads hammer the same single-threaded process concurrently.
		constexpr usize CLIENT_COUNT = 4;

		for (usize iter = 0; iter < 8; iter++) {
			const u64 seed = MASTER_SEED + iter;
			Watchdog  watchdog(base::strConcat("stressConcurrentClients seed=", seed));

			const vm::PID   pid = spawnAndLoad("../debugger/while_true.dbc");
			ScopedKill      kill_guard(pid);
			ScopedStatusLog scoped(*this, pid);

			assertSucceeded(api::run(pid), "run");

			std::atomic<bool>        status_ok{ true };
			std::vector<std::thread> clients;
			clients.reserve(CLIENT_COUNT);
			for (usize client = 0; client < CLIENT_COUNT; client++) {
				clients.emplace_back([this, pid, seed, client, &status_ok] {
					std::mt19937_64 rng(seed * CLIENT_COUNT + client);
					// No `resume` in the mix: it can park a concurrent `api::pause` forever, see
					// `randomOp`.
					for (usize op = 0; op < 20; op++) randomOp(pid, rng, false, false, status_ok);
				});
			}
			for (auto& client: clients) client.join();
			assertTrue(
				status_ok.load(),
				base::strConcat("getExecutionStatus must succeed for a live process, seed=", seed)
			);

			// Whatever the clients left pending, including a pause, must not prevent the stop.
			assertSucceeded(
				api::stop(pid), base::strConcat("stop after concurrent stress, seed=", seed)
			);
			waitUntilStatus(pid, isTerminal, "terminal after the stop");

			validateTransitions(scoped.log, base::strConcat("seed=", seed));
			assertSucceeded(api::kill(pid), "kill");
		}
	}

	void stressConcurrentPerThreadClients() {
		// Several client threads issue per-thread requests against several VMThreads at once,
		// including requests for thread ids that do not exist.
		constexpr usize CLIENT_COUNT   = 4;
		constexpr usize OPS_PER_CLIENT = 40;

		for (usize iter = 0; iter < 5; iter++) {
			const u64 seed = MASTER_SEED + iter;
			Watchdog  watchdog(base::strConcat("stressConcurrentPerThreadClients seed=", seed));

			const vm::PID   pid = spawnAndLoad("../debugger/spin_threads.dbc");
			ScopedKill      kill_guard(pid);
			ScopedStatusLog scoped(*this, pid);

			assertSucceeded(api::run(pid), "run of spin_threads.dbc");
			waitUntilStatus(pid, isRunning, "Running");

			std::atomic<bool>        status_ok{ true };
			std::vector<std::thread> clients;
			clients.reserve(CLIENT_COUNT);
			for (usize client = 0; client < CLIENT_COUNT; client++) {
				clients.emplace_back([this, pid, seed, client, &status_ok] {
					std::mt19937_64 rng(seed * CLIENT_COUNT + client);
					for (usize op = 0; op < OPS_PER_CLIENT; op++)
						randomPerThreadOp(pid, rng, status_ok);
				});
			}
			for (auto& client: clients) client.join();
			assertTrue(
				status_ok.load(),
				base::strConcat("getExecutionStatus must succeed for a live process, seed=", seed)
			);

			// Whatever the clients left pending, the process must still stop.
			for (usize tid = 0; tid < SPIN_THREAD_COUNT; tid++)
				(void) api::resume(pid, api::ThreadID{ tid });
			assertSucceeded(
				api::stop(pid), base::strConcat("stop after concurrent stress, seed=", seed)
			);
			waitUntilStatus(pid, isTerminal, "Stopped");

			validateTransitions(scoped.log, base::strConcat("seed=", seed));
			assertSucceeded(api::kill(pid), "kill");
		}
	}

	/**
	 * @brief One random, non-blocking API call against the process.
	 *
	 * @p allow_lifecycle adds `stop`. It terminates the program, so every op drawn after it only
	 * exercises the "illegal in a terminal state" path, which is worth covering in one place only:
	 * `stressInfiniteProgram`.
	 *
	 * @p allow_resume gates the `resume` op, and it is off wherever several clients hit the same
	 * thread. `IVMThread::awaitPause` waits on a *level* predicate, so a `resume` landing between
	 * the thread parking and the waiter looking makes `api::pause` miss that state and wait for a
	 * next pause that never comes for a spinning program. That is a defect in `api::pause`, not in
	 * the test. `step` does not have the problem: it waits on a fresh state change.
	 *
	 * The endpoints that may block by design, tear the process down or spawn extra threads are
	 * excluded here and covered by the dedicated tests: join, runFunctionAwait, output,
	 * waitForBreakpoint, kill, deinitAndValidate, run and runFunction.
	 *
	 * @p status_ok is cleared instead of asserting on the spot, because this also runs on client
	 * `std::thread`s, where `fail` throwing would call `std::terminate`.
	 */
	void randomOp(
		vm::PID            pid,
		std::mt19937_64&   rng,
		bool               allow_lifecycle,
		bool               allow_resume,
		std::atomic<bool>& status_ok
	) {
		static const std::array<std::string, 3> type_names{ "i64", "i32", "DoesNotExist" };
		const auto                              rand_type
			= [&] { return type_names.at(std::uniform_int_distribution<usize>(0, 2)(rng)); };

		const int max_op = allow_lifecycle ? 13 : 12;
		switch (std::uniform_int_distribution<int>(0, max_op)(rng)) {
		case 0:
			if (!api::getExecutionStatus(pid).has_value()) status_ok = false;
			break;
		case 1:
			(void) api::pause(pid);
			break;
		case 2:
			if (allow_resume) (void) api::resume(pid);
			break;
		case 3:
			(void) api::step(pid);
			break;
		case 4:
			(void) api::getCurrentPosition(pid);
			break;
		case 5:
			(void) api::setBreakpoint(
				pid,
				base::StrID("main"),
				std::uniform_int_distribution<u64>(0, MAX_BREAKPOINT_INDEX)(rng),
				std::uniform_int_distribution<int>(0, 1)(rng) == 1
			);
			break;
		case 6:
			(void) api::debuggerGetNumberOfStackFrames(pid, api::MAIN_THREAD_ID);
			break;
		case 7:
			(void) api::debuggerGetStackFrameData(
				pid, api::MAIN_THREAD_ID, std::uniform_int_distribution<u64>(0, 4)(rng)
			);
			break;
		case 8:
			(void) api::getExitValue(pid);
			break;
		case 9:
			(void) api::getType(pid, rand_type());
			break;
		case 10: {
			auto value = api::getVMValue(pid, rand_type());
			// The caller owns the value, so free it right away to keep the scenario leak-free.
			if (value.has_value()) value->vm_value->freeData();
			break;
		}
		case 11:
			(void) api::input(pid, "0 ");
			break;
		case 12:
			(void) api::pauseAll(pid);
			std::this_thread::yield();
			break;
		case 13:
			(void) api::stop(pid);
			break;
		default:
			break;
		}
	}

	/**
	 * @brief One random per-thread request, aimed at a random thread id.
	 *
	 * `resume` is deliberately NOT in the mix, for the reason spelled out on `randomOp`. It is
	 * covered by the deterministic debugger tests, which drive one client at a time.
	 */
	void randomPerThreadOp(vm::PID pid, std::mt19937_64& rng, std::atomic<bool>& status_ok) {
		const api::ThreadID tid{ std::uniform_int_distribution<usize>(0, SPIN_THREAD_COUNT)(rng) };

		switch (std::uniform_int_distribution<int>(0, 5)(rng)) {
		case 0:
			(void) api::pause(pid, tid);
			break;
		case 1:
			(void) api::step(pid, tid);
			break;
		case 2:
			(void) api::pauseAll(pid);
			// Leave the threads parked for a moment, so the other clients race a paused process.
			std::this_thread::yield();
			break;
		case 3:
			(void) api::debuggerGetNumberOfStackFrames(pid, tid);
			break;
		case 4:
			(void) api::debuggerGetStackFrameData(
				pid, tid, std::uniform_int_distribution<u64>(0, 3)(rng)
			);
			break;
		case 5:
			if (!api::getExecutionStatus(pid).has_value()) status_ok = false;
			break;
		default:
			break;
		}
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/api/");
