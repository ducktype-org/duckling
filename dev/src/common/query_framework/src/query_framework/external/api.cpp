#include "api.hpp"

#include <concurrent/base/locks/assert_lock.hpp>
#include <concurrent/base/locks/with_lock.hpp>

#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/node_marking.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/internal/query_graph/query_state.hpp>
#include <query_framework/internal/query_metadata/metadata_storage.hpp>

#include <vector>

namespace query::external {

	void setPreviousGraphFromRawBytes(
		std::span<const std::byte> graph_raw_bytes, std::vector<InputData>&& inputs
	) {
		static concurrent::AssertLock lock;
		concurrent::WithLock          guard(&lock);


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
}  // namespace query::external
