#include "api.hpp"

#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_data/query_data.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/internal/query_graph/node_making.hpp>
#include <query_framework/internal/query_graph/node_marking.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/internal/query_graph/query_state.hpp>
#include <query_framework/internal/query_metadata/metadata_storage.hpp>

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

	std::vector<byte> optAndSerializeQueryGraph() {
		auto state         = ::query::internal::ContextAccess::getState();
		auto reduced_graph = state->reduceOptimizeGraph(state->getGraph());
		return ::query::internal::QueryGraph::serializeReducedGraph(std::move(reduced_graph));
	}

	void setPreviousMetadataFromRawBytes(std::span<const std::byte> metadata_raw_bytes) {
		auto state    = ::query::internal::ContextAccess::getState();
		auto metadata = ::query::internal::MetadataStorage::deserialize(metadata_raw_bytes);
		state->setPreviousMetadata(std::move(metadata));
	}

	std::vector<byte> serializeMetadata() {
		auto state = ::query::internal::ContextAccess::getState();
		return state->getMetadataStorage().serialize();
	}

	void invalidateQueries(std::vector<InputData>&& new_inputs) {
		auto state = ::query::internal::ContextAccess::getState();

		// Step 1: Get all nodes to invalidate
		auto start_nodes = internal::findInputsRemovedFromCurrentGraph(std::move(new_inputs));
		auto nodes_to_invalidate = state->getGraphMutable()->getDependentNodes(start_nodes);

		// Step 2: Erase nodes from the graph
		state->getGraphMutable()->eraseNodes(nodes_to_invalidate);

		// Step 3: Erase values of the invalidated nodes from their cache
		for (const auto& node: nodes_to_invalidate) {
			variant_match(node.q_id.getData().impl_data.value().erase_function) {
				variant_case(internal::QueryImplData::EraseFunctionStableType, erase_func) {
					erase_func(node.hash.val);
				}
				variant_case(internal::QueryImplData::EraseFunctionUnstableType, erase_func) {
					erase_func(u64(node.hash.val));
				}
			}
			state->getMetadataStorageMutable()->clearNodeMetadata(node);
			state->clearDiagnosticForNode(node);
		}
	}
}  // namespace query::external
