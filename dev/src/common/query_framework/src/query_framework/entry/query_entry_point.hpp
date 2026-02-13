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

#include <iostream>  // PR remove

namespace query {

	namespace internal {
		/**
		 * Helper class implementing query entry point
		 * @note This exist only, so it can be easily friend-ed by queries.
		 */
		struct EntryPointHelper final {
			template<typename QueryType>
			auto static callQuery(const typename QueryType::QKey& key) -> decltype(auto) {
				std::cerr << "call query\n";

				// return QueryType::internal_query(key);
				auto node_id = makeNodeID<QueryType>(key);

				std::cerr << "...\n";

				Context::getState().getTaskPool()->addTask(internal::Task{
					node_id,
					[key](concurrent::worker::WRef) { QueryType::internal_query(key); },
				});

				std::cerr << "waiting soon\n";
				Context::getState().getTaskPool()->waitForTask(node_id);

				return QueryType::internal_load(node_id.hash.val);
			}
		};
	}

	/**
	 * @brief This function is used to invoke queries from "outside world".
	 * It should never be used to invoke query from within query.
	 */
	template<typename QueryType>
	auto entryPoint(const typename QueryType::QKey& key) -> decltype(auto) {
		// PR assert that we are not inside query

		// CORE_ASSERT(
		// 	Context::getState().activeQueryCount() == 0,
		// 	"query::entryPoint called from within query!"
		// );
		return internal::EntryPointHelper::callQuery<QueryType>(key);
	}
}
