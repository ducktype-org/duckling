#include "query_impl.hpp"

namespace query {
	uint64_t nextQueryId() {
		static uint64_t next = 0;
		return next++;
	}
}

