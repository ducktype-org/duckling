/**
 * Definition of query Context type.
 */

#pragma once

#include "context_fd.hpp"  // IWYU pragma: keep

#include <diagnostic_interactive/logger.hpp>
#include <diagnostic_interactive/logger_fwd.hpp>
#include <diagnostic_interactive/placeholder.hpp>  // @TODO: #1887 move to outer query-invocation layer

#include <base/extend_cpp/defer.hpp>

#include <query_framework/internal/cycle_handling/cycle_exception.hpp>
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

		/**
		 * A flag indicating that the query node associated with this context is part of a cycle in
		 * the query graph. This is set by the cycle detection logic in QueryGraphHandler when a
		 * cycle is detected, and can be used by query implementations to react to cycles if needed.
		 *
		 * @note This has to be atomic, as multiple workers can catch the cycle at the same time,
		 * and write to it concurrently.
		 *
		 * @important when setting this flag we must use memory order (at least) acquire-release
		 * to ensure that any removal of edges from the cycle that happens after the cycle
		 * detection, is only visible after all nodes on the cycle are marked as cyclic.
		 */
		std::atomic<bool> is_cyclic_node = false;

		Context(internal::NodeID my_node): my_node(my_node) {}
		friend struct query::internal::ContextAccess;

		/**
		 * Main query state, that query calls work on.
		 */
		static internal::QueryState main_query_state;

		void assertActive() const { CORE_ASSERT(active, "Context is inactive."); }

		/**
		 * @brief Sets whether the current thread is executing query code.
		 */
		static void setAreWeInsideQuery(bool value);

		/**
		 * @brief Helper RAII object to handle query graph, active graph, and cycle checks logic
		 * when calling another query. Used in query and await.
		 */
		struct QueryGraphHandler final {
		private:
			internal::NodeID caller;
			internal::NodeID callee;
			bool             enable_active_graph_operations;

			/**
			 * Helper method used to deduplicate logic related to
			 * active graph operations in the destructor.
			 */
			void deinitActiveGraph() { main_query_state.getActiveGraph()->removeEdge(caller); }

		public:
			QueryGraphHandler(
				Context&         this_context,
				internal::NodeID caller,
				internal::NodeID callee,
				bool             active_graph_operations
			):
				  caller(caller),
				  callee(callee),
				  enable_active_graph_operations(active_graph_operations) {
				main_query_state.addDependency(caller, callee);

				if (enable_active_graph_operations) {
					// @TODO: #2026 Optimize it, we only need to add edge here, when the query is
					// not ready.

					// Here, the node should already exist in the active graph.
					// We add edge from 'caller' to 'callee' to represent the dependency.
					// Important note #1945:
					// Current cycle detection algorithm works only when we use wait-on-await
					// strategy. For other strategies we will have to additionally register special
					// "working-on" edges. Also note, that we should not add any edges when
					// scheduling queries. Scheduling acts as if the schedule operation came from
					// outside the query framework.
					main_query_state.getActiveGraph()->setEdge(caller, callee);
					auto maybe_cycle = main_query_state.getActiveGraph()->cycleCheck(callee);

					if (maybe_cycle.has_value()) {
						// We hit a cycle!

						for (auto node_info: maybe_cycle.value().cycle_nodes) {
							// This is a critical part of the cycle handling.
							// We mark all nodes on the cycle as cyclic, so that query
							// implementations can react to that if needed.
							node_info.node_context_ref->is_cyclic_node = true;
						}

						// Log cyclic diagnostic with cycle information.
						// @TODO: #2615 move and improve this diagnostic.

						this_context.logInt(makeBox<dia_int::PlaceholderError>(
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
									for (auto node_info: cycle.cycle_nodes) {
										result += "  - Query node ";
										result += base::strConcat(
											node_info.node_id.q_id.getData().name,
											".",
											node_info.node_id.hash.val.toStringHex(),
											"\n"
										);
									}
									return result;
								}()
							)
						));

						// We have to repeat destructor logic here, since it will not be called
						// after a throw here.
						deinitActiveGraph();

						// Interrupt the query execution (i.e. provide function) by throwing the
						// cycle exception.
						throw internal::QueryCycleException();
					}
				}
			}

			~QueryGraphHandler() {
				if (enable_active_graph_operations) {
					// We remove the edge after the query call is done.
					// This is because active graph only tracks currently active queries and
					// dependencies.
					deinitActiveGraph();
				}
			}
		};

	public:
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

			if constexpr (OthQuery::QUERY_DATA.isInputQuery()) {
				QueryGraphHandler graph_handler(*this, my_node, dep_id, false);
				this->active = false;
				defer({ this->active = true; });

				return OthQuery::internal_query(key);
			} else {
				QueryGraphHandler graph_handler(*this, my_node, dep_id, true);
				this->active = false;
				defer({ this->active = true; });

				// note that this will block, until the task is completed
				main_query_state.getTaskPool()->query(internal::Task{
					dep_id, [key](concurrent::worker::WRef) { OthQuery::internal_query(key); } });


				// We would like to throw here, "after the return",
				// to avoid copy, but that would require throwing in a destructor.
				// For now we just do this. It should not be a problem since in practice most query
				// results are trivially copyable anyway, and the compiler should generally use here
				// copy-elision in non-trivial cases, so it should not be a problem.
				auto result = OthQuery::internal_load(dep_id.hash.val);
				if (is_cyclic_node) throw internal::QueryCycleException();
				return result;
			}
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

			// ->FALSE: LARGE improvement, even larger for 1 worker (huh..?????)
			QueryGraphHandler graph_handler(*this, my_node, handle.getID(), true);

			this->active = false;
			defer({ this->active = true; });

			// note that this will block, until the task is completed
			handle.await();

			auto result = OthQuery::internal_load(handle.getID().hash.val);
			if (is_cyclic_node) throw internal::QueryCycleException();
			return result;
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

			// Check that the query has preserve_in_graph = true
			CORE_ASSERT(
				my_node.q_id.getData().tags.preserve_in_graph,
				"Cannot add metadata to query without preserve_in_graph = true. "
				"Query: "
					+ std::string(my_node.q_id.getData().name)
			);

			main_query_state.addMetadataInternal<MetadataT>(my_node, std::forward<Args>(args)...);
		}

		/**
		 * @brief Add metadata to the current query node only if no metadata of this type exists.
		 *
		 * Use this method when you know that for the current node only one metadata
		 * instance of a given type should exist, but the same code path might be executed multiple
		 * times (e.g., during incremental re-computation of the node that has been merged from
		 * previous compilation).
		 *
		 * This is useful for metadata that acts as a "flag" or "singleton" per node,
		 * where duplicate entries would be redundant.
		 *
		 * @tparam MetadataT The metadata type (must derive from BaseMetadata)
		 * @tparam Args Argument types for constructing the metadata
		 * @param args Arguments forwarded to MetadataT constructor
		 * @return true if metadata was added, false if metadata of this type already exists
		 */
		template<typename MetadataT, typename... Args>
		requires std::derived_from<MetadataT, internal::BaseMetadata>
		bool addMetadataIfNotExists(Args&&... args) {
			assertActive();

			return main_query_state.addMetadataIfNotExistsInternal<MetadataT>(
				my_node, std::forward<Args>(args)...
			);
		}

		/**
		 * @brief Logs a diagnostic message for the current query node.
		 * Is thread safe.
		 */
		void logInt(Box<dia_int::MessageBase> diagnostic);

		/**
		 * @brief Collect all diagnostics from the main query state into the provided output vector.
		 * @warning @non_thread_safe
		 * It must not be called concurrently with any method that modifies the underlying collection.
		 */
		static void collectAllDiagnostic(std::vector<CRef<dia_int::dia_args::Diagnostic>>& output);

		/**
		 * @brief Collect all diagnostics from the main query state into the provided output vector,
		 * while also applying the provided position update function.
		 * @warning @non_thread_safe
		 * It must not be called concurrently with any method that modifies the underlying collection.
		 */
		static void collectAndUpdateAllDiagnostic(
			std::vector<CRef<dia_int::dia_args::Diagnostic>>& output,
			const dia_int::UpdatePositionFunc&                update_func
		);

		/**
		 * @brief Dump all loggers from all nodes into a single logger and clear them from the state.
		 * @warning @non_thread_safe
		 * It must not be called concurrently with any method that modifies the underlying collection.
		 */
		static Box<dia_int::Logger> dumpToOneLoggerAndClear();

		/**
		 * @brief Returns a const reference to the main query state.
		 * Can be safely used outside query framework.
		 */
		static const internal::QueryState& getState() { return main_query_state; }

		/**
		 * @brief Returns true if the current thread is executing query code.
		 */
		[[nodiscard]]
		static bool areWeInsideQuery();

		/**
		 * @brief Returns true when any query is currently active in any thread.
		 */
		[[nodiscard]]
		static bool isAnyQueryCurrentlyRunning();
	};
}
