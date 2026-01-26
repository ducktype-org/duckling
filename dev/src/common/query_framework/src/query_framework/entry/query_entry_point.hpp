/**
 * @file query_entry_point.hpp
 * @brief Implementation of "Query Entry Point" used to call queries from outside of
 * query-framework.
 */
#pragma once

#include <base/except/exceptions.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/internal/query_data/query_id.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/utils/simple_keys.hpp>

namespace query {

	namespace internal {
		/**
		 * Helper class implementing query entry point
		 * @note This exist only, so it can be easily friend-ed by queries.
		 */
		struct EntryPointHelper final {
			template<typename QueryType>
			auto static callQuery(const typename QueryType::QKey& key) -> decltype(auto) {
				// We create a node id here directly, so we can insert "outside world" as caller.
				// The node is is always the same, it is essentially the root of the query graph.
				return QueryType::internal_query(
					key, NodeID(internal::outsideWorldQueryID(), { 0 })
				);
			}
		};
	}

	/**
	 * @brief This function is used to invoke queries from "outside world".
	 * It should never be used to invoke query from within query.
	 */
	template<typename QueryType>
	auto entryPoint(const typename QueryType::QKey& key) -> decltype(auto) {
		CORE_ASSERT(
			Context::getState().queryStackSize() == 0, "query::entryPoint called from within query!"
		);
		return internal::EntryPointHelper::callQuery<QueryType>(key);
	}
}
