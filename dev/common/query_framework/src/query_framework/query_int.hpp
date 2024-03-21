#pragma once

#include <string_view>
#include "query_id.hpp"

namespace query {

	struct NodeID;
	
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
		// context?
		// cache?
	>
	struct QueryInterface {
		using QueryType = QueryType_tp;
		using QResult = QResult_tp;
		using QKey = QKey_tp;
	};
}

/**
 * @brief Macro emitting body of query interface struct.
 */
#define QUERY_INTERFACE_BOILERPLATE \
	static auto query(QKey, NodeID) -> QResult; \
	static std::string_view name;  \
	static ::query::QueryID id;


/**
 * @brief Macro used do delcare queries.
 * @example
 * 	DECLARE_QUERY (QueryName, QueryKey, QueryReturnValue) 
 */
#define DECLARE_QUERY(query_type, key, value) \
	struct query_type: ::query::QueryInterface<   \
		query_type,  \
		key,         \
		value        \
	> { QUERY_INTERFACE_BOILERPLATE };


