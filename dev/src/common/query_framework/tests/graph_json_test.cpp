#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/external/api.hpp>
#include <query_framework/input_query/query_input.hpp>
#include <query_framework/input_query/query_input_impl.hpp>
#include <query_framework/internal/query_graph/graph_json.hpp>
#include <query_framework/internal/query_graph/node_making.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <tester/tester.hpp>

#include <algorithm>
#include <sstream>
#include <string>

struct KeyOf_JsonSideInput {
	u64 v;

	KeyOf_JsonSideInput(u64 v): v(v) {}

	[[nodiscard]]
	query::QueryStableHash queryStablePerfectHash() const {
		return { v, 0, 0, 0 };
	}
};

DECLARE_QUERY_SIDE_INPUT(JsonSideInput, KeyOf_JsonSideInput);

struct KeyOf_JsonStable {
	u64 v;

	KeyOf_JsonStable(u64 v): v(v) {}

	[[nodiscard]]
	query::QueryStableHash queryStablePerfectHash() const {
		return { v, 0, 0, 0 };
	}
};

// Stable and preserved, so it survives graph optimization.
DECLARE_QUERY(
	JsonStableQuery,
	KeyOf_JsonStable,
	u64,
	({ .used_hashes       = query::UsedHashes::StableHash,
       .preserve_in_graph = true,
       .uses_qresult      = false })
);

// Unstable leaf, so graph optimization removes it.
DECLARE_QUERY(JsonUnstableQuery, query::U64Key, u64, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(JsonStableQuery, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		ctx.query<JsonSideInput>({ key.v });
		return ctx.query<JsonUnstableQuery>({ key.v });
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(JsonStableQuery);

struct IMPLEMENT_QUERY(JsonUnstableQuery, u64) {
	static auto provide([[maybe_unused]] Context& ctx, QKey key) -> PResult { return key.value; }

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(JsonUnstableQuery);

IMPLEMENT_QUERY_SIDE_INPUT(JsonSideInput);

class GraphJson: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS GraphJson

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testHandWrittenGraph);
		TESTER_ADD_TEST(testEmptyGraph);
		TESTER_ADD_TEST(testPreAndPostOptimization);
	}

private:
	static std::string dump(query::external::QueryGraphDumpStage stage) {
		std::ostringstream out;
		query::external::dumpQueryGraphAsJson(stage, out);
		return out.str();
	}

	/**
	 * @brief Nodes are sorted by NodeID and dependency indices follow the new order.
	 */
	void testHandWrittenGraph() {
		// Make sure the static registration of the queries has run.
		query::entryPoint<JsonStableQuery>({ 1 });

		const auto stable   = query::internal::makeNodeID<JsonStableQuery>(KeyOf_JsonStable{ 5 });
		const auto unstable = query::internal::makeNodeID<JsonUnstableQuery>(query::U64Key{ 5 });
		const auto input    = query::internal::makeNodeID<JsonSideInput>(KeyOf_JsonSideInput{ 5 });

		// Deliberately not in NodeID order, the writer has to sort them.
		query::internal::QueryGraph::ReducedGraphData data{
			.nodes     = { stable, unstable, input },
			.adjacency = { { 2, 1 }, {}, {} },
		};

		std::vector<query::internal::NodeID> sorted = data.nodes;
		std::ranges::sort(sorted, [](const auto& l, const auto& r) { return l < r; });
		auto index_of = [&sorted](const query::internal::NodeID& node) {
			return std::to_string(std::ranges::find(sorted, node) - sorted.begin());
		};

		std::ostringstream out;
		query::internal::writeReducedGraphAsJson(data, "test_stage", out);
		const std::string json = out.str();

		ASSERT_TRUE(json.contains("\"stage\": \"test_stage\""));

		const auto deps_of_stable = std::min(index_of(input), index_of(unstable)) + ", "
		                          + std::max(index_of(input), index_of(unstable));
		ASSERT_TRUE(json.contains(
			"{ \"index\": " + index_of(stable) + ", \"name\": \"JsonStableQuery\", \"query_id\": "
			+ std::to_string(stable.q_id.asInt())
			+ ", \"kind\": \"Normal\", \"category\": \"stable\", \"preserved\": true, \"hash\": \""
			+ stable.hash.val.toStringHex() + "\", \"deps\": [" + deps_of_stable + "] }"
		));
		ASSERT_TRUE(json.contains(
			"{ \"index\": " + index_of(unstable) + ", \"name\": \"JsonUnstableQuery\""
		));
		ASSERT_TRUE(
			json.contains("\"kind\": \"SideInput\", \"category\": \"input\", \"preserved\": true")
		);
		ASSERT_TRUE(json.contains("\"category\": \"unstable\", \"preserved\": false"));

		// Same input in a different order gives the same document.
		query::internal::QueryGraph::ReducedGraphData shuffled{
			.nodes     = { input, stable, unstable },
			.adjacency = { {}, { 0, 2 }, {} },
		};
		std::ostringstream out2;
		query::internal::writeReducedGraphAsJson(shuffled, "test_stage", out2);
		ASSERT_EQUAL(json, out2.str());
	}

	/**
	 * @brief An empty graph is still a valid document.
	 */
	void testEmptyGraph() {
		std::ostringstream out;
		query::internal::writeReducedGraphAsJson({}, "empty", out);
		ASSERT_EQUAL(std::string("{\n  \"stage\": \"empty\",\n  \"nodes\": []\n}\n"), out.str());
	}

	/**
	 * @brief The pre-optimization dump holds every node of the current graph, the
	 * post-optimization one drops the unstable leaf.
	 */
	void testPreAndPostOptimization() {
		query::entryPoint<JsonStableQuery>({ 1 });

		const auto pre  = dump(query::external::QueryGraphDumpStage::PreOptimization);
		const auto post = dump(query::external::QueryGraphDumpStage::PostOptimization);

		ASSERT_TRUE(pre.contains("\"stage\": \"pre_optimization\""));
		ASSERT_TRUE(pre.contains("\"name\": \"JsonStableQuery\""));
		ASSERT_TRUE(pre.contains("\"name\": \"JsonSideInput\""));
		ASSERT_TRUE(pre.contains("\"name\": \"JsonUnstableQuery\""));

		ASSERT_TRUE(post.contains("\"stage\": \"post_optimization\""));
		ASSERT_TRUE(post.contains("\"name\": \"JsonStableQuery\""));
		ASSERT_TRUE(post.contains("\"name\": \"JsonSideInput\""));
		ASSERT_TRUE(not post.contains("\"name\": \"JsonUnstableQuery\""));
	}
};

TESTER_COMMON_MAIN("/src/common/query_framework/tests/");
