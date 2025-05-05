/**
 * @file query_entry_point.hpp
 * @brief Implementation of "Query Entry Point" used to call queries from outside of
 * query-framework.
 */
#pragma once

#include "detail/query_data/query_id.hpp"
#include "detail/query_graph/dep_graph.hpp"
#include "detail/query_graph/node_making.hpp"
#include "empty_key.hpp"

#include <base/exceptions.hpp>

namespace query {

	namespace detail {
		/**
		 * Helper class implementing query entry point
		 * @note This exist only, so it can be easily friend-ed by queries.
		 */
		struct EntryPointHelper {
			template<typename QueryType>
			auto static callQuery(typename QueryType::QKey key) -> decltype(auto) {
				return QueryType::internal_query(
					key, detail::makeNodeID(detail::outsideWorldQueryID(), EmptyKey())
				);
			}
		};
	}

	/**
	 * @brief This function is used to invoke queries from "outside world".
	 * It should never be used to invoke query from within query.
	 */
	template<typename QueryType>
	auto entryPoint(typename QueryType::QKey key) -> decltype(auto) {
		CORE_ASSERT(
			detail::dep_graph::queryStackSize() == 0, "query::entryPoint called from within query!"
		);
		return detail::EntryPointHelper::callQuery<QueryType>(key);
	}
}
