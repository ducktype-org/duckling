/**
 * Definition of query Context type.
 */

#pragma once

#include "context_fd.hpp"  // IWYU pragma: keep

#include <diagnostic_interactive/logger.hpp>
#include <diagnostic_interactive/logger_fwd.hpp>
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

	public:
		Context(const Context&) = delete;
		Context(Context&&)      = delete;

		template<typename OthQuery>
		auto query(const typename OthQuery::QKey& key) -> decltype(auto) {
			assertActive();

			internal::NodeID dep_id = internal::makeNodeID<OthQuery>(key);

			main_query_state.addDependency(my_node, dep_id);

			// Here, the node should already exist in the active graph.
			// We add edge from 'my_node' to 'dep_id' to represent the dependency.
			// Important note #1945:
			// Current cycle detection algorithm works only when we use wait-on-await strategy.
			// For other strategies we will have to additionally register special "working-on" edges.
			// Also note, that we should not add any edges when scheduling queries.
			// Scheduling acts as if the schedule operation came from outside the query framework.
			main_query_state.getActiveGraph()->setEdge(my_node, dep_id);
			auto maybe_cycle = main_query_state.getActiveGraph()->cycleCheck(my_node);

			if (maybe_cycle.has_value()) {
				// we hit a cycle!
				// for now just panic
				// @TODO: #1888 change that

				logInt(makeBox<dia_int::PlaceholderHeaderError>(
					base::strConcat(
						"Query cycle detected involving query node:",
						my_node.q_id.asInt(),
						".",
						my_node.hash.val.toStringHex()
					),
					base::strConcat(
						"The cycle:\n",
						[&maybe_cycle]() -> std::string {
							std::string result;
							auto        cycle = maybe_cycle.value();
							for (auto node_id: cycle.cycle_nodes) {
								result += "  - Query node ";
								result += base::strConcat(
									node_id.q_id.asInt(), ".", node_id.hash.val.toStringHex(), "\n"
								);
							}
							return result;
						}()
					)
				));
				CORE_ASSERT(
					false,
					"Query cycle detected involving query node:",
					my_node.q_id.asInt(),
					".",
					my_node.hash.val.toStringHex()
				);
			}

			this->active = false;
			defer({
				this->active = true;
				// We remove the edge after the query call is done.
				// This is because active graph only tracks currently active queries and dependencies.
				main_query_state.getActiveGraph()->removeEdge(my_node);
			});

			return OthQuery::internal_query(key);
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
		 * @brief Logs a diagnostic message for the current query node.
		 * Is thread safe.
		 */
		void logInt(Box<dia_int::MessageBase> diagnostic);

		/**
		 * @brief Collect all diagnostics from the main query state into the provided output vector.
		 * @warning This method is not thread safe.
		 * It must not be called concurrently with any method that modifies the underlying collection.
		 */
		static void collectAllDiagnostic(std::vector<CRef<dia_int::dia_args::Diagnostic>>& output);

		/**
		 * @brief Dump all loggers from all nodes into a single logger and clear them from the state.
		 * @warning This method is not thread safe.
		 * It must not be called concurrently with any method that modifies the underlying collection.
		 */
		static Box<dia_int::Logger> dumpToOneLoggerAndClear();

		/**
		 * @brief Returns a const reference to the main query state.
		 * Can be safely used outside query framework.
		 */
		static const internal::QueryState& getState() { return main_query_state; }
	};
}
