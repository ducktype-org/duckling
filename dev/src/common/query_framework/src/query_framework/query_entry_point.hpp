/**
 * @file query_entry_point.hpp
 * @brief Implementation of "Query Entry Point" used to call queries from outside of
 * query-framework.
 */
#pragma once

#include "context.hpp"
#include "empty_key.hpp"
#include "internal/query_data/query_id.hpp"
#include "internal/query_graph/node_id.hpp"

#include <base/except/exceptions.hpp>

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
				return QueryType::internal_query(
					key,
					NodeID(internal::outsideWorldQueryID(), { EmptyKey().queryStablePerfectHash() })
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
