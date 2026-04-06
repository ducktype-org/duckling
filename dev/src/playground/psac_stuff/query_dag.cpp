#include "concurrent/module_flags/worker_count.hpp"

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
std::vector<u64>                   g_weights;
std::vector<std::vector<usize>>    g_adj;
u64                                g_mod;
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

int readData() {
	usize n{ 0 };
	u64   mod{ 0 };
	if (!(std::cin >> n >> mod)) return 1;
	g_mod = mod;
	g_weights.resize(n);
	for (usize i = 0; i < n; ++i) std::cin >> g_weights[i];

	usize m{ 0 };
	std::cin >> m;
	g_adj.resize(n);
	for (int i = 0; i < m; ++i) {
		usize u{ 0 };
		usize v{ 0 };
		std::cin >> u >> v;
		g_adj[v].push_back(u);
	}

	usize c = 0;
	std::cin >> c;
	std::vector<std::pair<int, u64>> updates(c);
	for (usize i = 0; i < c; ++i) {
		usize x = 0;
		u64   y = 0;
		std::cin >> x >> y;
		g_updates.emplace_back(x, y);
	}
	return 0;
}

int main(int argc, const char* argv[]) {
	init::InitObject _;
	concurrent::worker::setWorkerCount(9);
	query::track_reverse_graph = true;

	std::ios::sync_with_stdio(false);
	std::cin.tie(nullptr);

	if (argc == 1) {
		std::cout << "Usage: " << argv[0] << " [\"init\"/\"calc\"/\"update\"]\n";
		return 1;
	}
	std::string user_value = argv[1];
	if (user_value != "init" && user_value != "calc" && user_value != "update") {
		std::cout << "Usage: " << argv[0] << " [\"init\"/\"calc\"/\"update\"]\n";
		return 1;
	}

	if (readData() != 0) return -1;
	if (user_value == "init") return 0;

	for (usize i{ 0 }; i < g_weights.size(); i++) {
		auto res = query::entryPoint<ComputeNode>(NodeKey{ i });
		std::cout << "Node " << i << " sum = " << res << "\n";
	}

	if (user_value == "calc") return 0;
    
    std::cout << "UPDATE\n";

	if (user_value == "update") {
		std::vector<query::external::InputData> updates;
		for (const auto& [idx, new_val]: g_updates) {
			u64 old_val    = g_weights[idx];
			g_weights[idx] = new_val;

			WeightKey k{ .index = idx, .value = old_val };
			auto      node_id = query::internal::makeNodeID<WeightInput>(k);
			updates.emplace_back(WeightInput::getID(), node_id.hash.val);
		}
		query::external::invalidateQueries({}, updates);
		for (usize i{ 0 }; i < g_weights.size(); i++) {
			auto res = query::entryPoint<ComputeNode>(NodeKey{ i });
			std::cout << "Node " << i << " sum = " << res << "\n";
		}
	}

	return 0;
}
