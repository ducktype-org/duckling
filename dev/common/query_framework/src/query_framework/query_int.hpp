#pragma once

#include <string_view>
#include "query_id.hpp"
#include "node_id.hpp"

namespace query {

	namespace detail {
		/**
		 * @brief Base class for defining query interface
		 * 
		 * @tparam QueryType_tp a type of a query
		 * @tparam QKey_tp a type of a query ket
		 * @tparam cache? a type returned by the query
		 */
		template<
			typename QueryType_tp,
			typename QKey_tp,
			typename QResult_tp
		>
		struct QueryInterface {
			using QueryType = QueryType_tp;
			using QResult = QResult_tp;
			using QKey = QKey_tp;
		};
	}

	/**
	 * @brief Key used for queries without keys, input queries, and "outside world" query.
	 */
	struct EmptyKey { };
}

/**
 * @brief Macro emitting body of query interface struct.
 */
#define INTERNAL_QUERY_INTERFACE_BOILERPLATE \
	static auto internal_query(QKey, ::query::detail::NodeID) -> QResult; \
	static ::std::string_view name;  \
	static ::query::detail::QueryID id;


/**
 * @brief Macro used do delcare queries.
 * @example
 * 	DECLARE_QUERY (QueryName, QueryKey, QueryReturnValue) 
 */
#define DECLARE_QUERY(query_type, key, value) \
	struct query_type: ::query::detail::QueryInterface<   \
		query_type,  \
		key,         \
		value        \
	> { INTERNAL_QUERY_INTERFACE_BOILERPLATE };


