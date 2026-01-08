#include "api.hpp"

#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/node_marking.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/internal/query_graph/query_state.hpp>

#include <vector>

namespace query::external {

	void setPreviousGraphFromRawBytes(
		std::span<const std::byte> graph_raw_bytes, std::vector<InputData>&& inputs
	) {
		auto state = ::query::internal::ContextAccess::getState();
		// Remap NodeIDs while deserializing so the framework keeps all QueryIDs registered and
		// avoids unstable hash collisions.
		::query::internal::QueryGraph graph = ::query::internal::QueryGraph::deserialize(
			graph_raw_bytes,
			[state](::query::internal::NodeID node) {
				return state->remapUnstableOrUnregisteredNodes(node);
			}
		);

		state->setPreviousGraph(std::move(graph));

		::query::internal::markPreviousGraphNodesInputs(std::move(inputs));
	}

	std::vector<byte> serializeQueryGraph() {
		auto state = ::query::internal::ContextAccess::getState();
		state->reduceOptimizeGraph();
		auto graph_ref = state->getGraphMutable();
		return graph_ref->serialize();
	}
}  // namespace query::external
