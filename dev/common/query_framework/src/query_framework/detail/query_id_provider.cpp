#include "query_id_provider.hpp"

namespace query::detail {
	namespace {
		/**
		 * Query ID used in entry point. See also: outsideWorldQueryID, query::entryPoint.
		 */
		constexpr QueryID OUTSIDE_WORLD_QUERY = { 0 };

		/**
		 * @note It will be used before main, constinit is important.
		 */
		constinit QueryID next = { 1 };
	}

	QueryID newQueryID(const std::string_view pretty_name) {
		const QueryID ret = next;
		QueryID::setName(ret, pretty_name);

		next.val++;

		return ret;
	}

	QueryID outsideWorldQueryID() { return OUTSIDE_WORLD_QUERY; }
}
