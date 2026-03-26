#include "common.h"
#include "concurrent/module_flags/worker_count.hpp"

#include "base/types/ints.hpp"

#include "init/init.hpp"
#include "query_framework/context/context.hpp"
#include "query_framework/entry/query_entry_point.hpp"
#include "query_framework/external/api.hpp"
#include "query_framework/input_query/query_input.hpp"
#include "query_framework/input_query/query_input_impl.hpp"
#include "query_framework/internal/query_graph/node_making.hpp"
#include "query_framework/query_int.hpp"
#include "query_framework/standard_query/query_cache_macros.hpp"
#include "query_framework/standard_query/query_impl.hpp"
#include "query_framework/utils/query_hash.hpp"

#include <iostream>
#include <ostream>
#include <vector>

using ValueTp            = u64;
constexpr int ARRAY_SIZE = (1 << 18);
// std::atomic<int> count = 0;

std::vector<ValueTp> g_array;

struct ArrayKey {
	int index;

	bool operator==(const ArrayKey&) const = default;

	[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const {
		return { (u64) index, 0, 0, 0 };
	}
};

DECLARE_QUERY_SIDE_INPUT(ArrayInput, ArrayKey);
IMPLEMENT_QUERY_SIDE_INPUT(ArrayInput);

struct SumKey {
	int  l;
	int  r;
	auto operator<=>(const SumKey&) const = default;

	[[nodiscard]] u64 queryUnstablePerfectHash() const { return ((u64) l << 32) | (u32) r; }
};

DECLARE_QUERY(SumQuery, SumKey, ValueTp, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(SumQuery, ValueTp) {
	static auto provide(query::Context& ctx, QKey key) -> PResult {
		// count++;
		if (key.r - key.l == 1) {
			ctx.query<ArrayInput>(ArrayKey{ key.l });
			return g_array[static_cast<usize>(key.l)];
		}

		int mid = key.l + (key.r - key.l) / 2;

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
	init::InitObject init;
	concurrent::worker::setWorkerCount(1);
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

	g_array.resize(ARRAY_SIZE);

	std::mt19937_64                        rng(0);
	std::uniform_int_distribution<ValueTp> dist(0, 100);
	ValueTp                                simple_sum = 0;
	for (usize i = 0; i < ARRAY_SIZE; ++i) {
		auto val = dist(rng);
		std::cout << base::strConcat("arr[", i, "] = ", val, '\n');
		g_array[i] = val;
		simple_sum += val;
	}

	if (user_value == "calc" || user_value == "update") {
		auto query_sum = query::entryPoint<SumQuery>({ .l = 0, .r = ARRAY_SIZE });

		std::cout << "real_sum = " << simple_sum << '\n';
		std::cout << "query_sum = " << query_sum << '\n';
		// std::cout << "Query count: " << count << '\n';

		if (user_value == "update") {
			ValueTp new_val = 1'000;

			simple_sum += (new_val - g_array[0]);
			g_array[0] = new_val;

			std::vector<query::external::InputData> updates;
			auto node_id = query::internal::makeNodeID<ArrayInput>(ArrayKey{ 0 });
			updates.emplace_back(ArrayInput::getID(), node_id.hash.val);

			query::external::invalidateQueries({}, std::move(updates));
			// count = 0;

			query_sum = query::entryPoint<SumQuery>({ .l = 0, .r = ARRAY_SIZE });

			std::cout << "real_sum = " << simple_sum << '\n';
			std::cout << "query_sum = " << query_sum << '\n';
			// std::cout << "Query count: " << count << '\n';
		}
	}

	return 0;
}
