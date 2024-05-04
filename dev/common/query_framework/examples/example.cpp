#include "example.hpp"
#include <query_framework/query_impl.hpp>
#include <query_framework/query_entry_point.hpp>

#include <map>
#include <iostream>

/************
 * QUERY 1: *
 ************/


// make this link less bug-prone...:
struct IMPLEMENT_QUERY(Query1, uint64_t) {
	inline static std::map<QKey, query::CacheEntry<QResult>> cache{};

	// static auto provide(Context& context, QKey key) -> PResult;

	static auto provide(Context& context, QKey key) -> PResult {
		context.log("Some random log.");
		context.compilationError("Error at query1 -- example error.");
		return context.callExt<SquareValue>(key);
	}

	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}

	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { res, acd } });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(Query1);

/************
 * QUERY 2: *
 ************/

struct IMPLEMENT_QUERY(Query2, uint64_t) {
	inline static std::map<QKey, query::CacheEntry<QResult>> cache;

	static auto provide(Context& context, QKey key) -> PResult {
		return key + context.query<Query1>(key + 1);
	}

	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}

	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { res, acd } });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(Query2);

/*****************
 * Cyclic Query: *
 *****************/

struct IMPLEMENT_QUERY(CyclicQuery, uint64_t) {
	inline static std::map<QKey, query::CacheEntry<QResult>> cache;

	static auto provide(Context& context, QKey key) -> PResult {
		return key + context.query<CyclicQuery>((key + 1) % 5);
	}

	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}

	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { res, acd } });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(CyclicQuery);

// implement extension:
uint64_t SquareValue(query::Context&, uint64_t v) { return v * v; }

int main() {
	std::cout << query::entryPoint<Query2>(2) << "\n";
	query::debugPrintDependencyGraph();

	std::cout << query::entryPoint<CyclicQuery>(0) << "\n";

	return 0;
}
