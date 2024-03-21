#include "example.hpp"
#include <query_framework/query_impl.hpp>
#include <query_framework/query_entry_point.hpp>

#include <map>
#include <iostream>

/************
 * QUERY 1: *
 ************/


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
decltype(Query1Impl::cache) Query1Impl::cache{};



/************
 * QUERY 2: *
 ************/

struct Query2Impl: query::QueryImplementation<
	Query2,
	uint64_t
> {
	static std::map<QKey, AddACD<QResult> > cache;

	static auto provide(Context& context, QKey key) -> PResult {
		return key + context.query<Query1>(key + 1);
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
IMPLEMENT_QUERY_OF(Query2Impl, "Query 2");
decltype(Query2Impl::cache) Query2Impl::cache{};

/*****************
 * Cyclic Query: *
 *****************/

struct CyclicQueryImpl: query::QueryImplementation<
	CyclicQuery,
	uint64_t
> {
	static std::map<QKey, AddACD<QResult> > cache;

	static auto provide(Context& context, QKey key) -> PResult {
		return key + context.query<CyclicQuery>((key + 1) % 5);
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
IMPLEMENT_QUERY_OF(CyclicQueryImpl, "Cyclic query");
decltype(CyclicQueryImpl::cache) CyclicQueryImpl::cache{};



int main() {

	std::cout << queryEntryPoint<Query2>(2) << "\n";
	query::dep_graph::debugPrint();

	std::cout << queryEntryPoint<CyclicQuery>(0) << "\n";

	return 0;
}


