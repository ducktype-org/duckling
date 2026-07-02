#include <concurrent/module_flags/worker_count.hpp>
#include <querylinger/queries/version.hpp>

#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/module_flags/module_flags.hpp>
#include <tester/tester.hpp>

class QuerylingerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS QuerylingerTest

	void versionQueryComputesOnce() {
		const std::string first = query::entryPoint<ql::QueryCompilerVersion>(ql::VersionKey{});
		const usize       calls_after_first = ql::versionProvideCalls();

		const std::string second = query::entryPoint<ql::QueryCompilerVersion>(ql::VersionKey{});

		ASSERT_EQUAL_PRINT(std::string(ql::QUERYLINGER_VERSION), first);
		ASSERT_EQUAL_PRINT(first, second);
		ASSERT_EQUAL_PRINT(usize(1), calls_after_first);
		// The second entry point call must be served from the cache.
		ASSERT_EQUAL_PRINT(calls_after_first, ql::versionProvideCalls());
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		concurrent::worker::setWorkerCount(4);
		query::setTrackReverseGraph(true);

		TESTER_ADD_TEST(versionQueryComputesOnce);
	}
};

TESTER_COMMON_MAIN("/src/playground/psac_stuff/query_compiler/tests");
