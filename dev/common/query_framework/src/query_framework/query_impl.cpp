#include "query_impl.hpp"

namespace query {
	QueryID nextQueryId() {
		static QueryID next = 0;
		return next++;
	}
}

