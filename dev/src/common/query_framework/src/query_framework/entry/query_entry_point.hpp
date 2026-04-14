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

	/**
	 * @brief Opaque handle to a scheduled entry-point task.
	 *
	 * Returned by `query::scheduleEntryPoint` and consumed by `query::awaitEntryPoint`.
	 * Currently it is a thin wrapper over `internal::NodeID` (which the task pool already
	 * treats as a unique identifier), but the wrapper exists so the handle can grow into
	 * something richer in the future without touching call sites.
	 */
	struct EntryTaskHandle final {
		internal::NodeID node_id;
	};

	namespace internal {
		/**
		 * Helper class implementing query entry point
		 * @note This exist only, so it can be easily friend-ed by queries.
		 */
		struct EntryPointHelper final {
			/**
			 * @brief Schedules @p QueryType for @p key on the task pool without blocking.
			 * @return Handle that can later be passed to `awaitTask` to retrieve the result.
			 */
			template<typename QueryType>
			auto static scheduleQuery(const typename QueryType::QKey& key) -> EntryTaskHandle {
				static_assert(
					!QueryType::QUERY_DATA.isInputQuery(),
					"entryPoint cannot be used to call input queries"
				);

				auto node_id = makeNodeID<QueryType>(key);

				Context::getState().getTaskPool()->addTask(internal::Task{
					node_id,
					[key](concurrent::worker::WRef) { QueryType::internal_query(key); },
				});

				return EntryTaskHandle{ node_id };
			}

			/**
			 * @brief Blocks until the task referenced by @p handle finishes, then loads
			 * the stored result for @p QueryType.
			 */
			template<typename QueryType>
			auto static awaitTask(EntryTaskHandle handle) -> decltype(auto) {
				Context::getState().getTaskPool()->waitForTask(handle.node_id);
				return QueryType::internal_load(handle.node_id.hash.val);
			}
		};
	}

	/**
	 * @brief Schedules @p QueryType for @p key as an entry-point task on the task pool
	 * and returns a handle that can be awaited later.
	 *
	 * @details Unlike `entryPoint`, this function does not block. Use it to fan out
	 * many independent entry-point queries (e.g. per-module compilations) so that
	 * worker threads can execute them concurrently, then `awaitEntryPoint` each handle
	 * to collect results.
	 */
	template<typename QueryType>
	auto scheduleEntryPoint(const typename QueryType::QKey& key) -> EntryTaskHandle {
		CORE_ASSERT(
			!Context::areWeInsideQuery(),
			"query::scheduleEntryPoint called from within query!"
		);
		return internal::EntryPointHelper::scheduleQuery<QueryType>(key);
	}

	/**
	 * @brief Blocks until the entry-point task referenced by @p handle completes and
	 * returns the stored result for @p QueryType.
	 */
	template<typename QueryType>
	auto awaitEntryPoint(EntryTaskHandle handle) -> decltype(auto) {
		CORE_ASSERT(
			!Context::areWeInsideQuery(),
			"query::awaitEntryPoint called from within query!"
		);
		return internal::EntryPointHelper::awaitTask<QueryType>(handle);
	}

	/**
	 * @brief This function is used to invoke queries from "outside world".
	 * It should never be used to invoke query from within query.
	 *
	 * @details Convenience wrapper equivalent to
	 * `awaitEntryPoint(scheduleEntryPoint<QueryType>(key))`.
	 */
	template<typename QueryType>
	auto entryPoint(const typename QueryType::QKey& key) -> decltype(auto) {
		return awaitEntryPoint<QueryType>(scheduleEntryPoint<QueryType>(key));
	}
}
