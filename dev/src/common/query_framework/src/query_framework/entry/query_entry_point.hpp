/**
 * @file query_entry_point.hpp
 * @brief Implementation of "Query Entry Point" used to call queries from outside of
 * query-framework.
 */
#pragma once

#include <base/except/exceptions.hpp>
#include <base/pointers/ref.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_data/query_id.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/internal/task_pool/task_pool.hpp>
#include <query_framework/utils/simple_keys.hpp>

namespace query {

	namespace internal {
		struct EntryPointHelper;
	}

	/**
	 * @brief Opaque handle to a scheduled entry-point task.
	 *
	 * Returned by `query::scheduleEntryPoint` and consumed by `query::awaitEntryPoint`.
	 * The constructor is private — only `internal::EntryPointHelper` (befriended below) may
	 * produce handles, which guarantees every handle corresponds to a task that was actually
	 * scheduled on the task pool.
	 */
	class EntryTaskHandle final {
	public:
		/** @brief Returns the NodeID of the scheduled task. */
		[[nodiscard]] internal::NodeID getID() const { return task_id; }

	private:
		EntryTaskHandle(internal::TaskPool& pool, internal::NodeID id): pool(&pool), task_id(id) {}

		base::Ref<internal::TaskPool> pool;
		internal::NodeID              task_id;

		friend struct internal::EntryPointHelper;
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

				auto  node_id = makeNodeID<QueryType>(key);
				auto& pool    = *Context::getState().getTaskPool();

				pool.addTask(internal::Task{
					node_id,
					[key](concurrent::worker::WRef) {
						ContextAccess::setAreWeInsideQuery(true);
						defer({ ContextAccess::setAreWeInsideQuery(false); });
						QueryType::internal_query(key);
					},
				});

				return EntryTaskHandle{ pool, node_id };
			}

			/**
			 * @brief Blocks until the task referenced by @p handle finishes, then loads
			 * the stored result for @p QueryType.
			 */
			template<typename QueryType>
			auto static awaitTask(EntryTaskHandle handle) -> decltype(auto) {
				handle.pool->waitForTask(handle.task_id);
				return QueryType::internal_load(handle.task_id.hash.val);
			}
		};
	}

	/**
	 * @brief Schedules @p QueryType for @p key as an entry-point task on the task pool
	 * and returns a handle that can be awaited later.
	 *
	 * This function doeas not block and can be called from any thread, but must not be called from
	 * within a query This is default way to call queries from "outside world" (e.g. from the
	 * compiler driver)
	 */
	template<typename QueryType>
	auto scheduleEntryPoint(const typename QueryType::QKey& key) -> EntryTaskHandle {
		CORE_ASSERT(
			!Context::areWeInsideQuery(), "query::scheduleEntryPoint called from within query!"
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
			!Context::areWeInsideQuery(), "query::awaitEntryPoint called from within query!"
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
