#include "query_data.hpp"

#include <base/except/exceptions.hpp>

namespace query::internal {

	bool panicUnwiredErase(QueryStableHash) {
		CORE_PANIC("Query erase function was not wired for this query; this is a bug.");
	}

}
