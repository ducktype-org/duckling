#include "query_id_provider.hpp"

namespace query::detail {
	namespace {
		QueryID next = {1};
		constexpr QueryID outside_world_query = {0};
	}

	QueryID nextQueryId() {
		QueryID ret = next;
		next.val++;
		return ret;
	}

	QueryID outsideWorldQueryID() {
		return outside_world_query;
	}

}

