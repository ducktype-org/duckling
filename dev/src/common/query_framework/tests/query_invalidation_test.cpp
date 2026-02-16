#include "diagnostic_interactive/placeholder.hpp"

#include "query_framework/entry/query_entry_point.hpp"
#include <query_framework/input_query/query_input.hpp>
#include <query_framework/input_query/query_input_impl.hpp>
#include <query_framework/internal/query_graph/active_graph.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_metadata/declare_metadata.hpp>
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

// 3_1 depends on Input(1)
// 3_2 depends on Input(2)
// 3_3 depends on Input(3)
// 3_4 depends on Input(4)
// 3_5 depends on Input(5)

DECLARE_METADATA_SIMPLE(SimpleMeta, u64);

// Those queries are used just to get dummy QueryIDs for NodeID generation.
DECLARE_QUERY(DummyQuery1, query::U64Key, u64, ({ .uses_qresult = false }));
DECLARE_QUERY(
	DummyQuery2, query::U64Key, u64, ({ .preserve_in_graph = true, .uses_qresult = false })
);
DECLARE_QUERY(DummyQuery3, query::U64Key, u64, ({ .uses_qresult = false }));

struct KeyOf_SideInput {
	u64 v;

	KeyOf_SideInput(u64 v): v(v) {}

	[[nodiscard]]
	u64 queryUnstablePerfectHash() const {
		CORE_PANIC("Unstable perfect hash should not be used for SideInput");
	}

	[[nodiscard]]
	query::QueryStableHash queryStablePerfectHash() const {
		return { v, 0, 0, 0 };
	}
};

DECLARE_QUERY_SIDE_INPUT(SideInput, KeyOf_SideInput);

struct IMPLEMENT_QUERY(DummyQuery1, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		ctx.logInt(makeBox<dia_int::PlaceholderHeaderError>("..."));

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
		ctx.addMetadata<metadata_SimpleMeta>(key.value);

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
	static auto provide(Context& ctx, QKey key) -> PResult {
		ctx.query<SideInput>({ key.value });
		return key.value;
	}

	QUERY_AUTO_CACHE_COPY
};

IMPLEMENT_QUERY_SIDE_INPUT(SideInput);

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


		auto& state = query::Context::getState();
		auto& graph = state.getGraph();
		ASSERT_EQUAL(graph.getAllNodes().size(), 17); // 3 + 4 + 5 + 5

		ASSERT_EQUAL(graph.getNodeDeps<DummyQuery1>({ 1 }).size(), 8); // 2 + 3 + 3
		ASSERT_EQUAL(graph.getNodeDeps<DummyQuery1>({ 2 }).size(), 8);
		ASSERT_EQUAL(graph.getNodeDeps<DummyQuery1>({ 3 }).size(), 8);


		ASSERT_EQUAL(ImplementationOf_DummyQuery1::cache.size(), 3);
		ASSERT_EQUAL(ImplementationOf_DummyQuery2::cache.size(), 4);
		ASSERT_EQUAL(ImplementationOf_DummyQuery3::cache.size(), 5);

		auto node_2_1 = query::internal::makeNodeID<DummyQuery2>(query::U64Key{ 1 });
		auto node_2_2 = query::internal::makeNodeID<DummyQuery2>(query::U64Key{ 1 });
		auto node_2_3 = query::internal::makeNodeID<DummyQuery2>(query::U64Key{ 1 });
		auto node_2_4 = query::internal::makeNodeID<DummyQuery2>(query::U64Key{ 1 });

		ASSERT_EQUAL(state.getMetadata<metadata_SimpleMeta>(node_2_1).size(), 1);
		ASSERT_EQUAL(state.getMetadata<metadata_SimpleMeta>(node_2_2).size(), 1);
		ASSERT_EQUAL(state.getMetadata<metadata_SimpleMeta>(node_2_3).size(), 1);
		ASSERT_EQUAL(state.getMetadata<metadata_SimpleMeta>(node_2_4).size(), 1);

		auto node_3_1 = query::internal::makeNodeID<DummyQuery3>(query::U64Key{ 1 });
		auto node_3_2 = query::internal::makeNodeID<DummyQuery3>(query::U64Key{ 2 });
		auto node_3_3 = query::internal::makeNodeID<DummyQuery3>(query::U64Key{ 3 });
		auto node_3_4 = query::internal::makeNodeID<DummyQuery3>(query::U64Key{ 4 });
		auto node_3_5 = query::internal::makeNodeID<DummyQuery3>(query::U64Key{ 5 });
		ASSERT_EQUAL(state.getDiagnosticForNode(node_3_1).value()->errorCount(), 1);
		ASSERT_EQUAL(state.getDiagnosticForNode(node_3_2).value()->errorCount(), 1);
		ASSERT_EQUAL(state.getDiagnosticForNode(node_3_3).value()->errorCount(), 1);
		ASSERT_EQUAL(state.getDiagnosticForNode(node_3_4).value()->errorCount(), 1);
		ASSERT_EQUAL(state.getDiagnosticForNode(node_3_5).value()->errorCount(), 1);
	}
};
