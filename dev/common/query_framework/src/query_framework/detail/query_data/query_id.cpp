/**
 * @file query_id.cpp
 * @author Mateusz
 *
 */

#include "query_id.hpp"

#include "query_data.hpp"

#include <base/maps.hpp>

template<>
struct std::hash<query::detail::QueryID> {
	[[nodiscard]]
	size_t operator()(const query::detail::QueryID& id) const {
		return id.asInt();
	}
};

namespace query::detail {

	struct QueryIDMaker {
		static constexpr QueryID make(u64 val) { return { val }; }

		static constexpr QueryID next(QueryID id) { return { id.val + 1 }; }
	};

	namespace {

		/**
		 * Query ID used in entry point. See also: outsideWorldQueryID, query::entryPoint.
		 */
		constexpr QueryID OUTSIDE_WORLD_QUERY = QueryIDMaker::make(0);

		/**
		 * @note It will be used before main, constinit is important.
		 */
		constinit QueryID next = QueryIDMaker::make(1);

		using DataMap = base::HashMap<QueryID, QueryData>;

		DataMap& dataMap() {
			static DataMap data_map{};
			return data_map;
		}
	}

	const QueryData& QueryID::getData() const { return dataMap().at(*this); }

	QueryID registerQuery(QueryData query_data) {
		auto ret_id = next;
		next        = QueryIDMaker::next(ret_id);
		dataMap().put(ret_id, query_data);
		return ret_id;
	}

	QueryID outsideWorldQueryID() { return OUTSIDE_WORLD_QUERY; }

}
