#include "query_id_provider.hpp"

namespace query::detail {
	namespace {
		constexpr QueryID OUTSIDE_WORLD_QUERY = { 0 };
	}

	QueryID newQueryID(const std::string_view pretty_name) {
		// note: this is static, to allow pre-main use
		static QueryID next = { 1 };

		const QueryID ret = next;
		QueryID::setName(ret, pretty_name);

		next.val++;

		return ret;
	}

	QueryID outsideWorldQueryID() { return OUTSIDE_WORLD_QUERY; }
}
