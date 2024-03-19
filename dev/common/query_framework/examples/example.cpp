#include "example.hpp"
#include <query_framework/query_impl.hpp>
#include <query_framework/query_entry_point.hpp>

#include <map>
#include <iostream>

// make this link less bug-prone...:
struct Query1Impl: query::QueryImplementation<
	Query1,
	uint64_t
> {
	static std::map<QKey, AddACD<QResult> > cache;

	static auto provide(Context& context, QKey key) -> PResult {
		return key*key;
	}
	static auto load(QKey key) -> LoadRes {
		if (cache.contains(key)) return cache.at(key);
		else return {};
	}
	static auto store(QKey key, PResult res, ACD acd) -> QResult {
		cache.insert({key, {res, acd}});
		return res;
	}
};
IMPLEMENT_QUERY_OF(Query1Impl, "Query 1");

decltype(Query1Impl::cache) Query1Impl::cache;



int main() {

	std::cout << queryEntryPoint<Query1>(2) << "\n";

	return 0;
}


