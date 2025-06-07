#pragma once

#include "../context.hpp"
#include "query_framework/detail/query_graph/query_state.hpp"

namespace query::detail {
	/**
	 * @brief Internal helper struct used to access context private state
	 */
	struct ContextAccess final {
		static auto make(NodeID my_node) { return Context(my_node); }

		static Ref<detail::QueryState> getState() { return &Context::main_query_state; }
	};
}
