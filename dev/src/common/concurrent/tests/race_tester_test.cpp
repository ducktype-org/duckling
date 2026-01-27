#include <concurrent/race_tester/race_tester.hpp>

#include <tester/tester.hpp>

class RaceTesterTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS RaceTesterTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(historyTest);
		TESTER_ADD_TEST(linearizationTest);
	}

private:
	/**
	 * Test the functionality of recording a race history.
	 */
	void historyTest() {
		/**
		 * A simple class with methods that can be called concurrently. Its methods perform
		 * "heavy computation" in the form of sleeping for a controlled amount of time.
		 * The `wait10` method takes an returns a value to test History's ability to record values.
		 */
		class Waiter {
			u32 counter = 0;

		public:
			u32 wait(const u32 millis) {
				counter += millis;
				const u32 result = counter;
				std::this_thread::sleep_for(std::chrono::milliseconds(millis));
				return result;
			}

			std::monostate wait50() {
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				return {};
			}
		};

		// Prepare the instances (the sequential instance is not really used in this test).
		Waiter tested_instance{};
		Waiter sequential_instance{};

		// Instantiate the race tester with the instances.
		using RaceTester
			= concurrent::tester::RaceTester<Waiter, Waiter, Waiter, u32, std::monostate>;
		RaceTester race_tester{ &tested_instance, &sequential_instance };

		// Prepare the worker function. It should act on the tested instance by providing the
		// executor with function objects and their descriptions, where the function objects
		// perform operations on a reference to the tested instance.
		//
		// Cases:
		//   em-dash lines mean waiting using the waiter, numbers with pluses mean using wait(),
		//   underscore lines mean waiting using std::this_thread::sleep_for().
		//
		// 0: |—————+300—————|___300___|————500————|
		// 1: |__100_|———+300———|——————500——————|
		// 2: |____200___|———+300———|——+200——|
		auto worker = [](const u32 thread_id, RaceTester::_Executor executor) {
			switch (thread_id) {
			case 0:
				executor.execute("wait(30)", [](Ref<Waiter> waiter) { return waiter->wait(30); });
				std::this_thread::sleep_for(std::chrono::milliseconds(30));
				executor.execute("wait50()", [](Ref<Waiter> waiter) { return waiter->wait50(); });
				break;
			case 1:
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				executor.execute("wait(30)", [](Ref<Waiter> waiter) { return waiter->wait(30); });
				executor.execute("wait50()", [](Ref<Waiter> waiter) { return waiter->wait50(); });
				break;
			case 2:
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				executor.execute("wait(30)", [](Ref<Waiter> waiter) { return waiter->wait(30); });
				executor.execute("wait(20)", [](Ref<Waiter> waiter) { return waiter->wait(20); });
				break;
			default:
				CORE_UNREACHABLE();
			}
		};

		// Run the test.
		race_tester.run(3, worker);

		// Confirm that the history is as expected.
		assertEqual(
			race_tester.getHistory()->toString(),
			"History:\n"
			"Thread 0 calls wait(30)\n"
			"Thread 1 calls wait(30)\n"
			"Thread 2 calls wait(30)\n"
			"Thread 0 returns 30\n"
			"Thread 1 returns 60\n"
			"Thread 1 calls wait50()\n"
			"Thread 2 returns 90\n"
			"Thread 2 calls wait(20)\n"
			"Thread 0 calls wait50()\n"
			"Thread 2 returns 110\n"
			"Thread 1 returns <monostate>\n"
			"Thread 0 returns <monostate>",
			"History should match expected output."
		);
	}

	/**
	 * Test the linearization testing functionality, both for correct and incorrect implementations.
	 */
	void linearizationTest() {
		class CounterInterface {
		public:
			/// Increments the counter and returns its previous value.
			virtual usize increment()   = 0;
			virtual ~CounterInterface() = default;
		};

		/// A correct concurrent counter using atomic operations.
		class GoodConcurrentCounter: public CounterInterface {
			std::atomic<usize> counter{};

			usize increment() override { return counter.fetch_add(1); }
		};

		/// An incorrect concurrent counter which assumes increment is atomic.
		class BadConcurrentCounter: public CounterInterface {
			usize counter{};

			usize increment() override { return counter++; }
		};

		/// A correct sequential counter, coincidentally identical to BadConcurrentCounter.
		class SequentialConcurrentCounter: public CounterInterface {
			usize counter{};

			usize increment() override { return counter++; }
		};

		// Run for the good counter.
		{
			// Prepare the worker function.
			using _RaceTesterGood = concurrent::tester::RaceTester<
				CounterInterface,
				GoodConcurrentCounter,
				SequentialConcurrentCounter,
				usize>;

			const std::function<void(u32, _RaceTesterGood::_Executor)> worker
				= [](u32, _RaceTesterGood::_Executor executor) {
					  for (usize i = 0; i < 1'000; ++i)
						  executor.execute("inc", [](const Ref<CounterInterface> counter) {
							  return counter->increment();
						  });
				  };

			// Run the tests
			constexpr usize reps         = 10;
			constexpr usize worker_count = 4;

			for (usize rep = 0; rep < reps; ++rep) {
				auto            tested     = makeBox<GoodConcurrentCounter>();
				auto            sequential = makeBox<SequentialConcurrentCounter>();
				_RaceTesterGood race_tester{ tested.refMut(), sequential.ref() };
				assertTrue(
					race_tester.runAndCheck(worker_count, worker),
					"Good counter should be linearizable."
				);
			}
		}

		// Run for the bad counter.
		{
			// Prepare the worker function.
			using _RaceTesterBad = concurrent::tester::RaceTester<
				CounterInterface,
				BadConcurrentCounter,
				SequentialConcurrentCounter,
				usize>;

			const std::function<void(u32, _RaceTesterBad::_Executor)> worker
				= [](u32, _RaceTesterBad::_Executor executor) {
					  for (usize i = 0; i < 1'000; ++i)
						  executor.execute("inc", [](const Ref<CounterInterface> counter) {
							  return counter->increment();
						  });
				  };

			// Run the tests
			constexpr usize reps         = 10;
			constexpr usize worker_count = 4;

			bool failed = false;
			for (usize rep = 0; rep < reps and not failed; ++rep) {
				std::cerr << "\rRep " << rep + 1 << "/" << reps << ": ";
				auto           tested     = makeBox<BadConcurrentCounter>();
				auto           sequential = makeBox<SequentialConcurrentCounter>();
				_RaceTesterBad race_tester{ tested.refMut(), sequential.ref() };
				failed = not race_tester.runAndCheck(worker_count, worker);
			}
			assertTrue(failed, "Bad counter should not be linearizable.");
		}
	}

public:
	~RaceTesterTest() override = default;
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/")
