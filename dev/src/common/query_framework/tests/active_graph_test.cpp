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

class ActiveGraph: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ActiveGraph

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testActiveGraph);
		TESTER_ADD_TEST(testEdgeCases);
	}

private:
	/**
	 * Helper to generate a set of dummy NodeIDs for testing.
	 * @note These nodes will not correspond to any actual query operations.
	 */
	std::vector<query::internal::NodeID> generateNodeIDs(u64 node_count) {
		CORE_ASSERT(node_count % 2 == 0, "We rely on that later");

		const auto q_id_1 = DummyQuery1::getID();
		const auto q_id_2 = DummyQuery2::getID();

		std::vector<query::internal::NodeID> node_ids_fixtures;

		// Fill half with q_id_1 and half with q_id_2

		for (u64 q_hash = 0; q_hash < node_count / 2; ++q_hash) {
			query::internal::NodeID node_id{ q_id_1, query::internal::KeyHash{ q_hash } };
			node_ids_fixtures.push_back(node_id);
		}

		for (u64 q_hash = 0; q_hash < node_count / 2; ++q_hash) {
			query::internal::NodeID node_id{ q_id_2, query::internal::KeyHash{ q_hash } };
			node_ids_fixtures.push_back(node_id);
		}

		// shuffle the node ids:
		std::shuffle(
			node_ids_fixtures.begin(),
			node_ids_fixtures.end(),
			std::mt19937{ std::random_device{}() }
		);

		ASSERT_TRUE(node_ids_fixtures.size() == node_count);  // Sanity check

		return node_ids_fixtures;
	}

	void testActiveGraph() {
		// Note that when panic/assertion fails inside a thread,
		// std::terminate will be called (as per cpp reference),
		// which should fail the test.

		constexpr u64 TEST_NODE_COUNT = 100;
		constexpr u64 THREAD_COUNT    = 4;

		static_assert(
			TEST_NODE_COUNT % THREAD_COUNT == 0,
			"TEST_NODE_COUNT must be divisible by THREAD_COUNT, we rely on that later."
		);

		auto node_ids_fixtures = generateNodeIDs(TEST_NODE_COUNT);
		auto context_fixture = query::internal::ContextAccess::makeShared(node_ids_fixtures.at(0));

		// We spawn THREAD_COUNT threads that will concurrently create a full cycle

		std::vector<std::jthread> threads;
		threads.reserve(THREAD_COUNT);

		std::atomic<u64> cycle_detection_count = 0;

		query::internal::ActiveGraph active_graph;

		for (u64 t_id = 0; t_id < THREAD_COUNT; ++t_id) {
			auto range_begin = (node_ids_fixtures.size() / THREAD_COUNT) * t_id;
			auto range_end   = (node_ids_fixtures.size() / THREAD_COUNT) * (t_id + 1);

			threads.emplace_back([&, range_begin, range_end]() {
				for (u64 i = range_begin; i < range_end; ++i) {
					const auto& node_id = node_ids_fixtures[i];
					active_graph.putNode(node_id, context_fixture);
					active_graph.setEdge(
						node_id, node_ids_fixtures[(i + 1) % node_ids_fixtures.size()]
					);
					auto was_cycle = active_graph.cycleCheck(node_id);
					if (was_cycle.has_value()) {
						cycle_detection_count.fetch_add(1, std::memory_order_relaxed);
						ASSERT_TRUE(was_cycle->cycle_nodes.size() == TEST_NODE_COUNT);
					}
				}
			});
		}

		// join threads:
		for (auto& th: threads) th.join();

		const auto found_cycles = cycle_detection_count.load();
		ASSERT_TRUE(found_cycles >= 1 and found_cycles <= THREAD_COUNT);
		ASSERT_TRUE(active_graph.size() == TEST_NODE_COUNT);
		message(base::strConcat("Found cycles: ", found_cycles));

		// now remove all nodes, again concurrently:
		threads.clear();
		for (u64 t_id = 0; t_id < THREAD_COUNT; ++t_id) {
			auto range_begin = (node_ids_fixtures.size() / THREAD_COUNT) * t_id;
			auto range_end   = (node_ids_fixtures.size() / THREAD_COUNT) * (t_id + 1);
			threads.emplace_back([&, range_begin, range_end]() {
				for (u64 i = range_begin; i < range_end; ++i) {
					const auto& node_id = node_ids_fixtures[i];
					active_graph.removeEdge(node_id);
					active_graph.removeNode(node_id);
				}
			});
		}

		// join threads again:
		for (auto& th: threads) th.join();

		ASSERT_TRUE(active_graph.size() == 0);

		// message is here to make sure that if something strange with threads happens,
		// and we exit before reaching this point, we will see it in the test logs.
		message("ActiveGraph test passed.");
	}

	void testEdgeCases() {
		query::internal::ActiveGraph active_graph;

		auto node_ids_fixtures = generateNodeIDs(2);
		auto context_fixture = query::internal::ContextAccess::makeShared(node_ids_fixtures.at(0));

		// empty graph sanity checks:
		auto was_cycle_empty = active_graph.cycleCheck(node_ids_fixtures.at(0));
		ASSERT_TRUE(was_cycle_empty.empty());

		// edge to itself:
		active_graph.putNode(node_ids_fixtures.at(0), context_fixture);
		active_graph.setEdge(node_ids_fixtures.at(0), node_ids_fixtures.at(0));
		auto was_cycle_loop = active_graph.cycleCheck(node_ids_fixtures.at(0));
		ASSERT_HAS_VALUE(was_cycle_loop);
		ASSERT_TRUE(was_cycle_loop.value().cycle_nodes.size() == 1);
		ASSERT_TRUE(was_cycle_loop.value().cycle_nodes.at(0).node_id == node_ids_fixtures.at(0));

		active_graph.putNode(node_ids_fixtures.at(1), context_fixture);
		active_graph.setEdge(node_ids_fixtures.at(1), node_ids_fixtures.at(0));
		auto was_cycle_loop_indirect = active_graph.cycleCheck(node_ids_fixtures.at(1));
		ASSERT_TRUE(was_cycle_loop_indirect.empty());
	}
};

TESTER_COMMON_MAIN("/src/common/query_framework/tests/");
