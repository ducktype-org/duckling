#include <base/maps.hpp>

#include "query_id_provider.hpp"
#include "query_id.hpp"

namespace query::detail {
	namespace {
		QueryID           next                = { 1 };
		constexpr QueryID outside_world_query = { 0 };
	}

	QueryID newQueryId(const std::string_view pretty_name) {
		const QueryID ret = next;
		QueryID::setName(ret, pretty_name);

		next.val++;

		return ret;
	}

	QueryID outsideWorldQueryID() { return outside_world_query; }
}
