#include "api.hpp"

#include <concurrent/base/locks/assert_lock.hpp>
#include <concurrent/base/locks/with_lock.hpp>
#include <concurrent/worker/worker_manager.hpp>

#include <logger/logger.hpp>
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_data/query_data.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/internal/query_graph/node_making.hpp>
#include <query_framework/internal/query_graph/node_marking.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/internal/query_graph/query_state.hpp>
#include <query_framework/internal/query_metadata/metadata_storage.hpp>
#include <ser/ser.hpp>

#include <string>
#include <vector>

namespace query::external {

	void setPreviousGraphFromRawBytes(std::span<const std::byte> graph_raw_bytes) {
		static concurrent::AssertLock lock;
		concurrent::WithLock          guard(&lock);


		auto state = ::query::internal::ContextAccess::getState();

		// A damaged cache is information, not a failure: the previous graph is dropped and
		// the build carries on with none, exactly as it does on a first build. Two ways to be
		// damaged - bytes that are not a graph, and a graph whose adjacency indices do not
		// match its nodes - and fromReducedGraphData answers the second one itself.
		const auto drop_previous_graph = [&state](const std::string& why) {
			CORE_USER_LOG("Previous query graph was damaged, compiling without it.\n");
			CORE_DEV_LOG(Incremental, "Previous query graph rejected: ", why, "\n");
			state->setPreviousGraph(::query::internal::QueryGraph{});
		};

		auto reduced
			= ::ser::read<::query::internal::QueryGraph::ReducedGraphData>(graph_raw_bytes);
		if (!reduced) {
			drop_previous_graph(reduced.err().message());
			return;
		}

		// Remap NodeIDs while rebuilding so the framework keeps all QueryIDs registered and
		// avoids unstable hash collisions.
		auto graph = ::query::internal::QueryGraph::fromReducedGraphData(
			std::move(*reduced).take(),
			[state](::query::internal::NodeID node) {
				return state->remapUnstableOrUnregisteredNodes(node);
			}
		);
		if (!graph.has_value()) {
			drop_previous_graph("adjacency index out of range");
			return;
		}

		state->setPreviousGraph(std::move(graph).value());
	}

	void markPreviousGraphNodesInputs(std::vector<query::external::InputData>&& inputs) {
		::query::internal::markPreviousGraphNodesInputs(std::move(inputs));
	}

	std::vector<byte> optAndSerializeQueryGraph() {
		auto state         = ::query::internal::ContextAccess::getState();
		auto reduced_graph = state->reduceOptimizeGraph(state->getGraph());
		CORE_ASSERT(reduced_graph.isConsistent(), "Reduced graph data is inconsistent");

		// Writing a graph we have just built cannot fail on the data - a code here means the
		// buffer or the format is wrong, which is a bug rather than a state to recover from.
		std::vector<byte> bytes;
		if (const auto r = ::ser::write(bytes, reduced_graph); !r)
			CORE_PANIC("Failed to serialize the query graph: ", r.err().message());
		return bytes;
	}

	u64 deleteOrphanedDiskCaches() {
		auto state = ::query::internal::ContextAccess::getState();
		return state->cleanupOrphanedDiskCaches();
	}

	void setPreviousMetadataFromRawBytes(std::span<const std::byte> metadata_raw_bytes) {
		auto state = ::query::internal::ContextAccess::getState();

		// No bytes is no previous metadata rather than a damaged stream
		if (metadata_raw_bytes.empty()) {
			state->setPreviousMetadata(::query::internal::MetadataStorage{});
			return;
		}

		// Damaged metadata is dropped, not fatal - same reasoning as the graph above.
		auto metadata = ::ser::read<::query::internal::MetadataStorage>(metadata_raw_bytes);
		if (!metadata) {
			CORE_USER_LOG("Previous metadata was damaged, compiling without it.\n");
			CORE_DEV_LOG(
				Incremental, "Previous metadata rejected: ", metadata.err().message(), "\n"
			);
			state->setPreviousMetadata(::query::internal::MetadataStorage{});
			return;
		}

		state->setPreviousMetadata(std::move(*metadata).take());
	}

	std::vector<byte> serializeMetadata() {
		auto state = ::query::internal::ContextAccess::getState();

		// As with the graph: a failure to write what we are holding is a bug, not a state.
		std::vector<byte> bytes;
		if (const auto r = ::ser::write(bytes, *state->getMetadataStorage()); !r)
			CORE_PANIC("Failed to serialize the query metadata: ", r.err().message());
		return bytes;
	}

	bool prevMetadataExists() {
		auto state = ::query::internal::ContextAccess::getState();
		return state->getPreviousMetadataStorage().has_value();
	}

	void invalidateQueries(
		std::vector<InputData>&&                    new_inputs,
		base::Optional<std::vector<InputData>>      previous_inputs_opt,
		base::Optional<Ref<std::vector<InputData>>> invalidated_inputs_opt
	) {
		CORE_ASSERT(
			!query::Context::areWeInsideQuery(),
			"invalidateQueries() must not be called from inside a query"
		);

		auto state = ::query::internal::ContextAccess::getState();

		// Wait until all query execution has stopped: invalidation mutates the graph, caches and
		// task statuses, so it must not run concurrently with any query work.
		concurrent::worker::WorkerManager::get().waitForAllWorkersFree();

		// Step 0: Find start nodes (inputs) from the previous inputs not present in the new inputs.
		std::vector<internal::NodeID> start_nodes;

		if_opt_some(previous_inputs_opt, previous_inputs) {
			// This is the difference: previous_inputs - new_inputs
			start_nodes = internal::findRemovedInputsFromSelectedInputs(
				previous_inputs, std::move(new_inputs)
			);
		}
		if_opt_none(previous_inputs_opt) {
			start_nodes = internal::findRemovedInputsFromCurrentGraph(std::move(new_inputs));
		}

		// If requested, fill the invalidated_inputs vector with the inputs corresponding to the
		// invalidated nodes.
		if_opt_some(invalidated_inputs_opt, invalidated_inputs) {
			std::ranges::copy(
				start_nodes | std::views::transform([](const internal::NodeID& node) {
					return InputData(node.q_id, node.hash.val);
				}),
				std::back_inserter(*invalidated_inputs)
			);
		}

		// Step 1: Get all nodes to invalidate
		auto nodes_to_invalidate = state->getGraph().getDependentNodes(start_nodes);

		// Step 2: Erase nodes from the graph
		state->getGraphMutable().eraseNodes(nodes_to_invalidate);

		// Step 3: Erase values of the invalidated nodes from their cache
		for (const auto& node: nodes_to_invalidate.dependents_recursive) {
			if (not node.q_id.getData().isInputQuery()) {
				internal::ContextAccess::getState()->getTaskPool()->invalidateTask(node);
				// Disk-cached queries also leave an on-disk artifact; remove it too so invalidated
				// results are not silently reloaded from disk in a later compilation.
				if (node.q_id.getData().tags.can_be_loaded_from_disk)
					node.q_id.getData().cache_data.disk_erase_function(node.hash.val);
				node.q_id.getData().cache_data.erase_function(node.hash.val);
			}

			state->getMetadataStorageMutable()->clearNodeMetadata(node);
			state->clearDiagnosticForNode(node);
		}
	}
}  // namespace query::external
