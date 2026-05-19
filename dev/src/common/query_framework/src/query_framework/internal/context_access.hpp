#pragma once

#include "query_graph/query_state.hpp"

#include <query_framework/context/context.hpp>

#include <memory>

namespace query::internal {
	/**
	 * @brief Internal helper struct used to access context private state
	 */
	struct ContextAccess final {
		static auto make(NodeID my_node) { return Context(my_node); }

		/**
		 * @brief Create a shared pointer to a new context
		 * Currently the context is shared between the query_implementation and the active graph,
		 * to allow cycle detection logic.
		 * The context might be acessed by one thread, when the other thread will throw a cycle
		 * exception
		 * @param my_node The node ID for the new context
		 * @return Shared pointer to the new context
		 */
		static std::shared_ptr<Context> makeShared(NodeID my_node) {
			return std::shared_ptr<Context>(new Context(my_node));
		}

		static Ref<internal::QueryState> getState() { return &Context::main_query_state; }

		static void setAreWeInsideQuery(bool value) { Context::setAreWeInsideQuery(value); }
	};
}
