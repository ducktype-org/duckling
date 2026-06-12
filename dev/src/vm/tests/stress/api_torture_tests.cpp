#include <base/misc/int_conv.hpp>

#include <tester/tester.hpp>

#include <vm/api/vm.hpp>
#include <vm/core/process/lifecycle.hpp>

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <variant>
#include <vector>

/**
 * @brief Model-based API torture tests for the DVM.
 *
 * Runs seeded, randomized sequences of API calls against processes executing
 * small programs and checks strict oracles instead of exact outcomes:
 *  - every status transition emitted by the process is a legal single-event
 *    edge of `vm::lifecycle::statusTransitions()`,
 *  - no scenario exceeds its time budget (watchdog aborts and prints the seed),
 *  - terminating scenarios pass `deinitAndValidate` (guest memory state).
 *
 * Individual API calls are allowed to fail (racing a completing program is
 * legal); they are not allowed to crash, hang or produce illegal transitions.
 *
 * Reproduction: the master seed is printed on start and on watchdog timeout.
 * Override with `DUCK_TORTURE_SEED`, scale with `DUCK_TORTURE_ITERATIONS`.
 *
 * See README.md in this directory for the full plan (phases 2 and 3).
 */
class VmApiTortureTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VmApiTortureTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(tortureTerminatingProgram);
		TESTER_ADD_TEST(tortureInfiniteProgram);
		TESTER_ADD_TEST(tortureConcurrentClients);
	}

	~VmApiTortureTest() override = default;

private:
	static constexpr auto SCENARIO_TIME_BUDGET = std::chrono::seconds(60);

	/**
	 * @brief Aborts the whole suite (printing the seed) if a scenario does not
	 * finish within its time budget. A hung API call cannot be recovered from
	 * within the process, so failing hard with the reproduction seed is the
	 * only useful outcome.
	 */
	class Watchdog {
	public:
		Watchdog(u64 seed, std::chrono::seconds budget): seed(seed) {
			thread = std::thread([this, budget] {
				std::unique_lock lock(mutex);
				if (!cv.wait_for(lock, budget, [this] { return done; })) {
					std::cerr << "TORTURE WATCHDOG: scenario exceeded its time budget, seed="
							  << this->seed << "\n";
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
		u64                     seed;
		std::thread             thread;
		std::mutex              mutex;
		std::condition_variable cv;
		bool                    done = false;
	};

	/**
	 * @brief Records every status the process emits, for transition validation.
	 */
	struct TransitionLog {
		std::mutex                       mutex;
		std::vector<vm::api::ProcStatus> statuses;

		void record(const vm::api::ProcStatus& status) {
			std::lock_guard lock(mutex);
			statuses.push_back(status);
		}
	};

	static constexpr usize STATUS_COUNT = std::variant_size_v<vm::api::ProcStatus>;

	static vm::api::ProcStatus statusFromIndex(usize index) {
		using namespace vm::api;
		switch (index) {
		case 0:
			return NotStarted{};
		case 1:
			return Running{};
		case 2:
			return Paused{};
		case 3:
			return Sleeping{};
		case 4:
			return ExecutionCompleted{};
		case 5:
			return ExecutionStopped{};
		case 6:
			return ExecutionPanicked{ "" };
		default:
			CORE_PANIC("Invalid status index");
		}
	}

	static std::vector<vm::lifecycle::Event> allEvents() {
		using namespace vm::lifecycle;
		return { Start{},        Pause{}, Resume{},    Sleep{}, Wake{},
			     Complete{ {} }, Stop{},  Panic{ "" }, Reset{} };
	}

	/**
	 * @brief `edge_matrix[from][to]` is true when some single lifecycle event
	 * transitions a machine from status index `from` to status index `to`.
	 * Built once by dispatching every event from every status.
	 */
	static const std::array<std::array<bool, STATUS_COUNT>, STATUS_COUNT>& edgeMatrix() {
		static const auto matrix = [] {
			std::array<std::array<bool, STATUS_COUNT>, STATUS_COUNT> result{};
			for (usize from = 0; from < STATUS_COUNT; from++) {
				for (const auto& event: allEvents()) {
					vm::lifecycle::Machine machine(
						statusFromIndex(from), &vm::lifecycle::statusTransitions()
					);
					auto dispatched = machine.handleEvent(event);
					if (dispatched.has_value() && dispatched.value().has_value())
						result.at(from).at(dispatched.value().value().state.index()) = true;
				}
			}
			return result;
		}();
		return matrix;
	}

	void validateTransitions(TransitionLog& log, u64 seed) {
		std::lock_guard lock(log.mutex);
		for (usize i = 1; i < log.statuses.size(); i++) {
			const usize from = log.statuses[i - 1].index();
			const usize to   = log.statuses[i].index();
			assertTrue(
				edgeMatrix().at(from).at(to),
				"Illegal status transition emitted by the process: " + std::to_string(from) + " -> "
					+ std::to_string(to) + " (seed=" + std::to_string(seed) + ")"
			);
		}
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

	vm::PID loadProgram(std::string_view program) {
		auto spawned = vm::api::spawn();
		assertTrue(spawned.has_value(), "Spawn failed");
		auto pid = spawned.value().pid;

		fs::File file(path(std::string(program)));
		assertTrue(vm::api::loadFiles(pid, { file }).has_value(), "Load failed");
		return pid;
	}

	/**
	 * @brief One random debugger/status API call against the process.
	 * Results are intentionally not asserted in detail: a call racing the
	 * program's natural progress may legally fail. `getExecutionStatus` is the
	 * exception - it must always succeed for a live process.
	 */
	void randomOp(vm::PID pid, std::mt19937_64& rng) {
		switch (std::uniform_int_distribution<int>(0, 7)(rng)) {
		case 0:
			(void) vm::api::pause(pid);
			break;
		case 1:
			(void) vm::api::resume(pid);
			break;
		case 2:
			(void) vm::api::step(pid);
			break;
		case 3:
			assertTrue(
				vm::api::getExecutionStatus(pid).has_value(), "getExecutionStatus must succeed"
			);
			break;
		case 4:
			(void) vm::api::getCurrentPosition(pid);
			break;
		case 5:
			(void) vm::api::setBreakpoint(
				pid,
				base::StrID("main"),
				std::uniform_int_distribution<u64>(0, 8)(rng),
				std::uniform_int_distribution<int>(0, 1)(rng) == 1
			);
			break;
		case 6:
			(void) vm::api::debuggerGetNumberOfStackFrames(pid, vm::api::ThreadID{ 0 });
			break;
		case 7:
			std::this_thread::yield();
			break;
		default:
			break;
		}
	}

	/**
	 * @brief Drives a terminating program the rest of the way: disables all the
	 * breakpoints the random ops may have set and resumes whenever the program
	 * parks, until it reaches a terminal status. Bounded by the watchdog.
	 */
	static void releaseUntilTerminal(vm::PID pid) {
		for (u64 instr = 0; instr <= 8; instr++)
			(void) vm::api::setBreakpoint(pid, base::StrID("main"), instr, false);

		while (true) {
			auto status = vm::api::getExecutionStatus(pid);
			if (!status.has_value() || vm::api::isStatusTerminal(status.value())) return;
			(void) vm::api::resume(pid);
			std::this_thread::yield();
		}
	}

	/**
	 * @brief Run a fast-terminating program to completion while hammering it
	 * with random requests; then join and validate the guest memory state.
	 */
	void tortureTerminatingProgram() {
		const u64 master_seed = masterSeed();
		std::cout << "tortureTerminatingProgram seed=" << master_seed << "\n";

		for (usize iter = 0; iter < iterations(15); iter++) {
			const u64       seed = master_seed + iter;
			std::mt19937_64 rng(seed);
			Watchdog        watchdog(seed, SCENARIO_TIME_BUDGET);

			auto                                  pid = loadProgram("breakpoint.dbc");
			TransitionLog                         log;
			events::Listener<vm::api::ProcStatus> listener([&log](const vm::api::ProcStatus& s) {
				log.record(s);
			});
			assertTrue(
				vm::api::attachStatusListener(pid, &listener).has_value(), "Attach listener failed"
			);

			assertTrue(vm::api::run(pid).has_value(), "Run failed");

			const usize op_count = std::uniform_int_distribution<usize>(0, 30)(rng);
			for (usize op = 0; op < op_count; op++) randomOp(pid, rng);

			// The program may be paused by racing pauses/breakpoints - release it
			// fully so join can finish.
			releaseUntilTerminal(pid);
			(void) vm::api::join(pid);

			validateTransitions(log, seed);
			listener.detach();

			auto valid = vm::api::deinitAndValidate(pid);
			assertTrue(
				valid.has_value() && valid.value(),
				"Guest memory state invalid after torture (seed=" + std::to_string(seed) + ")"
			);
		}
	}

	/**
	 * @brief Random pause/resume/step/breakpoint mix against a program that
	 * never terminates on its own; then stop and kill.
	 */
	void tortureInfiniteProgram() {
		const u64 master_seed = masterSeed();
		std::cout << "tortureInfiniteProgram seed=" << master_seed << "\n";

		for (usize iter = 0; iter < iterations(15); iter++) {
			const u64       seed = master_seed + iter;
			std::mt19937_64 rng(seed);
			Watchdog        watchdog(seed, SCENARIO_TIME_BUDGET);

			auto                                  pid = loadProgram("while_true.dbc");
			TransitionLog                         log;
			events::Listener<vm::api::ProcStatus> listener([&log](const vm::api::ProcStatus& s) {
				log.record(s);
			});
			assertTrue(
				vm::api::attachStatusListener(pid, &listener).has_value(), "Attach listener failed"
			);

			assertTrue(vm::api::run(pid).has_value(), "Run failed");

			const usize op_count = std::uniform_int_distribution<usize>(1, 40)(rng);
			for (usize op = 0; op < op_count; op++) randomOp(pid, rng);

			// A pending pause must not prevent stopping.
			(void) vm::api::resume(pid);
			assertTrue(
				vm::api::stop(pid).has_value(),
				"Stop of infinite program failed (seed=" + std::to_string(seed) + ")"
			);

			validateTransitions(log, seed);
			listener.detach();
			assertTrue(vm::api::kill(pid).has_value(), "Kill failed");
		}
	}

	/**
	 * @brief Several client threads hammer the same process concurrently.
	 * Exercises the API surface against simultaneous requests - the class of
	 * race the response-queue removal in #2922 is meant to survive.
	 */
	void tortureConcurrentClients() {
		constexpr usize CLIENT_COUNT = 4;

		const u64 master_seed = masterSeed();
		std::cout << "tortureConcurrentClients seed=" << master_seed << "\n";

		for (usize iter = 0; iter < iterations(8); iter++) {
			const u64 seed = master_seed + iter;
			Watchdog  watchdog(seed, SCENARIO_TIME_BUDGET);

			auto                                  pid = loadProgram("while_true.dbc");
			TransitionLog                         log;
			events::Listener<vm::api::ProcStatus> listener([&log](const vm::api::ProcStatus& s) {
				log.record(s);
			});
			assertTrue(
				vm::api::attachStatusListener(pid, &listener).has_value(), "Attach listener failed"
			);

			assertTrue(vm::api::run(pid).has_value(), "Run failed");

			std::vector<std::thread> clients;
			clients.reserve(CLIENT_COUNT);
			for (usize client = 0; client < CLIENT_COUNT; client++) {
				clients.emplace_back([this, pid, seed, client] {
					std::mt19937_64 rng(seed * CLIENT_COUNT + client);
					for (usize op = 0; op < 20; op++) randomOp(pid, rng);
				});
			}
			for (auto& client: clients) client.join();

			(void) vm::api::resume(pid);
			assertTrue(
				vm::api::stop(pid).has_value(),
				"Stop after concurrent torture failed (seed=" + std::to_string(seed) + ")"
			);

			validateTransitions(log, seed);
			listener.detach();
			assertTrue(vm::api::kill(pid).has_value(), "Kill failed");
		}
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/stress/");
