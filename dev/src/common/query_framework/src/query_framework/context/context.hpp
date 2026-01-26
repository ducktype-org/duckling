/**
 * Definition of query Context type.
 */

#pragma once

#include "context_fd.hpp"  // IWYU pragma: keep

#include <diagnostic_interactive/logger.hpp>
#include <diagnostic_interactive/placeholder.hpp> // @TODO: 1887 move to outer query-invocation layer

#include <base/extend_cpp/defer.hpp>

#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/internal/query_graph/node_making.hpp>  // IWYU pragma: export
#include <query_framework/internal/query_graph/query_state.hpp>

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
		// @TODO: Make the context (and thus the logger) be propagated through query calls,
		// so that all queries run on the same file / in the same compilation thread / whatever
		// use a single, *non-static* logger object.
		/**
		 * @name Logs storage
		 * @brief Global/vector-backed logging facility.
		 * \parallel Current implementation uses a global vector; not thread-safe; serialize or
		 * buffer per-thread.
		 * @{
		 */
		// static dia::Logger     logger;

		static dia_int::Logger int_logger;
		/**
		 * @}
		 */

		Context(const Context&) = delete;
		Context(Context&&)      = delete;

		template<typename OthQuery>
		auto query(const typename OthQuery::QKey& key) -> decltype(auto) {
			assertActive();
			
			internal::NodeID dep_id = internal::makeNodeID<OthQuery>(key);

			main_query_state.getGraphMutable()->addDependency(my_node, dep_id);

			// Here, the node should already exist in the active graph.
			// We add edge from 'my_node' to 'dep_id' to represent the dependency.
			main_query_state.getActiveGraph()->setEdge(my_node, dep_id);
			auto maybe_cycle = main_query_state.getActiveGraph()->cycleCheck(my_node);

			if (maybe_cycle.has_value()) {
				// we hit a cycle!
				// for now just panic

				logInt(makeBox<dia_int::PlaceholderHeaderError>(
					base::strConcat("Query cycle detected involving query node:",
					my_node.q_id.asInt(),
					".",
					my_node.hash.val.toStringHex()),
					base::strConcat("The cycle:\n",
						[&maybe_cycle]() -> std::string {
							std::string result;
							auto cycle = maybe_cycle.value();
							for (auto node_id : cycle.cycle_nodes) {
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
				)));
				CORE_ASSERT(false, "Query cycle detected involving query node:", my_node.q_id.asInt(), ".", my_node.hash.val.toStringHex());
			}

			this->active = false;
			defer({
				this->active = true;
				// We remove the edge after the query call is done.
				// This is because active graph only tracks currently active queries and dependencies.
				main_query_state.getActiveGraph()->removeEdge(my_node);
			});

			return OthQuery::internal_query(key, my_node);
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
