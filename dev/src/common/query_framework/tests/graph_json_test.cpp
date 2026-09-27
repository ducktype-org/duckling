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
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <span>
#include <sstream>
#include <stdexcept>
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
		TESTER_ADD_TEST(testDummyNode);
		TESTER_ADD_TEST(testDumpToDirectory);
		TESTER_ADD_TEST(testDumpToDirectoryErrors);
		TESTER_ADD_TEST(testReducedGraphRoundTrip);
		TESTER_ADD_TEST(testDeserializeRejectsBrokenData);
	}

private:
	static std::string dump(query::external::QueryGraphDumpStage stage) {
		std::ostringstream out;
		query::external::dumpQueryGraphAsJson(stage, out);
		return out.str();
	}

	static std::string readFile(const std::filesystem::path& path) {
		std::ifstream      in(path);
		std::ostringstream content;
		content << in.rdbuf();
		return content.str();
	}

	/**
	 * @brief A fresh, not yet existing directory under the system temp directory.
	 */
	static std::filesystem::path uniqueTempPath() {
		return std::filesystem::temp_directory_path()
		     / ("graph_json_test_" + std::to_string(std::random_device{}()));
	}

	/**
	 * @brief Three nodes (stable -> side input, stable -> unstable), not in NodeID order.
	 */
	static query::internal::QueryGraph::ReducedGraphData makeSmallGraph() {
		return {
			.nodes     = { query::internal::makeNodeID<JsonStableQuery>(KeyOf_JsonStable{ 5 }),
			               query::internal::makeNodeID<JsonUnstableQuery>(query::U64Key{ 5 }),
			               query::internal::makeNodeID<JsonSideInput>(KeyOf_JsonSideInput{ 5 }) },
			.adjacency = { { 2, 1 }, {}, {} },
		};
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

	/**
	 * @brief A node remapped to a dummy while loading a previous graph is reported as such.
	 */
	void testDummyNode() {
		query::entryPoint<JsonStableQuery>({ 1 });

		const auto unstable = query::internal::makeNodeID<JsonUnstableQuery>(query::U64Key{ 9 });
		auto       state    = query::internal::ContextAccess::getState();
		const auto dummy    = state->remapUnstableOrUnregisteredNodes(unstable);

		std::ostringstream out;
		query::internal::writeReducedGraphAsJson(
			{ .nodes = { dummy }, .adjacency = { {} } }, "dummy", out
		);
		ASSERT_TRUE(out.str().contains(
			"\"kind\": \"Dummy\", \"category\": \"unstable\", \"preserved\": false"
		));
	}

	/**
	 * @brief Both stages land in their own file, and the directory is created on the way.
	 */
	void testDumpToDirectory() {
		query::entryPoint<JsonStableQuery>({ 1 });

		const auto dir    = uniqueTempPath() / "nested";
		const auto result = query::external::dumpQueryGraphsToDirectory(dir);
		ASSERT_TRUE(result.has_value());
		ASSERT_EQUAL(usize{ 2 }, result->size());
		ASSERT_EQUAL(dir / "query_graph_pre_opt.json", result->at(0));
		ASSERT_EQUAL(dir / "query_graph_post_opt.json", result->at(1));

		const auto pre  = readFile(result->at(0));
		const auto post = readFile(result->at(1));
		ASSERT_EQUAL(dump(query::external::QueryGraphDumpStage::PreOptimization), pre);
		ASSERT_EQUAL(dump(query::external::QueryGraphDumpStage::PostOptimization), post);
		ASSERT_TRUE(pre.contains("\"name\": \"JsonUnstableQuery\""));
		ASSERT_TRUE(not post.contains("\"name\": \"JsonUnstableQuery\""));

		std::filesystem::remove_all(dir.parent_path());
	}

	/**
	 * @brief A directory that cannot be created, or a file that cannot be written, is an error
	 * naming the offending path instead of a silent partial dump.
	 */
	void testDumpToDirectoryErrors() {
		query::entryPoint<JsonStableQuery>({ 1 });

		const auto root = uniqueTempPath();
		std::filesystem::create_directories(root);

		// The output "directory" is an existing regular file.
		const auto not_a_dir = root / "file";
		std::ofstream(not_a_dir) << "x";
		const auto dir_error = query::external::dumpQueryGraphsToDirectory(not_a_dir);
		ASSERT_TRUE(not dir_error.has_value());
		ASSERT_TRUE(dir_error.error().contains("cannot create directory"));
		ASSERT_TRUE(dir_error.error().contains(not_a_dir.string()));

		// A directory sits where the first dump file should go.
		const auto blocked = root / "blocked";
		std::filesystem::create_directories(blocked / "query_graph_pre_opt.json");
		const auto write_error = query::external::dumpQueryGraphsToDirectory(blocked);
		ASSERT_TRUE(not write_error.has_value());
		ASSERT_TRUE(write_error.error().contains("cannot write query graph"));
		ASSERT_TRUE(write_error.error().contains("query_graph_pre_opt.json"));
		ASSERT_TRUE(not std::filesystem::exists(blocked / "query_graph_post_opt.json"));

		std::filesystem::remove_all(root);
	}

	/**
	 * @brief toReducedGraphData() and serializeReducedGraph() round-trip through deserialize().
	 */
	void testReducedGraphRoundTrip() {
		query::entryPoint<JsonStableQuery>({ 1 });

		const auto data  = makeSmallGraph();
		const auto bytes = query::internal::QueryGraph::serializeReducedGraph(data);
		const auto graph = query::internal::QueryGraph::deserialize(bytes);

		ASSERT_TRUE(graph.hasDependencies(data.nodes[0]));
		ASSERT_TRUE(not graph.hasDependencies(data.nodes[1]));
		ASSERT_TRUE(not graph.hasDependencies(
			query::internal::makeNodeID<JsonUnstableQuery>(query::U64Key{ 1'234 })
		));
		ASSERT_EQUAL(usize{ 2 }, graph.getDirectDependencies(data.nodes[0]).size());

		// Serializing the deserialized graph again gives the same JSON document.
		std::ostringstream original;
		std::ostringstream round_tripped;
		query::internal::writeReducedGraphAsJson(data, "s", original);
		query::internal::writeReducedGraphAsJson(graph.toReducedGraphData(), "s", round_tripped);
		ASSERT_EQUAL(original.str(), round_tripped.str());
	}

	/**
	 * @brief A truncated buffer or a dependency index past the node list throws instead of
	 * reading out of bounds.
	 */
	void testDeserializeRejectsBrokenData() {
		query::entryPoint<JsonStableQuery>({ 1 });

		auto throws_out_of_range = [](std::span<const byte> bytes) {
			try {
				(void) query::internal::QueryGraph::deserialize(bytes);
			} catch (const std::out_of_range&) { return true; }
			return false;
		};

		auto truncated = query::internal::QueryGraph::serializeReducedGraph(makeSmallGraph());
		truncated.pop_back();
		ASSERT_TRUE(throws_out_of_range(truncated));

		// The last usize of the buffer is the only dependency index of the last node.
		auto data             = makeSmallGraph();
		data.adjacency        = { {}, {}, { 0 } };
		auto        bad_index = query::internal::QueryGraph::serializeReducedGraph(data);
		const usize too_high  = 7;
		std::memcpy(bad_index.data() + bad_index.size() - sizeof(usize), &too_high, sizeof(usize));
		ASSERT_TRUE(throws_out_of_range(bad_index));
	}
};

TESTER_COMMON_MAIN("/src/common/query_framework/tests/");
