#include "array_sum_common.hpp"
#include "concurrent/module_flags/worker_count.hpp"

#include "base/types/ints.hpp"

#include "init/init.hpp"
#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/external/api.hpp>
#include <query_framework/input_query/query_input.hpp>
#include <query_framework/input_query/query_input_impl.hpp>
#include <query_framework/internal/query_graph/node_making.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <query_framework/utils/query_hash.hpp>

#include <iostream>
#include <ostream>
#include <vector>

struct ArrayKey {
	usize index;

	bool operator==(const ArrayKey&) const = default;

	[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const {
		return { (u64) index, 0, 0, 0 };
	}
};

DECLARE_QUERY_SIDE_INPUT(ArrayInput, ArrayKey);
IMPLEMENT_QUERY_SIDE_INPUT(ArrayInput);

struct SumKey {
	usize l, r;
	auto  operator<=>(const SumKey&) const = default;

	[[nodiscard]] u64 queryUnstablePerfectHash() const { return ((u64) l << 32) | (u32) r; }
};

DECLARE_QUERY(SumQuery, SumKey, ValueTp, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(SumQuery, ValueTp) {
	static auto provide(query::Context& ctx, QKey key) -> PResult {
		work(g_config.workload);
		if (key.r - key.l == 1) {
			ctx.query<ArrayInput>(ArrayKey{ key.l });
			return g_source_array[key.l];
		}

		usize mid = key.l + (key.r - key.l) / 2;

		auto left_handle  = ctx.schedule<SumQuery>({ .l = key.l, .r = mid });
		auto right_handle = ctx.schedule<SumQuery>({ .l = mid, .r = key.r });
		auto left_sum     = ctx.await<SumQuery>(left_handle);
		auto right_sum    = ctx.await<SumQuery>(right_handle);

		return left_sum + right_sum;
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(SumQuery);

int main(int argc, const char* argv[]) {
	init::InitObject _;
	if (!parseArgs(argc, argv)) {
		printUsage(argv[0]);
		return 1;
	}

	concurrent::worker::setWorkerCount(g_config.threads);
	query::setTrackReverseGraph(true);
	std::ios::sync_with_stdio(false);
	std::cin.tie(nullptr);

	generateArray();
	// TODO: Remove this from here.
	ValueTp simple_sum = 0;
	for (ValueTp x: g_source_array) simple_sum += x;

	auto q_sum = query::entryPoint<SumQuery>({ .l = 0, .r = g_config.array_size });
	std::cout << "Initial real_sum = " << simple_sum << " query_sum = " << q_sum << '\n';

	if (g_config.command == "update") {
		const usize num_updates = g_config.array_size * g_config.update_percent / 100;
		std::cout << "--- Updating " << num_updates << " elements (" << g_config.update_percent
				  << "% -> " << num_updates << "/" << g_config.array_size << ") ---\n";

		std::vector<query::external::InputData> updates;
		for (usize i = 0; i < num_updates; ++i) {
			usize idx = i % g_config.array_size;
			g_source_array[idx]++;
			simple_sum += 1;
			auto node_id = query::internal::makeNodeID<ArrayInput>(ArrayKey{ idx });
			updates.emplace_back(ArrayInput::getID(), node_id.hash.val);
		}
		query::external::invalidateQueries({}, std::move(updates));
		q_sum = query::entryPoint<SumQuery>({ .l = 0, .r = g_config.array_size });
		std::cout << "After update real_sum = " << simple_sum << " query_sum = " << q_sum << '\n';
	}

	return 0;
}
