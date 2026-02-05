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
		class Counter {
			u32 counter = 0;

		public:
			u32 add(const u32 x) {
				counter += x;
				return counter;
			}

			std::monostate noop() { return {}; }
		};

		// Prepare the instances (the sequential instance is not really used in this test).
		Counter tested_instance{};
		Counter sequential_instance{};

		// Instantiate the race tester with the instances.
		using RaceTester
			= concurrent::tester::RaceTester<Counter, Counter, Counter, u32, std::monostate>;
		RaceTester race_tester{ &tested_instance, &sequential_instance };

		/**
		 * @brief Helper struct to force the desired race.
		 */
		struct Sequencer {
			std::atomic<int> stage{ 0 };

			void waitFor(const int s) const {
				while (stage.load(std::memory_order_acquire) < s) std::this_thread::yield();
			}

			void next() { stage.fetch_add(1, std::memory_order_release); }
		};

		Sequencer seq{};

		// Prepare the worker function. It should act on the tested instance by providing the
		// executor with function objects and their descriptions, where the function objects
		// perform operations on a reference to the tested instance.
		//
		// Cases:
		//
		// 0: |—————+30—————|________|———noop———|
		// 1: |_____|———+30————|—————noop————|
		// 2: |_________|———+30———|——+20——|
		auto worker = [&seq](const u32 thread_id, RaceTester::Executor_ executor) {
			switch (thread_id) {
			case 0:
				seq.waitFor(0);
				executor.execute("add(30)", [&seq](const Ref<Counter> counter) {
					const auto res = counter->add(30);
					seq.next();
					seq.waitFor(3);
					return res;
				});
				seq.next();
				seq.waitFor(6);
				executor.execute("noop()", [&seq](const Ref<Counter> counter) {
					const auto res = counter->noop();
					seq.next();
					seq.waitFor(9);
					return res;
				});
				seq.next();
				break;
			case 1:
				seq.waitFor(1);
				executor.execute("add(30)", [&seq](const Ref<Counter> counter) {
					const auto res = counter->add(30);
					seq.next();
					seq.waitFor(4);
					return res;
				});
				executor.execute("noop()", [&seq](const Ref<Counter> counter) {
					const auto res = counter->noop();
					seq.next();
					seq.waitFor(8);
					return res;
				});
				seq.next();
				break;
			case 2:
				seq.waitFor(2);
				executor.execute("add(30)", [&seq](const Ref<Counter> counter) {
					const auto res = counter->add(30);
					seq.next();
					seq.waitFor(5);
					return res;
				});
				executor.execute("add(20)", [&seq](const Ref<Counter> counter) {
					const auto res = counter->add(20);
					seq.next();
					seq.waitFor(7);
					return res;
				});
				seq.next();
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
			"0: Thread 0 calls add(30)\n"
			"1: Thread 1 calls add(30)\n"
			"2: Thread 2 calls add(30)\n"
			"3: Thread 0 returns 30\n"
			"4: Thread 1 returns 60\n"
			"5: Thread 1 calls noop()\n"
			"6: Thread 2 returns 90\n"
			"7: Thread 2 calls add(20)\n"
			"8: Thread 0 calls noop()\n"
			"9: Thread 2 returns 110\n"
			"10: Thread 1 returns <monostate>\n"
			"11: Thread 0 returns <monostate>",
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
			std::atomic<usize> counter{};

			usize increment() override {
				auto i = counter.load(std::memory_order_relaxed);
				counter.store(i + 1, std::memory_order_relaxed);
				return i;
			}
		};

		/// A correct sequential counter, coincidentally identical to BadConcurrentCounter.
		class SequentialCounter: public CounterInterface {
			usize counter{};

			usize increment() override { return counter++; }
		};

		// Run for the good counter.
		{
			// Prepare the worker function.
			using RaceTesterGood = concurrent::tester::RaceTester<
				CounterInterface,
				GoodConcurrentCounter,
				SequentialCounter,
				usize>;

			const std::function<void(u32, RaceTesterGood::Executor_)> worker
				= [](u32, RaceTesterGood::Executor_ executor) {
					  for (usize i = 0; i < 20; ++i)
						  executor.execute("inc", [](const Ref<CounterInterface> counter) {
							  return counter->increment();
						  });
				  };

			// Run the tests
			const usize reps         = 10;
			const usize worker_count = 4;

			for (usize rep = 0; rep < reps; ++rep) {
				auto           tested     = makeBox<GoodConcurrentCounter>();
				auto           sequential = makeBox<SequentialCounter>();
				RaceTesterGood race_tester{ tested.refMut(), sequential.ref() };
				assertTrue(
					race_tester.runAndCheck(worker_count, worker),
					"Good counter should be linearizable."
				);
			}
		}

		// Run for the bad counter.
		{
			// Prepare the worker function.
			using RaceTesterBad = concurrent::tester::RaceTester<
				CounterInterface,
				BadConcurrentCounter,
				SequentialCounter,
				usize>;

			const std::function<void(u32, RaceTesterBad::Executor_)> worker
				= [](u32, RaceTesterBad::Executor_ executor) {
					  for (usize i = 0; i < 20; ++i)
						  executor.execute("inc", [](const Ref<CounterInterface> counter) {
							  return counter->increment();
						  });
				  };

			// Run the tests
			const usize reps         = 100;
			const usize worker_count = 4;

			bool failed = false;
			for (usize rep = 0; rep < reps and not failed; ++rep) {
				std::cerr << "\rRep " << rep + 1 << "/" << reps << ": ";
				auto          tested     = makeBox<BadConcurrentCounter>();
				auto          sequential = makeBox<SequentialCounter>();
				RaceTesterBad race_tester{ tested.refMut(), sequential.ref() };
				failed = not race_tester.runAndCheck(worker_count, worker);
			}
			assertTrue(failed, "Bad counter should not be linearizable.");
		}
	}

public:
	~RaceTesterTest() override = default;
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/")
