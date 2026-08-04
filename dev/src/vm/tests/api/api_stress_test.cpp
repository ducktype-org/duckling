#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <variant>
#include <vector>

/**
 * @brief Randomized API stress tests for the DVM.
 *
 * Clients may call ANY endpoint at ANY time, from any number of threads. A call that is illegal in
 * the current state must return an `ApiError` - it must never crash, corrupt
 * state, hang or deadlock. These tests assert exactly that:
 * - Seeded random sequences of (bounded, non-blocking) API calls are fired at processes running
 * programs. Individual results are not asserted (racing the program's progress may legally fail).
 *
 * The harness asserts the global properties instead.
 * - Every status the process emits must be a legal edge of the process-state model (see
 * `legalStatusEdge`). An illegal transition means the aggregation is broken.
 * - A per-scenario watchdog aborts (printing the seed) if anything fails to
 * return within its budget. A hung API call is unrecoverable from inside the
 * process, so failing hard with a reproduction seed is the only useful outcome.
 * - `kill` must succeed from EVERY reachable state - including a thread blocked on a bytecode
 * mutex/condition-variable and a thread blocked on IO - and must leave no process behind.
 */
class VmApiStressTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmApiStressTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(killFromEveryState);
		TESTER_ADD_TEST(exerciseBlockingEndpoints);
		TESTER_ADD_TEST(stopEmitsStoppingStatus);
		TESTER_ADD_TEST(stressInfiniteProgram);
		TESTER_ADD_TEST(stressConcurrentClients);
		TESTER_ADD_TEST(stressTerminatingProgram);
	}

	~VmApiStressTest() override = default;

private:
	static constexpr auto SCENARIO_TIME_BUDGET  = std::chrono::seconds(60);
	static constexpr u64  MAX_RANDOM_BREAKPOINT = 12;

	/**
	 * @brief Aborts the whole suite (printing the id) if a scenario does not
	 * finish within its time budget.
	 */
	class Watchdog {
	public:
		Watchdog(u64 id, std::string_view what, std::chrono::seconds budget): id(id), what(what) {
			thread = std::thread([this, budget] {
				std::unique_lock lock(mutex);
				if (!cv.wait_for(lock, budget, [this] { return done; })) {
					std::cerr << "API STRESS WATCHDOG: '" << this->what
							  << "' exceeded its time budget, id=" << this->id << "\n";
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
		Watchdog(Watchdog&&)                 = delete;
		Watchdog& operator=(Watchdog&&)      = delete;

	private:
		u64                     id;
		std::string             what;
		std::thread             thread;
		std::mutex              mutex;
		std::condition_variable cv;
		bool                    done = false;
	};

	/**
	 * @brief Records every status the process emits, for transition validation.
	 */
	struct TransitionLog {
		static constexpr usize STATUS_COUNT = std::variant_size_v<vm::api::ProcStatus>;

		void record(const vm::api::ProcStatus& status) {
			std::lock_guard lock(mutex);
			statuses.push_back(status);
		}

		/**
		 * @brief Checks the recorded sequence against `legalStatusEdge`.
		 * @return A description of the first illegal edge, or an empty optional if the whole
		 * recorded sequence is legal.
		 */
		[[nodiscard]] base::Optional<std::string> findIllegalEdge() const {
			std::lock_guard lock(mutex);
			for (usize i = 1; i < statuses.size(); i++) {
				const usize from = statuses[i - 1].index();
				const usize to   = statuses[i].index();
				// A status is only recorded on change, but be defensive.
				if (from == to) continue;
				if (!legalStatusEdge(from, to))
					return std::string("Illegal status transition emitted by the process: ")
					     + std::string(statusName(from)) + " -> " + std::string(statusName(to));
			}
			return std::nullopt;
		}

		/**
		 * @brief True if the given `vm::api::ProcStatus` alternative was ever emitted.
		 */
		[[nodiscard]] bool wasEmitted(usize status_index) const {
			std::lock_guard lock(mutex);
			return std::ranges::any_of(statuses, [status_index](const vm::api::ProcStatus& s) {
				return s.index() == status_index;
			});
		}

	private:
		static std::string_view statusName(usize index) {
			static constexpr std::array<std::string_view, STATUS_COUNT> NAMES = [] {
				std::array<std::string_view, STATUS_COUNT> names{};
				using vm::api::ProcStatus;
				names[base::variantTypeIndex<ProcStatus, vm::api::NotStarted>()] = "NotStarted";
				names[base::variantTypeIndex<ProcStatus, vm::api::Running>()]    = "Running";
				names[base::variantTypeIndex<ProcStatus, vm::api::Paused>()]     = "Paused";
				names[base::variantTypeIndex<ProcStatus, vm::api::Sleeping>()]   = "Sleeping";
				names[base::variantTypeIndex<ProcStatus, vm::api::ExecutionStopping>()]
					= "ExecutionStopping";
				names[base::variantTypeIndex<ProcStatus, vm::api::ExecutionCompleted>()]
					= "ExecutionCompleted";
				names[base::variantTypeIndex<ProcStatus, vm::api::ExecutionStopped>()]
					= "ExecutionStopped";
				names[base::variantTypeIndex<ProcStatus, vm::api::ExecutionPanicked>()]
					= "ExecutionPanicked";
				return names;
			}();
			return NAMES.at(index);
		}

		/**
		 * @brief Legal directed edges of the emitted process-status sequence.
		 */
		static bool legalStatusEdge(usize from, usize to) {
			constexpr usize NOT_STARTED
				= base::variantTypeIndex<vm::api::ProcStatus, vm::api::NotStarted>();
			constexpr usize RUNNING
				= base::variantTypeIndex<vm::api::ProcStatus, vm::api::Running>();
			constexpr usize PAUSED = base::variantTypeIndex<vm::api::ProcStatus, vm::api::Paused>();
			constexpr usize SLEEPING
				= base::variantTypeIndex<vm::api::ProcStatus, vm::api::Sleeping>();
			constexpr usize STOPPING
				= base::variantTypeIndex<vm::api::ProcStatus, vm::api::ExecutionStopping>();
			constexpr usize COMPLETED
				= base::variantTypeIndex<vm::api::ProcStatus, vm::api::ExecutionCompleted>();
			constexpr usize STOPPED
				= base::variantTypeIndex<vm::api::ProcStatus, vm::api::ExecutionStopped>();
			constexpr usize PANICKED
				= base::variantTypeIndex<vm::api::ProcStatus, vm::api::ExecutionPanicked>();

			static const auto matrix = [] {
				std::array<std::array<bool, STATUS_COUNT>, STATUS_COUNT> m{};
				const auto allow = [&m](usize f, std::initializer_list<usize> tos) {
					for (usize t: tos) m.at(f).at(t) = true;
				};
				allow(
					NOT_STARTED,
					{ RUNNING, PAUSED, SLEEPING, STOPPING, COMPLETED, STOPPED, PANICKED }
				);
				allow(RUNNING, { PAUSED, SLEEPING, STOPPING, COMPLETED, STOPPED, PANICKED });
				allow(PAUSED, { RUNNING, STOPPING, COMPLETED, STOPPED, PANICKED });
				allow(SLEEPING, { RUNNING, STOPPING, COMPLETED, STOPPED, PANICKED });
				allow(STOPPING, { COMPLETED, STOPPED, PANICKED });
				// A terminal process can be re-run: the reset emits NotStarted first.
				allow(COMPLETED, { NOT_STARTED, RUNNING });
				allow(STOPPED, { NOT_STARTED, RUNNING });
				allow(PANICKED, { NOT_STARTED, RUNNING });
				return m;
			}();
			return matrix.at(from).at(to);
		}

		mutable std::mutex               mutex;
		std::vector<vm::api::ProcStatus> statuses;
	};

	void validateTransitions(const TransitionLog& log, u64 seed) {
		const auto illegal_edge = log.findIllegalEdge();
		assertTrue(
			!illegal_edge.has_value(),
			illegal_edge.has_value() ? *illegal_edge + " (seed=" + std::to_string(seed) + ")" : ""
		);
	}

	u64 masterSeed() {
		if (const char* env_seed = std::getenv("DUCK_TORTURE_SEED"))  // NOLINT
			return std::strtoull(env_seed, nullptr, 10);
		return std::random_device{}();
	}

	usize iterations(usize default_count) {
		if (const char* env_iters = std::getenv("DUCK_TORTURE_ITERATIONS"))  // NOLINT
			return std::strtoull(env_iters, nullptr, 10);
		return default_count;
	}

	vm::PID spawnAndLoad(std::string_view program) {
		auto spawned = vm::api::spawn();
		assertTrue(spawned.has_value(), "Spawn failed");
		auto pid = spawned.value().pid;

		fs::File file(path(std::string(program)));
		assertTrue(vm::api::loadFiles(pid, { file }).has_value(), "Load failed");
		return pid;
	}

	/**
	 * @brief Polls the status until `pred` holds or the budget elapses.
	 */
	template<typename Pred>
	void waitUntilStatus(vm::PID pid, Pred pred, std::string_view what) {
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
		while (std::chrono::steady_clock::now() < deadline) {
			auto status = vm::api::getExecutionStatus(pid);
			if (status.has_value() && pred(status.value())) return;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		fail("Process never reached expected status: " + std::string(what));
	}

	/**
	 * @brief One random, bounded, non-blocking API call against the process.
	 *
	 * Cases 0-12 are debugger control calls that always return.
	 * When `allow_lifecycle` is set, `stop` is also included - it terminates the
	 * program, so it is only used where guest-memory validity is not asserted
	 * afterwards.
	 *
	 * `allow_step` gates the `step` op. It is a real bug, not a test limitation: stepping past
	 * the end of the stepped function live-locks the execution thread and `api::step` never
	 * returns (verified with a core dump - the exec thread spins committing Pause/Resume).
	 * The bug predates the VMProcess/VMThread state refactor.
	 * @TODO: #3274 Re-enable stepping everywhere once the step path handles a returning
	 * function. Until then step is disabled in the memory-validated scenario and only
	 * exercised against the never-completing program, where it cannot reach a function end.
	 *
	 * Deliberately excluded everywhere (handled by dedicated tests, because they
	 * may block by design, tear the process down, or spawn extra threads): join,
	 * runFunctionAwait, output, waitForBreakpoint, kill, deinitAndValidate, run,
	 * runFunction.
	 */
	void randomBoundedOp(vm::PID pid, std::mt19937_64& rng, bool allow_lifecycle, bool allow_step) {
		static const std::array<std::string, 3> type_names{ "i64", "i32", "DoesNotExist" };
		const auto                              rand_type
			= [&] { return type_names.at(std::uniform_int_distribution<usize>(0, 2)(rng)); };

		const int max_op = allow_lifecycle ? 13 : 12;
		switch (std::uniform_int_distribution<int>(0, max_op)(rng)) {
		case 0:
			assertTrue(
				vm::api::getExecutionStatus(pid).has_value(),
				"getExecutionStatus must succeed for a live process"
			);
			break;
		case 1:
			(void) vm::api::pause(pid);
			break;
		case 2:
			(void) vm::api::resume(pid);
			break;
		case 3:
			if (allow_step) (void) vm::api::step(pid);
			break;
		case 4:
			(void) vm::api::getCurrentPosition(pid);
			break;
		case 5:
			(void) vm::api::setBreakpoint(
				pid,
				base::StrID("main"),
				std::uniform_int_distribution<u64>(0, MAX_RANDOM_BREAKPOINT)(rng),
				std::uniform_int_distribution<int>(0, 1)(rng) == 1
			);
			break;
		case 6:
			(void) vm::api::debuggerGetNumberOfStackFrames(pid, vm::api::ThreadID{ 0 });
			break;
		case 7:
			(void) vm::api::debuggerGetStackFrameData(
				pid, vm::api::ThreadID{ 0 }, std::uniform_int_distribution<u64>(0, 4)(rng)
			);
			break;
		case 8:
			(void) vm::api::getExitValue(pid);
			break;
		case 9:
			(void) vm::api::getType(pid, rand_type());
			break;
		case 10: {
			auto res = vm::api::getVMValue(pid, rand_type());
			// Free the data immediately, so no leaks appear. The VMValue itself isn't used by the tests.
			if (res.has_value()) res->vm_value->freeData();
			break;
		}
		case 11:
			(void) vm::api::input(pid, "0 ");
			break;
		case 12:
			(void) vm::api::pauseAll(pid);
			std::this_thread::yield();
			break;
		case 13:
			(void) vm::api::stop(pid);
			break;
		default:
			break;
		}
	}

	/**
	 * @brief Drives a terminating program the rest of the way. Disables every
	 * breakpoint the random ops may have set and resumes whenever the program
	 * parks, until it reaches a terminal status. Bounded by the watchdog.
	 */
	void releaseUntilTerminal(vm::PID pid) {
		for (u64 instr = 0; instr <= MAX_RANDOM_BREAKPOINT; instr++)
			(void) vm::api::setBreakpoint(pid, base::StrID("main"), instr, false);

		while (true) {
			auto status = vm::api::getExecutionStatus(pid);
			if (!status.has_value() || vm::api::isStatusTerminal(status.value())) return;
			(void) vm::api::resume(pid);
			std::this_thread::yield();
		}
	}

	/**
	 * @brief Attaches a status-recording listener for the lifetime of a scenario.
	 */
	struct ScopedStatusLog {
		TransitionLog                         log;
		events::Listener<vm::api::ProcStatus> listener;

		explicit ScopedStatusLog(VmApiStressTest& test, vm::PID pid):
			  listener([this](const vm::api::ProcStatus& s) { log.record(s); }) {
			test.assertTrue(
				vm::api::attachStatusListener(pid, &listener).has_value(), "Attach listener failed"
			);
		}

		~ScopedStatusLog() { listener.detach(); }

		ScopedStatusLog(const ScopedStatusLog&)            = delete;
		ScopedStatusLog& operator=(const ScopedStatusLog&) = delete;
	};

	// ------------------------------------------------------------------
	// Scenarios
	// ------------------------------------------------------------------

	/**
	 * @brief Run a fast-terminating program to completion while hammering it
	 * with random requests, then join and validate the memory state.
	 */
	void stressTerminatingProgram() {
		const u64 master_seed = masterSeed();
		std::cout << "stressTerminatingProgram seed=" << master_seed << "\n";

		for (usize iter = 0; iter < iterations(15); iter++) {
			const u64       seed = master_seed + iter;
			std::mt19937_64 rng(seed);
			Watchdog        watchdog(seed, "stressTerminatingProgram", SCENARIO_TIME_BUDGET);

			auto            pid = spawnAndLoad("breakpoint.dbc");
			ScopedStatusLog scoped(*this, pid);

			assertTrue(vm::api::run(pid).has_value(), "Run failed");

			const usize op_count = std::uniform_int_distribution<usize>(0, 30)(rng);
			// Debugger-only ops, no stop/run and no step (stepping a terminating
			// program to completion is currently broken): the program is allowed to
			// complete cleanly so guest-memory validity is a meaningful post-condition.
			for (usize op = 0; op < op_count; op++) randomBoundedOp(pid, rng, false, false);

			// The program may be parked by racing pauses/breakpoints - release it
			// fully so join can finish.
			releaseUntilTerminal(pid);
			(void) vm::api::join(pid);

			validateTransitions(scoped.log, seed);

			// The program completed cleanly (no step/stop interrupted it), so guest-memory
			// validity is a hard post-condition - an API error or a leak here is a regression.
			auto valid = vm::api::deinitAndValidate(pid);
			ASSERT_HAS_VALUE(valid);
			ASSERT_TRUE(valid.value());
		}
	}

	/**
	 * @brief Running program with a live thread must emit STOPPING before the terminal STOPPED.
	 * Deterministic.
	 */
	void stopEmitsStoppingStatus() {
		Watchdog watchdog(0, "stopEmitsStoppingStatus", SCENARIO_TIME_BUDGET);

		auto            pid = spawnAndLoad("while_true.dbc");
		ScopedStatusLog scoped(*this, pid);

		assertTrue(vm::api::run(pid).has_value(), "Run failed");
		waitUntilStatus(
			pid,
			[](const vm::api::ProcStatus& s) { return std::holds_alternative<vm::api::Running>(s); },
			"Running before stop"
		);

		assertTrue(vm::api::stop(pid).has_value(), "Stop failed");

		assertTrue(
			scoped.log.wasEmitted(
				base::variantTypeIndex<vm::api::ProcStatus, vm::api::ExecutionStopping>()
			),
			"Stopping a Running program never emitted the ExecutionStopping status"
		);
		validateTransitions(scoped.log, /*seed=*/0);
		assertTrue(vm::api::kill(pid).has_value(), "Kill failed");
	}

	/**
	 * @brief Random mix against a program that never terminates on its own, then
	 * stop and kill. A pending pause must not prevent stopping.
	 */
	void stressInfiniteProgram() {
		const u64 master_seed = masterSeed();
		std::cout << "stressInfiniteProgram seed=" << master_seed << "\n";

		for (usize iter = 0; iter < iterations(15); iter++) {
			const u64       seed = master_seed + iter;
			std::mt19937_64 rng(seed);
			Watchdog        watchdog(seed, "stressInfiniteProgram", SCENARIO_TIME_BUDGET);

			auto            pid = spawnAndLoad("while_true.dbc");
			ScopedStatusLog scoped(*this, pid);

			assertTrue(vm::api::run(pid).has_value(), "Run failed");

			const usize op_count = std::uniform_int_distribution<usize>(1, 40)(rng);
			for (usize op = 0; op < op_count; op++) randomBoundedOp(pid, rng, true, false);

			// A pending pause must not prevent stopping.
			(void) vm::api::resume(pid);
			assertTrue(
				vm::api::stop(pid).has_value(),
				"Stop of infinite program failed (seed=" + std::to_string(seed) + ")"
			);

			validateTransitions(scoped.log, seed);
			assertTrue(vm::api::kill(pid).has_value(), "Kill failed");
		}
	}

	/**
	 * @brief Several client threads hammer the same process concurrently through the API.
	 */
	void stressConcurrentClients() {
		constexpr usize CLIENT_COUNT = 4;

		const u64 master_seed = masterSeed();
		std::cout << "stressConcurrentClients seed=" << master_seed << "\n";

		for (usize iter = 0; iter < iterations(8); iter++) {
			const u64 seed = master_seed + iter;
			Watchdog  watchdog(seed, "stressConcurrentClients", SCENARIO_TIME_BUDGET);

			auto            pid = spawnAndLoad("while_true.dbc");
			ScopedStatusLog scoped(*this, pid);

			assertTrue(vm::api::run(pid).has_value(), "Run failed");

			std::vector<std::thread> clients;
			clients.reserve(CLIENT_COUNT);
			for (usize client = 0; client < CLIENT_COUNT; client++) {
				clients.emplace_back([this, pid, seed, client] {
					std::mt19937_64 rng(seed * CLIENT_COUNT + client);
					for (usize op = 0; op < 20; op++) randomBoundedOp(pid, rng, true, true);
				});
			}
			for (auto& client: clients) client.join();

			(void) vm::api::resume(pid);
			assertTrue(
				vm::api::stop(pid).has_value(),
				"Stop after concurrent stress failed (seed=" + std::to_string(seed) + ")"
			);

			validateTransitions(scoped.log, seed);
			assertTrue(vm::api::kill(pid).has_value(), "Kill failed");
		}
	}

	/**
	 * @brief `kill` must succeed from every reachable state and leave no process
	 * behind. After a successful kill the process is removed from the supervisor,
	 * so a subsequent status query must fail.
	 */
	void killAndExpectGone(vm::PID pid, std::string_view from_state) {
		assertTrue(
			vm::api::kill(pid).has_value(),
			"kill must succeed from state: " + std::string(from_state)
		);
		assertFalse(
			vm::api::getExecutionStatus(pid).has_value(),
			"process must be gone after kill from state: " + std::string(from_state)
		);
	}

	void killFromEveryState() {
		namespace api = vm::api;
		u64 case_id   = 0;

		const auto is_running = [](const api::ProcStatus& s) { return v_matches(s, api::Running); };
		const auto is_paused  = [](const api::ProcStatus& s) { return v_matches(s, api::Paused); };
		const auto is_sleeping
			= [](const api::ProcStatus& s) { return v_matches(s, api::Sleeping); };
		const auto is_terminal = [](const api::ProcStatus& s) { return api::isStatusTerminal(s); };

		auto test_kill_state =
			[&](std::string_view state_name, std::string_view prog, auto setup_func) {
				Watchdog watchdog(
					case_id++, std::string("kill@") + std::string(state_name), SCENARIO_TIME_BUDGET
				);
				auto pid = spawnAndLoad(prog);
				setup_func(pid);
				killAndExpectGone(pid, state_name);
			};

		// NotStarted: loaded but never run.
		test_kill_state("NotStarted", "while_true.dbc", [](vm::PID) {});

		// Running: the interpreter loop is actively executing.
		test_kill_state("Running", "while_true.dbc", [&](vm::PID pid) {
			assertTrue(api::run(pid).has_value(), "Run failed");
			waitUntilStatus(pid, is_running, "Running");
		});

		// Paused: suspended by the debugger.
		test_kill_state("Paused", "while_true.dbc", [&](vm::PID pid) {
			assertTrue(api::run(pid).has_value(), "Run failed");
			waitUntilStatus(pid, is_running, "Running");
			(void) api::pause(pid);
			waitUntilStatus(pid, is_paused, "Paused");
		});

		// Blocked on a bytecode condition variable / mutex.
		test_kill_state("Blocked(mutex/cv)", "mutex_hang.dbc", [&](vm::PID pid) {
			assertTrue(api::run(pid).has_value(), "Run failed");
			waitUntilStatus(pid, is_running, "Running (CV-blocked)");
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		});

		// Sleeping on IO (blocked reading input that never arrives).
		test_kill_state("Sleeping(IO)", "io_hang.dbc", [&](vm::PID pid) {
			assertTrue(api::run(pid).has_value(), "Run failed");
			waitUntilStatus(pid, is_sleeping, "Sleeping(IO)");
		});

		// Terminal: Completed (ran to normal completion).
		test_kill_state("Completed", "breakpoint.dbc", [&](vm::PID pid) {
			assertTrue(api::run(pid).has_value(), "Run failed");
			releaseUntilTerminal(pid);
			waitUntilStatus(pid, is_terminal, "Terminal");
		});

		// Terminal: Panicked (division by zero).
		test_kill_state("Panicked", "panic.dbc", [&](vm::PID pid) {
			assertTrue(api::run(pid).has_value(), "Run failed");
			waitUntilStatus(
				pid,
				[](const api::ProcStatus& s) { return v_matches(s, api::ExecutionPanicked); },
				"Panicked"
			);
		});

		// Terminal: Stopped (explicitly stopped, process kept in the table).
		test_kill_state("Stopped", "while_true.dbc", [&](vm::PID pid) {
			assertTrue(api::run(pid).has_value(), "Run failed");
			waitUntilStatus(pid, is_running, "Running");
			assertTrue(api::stop(pid).has_value(), "Stop failed");
			waitUntilStatus(pid, is_terminal, "Terminal");
		});
	}

	/**
	 * @brief Exercises the endpoints excluded from the bounded hammer because
	 * they may block: runFunctionAwait, join, output, waitForBreakpoint,
	 * input, attach/detach and attachOutputListener. Each is driven in a context where it is bounded.
	 */
	void exerciseBlockingEndpoints() {
		namespace api = vm::api;
		Watchdog watchdog(0, "exerciseBlockingEndpoints", SCENARIO_TIME_BUDGET);

		// runFunctionAwait
		{
			auto pid = spawnAndLoad("breakpoint.dbc");
			(void) api::runFunctionAwait(pid, "main");
			assertTrue(api::getExecutionStatus(pid).has_value(), "status query after await");
		}

		// join after a normal run to completion.
		{
			auto pid = spawnAndLoad("breakpoint.dbc");
			assertTrue(api::run(pid).has_value(), "Run failed");
			releaseUntilTerminal(pid);
			(void) api::join(pid, api::ThreadID{ 0 });
			(void) api::join(pid);  // already terminal - must return promptly
		}

		// waitForBreakpoint
		{
			auto pid = spawnAndLoad("breakpoint.dbc");
			assertTrue(
				api::setBreakpoint(pid, base::StrID("main"), 5, true).has_value(),
				"setBreakpoint failed"
			);
			assertTrue(api::run(pid).has_value(), "Run failed");
			auto pos = api::waitForBreakpoint(pid);
			assertTrue(pos.has_value(), "waitForBreakpoint should report the hit position");
			releaseUntilTerminal(pid);
			(void) api::join(pid);
		}

		// step: a few bounded steps from a breakpoint, then release.
		// @TODO: #3274 Step all the way to completion here once stepping past the end of a
		// function no longer live-locks the execution thread. Until then only a few steps are
		// taken and the rest of the program is resumed normally.
		{
			auto pid = spawnAndLoad("breakpoint.dbc");
			assertTrue(
				api::setBreakpoint(pid, base::StrID("main"), 2, true).has_value(), "setBreakpoint"
			);
			assertTrue(api::run(pid).has_value(), "Run failed");
			(void) api::waitForBreakpoint(pid);
			(void) api::setBreakpoint(pid, base::StrID("main"), 2, false);
			for (int s = 0; s < 3; s++) (void) api::step(pid);
			releaseUntilTerminal(pid);
			(void) api::join(pid);
		}

		// Input.
		{
			auto pid = spawnAndLoad("io_hang.dbc");
			assertTrue(api::run(pid).has_value(), "Run failed");
			waitUntilStatus(
				pid,
				[](const api::ProcStatus& s) { return v_matches(s, api::Sleeping); },
				"Sleeping(IO)"
			);
			assertTrue(api::input(pid, "42 ").has_value(), "input failed");
			waitUntilStatus(
				pid,
				[](const api::ProcStatus& s) { return api::isStatusTerminal(s); },
				"Terminal after input"
			);
			(void) api::join(pid);
		}

		// runFunction
		{
			auto pid = spawnAndLoad("while_true.dbc");
			(void) api::runFunction(pid, "main");
			(void) api::kill(pid);
		}

		// output on a non-executing process returns immediately.
		{
			auto pid = spawnAndLoad("breakpoint.dbc");
			(void) api::output(pid);  // NotStarted: returns empty, must not block
		}

		// attach / detach and an output listener.
		{
			auto                          pid = spawnAndLoad("breakpoint.dbc");
			std::istringstream            in("7 ");
			std::ostringstream            out;
			events::Listener<std::string> out_listener([](const std::string&) {});
			(void) api::attachOutputListener(pid, &out_listener);
			(void) api::attach(pid, in, out);
			(void) api::detach(pid);
			(void
			) api::mapFileLineToCodeCollectionPosition(pid, fs::File(path("breakpoint.dbc")), 1);
			out_listener.detach();
		}
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/api/");
