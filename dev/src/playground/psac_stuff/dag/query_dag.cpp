#include "concurrent/base/collections/queue.hpp"
#include "dag_common.hpp"

#include "base/misc/int_conv.hpp"
#include "base/types/ints.hpp"

#include "init/init.hpp"
#include "query_framework/context/context.hpp"
#include "query_framework/entry/query_entry_point.hpp"
#include "query_framework/external/api.hpp"
#include "query_framework/input_query/query_input.hpp"
#include "query_framework/input_query/query_input_impl.hpp"
#include "query_framework/internal/query_graph/node_making.hpp"
#include "query_framework/internal/task_pool/task_pool.hpp"
#include "query_framework/module_flags/module_flags.hpp"
#include "query_framework/query_int.hpp"
#include "query_framework/standard_query/query_cache_macros.hpp"
#include "query_framework/standard_query/query_impl.hpp"
#include "query_framework/utils/query_hash.hpp"

#include <iostream>
#include <vector>

// ============= GLOBAL STUFF =============
std::atomic<usize>                 g_invocations{ 0 };
std::vector<std::pair<usize, u64>> g_updates;

// ================ INPUT ================
struct WeightKey {
	usize index;
	u64   value;

	[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const {
		return { (u64) index, value, 0, 0 };
	}
};

DECLARE_QUERY_SIDE_INPUT(WeightInput, WeightKey);
IMPLEMENT_QUERY_SIDE_INPUT(WeightInput);

// ================ QUERY ================

struct NodeKey {
	usize          index;
	constexpr auto operator<=>(const NodeKey&) const = default;

	[[nodiscard]] u64 queryUnstablePerfectHash() const { return static_cast<u64>(index); }
};

DECLARE_QUERY(ComputeNode, NodeKey, u64, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(ComputeNode, u64) {
	static auto provide(query::Context& ctx, QKey key) -> PResult {
		g_invocations++;

		// Simulate workload.
		work(g_config.workload);

		usize node_idx = key.index;
		u64   weight   = g_weights[node_idx];
		ctx.query<WeightInput>(WeightKey{ .index = node_idx, .value = weight });

		u64 sum = weight;

		std::vector<query::internal::TaskHandle> handles;
		for (usize pred: g_adj[node_idx])
			handles.push_back(ctx.schedule<ComputeNode>(NodeKey{ pred }));

		for (auto& handle: handles) {
			u64 pred_val = ctx.await<ComputeNode>(handle);
			sum          = (sum + pred_val) % g_mod;
		}

		return sum;
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(ComputeNode);

void runComputation() {
	std::cout << "--- Initial Calculation ---\n";
	g_invocations = 0;
	std::vector<u64> results;
	results.reserve(g_weights.size());
	for (usize i = 0; i < g_weights.size(); i++)
		results.push_back(query::entryPoint<ComputeNode>(NodeKey{ i }));
	std::cout << "Total query invocations: " << g_invocations << "\n";
	printFinalChecksum("Query Initial", results);
}

void runUpdateAndRecompute() {
	std::cout << "--- Updating and Recomputing ---\n";

	std::vector<query::external::InputData> updates;
	std::mt19937_64                         rng(6'767);
	const usize num_updates = g_weights.size() * g_config.update_percent / 100;

	for (usize i = 0; i < num_updates; ++i) {
		usize idx      = rng() % g_weights.size();
		u64   old_val  = g_weights[idx];
		g_weights[idx] = (old_val + 1) % g_mod;
		auto node_id
			= query::internal::makeNodeID<WeightInput>(WeightKey{ .index = idx, .value = old_val });
		updates.emplace_back(WeightInput::getID(), node_id.hash.val);
	}
	query::external::invalidateQueries({}, std::move(updates));

	g_invocations = 0;
	std::vector<u64> results;
	results.reserve(g_weights.size());
	for (usize i = 0; i < g_weights.size(); i++)
		results.push_back(query::entryPoint<ComputeNode>(NodeKey{ i }));
	std::cout << "Total invocations after update: " << g_invocations << "\n";
	printFinalChecksum("Query Recompute", results);
}

int main(int argc, const char* argv[]) {
	init::InitObject _;

	if (!parseArgs(argc, argv)) {
		printUsage(argv[0]);
		return 1;
	}

	concurrent::worker::setWorkerCount(g_config.threads);
	query::track_reverse_graph = true;
	std::ios::sync_with_stdio(false);
	std::cin.tie(nullptr);

	generateDag(g_config.layers, g_config.verts_per_layer, g_config.total_edges, g_config.mod);

	if (g_config.command == "calc" || g_config.command == "update") {
		runComputation();
		if (g_config.command == "update") runUpdateAndRecompute();
	}

	return 0;
}
