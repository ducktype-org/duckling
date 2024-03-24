#include "example.hpp"
#include <query_framework/query_impl.hpp>
#include <query_framework/query_entry_point.hpp>

#include <map>
#include <iostream>

/************
 * QUERY 1: *
 ************/


// make this link less bug-prone...:
struct ImplementationOf_Query1: query::QueryImplementation<
	Query1,
	uint64_t
> {
	inline static std::map<QKey, query::AddACD<QResult> > cache{};

	// static auto provide(Context& context, QKey key) -> PResult;

	static auto provide(Context& context, QKey key) -> PResult {
		context.log("Some random log.");
		context.compilationError("Error at query1 -- example error.");
		return key*key;
	}
	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key)) return cache.at(key);
		else return {};
	}
	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({key, {res, acd}});
		return res;
	}
};
QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_Query1, "Query 1");



/************
 * QUERY 2: *
 ************/

struct ImplementationOf_Query2: query::QueryImplementation<
	Query2,
	uint64_t
> {
	inline static std::map<QKey, query::AddACD<QResult> > cache;

	static auto provide(Context& context, QKey key) -> PResult {
		return key + context.query<Query1>(key + 1);
	}
	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key)) return cache.at(key);
		else return {};
	}
	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({key, {res, acd}});
		return res;
	}
};
QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_Query2, "Query 2");

/*****************
 * Cyclic Query: *
 *****************/

struct ImplementationOf_CyclicQuery: query::QueryImplementation<
	CyclicQuery,
	uint64_t
> {
	inline static std::map<QKey, query::AddACD<QResult> > cache;

	static auto provide(Context& context, QKey key) -> PResult {
		return key + context.query<CyclicQuery>((key + 1) % 5);
	}
	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key)) return cache.at(key);
		else return {};
	}
	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({key, {res, acd}});
		return res;
	}
};
QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_CyclicQuery, "Cyclic query");


int main() {

	std::cout << query::queryEntryPoint<Query2>(2) << "\n";
	query::debugPrintDependencyGraph();

	std::cout << query::queryEntryPoint<CyclicQuery>(0) << "\n";

	return 0;
}


