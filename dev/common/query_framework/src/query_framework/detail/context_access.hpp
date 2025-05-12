#pragma once

#include "../context.hpp"

namespace query::detail {
	/**
	 * @brief Internal helper struct used to access context private state
	 */
	struct ContextAccess final {
		static auto make(NodeID my_node) { return Context(my_node); }

		static Ref<detail::QueryGraph> getGraph() { return &Context::main_query_graph; }
	};
}
