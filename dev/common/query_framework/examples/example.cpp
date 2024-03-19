#include "example.hpp"
#include <query_framework/query_impl.hpp>



// make this link less bug-prone...:
struct Query1Impl: query::QueryImplementation<
	Query1,
	uint64_t
> {
	static auto provider(QKey, Context&) -> PResult {
		
	}
	static auto load(QKey key) -> base::Optional<QResult> {
		
	}
	static auto store(QKey key, PResult) -> QResult {

	}
};
IMPLEMENT_QUERY_OF(Query1Impl)



