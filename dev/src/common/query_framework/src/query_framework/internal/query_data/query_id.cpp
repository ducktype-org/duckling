/**
 * @file query_id.cpp
 * @author Mateusz
 *
 */

#include "query_id.hpp"

#include <base/maps.hpp>

template<>
struct std::hash<query::internal::QueryID> {
	[[nodiscard]]
	size_t operator()(const query::internal::QueryID& id) const {
		return id.asInt();
	}
};

namespace query::internal {

	struct QueryIDMaker {
		static constexpr QueryID make(u64 val) { return { QueryID(val) }; }

		static constexpr QueryID next(QueryID id) { return { QueryID(id.val + 1) }; }
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

		/**
		 * @note Access to data is done this way, to make it safe to use before main.
		 * @note data is not stored directly in QueryID, to keep QueryID light.
		 */
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
