#include <query_framework/query_impl.hpp>
#include "decl.hpp" // query declaration

#include <base/str_concat.hpp> // base::strConcat
#include <base/optional.hpp> // base::Optional
#include <base/maps.hpp> // base::Map


/**
 * PResult type for MyQuery
 */
struct PResult {
	uint64_t v;
};

struct ImplementationOf_MyQuery: query::QueryImplementation<
    MyQuery,
    PResult
> {
	/**
	 * Lets define some cache:
	 */
	inline static base::Map<QKey, query::AddACD<QResult> > cache;

	static auto provide(Context& context, QKey key) -> PResult {
		// lets call Query2:
		auto result = context.query<Query2>(123);

		// Normally we would do it because we need
		// it in some computation.
		// Here we will just log it:
		context.log(base::strConcat("Result of query 2 : "));

		// some trivial implementation:
		return PResult{key.v};
	}
	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key)) {
			return cache.at(key);
		}
		else {
			return {};
		}
	}
	static auto store(QKey key, PResult q_res, query::ACD acd) -> QResult {
		QResult res = { q_res.v };
		cache.put(key, res);
		return res;
	}
};
QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_MyQuery, "MyQuery");


/**
 * Lets also implement Query2 as a very simple query without any caching:
 */

struct ImplementationOf_Query2: query::QueryImplementation<
    Query2,
    std::string
> {
	static auto provide(Context& context, QKey key) -> PResult {
		return std::to_string(key);
	}
	static auto load(QKey key) -> LoadResult {
		return {};
	}
	static auto store(QKey key, PResult q_res, query::ACD acd) -> QResult {
		RIFT_PANIC("Call to store on cache-less query!");
	}
};
QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_Query2, "Query 2");