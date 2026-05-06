/**
 * @file cycle_detection_race_test.cpp
 * This file intentially contains only one test, as there is concurrent global state involved and
 * the code is already quite complex.
 */

#include <base/except/exceptions.hpp>
#include <base/types/ints.hpp>

#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <query_framework/utils/simple_keys.hpp>
#include <tester/tester.hpp>

#include <barrier>

std::atomic<u64> current_cycle_id{ 0 };
constexpr u64    WORKER_COUNT = 3;

struct CycleInitiatorKey {
	u64 expected_worker_id;
	u64 cycle_id;

	[[nodiscard]]
	base::Bit256 queryUnstablePerfectHash() const {
		return { expected_worker_id, cycle_id };
	}
};

DECLARE_QUERY(CycleInitiator, CycleInitiatorKey, query::QResult<u64>, ({}));

DECLARE_QUERY(Cycle1, query::U64Key, query::QResult<u64>, ({}));
DECLARE_QUERY(Cycle2, query::U64Key, query::QResult<u64>, ({}));
DECLARE_QUERY(Cycle3, query::U64Key, query::QResult<u64>, ({}));

/**
 * @brief Implementation for the cycle initiator query.
 * This is a special query, that asserts for each new cycle_id
 * that different workers are executing it, and then concurrently "fires" the cycle at the same time
 * on all workers using a barrier.
 */
struct IMPLEMENT_QUERY(CycleInitiator, query::QResult<u64>) {
	inline static std::barrier<> cycle_barrier{ 3 };

	static auto provide(Context& ctx, QKey key) -> PResult {
		CORE_ASSERT(current_cycle_id.load() == key.cycle_id, "Bad cycle id");
		CORE_ASSERT(
			concurrent::worker::Worker::getCurrentWorker()->getID() == key.expected_worker_id,
			"Bad worker id (1)"
		);
		CORE_ASSERT(
			0 <= key.expected_worker_id and key.expected_worker_id < WORKER_COUNT, "Bad worker id (2)"
		);

		cycle_barrier.arrive_and_wait();  // Ensure all workers "fire" at the same time

		switch (key.expected_worker_id) {
		case 0:
			return ctx.query<Cycle1>(key.cycle_id);
		case 1:
			return ctx.query<Cycle2>(key.cycle_id);
		case 2:
			return ctx.query<Cycle3>(key.cycle_id);
		default:
			CORE_UNREACHABLE();
		}
		CORE_UNREACHABLE();
	}

	QUERY_AUTO_CACHE_COPY
};

struct IMPLEMENT_QUERY(Cycle1, query::QResult<u64>) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		[[maybe_unused]]
		auto result
			= ctx.query<Cycle2>(key);
		CORE_PANIC("Cycle was not detected");
	}

	QUERY_AUTO_CACHE_COPY
};

struct IMPLEMENT_QUERY(Cycle2, query::QResult<u64>) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		[[maybe_unused]]
		auto result
			= ctx.query<Cycle3>(key);
		CORE_PANIC("Cycle was not detected");
	}

	QUERY_AUTO_CACHE_COPY
};

struct IMPLEMENT_QUERY(Cycle3, query::QResult<u64>) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		[[maybe_unused]]
		auto result
			= ctx.query<Cycle1>(key);
		CORE_PANIC("Cycle was not detected");
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(CycleInitiator);
QUERY_IMPLEMENTATION_BOILERPLATE(Cycle1);
QUERY_IMPLEMENTATION_BOILERPLATE(Cycle2);
QUERY_IMPLEMENTATION_BOILERPLATE(Cycle3);

class QueryTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS QueryTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(cycleDetectionTest); }

private:
	void beforeAll() override { concurrent::worker::setWorkerCount(WORKER_COUNT); }

	void runSingleCycleTest(u64 cycle_id) {
		// Note: this test assumes that the workers ids are counted from 0 to WORKER_COUNT-1.

		current_cycle_id.store(cycle_id);

		std::vector<query::EntryTaskHandle> handles;
		handles.reserve(WORKER_COUNT);
		for (u64 worker_id = 0; worker_id < WORKER_COUNT; worker_id++) {
			handles.emplace_back(query::scheduleEntryPoint<CycleInitiator>({ worker_id, cycle_id }));
		}

		for (auto& handle: handles) {
			auto result = query::awaitEntryPoint<CycleInitiator>(handle);
			assertTrue(result.hasFailed(), "Cycle was not detected");
		}

		return;
	}

	void cycleDetectionTest() {
		constexpr u64 REPEAT_COUNT = 10'000;

		std::map<u64, u64>
			cycle_errors_count;  // Map from cycle_id to number of errors detected for that cycle

		for (u64 i = 0; i < REPEAT_COUNT; i++) {
			runSingleCycleTest(i);

			auto logs              = query::Context::dumpToOneLoggerAndClear();
			u64  cycle_error_count = logs->errorCount();

			assertTrue(
				1 <= cycle_error_count and cycle_error_count <= WORKER_COUNT,
				"Expected from 1 to WORKER_COUNT error logs, but got "
					+ std::to_string(cycle_error_count)
			);

			cycle_errors_count[cycle_error_count]++;
		}

		// Print the distribution of detected cycle errors
		message(base::strConcat(
			"Cycle error count distribution over ",
			std::to_string(REPEAT_COUNT),
			" runs:\n",
			[&]() -> std::string {
				std::string result;
				for (const auto& [reported_diagnostic_count, hits]: cycle_errors_count) {
					result += std::to_string(reported_diagnostic_count)
				            + " error: " + std::to_string(hits) + " hits\n";
				}
				return result;
			}()
		));
	}
};

TESTER_COMMON_MAIN("/src/common/query_framework/tests/");
