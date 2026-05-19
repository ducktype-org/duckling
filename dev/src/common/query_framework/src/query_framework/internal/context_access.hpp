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

		static std::shared_ptr<Context> makeShared(NodeID my_node) {
			return std::shared_ptr<Context>(new Context(my_node));
		}

		static Ref<internal::QueryState> getState() { return &Context::main_query_state; }

		static void setAreWeInsideQuery(bool value) { Context::setAreWeInsideQuery(value); }
	};
}
