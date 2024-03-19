#include "example.hpp"
#include <query_framework/query_impl.hpp>

#include <map>

// make this link less bug-prone...:
struct Query1Impl: query::QueryImplementation<
	Query1,
	uint64_t
> {
	static std::map<QKey, QResult> cache;

	static auto provide(Context& context, QKey key) -> PResult {
		return 2;
	}
	static auto load(QKey key) -> base::Optional<QResult> {
		if (cache.contains(key)) return cache.at(key);
		else return {};
	}
	static auto store(QKey key, PResult res) -> QResult {
		cache.insert({key, res});
		return res;
	}
};
IMPLEMENT_QUERY_OF(Query1Impl)

std::map<Query1Impl::QKey, Query1Impl::QResult> Query1Impl::cache{};



