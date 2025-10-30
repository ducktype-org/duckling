/**
 * @file query_id.cpp
 * @author Mateusz
 *
 */

#include "query_id.hpp"

#include "query_data.hpp"

#include <base/collections/maps.hpp>

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

		using hasStableHashMap = base::HashMap<QueryID, bool>;

		/**
		 * @note Access to data is done this way, to make it safe to use before main.
		 * @note data is not stored directly in QueryID, to keep QueryID light.
		 */
		DataMap& dataMap() {
			static DataMap data_map{};
			return data_map;
		}

		hasStableHashMap& hasStableHashMapInstance() {
			static hasStableHashMap map{};
			return map;
		}
	}

	const QueryData& QueryID::getData() const { return dataMap().at(*this); }

	bool QueryID::hasStableHash() const { return hasStableHashMapInstance().at(*this); }

	QueryID registerQuery(QueryData query_data, bool has_stable_hash) {
		auto ret_id = next;
		next        = QueryIDMaker::next(ret_id);
		dataMap().put(ret_id, query_data);
		hasStableHashMapInstance().put(ret_id, has_stable_hash);
		return ret_id;
	}

	QueryID outsideWorldQueryID() { return OUTSIDE_WORLD_QUERY; }

}
