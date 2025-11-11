/**
 * @file query_id.hpp
 * @brief Definition of query id type.
 */
#pragma once

#include "query_data.hpp"

#include <base/types/ints.hpp>

namespace query::internal {
	/**
	 * Unique identifier of query type.
	 */
	struct QueryID final {
	private:
		u64 val;

		constexpr explicit QueryID(u64 val): val(val) {}

		friend struct QueryIDMaker;
		friend class QueryGraph;

	public:
		[[nodiscard]]
		constexpr u64 asInt() const {
			return val;
		}

		[[nodiscard]]
		const QueryData& getData() const;

		// @TODO: #1433 change this to use query tags to determine whether the query has stable hash
		// or loads cache from disk
		[[nodiscard]]
		bool hasStableHash() const;

		[[nodiscard]]
		constexpr bool operator==(const QueryID& other) const {
			return val == other.val;
		}

		[[nodiscard]]
		constexpr bool operator<(const QueryID& other) const {
			return val < other.val;
		}
	};

	/**
	 * @brief A function for generering query id for each query.
	 * This function should never be used outside the framework.
	 * @param query_data data of given query. Framework will keep a copy of the data for easy acces
	 * @param has_stable_hash whether the query has a stable hash function
	 * @TODO: #1433 change this to use query tags to determine whether the query has stable hash /
	 * cache loads from disk from just query id
	 */
	QueryID registerQuery(QueryData query_data, bool has_stable_hash = false);

	/**
	 * @brief Provides query id of "outside world" query.
	 * This function should never be used outside the framework.
	 */
	QueryID outsideWorldQueryID();

}
