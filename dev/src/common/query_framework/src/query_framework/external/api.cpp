#include "api.hpp"

#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/node_marking.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>

#include <vector>

namespace query::external {

	void setPreviousGraph(::query::internal::QueryGraph&& graph, std::vector<InputData>&& inputs) {
		auto state = ::query::internal::ContextAccess::getState();
		state->setPreviousGraph(std::move(graph));

		::query::internal::markPreviousGraphNodesInputs(std::move(inputs));
	}

	::query::internal::QueryGraph deserialize(std::span<const std::byte> data) {
		return ::query::internal::QueryGraph::deserialize(data);
	}

}  // namespace query::external
