#pragma once

#include "..context/context.hpp"
#include "query_graph/query_state.hpp"

namespace query::internal {
	/**
	 * @brief Internal helper struct used to access context private state
	 */
	struct ContextAccess final {
		static auto make(NodeID my_node) { return Context(my_node); }

		static Ref<internal::QueryState> getState() { return &Context::main_query_state; }
	};
}
