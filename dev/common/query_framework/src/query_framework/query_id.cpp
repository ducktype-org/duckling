#include "query_id.hpp"

namespace query {
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

