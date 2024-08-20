/**
 * @file query_entry_point.hpp
 * @brief Implementation of "Query Entry Point" used to call queries from outside of
 * query-framework.
 */
#pragma once

#include "query_id_provider.hpp"
#include "node_making.hpp"
#include "empty_key.hpp"
#include "dep_graph.hpp"

#include <base/exceptions.hpp>

namespace query {

	/**
	 * @brief This function is used to invoke queries from "outside world".
	 * It should never be used to invoke query from within query.
	 */
	template<typename QueryType>
	auto entryPoint(typename QueryType::QKey key) -> decltype(auto) {
		RIFT_ASSERT(
			detail::dep_graph::queryStackSize() == 0, "query::entryPoint called from within query!"
		);
		return QueryType::internal_query(
			key, detail::makeNodeID(detail::outsideWorldQueryID(), EmptyKey())
		);
	}
}
