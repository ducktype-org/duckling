#include "query_framework/entry/query_entry_point.hpp"
#include <query_framework/internal/query_graph/active_graph.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <tester/tester.hpp>

// We need to test things:
// 1. All queries dependend on start nodes are invalidated correctly, meaning
//   - they are removed from the graph
//   - their cache is cleared
//   - their metadata is cleared
//   - their diagnostics are cleared
//   - other nodes are not affected
//   - if an erased node is in the reversed dependency

// 1_1 depends on (2_1, 2_2)

// 1_2 depends on (2_2, 2_3)

// 1_3 depends on (2_3, 2_4)

// 2_1 depends on (3_1, 3_2)
// 2_2 depends on (3_2, 3_3)
// 2_3 depends on (3_3, 3_4)
// 2_4 depends on (3_4, 3_5)

// Those queries are used just to get dummy QueryIDs for NodeID generation.
DECLARE_QUERY(DummyQuery1, query::U64Key, u64, ({ .uses_qresult = false }));
DECLARE_QUERY(DummyQuery2, query::U64Key, u64, ({ .uses_qresult = false }));
DECLARE_QUERY(DummyQuery3, query::U64Key, u64, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(DummyQuery1, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		if (key.value == 1) {
			ctx.query<DummyQuery2>({ 2 });
			ctx.query<DummyQuery2>({ 3 });
		} else if (key.value == 2) {
			ctx.query<DummyQuery3>({ 4 });
			ctx.query<DummyQuery3>({ 5 });
		} else if (key.value == 3) {
			ctx.query<DummyQuery3>({ 6 });
			ctx.query<DummyQuery3>({ 7 });
		}
		return key.value;
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(DummyQuery1);

struct IMPLEMENT_QUERY(DummyQuery2, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		if (key.value == 1) {
			ctx.query<DummyQuery3>({ 1 });
			ctx.query<DummyQuery3>({ 2 });
		} else if (key.value == 2) {
			ctx.query<DummyQuery3>({ 2 });
			ctx.query<DummyQuery3>({ 3 });
		} else if (key.value == 3) {
			ctx.query<DummyQuery3>({ 3 });
			ctx.query<DummyQuery3>({ 4 });
		} else if (key.value == 4) {
			ctx.query<DummyQuery3>({ 4 });
			ctx.query<DummyQuery3>({ 5 });
		}
		return key.value;
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(DummyQuery2);

struct IMPLEMENT_QUERY(DummyQuery3, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult { return key.value; }

	QUERY_AUTO_CACHE_COPY
};

class ActiveGraph: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ActiveGraph

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(testInvalidation); }

private:
	void testInvalidation() {
		query::entryPoint<DummyQuery1>({ 1 });
		query::entryPoint<DummyQuery1>({ 2 });
		query::entryPoint<DummyQuery1>({ 3 });


		const auto& graph = query::Context::getState().getGraph();
		graph.get
	}
};
