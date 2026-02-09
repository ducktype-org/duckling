#include <query_framework/internal/query_graph/active_graph.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <tester/tester.hpp>

#include <random>

// Those queries are used just to get dummy QueryIDs for NodeID generation.
DECLARE_QUERY(DummyQuery1, query::U64Key, u64, ({ .uses_qresult = false }));
DECLARE_QUERY(DummyQuery2, query::U64Key, u64, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(DummyQuery1, u64) {
	static auto provide(Context&, QKey key) -> PResult { return key.value; }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(DummyQuery1);

struct IMPLEMENT_QUERY(DummyQuery2, u64) {
	static auto provide(Context&, QKey key) -> PResult { return key.value; }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(DummyQuery2);


// class ActiveGraph: public tester::TestSuite {
// #undef TESTER_CLASS
// #define TESTER_CLASS ActiveGraph

// public:
// 	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
// 		TESTER_ADD_TEST(test);
// 	}

// private:
// }