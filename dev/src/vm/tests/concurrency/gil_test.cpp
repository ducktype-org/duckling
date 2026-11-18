// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <base/types/ints.hpp>

#include <tester/tester.hpp>

#include <vm/core/safe/concurrency/gil.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief What belongs here: the GIL used directly, without running a program.
 */
class GilTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS GilTester

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(releasedGilGoesToEarlierWaiter);
		TESTER_ADD_TEST(lateWaiterGetsGilWhileOthersTradeIt);
	}

	static constexpr auto WAIT_BUDGET = std::chrono::seconds(10);

	/**
	 * @brief Spins until a waiter asks the holder to release, which means it is queued.
	 */
	bool waitForReleaseRequest(vm::GIL& gil) {
		const auto deadline = std::chrono::steady_clock::now() + WAIT_BUDGET;
		while (std::chrono::steady_clock::now() < deadline) {
			if (gil.shouldRelease()) return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return false;
	}

	void releasedGilGoesToEarlierWaiter() {
		vm::GIL                  gil;
		std::vector<std::string> order;

		gil.acquire();
		std::thread waiter([&] {
			gil.acquire();
			order.emplace_back("waiter");
			gil.release();
		});

		const bool requested = waitForReleaseRequest(gil);
		gil.release();
		gil.acquire();
		order.emplace_back("holder");
		gil.release();
		waiter.join();

		assertTrue(requested, "the waiter never asked for the GIL");
		assertEqual(
			std::vector<std::string>{ "waiter", "holder" },
			order,
			"a thread re-acquiring right after release must queue behind the waiter"
		);
	}

	void lateWaiterGetsGilWhileOthersTradeIt() {
		vm::GIL           gil;
		std::atomic<bool> stop{ false };
		std::atomic<bool> late_acquired{ false };

		// Mirrors SafeVMThread::stepGil.
		auto spinner = [&] {
			gil.acquire();
			while (!stop.load(std::memory_order_relaxed)) {
				if (!gil.shouldRelease()) continue;
				gil.release();
				std::this_thread::yield();
				gil.acquire();
			}
			gil.release();
		};

		std::thread first(spinner);
		std::thread second(spinner);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		std::thread late([&] {
			gil.acquire();
			late_acquired.store(true);
			gil.release();
		});

		const auto deadline = std::chrono::steady_clock::now() + WAIT_BUDGET;
		while (!late_acquired.load() && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		const bool acquired_in_time = late_acquired.load();

		stop.store(true);
		first.join();
		second.join();
		late.join();

		assertTrue(acquired_in_time, "the late waiter was starved while two threads traded the GIL");
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/concurrency/");
