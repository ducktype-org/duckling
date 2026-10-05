// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <diagnostic/placeholder.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/external/api.hpp>
#include <query_framework/input_query/query_input.hpp>
#include <query_framework/input_query/query_input_impl.hpp>
#include <query_framework/internal/query_graph/active_graph.hpp>
#include <query_framework/module_flags/module_flags.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_metadata/declare_metadata.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <tester/tester.hpp>


DECLARE_METADATA(SimpleMeta, u64);

// Those queries are used just to get dummy QueryIDs for NodeID generation.
DECLARE_QUERY(DummyQuery1, query::U64Key, CRef<u64>, ({ .uses_qresult = false }));
DECLARE_QUERY(
	DummyQuery2, query::U64Key, u64, ({ .preserve_in_graph = true, .uses_qresult = false })
);
DECLARE_QUERY(DummyQuery3, query::U64Key, u64, ({ .uses_qresult = false }));

struct KeyOf_SideInput {
	u64 v;

	KeyOf_SideInput(u64 v): v(v) {}

	[[nodiscard]]
	query::QueryStableHash queryStablePerfectHash() const {
		return { v, 0, 0, 0 };
	}
};

DECLARE_QUERY_SIDE_INPUT(SideInput, KeyOf_SideInput);

struct IMPLEMENT_QUERY(DummyQuery1, u64) {
	static auto provide(Context& ctx, QKey key) -> PResult {
		ctx.logInt(makeBox<dia::PlaceholderError>("...", ""));

		if (key.value == 1) {
			ctx.query<DummyQuery2>({ 1 });
			ctx.query<DummyQuery2>({ 2 });
		} else if (key.value == 2) {
			ctx.query<DummyQuery2>({ 2 });
			ctx.query<DummyQuery2>({ 3 });
		} else if (key.value == 3) {
			ctx.query<DummyQuery2>({ 3 });
			ctx.query<DummyQuery2>({ 4 });
		}
		return key.value;
	}

	QUERY_AUTO_CACHE_CREF
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

QUERY_IMPLEMENTATION_BOILERPLATE(DummyQuery3);

IMPLEMENT_QUERY_SIDE_INPUT(SideInput);

/**
 * @brief Utility function to create a NodeID and InputData for a given SideInput query key.
 */
template<typename QueryInterface>
std::tuple<query::internal::NodeID, query::external::InputData> nodesFromSideInput(
	const typename QueryInterface::QKey& key
) {
	query::internal::NodeID    node_id = query::internal::makeNodeID<QueryInterface>(key);
	query::external::InputData input_data(QueryInterface::getID(), node_id.hash.val);
	return { node_id, input_data };
}

class ActiveGraph: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ActiveGraph

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		query::setTrackReverseGraph(true);
		TESTER_ADD_TEST(testInvalidation);
		TESTER_ADD_TEST(testInvalidationOrder);
	}

private:
	/**
	 * @brief Utility function to check if the node with @p node_id has
	 * expected number of dependencies, dependents, metadata and diagnostics.
	 * If the @p expected_deps is 0, it also checks that the node does not exist in the graph
	 * and if the @p expected_dependents is 0, it also checks that the node does not exist in
	 * the graph.
	 */
	void checkNodeStateEqualTo(
		const query::internal::NodeID& node_id,
		usize                          expected_deps,
		usize                          expected_dependents,
		usize                          expected_metadata,
		usize                          expected_diagnostics
	) {
		auto& state = query::Context::getState();
		auto& graph = state.getGraph();

		// 1. Number of dependencies in graph
		if (expected_deps == 0) {
			ASSERT_TRUE(not graph.nodeExists(node_id));
		} else {
			auto deps = graph.getNodeDeps(node_id);
			ASSERT_EQUAL(deps.size(), expected_deps);
		}

		// 2. Number of dependents in graph
		if (expected_dependents == 0) {
			ASSERT_TRUE(not graph.nodeExists(node_id));
		} else {
			auto dependents = graph.getDependentNodes({ node_id });
			ASSERT_EQUAL(dependents.dependents_recursive.size(), expected_dependents);
		}

		// 3. Number of metadata in graph
		auto metadata = state.getMetadata<metadata_SimpleMeta>(node_id);
		ASSERT_EQUAL(metadata.size(), expected_metadata);

		// 4. Number of diagnostics in graph
		auto diagnostics_opt = state.getDiagnosticForNode(node_id);
		if_opt_some(diagnostics_opt, diag) {
			ASSERT_EQUAL(diag->errorCount(), expected_diagnostics);
		}
		if_opt_none(diagnostics_opt) { ASSERT_EQUAL(expected_diagnostics, 0); }
	}

	/**
	 * We need to test if all queries dependent on start nodes are invalidated correctly, meaning
	 * - they are removed from the graph
	 * - their cache is cleared
	 * - their metadata is cleared
	 * - their diagnostics are cleared
	 * - other nodes are not affected
	 * - if an erased node is in the reversed dependency
	 *
	 * Note the graph topology is like lattice with 4 levels
	 * - [DummyQuery1, DummyQuery2, DummyQuery3, SideInput]
	 * and the following dependencies:
	 *
	 *   1_1 depends on (2_1, 2_2)
	 *   1_2 depends on (2_2, 2_3)
	 *   1_3 depends on (2_3, 2_4)
	 *
	 *   2_1 depends on (3_1, 3_2)
	 *   2_2 depends on (3_2, 3_3)
	 *   2_3 depends on (3_3, 3_4)
	 *   2_4 depends on (3_4, 3_5)
	 *
	 *   3_1 depends on Input(1) (4_1)
	 *   3_2 depends on Input(2) (4_2)
	 *   3_3 depends on Input(3) (4_3)
	 *   3_4 depends on Input(4) (4_4)
	 *   3_5 depends on Input(5) (4_5)
	 *
	 *            1_1     1_2    1_3
	 *           /   \   /   \  /   \
	 *         2_1    2_2    2_3    2_4
	 *        /   \  /   \  /   \  /   \
	 *      3_1   3_2    3_3    3_4    3_5
	 *       |     |      |      |      |
	 *      4_1   4_2    4_3    4_4    4_5
	 */
	void testInvalidation() {
		// =============================== Part 1 ===============================
		// We populate the graph with some queries and metadata.

		query::entryPoint<DummyQuery1>({ 1 });
		query::entryPoint<DummyQuery1>({ 2 });
		query::entryPoint<DummyQuery1>({ 3 });

		auto  state = query::internal::ContextAccess::getState();
		auto& graph = state->getGraph();
		ASSERT_EQUAL(graph.getAllNodes().size(), 5 + 5 + 4 + 3);

		auto node_1_1 = query::internal::makeNodeID<DummyQuery1>(query::U64Key{ 1 });
		auto node_1_2 = query::internal::makeNodeID<DummyQuery1>(query::U64Key{ 2 });
		auto node_1_3 = query::internal::makeNodeID<DummyQuery1>(query::U64Key{ 3 });

		auto node_2_1 = query::internal::makeNodeID<DummyQuery2>(query::U64Key{ 1 });
		auto node_2_2 = query::internal::makeNodeID<DummyQuery2>(query::U64Key{ 2 });
		auto node_2_3 = query::internal::makeNodeID<DummyQuery2>(query::U64Key{ 3 });
		auto node_2_4 = query::internal::makeNodeID<DummyQuery2>(query::U64Key{ 4 });

		auto node_3_1 = query::internal::makeNodeID<DummyQuery3>(query::U64Key{ 1 });
		auto node_3_2 = query::internal::makeNodeID<DummyQuery3>(query::U64Key{ 2 });
		auto node_3_3 = query::internal::makeNodeID<DummyQuery3>(query::U64Key{ 3 });
		auto node_3_4 = query::internal::makeNodeID<DummyQuery3>(query::U64Key{ 4 });
		auto node_3_5 = query::internal::makeNodeID<DummyQuery3>(query::U64Key{ 5 });

		checkNodeStateEqualTo(node_1_1, 9, 1, 0, 1);
		checkNodeStateEqualTo(node_1_2, 9, 1, 0, 1);
		checkNodeStateEqualTo(node_1_3, 9, 1, 0, 1);

		checkNodeStateEqualTo(node_2_1, 5, 2, 1, 0);
		checkNodeStateEqualTo(node_2_2, 5, 3, 1, 0);
		checkNodeStateEqualTo(node_2_3, 5, 3, 1, 0);
		checkNodeStateEqualTo(node_2_4, 5, 2, 1, 0);

		checkNodeStateEqualTo(node_3_1, 2, 3, 0, 0);
		checkNodeStateEqualTo(node_3_2, 2, 5, 0, 0);
		checkNodeStateEqualTo(node_3_3, 2, 6, 0, 0);
		checkNodeStateEqualTo(node_3_4, 2, 5, 0, 0);
		checkNodeStateEqualTo(node_3_5, 2, 3, 0, 0);

		ASSERT_EQUAL(ImplementationOf_DummyQuery1::cache.size(), 3);
		ASSERT_EQUAL(ImplementationOf_DummyQuery2::cache.size(), 4);
		ASSERT_EQUAL(ImplementationOf_DummyQuery3::cache.size(), 5);

		auto [node_4_1, input_1] = nodesFromSideInput<SideInput>(KeyOf_SideInput{ 1 });
		auto [node_4_2, input_2] = nodesFromSideInput<SideInput>(KeyOf_SideInput{ 2 });
		auto [node_4_3, input_3] = nodesFromSideInput<SideInput>(KeyOf_SideInput{ 3 });
		auto [node_4_4, input_4] = nodesFromSideInput<SideInput>(KeyOf_SideInput{ 4 });
		auto [node_4_5, input_5] = nodesFromSideInput<SideInput>(KeyOf_SideInput{ 5 });

		// Test dependents from two nodes at the same time.
		auto  dependents     = graph.getDependentNodes({ node_4_1, node_4_2 });
		auto& nodes_to_erase = dependents.dependents_recursive;

		// Sanity checks:
		ASSERT_TRUE(std::ranges::find(nodes_to_erase, node_3_1) != nodes_to_erase.end());
		ASSERT_TRUE(std::ranges::find(nodes_to_erase, node_3_2) != nodes_to_erase.end());
		ASSERT_TRUE(std::ranges::find(nodes_to_erase, node_1_1) != nodes_to_erase.end());
		ASSERT_TRUE(std::ranges::find(nodes_to_erase, node_1_2) != nodes_to_erase.end());

		// =============================== Part 2 ===============================
		// We invalidate the inputs.
		// We check which inputs are not present in the new inputs among the previous inputs
		// and in this case the {input_1, input_2} are missing.
		std::vector<query::external::InputData> invalidated_inputs;
		query::external::invalidateQueries(
			{ input_3, input_4, input_5 }, {}, { &invalidated_inputs }
		);

		ASSERT_TRUE(std::ranges::find(invalidated_inputs, input_1) != invalidated_inputs.end());
		ASSERT_TRUE(std::ranges::find(invalidated_inputs, input_2) != invalidated_inputs.end());
		ASSERT_TRUE(invalidated_inputs.size() == 2);


		ASSERT_EQUAL(graph.getAllNodes().size(), 3 + 3 + 2 + 1);

		checkNodeStateEqualTo(node_1_1, 0, 0, 0, 0);
		checkNodeStateEqualTo(node_1_2, 0, 0, 0, 0);
		checkNodeStateEqualTo(node_1_3, 9, 1, 0, 1);

		checkNodeStateEqualTo(node_2_1, 0, 0, 0, 0);
		checkNodeStateEqualTo(node_2_2, 0, 0, 0, 0);
		checkNodeStateEqualTo(node_2_3, 5, 2, 1, 0);
		checkNodeStateEqualTo(node_2_4, 5, 2, 1, 0);

		checkNodeStateEqualTo(node_3_1, 0, 0, 0, 0);
		checkNodeStateEqualTo(node_3_2, 0, 0, 0, 0);
		checkNodeStateEqualTo(node_3_3, 2, 3, 0, 0);
		checkNodeStateEqualTo(node_3_4, 2, 4, 0, 0);
		checkNodeStateEqualTo(node_3_5, 2, 3, 0, 0);

		ASSERT_EQUAL(ImplementationOf_DummyQuery1::cache.size(), 1);
		ASSERT_EQUAL(ImplementationOf_DummyQuery2::cache.size(), 2);
		ASSERT_EQUAL(ImplementationOf_DummyQuery3::cache.size(), 3);

		query::entryPoint<DummyQuery1>({ 1 });
		query::entryPoint<DummyQuery1>({ 2 });
		query::entryPoint<DummyQuery1>({ 3 });

		ASSERT_EQUAL(graph.getAllNodes().size(), 5 + 5 + 4 + 3);

		// =============================== Part 3 ===============================
		// This should invalidate the missing inputs from the selected previous inputs.
		// Here the `input_1` is missing from new inputs compared to the previous selected inputs
		// and all its dependents should be invalidated.

		invalidated_inputs.clear();

		query::external::invalidateQueries(
			{ input_2 }, { { input_1, input_2 } }, { &invalidated_inputs }
		);

		ASSERT_TRUE(std::ranges::find(invalidated_inputs, input_1) != invalidated_inputs.end());
		ASSERT_TRUE(invalidated_inputs.size() == 1);

		ASSERT_EQUAL(graph.getAllNodes().size(), 4 + 4 + 3 + 2);

		checkNodeStateEqualTo(node_1_1, 0, 0, 0, 0);
		checkNodeStateEqualTo(node_1_2, 9, 1, 0, 1);
		checkNodeStateEqualTo(node_1_3, 9, 1, 0, 1);

		checkNodeStateEqualTo(node_2_1, 0, 0, 0, 0);
		checkNodeStateEqualTo(node_2_2, 5, 2, 1, 0);
		checkNodeStateEqualTo(node_2_3, 5, 3, 1, 0);
		checkNodeStateEqualTo(node_2_4, 5, 2, 1, 0);

		checkNodeStateEqualTo(node_3_1, 0, 0, 0, 0);
		checkNodeStateEqualTo(node_3_2, 2, 3, 0, 0);
		checkNodeStateEqualTo(node_3_3, 2, 5, 0, 0);
		checkNodeStateEqualTo(node_3_4, 2, 5, 0, 0);
		checkNodeStateEqualTo(node_3_5, 2, 3, 0, 0);

		ASSERT_EQUAL(ImplementationOf_DummyQuery1::cache.size(), 2);
		ASSERT_EQUAL(ImplementationOf_DummyQuery2::cache.size(), 3);
		ASSERT_EQUAL(ImplementationOf_DummyQuery3::cache.size(), 4);

		// No new inputs, meaning invalidate all queries.
		query::external::invalidateQueries({});
	}

	/* Here we test the topological order of the invalidation.
	 *    1_2  (entry query)
	 *   /   \
	 * 2_2   2_3
	 *   \  /
	 *   3_3
	 */
	void testInvalidationOrder() {
		query::entryPoint<DummyQuery1>({ 2 });
		std::unordered_map<query::internal::NodeID, std::string> stringified;

		auto make_node = [&]<typename QueryType>(u64 key, const std::string& name) {
			auto node = query::internal::makeNodeID<QueryType>(query::U64Key{ key });
			stringified.emplace(node, name);
			return node;
		};
		auto node_1_2 = make_node.operator()<DummyQuery1>(2, "1_2");
		auto node_2_2 = make_node.operator()<DummyQuery2>(2, "2_2");
		auto node_2_3 = make_node.operator()<DummyQuery2>(3, "2_3");
		auto node_3_3 = make_node.operator()<DummyQuery3>(3, "3_3");

		auto& state      = query::Context::getState();
		auto& graph      = state.getGraph();
		auto  dependents = graph.getDependentNodes({ node_3_3 }).dependents_recursive;
		ASSERT_EQUAL_PRINT(dependents.size(), 4);
		std::cerr << stringified.at(dependents[0]) << '\n';
		std::cerr << stringified.at(dependents[1]) << '\n';
		std::cerr << stringified.at(dependents[2]) << '\n';
		std::cerr << stringified.at(dependents[3]) << '\n';
		ASSERT_EQUAL(dependents[0], node_1_2);
		ASSERT_EQUAL(dependents[1], node_2_3);
		ASSERT_EQUAL(dependents[2], node_2_2);
		ASSERT_EQUAL(dependents[3], node_3_3);
	}
};

TESTER_COMMON_MAIN("/src/common/query_framework/tests/");
