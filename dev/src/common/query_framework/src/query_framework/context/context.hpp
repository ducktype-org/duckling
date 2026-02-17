/**
 * Definition of query Context type.
 */

#pragma once

#include "context_fd.hpp"  // IWYU pragma: keep

#include <diagnostic_interactive/logger.hpp>
#include <diagnostic_interactive/placeholder.hpp>  // @TODO: #1887 move to outer query-invocation layer

#include <base/extend_cpp/defer.hpp>

#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/internal/query_graph/node_making.hpp>  // IWYU pragma: export
#include <query_framework/internal/query_graph/query_state.hpp>
#include <query_framework/internal/query_metadata/metadata_storage.hpp>

namespace query {

	namespace internal {
		struct ContextAccess;
	}

	/**
	 * We expose the TaskHandle type here, as it is used in the Context interface,
	 * but we want to avoid exposing the whole TaskPool interface.
	 */
	using internal::TaskHandle;

	/**
	 * @brief Context is type of a special object
	 * that query implementation use to perform three key operations:
	 * 	* call other query
	 *  * log
	 *  * report compiler error
	 *  * add metadata to the current query node
	 *
	 * @FUTURE: there exist a concept of "custom context" types as
	 * a way to hack-in the query model. This however will most likely be
	 * discarded.
	 *
	 * \parallel Current implementation uses a global vector; not thread-safe; serialize or buffer
	 * per-thread.
	 */
	struct Context final {
	private:
		internal::NodeID my_node;
		bool             active = true;

		Context(internal::NodeID my_node): my_node(my_node) {}
		friend struct query::internal::ContextAccess;

		/**
		 * Main query state, that query calls work on.
		 */
		static internal::QueryState main_query_state;

		void assertActive() const { CORE_ASSERT(active, "Context is inactive."); }

		/**
		 * @brief Helper RAII object to handle query graph, active graph, and cycle checks logic
		 * when calling another query. Used in query and await.
		 */
		struct QueryGraphHandler final {
		private:
			internal::NodeID caller;
			internal::NodeID callee;


		public:
			QueryGraphHandler(
				Context& this_context, internal::NodeID caller, internal::NodeID callee
			):
				  caller(caller),
				  callee(callee) {
				main_query_state.addDependency(caller, callee);

				// @TODO: #2026: Optimize it, we only need to add edge here, when the query is not ready.

				// Here, the node should already exist in the active graph.
				// We add edge from 'caller' to 'callee' to represent the dependency.
				// Important note #1945:
				// Current cycle detection algorithm works only when we use wait-on-await strategy.
				// For other strategies we will have to additionally register special "working-on"
				// edges. Also note, that we should not add any edges when scheduling queries.
				// Scheduling acts as if the schedule operation came from outside the query
				// framework.
				main_query_state.getActiveGraph()->setEdge(caller, callee);
				auto maybe_cycle = main_query_state.getActiveGraph()->cycleCheck(caller);

				if (maybe_cycle.has_value()) {
					// we hit a cycle!
					// for now just panic
					// @TODO: #1888 change that

					this_context.logInt(makeBox<dia_int::PlaceholderHeaderError>(
						base::strConcat(
							"Query cycle detected involving query node:",
							caller.q_id.asInt(),
							".",
							caller.hash.val.toStringHex()
						),
						base::strConcat(
							"The cycle:\n",
							[&maybe_cycle]() -> std::string {
								std::string result;
								auto        cycle = maybe_cycle.value();
								for (auto node_id: cycle.cycle_nodes) {
									result += "  - Query node ";
									result += base::strConcat(
										node_id.q_id.asInt(),
										".",
										node_id.hash.val.toStringHex(),
										"\n"
									);
								}
								return result;
							}()
						)
					));
					CORE_ASSERT(
						false,
						"Query cycle detected involving query node:",
						caller.q_id.asInt(),
						".",
						caller.hash.val.toStringHex()
					);
				}
			}

			~QueryGraphHandler() {
				// We remove the edge after the query call is done.
				// This is because active graph only tracks currently active queries and dependencies.
				main_query_state.getActiveGraph()->removeEdge(caller);
			}
		};

	public:
		// @TODO: Make the context (and thus the logger) be propagated through query calls,
		// so that all queries run on the same file / in the same compilation thread / whatever
		// use a single, *non-static* logger object.
		/**
		 * @name Logs storage
		 * @brief Global/vector-backed logging facility.
		 * \parallel Current implementation uses a global vector; not thread-safe; serialize or
		 * buffer per-thread.
		 */
		static dia_int::Logger int_logger;


		Context(const Context&) = delete;
		Context(Context&&)      = delete;

		/**
		 * This is the main query invocation method, used to call other queries from a query
		 * implementation.
		 *
		 * This logically acts very similar as schedule and instant await.
		 * The task will be executed by the caller worker immediately, unless another worker is
		 * already executing it, in which case we will wait for it to complete and then load the
		 * result.
		 *
		 * @note This is also an external query invocation layer.
		 * @TODO: #1887 change later to make separation clearer.
		 * See also: #2026
		 */
		template<typename OthQuery>
		auto query(const typename OthQuery::QKey& key) -> decltype(auto) {
			assertActive();

			internal::NodeID dep_id = internal::makeNodeID<OthQuery>(key);

			QueryGraphHandler graph_handler(*this, my_node, dep_id);

			this->active = false;
			defer({ this->active = true; });

			if constexpr (OthQuery::QUERY_DATA.isInputQuery()) {
				return OthQuery::internal_query(key);
			} else {
				// note that this will block, until the task is completed
				main_query_state.getTaskPool()->query(internal::Task{
					dep_id, [key](concurrent::worker::WRef) { OthQuery::internal_query(key); } });

				return OthQuery::internal_load(dep_id.hash.val);
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Schedules another query call as a task and returns a handle to await its
		 * completion. This is non blocking operation, the task will be scheduled for execution and
		 * this method will return immediately with a handle.
		 *
		 * @note The intended use case for this method is to schedule large tasks that could likely
		 * be executed by another worker, before we require the result.
		 * @note This is also the secondary starting-point of parallelism in the query framework
		 * (first one beeing the scheduling of multiple global tasks by the query framework user).
		 */
		template<typename OthQuery>
		TaskHandle schedule(const typename OthQuery::QKey& key) {
			static_assert(
				not OthQuery::QUERY_DATA.isInputQuery(), "Cannot schedule an input query."
			);

			auto handle = main_query_state.getTaskPool()->schedule(internal::Task{
				internal::makeNodeID<OthQuery>(key),
				[key](concurrent::worker::WRef) { OthQuery::internal_query(key); } });
			return handle;
		}

		/**
		 * @brief Waits for the completion of a scheduled query and returns its result.
		 * The task will be executed by the caller worker immediately, unless another worker is
		 * already executing it, in which case we will wait for it to complete and then load the
		 * result.
		 *
		 * @param handle The handle of the scheduled query to wait for, returned by the ctx.schedule
		 * method.
		 */
		template<typename OthQuery>
		auto await(internal::TaskHandle& handle) {
			CORE_ASSERT(
				OthQuery::getID() == handle.getID().q_id,
				"Task handle query ID does not match the awaited query type."
			);

			QueryGraphHandler graph_handler(*this, my_node, handle.getID());

			this->active = false;
			defer({ this->active = true; });

			// note that this will block, until the task is completed
			handle.await();

			return OthQuery::internal_load(handle.getID().hash.val);
		}

		/**
		 * @brief Add metadata to the current query node.
		 *
		 * This method allows attaching typed metadata to the current query node (my_node).
		 * Metadata can only be added to queries that have preserve_in_graph = true.
		 */
		template<typename MetadataT, typename... Args>
		requires std::derived_from<MetadataT, internal::BaseMetadata>
		void addMetadata(Args&&... args) {
			assertActive();

			main_query_state.addMetadataInternal<MetadataT>(my_node, std::forward<Args>(args)...);
		}

		/**
		 * Log message to be shown to the user.
		 * @param message The dia::Message to be logged.
		 */
		void log(Box<dia::Message> message);

		void logInt(Box<dia_int::MessageBase> diagnostic);

		/**
		 * @brief Returns a const reference to the main query state.
		 * Can be safely used outside query framework.
		 */
		static const internal::QueryState& getState() { return main_query_state; }
	};
}
