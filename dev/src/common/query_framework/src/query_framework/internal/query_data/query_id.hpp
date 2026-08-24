/**
 * @file query_id.hpp
 * @brief Definition of query id type.
 */
#pragma once

#include "query_data.hpp"

#include <base/types/ints.hpp>

#include <ser/macros.hpp>
#include <ser/ser.hpp>

#include <vector>

namespace query::external {
	struct InputData;
}

namespace query::internal {
	class MetadataStorage;  // Forward declaration for friend access
}

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
		friend class QueryState;
		friend class MetadataStorage;
		friend void markPreviousGraphNodesInputs(std::vector<query::external::InputData> inputs);

		/**
		 * @note Unregistered queries occurs only during the deserialisation of previous graph
		 * This method is used to determinate whether the query is registered - it points to query
		 * actually implemented in the system. Dummy queries from previous graph aren't registered.
		 * All other queries should be registered.
		 *
		 * Unregistered query does not have QueryData associated with it.
		 * This also means that unregistered queries don't have any kind associated with it, but
		 * they should be viewed as implicitly dummy.
		 */
		[[nodiscard]] bool registered() const;

	public:
		[[nodiscard]]
		constexpr u64 asInt() const {
			return val;
		}

		[[nodiscard]]
		const QueryData& getData() const;

		[[nodiscard]]
		constexpr bool operator==(const QueryID& other) const {
			return val == other.val;
		}

		[[nodiscard]]
		constexpr bool operator<(const QueryID& other) const {
			return val < other.val;
		}

		/**
		 * @note Used for VectorMap
		 */
		[[nodiscard]]
		explicit constexpr operator usize() const {
			return static_cast<usize>(val);
		}

		/**
		 * @brief `ser` hooks: the query id as its underlying integer.
		 */
		SER_DESCRIBE_MAKE(QueryID, val)
	};

	/**
	 * @brief A function for generating query id for each query.
	 * This function should never be used outside the framework.
	 * @param query_data data of given query. Framework will keep a copy of the data for easy access.
	 */
	QueryID registerQuery(QueryData query_data);
}

template<>
struct std::hash<query::internal::QueryID> {
	[[nodiscard]]
	size_t operator()(const query::internal::QueryID& id) const {
		return id.asInt();
	}
};
