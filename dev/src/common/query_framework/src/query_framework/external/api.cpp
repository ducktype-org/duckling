#include "api.hpp"

#include <concurrent/base/locks/assert_lock.hpp>
#include <concurrent/base/locks/with_lock.hpp>
#include <concurrent/worker/worker_manager.hpp>

#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_data/query_data.hpp>
#include <query_framework/internal/query_graph/graph_json.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/internal/query_graph/node_making.hpp>
#include <query_framework/internal/query_graph/node_marking.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/internal/query_graph/query_state.hpp>
#include <query_framework/internal/query_metadata/metadata_storage.hpp>

#include <array>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

namespace query::external {

	void setPreviousGraphFromRawBytes(std::span<const std::byte> graph_raw_bytes) {
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
	}

	void markPreviousGraphNodesInputs(std::vector<query::external::InputData>&& inputs) {
		::query::internal::markPreviousGraphNodesInputs(std::move(inputs));
	}

	std::vector<byte> optAndSerializeQueryGraph() {
		auto state         = ::query::internal::ContextAccess::getState();
		auto reduced_graph = state->reduceOptimizeGraph(state->getGraph());
		return ::query::internal::QueryGraph::serializeReducedGraph(std::move(reduced_graph));
	}

	void dumpQueryGraphAsJson(
		QueryGraphDumpStage stage, std::ostream& out, QueryGraphDumpPasses passes
	) {
		auto                                state = ::query::internal::ContextAccess::getState();
		const auto&                         graph = state->getGraph();
		const ::query::internal::DumpPasses internal_passes{ .rename   = passes.rename,
			                                                 .simplify = passes.simplify };
		switch (stage) {
		case QueryGraphDumpStage::PreOptimization:
			::query::internal::writeReducedGraphAsJson(
				graph.toReducedGraphData(), "pre_optimization", out, internal_passes
			);
			return;
		case QueryGraphDumpStage::PostOptimization:
			::query::internal::writeReducedGraphAsJson(
				state->reduceOptimizeGraph(graph), "post_optimization", out, internal_passes
			);
			return;
		}
	}

	std::expected<std::vector<std::filesystem::path>, std::string> dumpQueryGraphsToDirectory(
		const std::filesystem::path& output_dir, QueryGraphDumpPasses passes
	) {
		std::error_code error;
		std::filesystem::create_directories(output_dir, error);
		if (error) {
			return std::unexpected(
				"cannot create directory '" + output_dir.string() + "': " + error.message()
			);
		}

		const std::array<std::pair<const char*, QueryGraphDumpStage>, 2> dumps{ {
			{ "query_graph_pre_opt.json", QueryGraphDumpStage::PreOptimization },
			{ "query_graph_post_opt.json", QueryGraphDumpStage::PostOptimization },
		} };
		std::vector<std::filesystem::path>                               written;
		for (const auto& [file_name, stage]: dumps) {
			auto          path = output_dir / file_name;
			std::ofstream out(path);
			dumpQueryGraphAsJson(stage, out, passes);
			out.close();
			if (!out) return std::unexpected("cannot write query graph to '" + path.string() + "'");
			written.push_back(std::move(path));
		}
		return written;
	}

	u64 deleteOrphanedDiskCaches() {
		auto state = ::query::internal::ContextAccess::getState();
		return state->cleanupOrphanedDiskCaches();
	}

	void setPreviousMetadataFromRawBytes(std::span<const std::byte> metadata_raw_bytes) {
		auto state    = ::query::internal::ContextAccess::getState();
		auto metadata = ::query::internal::MetadataStorage::deserialize(metadata_raw_bytes);
		state->setPreviousMetadata(std::move(metadata));
	}

	std::vector<byte> serializeMetadata() {
		auto state = ::query::internal::ContextAccess::getState();
		return state->getMetadataStorage()->serialize();
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
