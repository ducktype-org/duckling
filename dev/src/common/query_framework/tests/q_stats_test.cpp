#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/q_stats/q_stats.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <tester/tester.hpp>

#include <iostream>
#include <sstream>
#include <string>

DECLARE_QUERY(StatsCountedQuery, query::U64Key, u64, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(StatsCountedQuery, u64) {
	static auto provide([[maybe_unused]] Context& ctx, QKey key) -> PResult { return key.value; }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(StatsCountedQuery);

class QStats: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS QStats

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testPrintStats); }

private:
	/**
	 * @brief printStats() lists every query that ran, then the global timers.
	 */
	void testPrintStats() {
		static_assert(query::USE_STATS, "this test needs the statistics to be collected");
		query::entryPoint<StatsCountedQuery>({ 1 });
		query::entryPoint<StatsCountedQuery>({ 2 });

		std::ostringstream captured;
		auto*              old_buffer = std::cerr.rdbuf(captured.rdbuf());
		query::printStats();
		std::cerr.rdbuf(old_buffer);
		const std::string out = captured.str();

		ASSERT_TRUE(out.starts_with("=== Query Framework Per Query Statistics ===\n\n"));
		ASSERT_TRUE(out.contains("Query ID: StatsCountedQuery\n    Number of Calls:   2\n"));
		ASSERT_TRUE(out.contains("    Number of P-Calls: 2\n"));
		ASSERT_TRUE(out.contains("=== Query Framework Other Statistics ===\n\n"));
		ASSERT_TRUE(out.contains("Total time spent in red-green sweeps: "));
		ASSERT_TRUE(out.contains("Total time spent in graph merges: "));
	}
};

TESTER_COMMON_MAIN("/src/common/query_framework/tests/");
