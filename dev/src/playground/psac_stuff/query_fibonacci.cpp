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
#include <string>


using ValueTp = u64;
int g_n_input;

struct FibInputKey {
	bool operator==(const FibInputKey&) const = default;

	[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const { return { 0, 0, 0, 0 }; }
};

DECLARE_QUERY_SIDE_INPUT(FibInput, FibInputKey);
IMPLEMENT_QUERY_SIDE_INPUT(FibInput);

struct FibKey {
	u64  n;
	bool operator==(const FibKey&) const = default;

	[[nodiscard]] query::QueryStableHash queryStablePerfectHash() const {
		return { (u64) n, 0, 0, 0 };
	}

	[[nodiscard]] u64 queryUnstablePerfectHash() const { return n; }
};

DECLARE_QUERY(FibQuery, FibKey, ValueTp, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(FibQuery, ValueTp) {
	static auto provide(query::Context& ctx, QKey key) -> PResult {
		if (key.n < 2) return key.n;

		auto fib1_handle = ctx.schedule<FibQuery>({ .n = key.n - 1 });
		auto fib2_handle = ctx.schedule<FibQuery>({ .n = key.n - 2 });

		auto res1 = ctx.await<FibQuery>(fib1_handle);
		auto res2 = ctx.await<FibQuery>(fib2_handle);

		return res1 + res2;
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(FibQuery);

struct MainFibKey {
	u64  n;
	bool operator==(const MainFibKey&) const = default;

	[[nodiscard]] query::QueryStableHash queryUnstablePerfectHash() const {
		return { (u64) n, 0, 0, 0 };
	}
};

DECLARE_QUERY(MainFibQuery, MainFibKey, ValueTp, ({ .uses_qresult = false }));

struct IMPLEMENT_QUERY(MainFibQuery, ValueTp) {
	static auto provide(query::Context& ctx, QKey key) -> PResult {
		return ctx.query<FibQuery>({ static_cast<u64>(key.n) });
	}

	QUERY_AUTO_CACHE_COPY
};

QUERY_IMPLEMENTATION_BOILERPLATE(MainFibQuery);

int main(int argc, const char* argv[]) {
	init::InitObject init;
	concurrent::worker::setWorkerCount(16);
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

	const int n_initial = 30;
	g_n_input           = n_initial;


	if (user_value == "calc" || user_value == "update") {
		auto result = query::entryPoint<MainFibQuery>(MainFibKey{ .n = 10 });
		std::cout << "Result of fib: " << result << '\n';
		if (user_value == "update") {
			const int n_new = g_n_input + 1;
			g_n_input       = n_new;
			std::vector<query::external::InputData> updates;
			auto node_id = query::internal::makeNodeID<FibInput>(FibInputKey{});
			updates.emplace_back(FibInput::getID(), node_id.hash.val);
			query::external::invalidateQueries({}, std::move(updates));

			result = query::entryPoint<MainFibQuery>(MainFibKey{ .n = 31 });
			std::cout << "Result of sum after propagation is: " << result << '\n';
		}
	}
	return 0;
}
