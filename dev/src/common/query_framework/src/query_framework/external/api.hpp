#pragma once

#include <base/pointers/ref.hpp>
#include <base/types/bit256.hpp>

#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_data/query_id.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/internal/query_graph/query_state.hpp>
#include <query_framework/internal/query_metadata/metadata_storage.hpp>
#include <query_framework/utils/query_hash.hpp>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <ostream>
#include <span>
#include <string>
#include <vector>

/**
 * This is query external API used by the compiler driver.
 * This API allows the driver to set previous query graph and provide input data for incremental
 * compilation.
 */
namespace query::external {

	struct InputData final {
		// Same data carried by internal::NodeID
		query::internal::QueryID q_id;
		query::QueryStableHash   hash;

		InputData(query::internal::QueryID q_id, query::QueryStableHash hash):
			  q_id(q_id),
			  hash(hash) {}

		bool operator==(const InputData& other) const {
			return q_id == other.q_id && hash == other.hash;
		}
	};

	/**
	 * @brief Structure to hold metadata with its associated InputData (for previous nodes).
	 * @tparam MetadataT The metadata type.
	 */
	template<typename MetadataT>
	struct MetadataInfo final {
		InputData       input_data;
		CRef<MetadataT> value;
	};

	/**
	 * Set the previous query graph from raw bytes.
	 * Driver must call this before setPreviousMetadataFromRawBytes().
	 * This function wires external input knowledge into query internals.
	 * @param graph_raw_bytes Raw bytes of serialized previous query graph.
	 */
	void setPreviousGraphFromRawBytes(std::span<const std::byte> graph_raw_bytes);

	/**
	 * @brief Mark previous graph input nodes (Input/SideInput) as Green/Red based on provided input
	 * hashes.
	 * @param inputs Vector of input data (QueryID + hash) used in previous compilation.
	 * Driver must call this function after collecting all input data from its packages.
	 * This function wires external input knowledge into query internals.
	 */
	void markPreviousGraphNodesInputs(std::vector<query::external::InputData>&& inputs);


	/**
	 * @brief Takes new input data for the current compilation and invalidates queries
	 * that depend on the inputs not present in the new input set.
	 * If previous_inputs_opt is provided, the function will only invalidate inputs after
	 * subtracting previous_inputs_opt - new_inputs. The query invalidation involves removing them
	 * from the graph, erasing their cache entries, erasing their diagnostics and all the state that
	 * needs to be erased when a query is invalidated. For queries cached on disk, their on-disk
	 * artifacts are also deleted.
	 *
	 * @note This is for incremental LS.
	 * @note This function waits for all workers to become free, i.e. until all query execution
	 * has stopped, before invalidating.
	 * @warning Should not be executed concurrently with any query execution.
	 * @warning Must not be called from a worker thread, as waiting for all workers to be free
	 * would deadlock.
	 *
	 * @param new_inputs Vector of input data (QueryID + hash) used in current compilation.
	 * @param previous_inputs_opt Optional vector of input data. If provided, the function will only
	 * invalidate previous_inputs_opt - new_inputs. If not provided will invalidate
	 * all_inputs_in_graph - new_inputs.
	 * @param invalidated_inputs_opt[out] Optional output vector of input data that will be filled
	 * with the inputs corresponding to the invalidated nodes.
	 *
	 * Used when we know the rest of the previous inputs are the same as the new ones.
	 */
	void invalidateQueries(
		std::vector<InputData>&&                    new_inputs,
		base::Optional<std::vector<InputData>>      previous_inputs_opt    = {},
		base::Optional<Ref<std::vector<InputData>>> invalidated_inputs_opt = {}
	);

	/**
	 * @brief Optimize and serialize the current query graph for persistence on disk.
	 */
	[[nodiscard]] std::vector<byte> optAndSerializeQueryGraph();

	/**
	 * @brief Which state of the query graph dumpQueryGraphAsJson() writes.
	 */
	enum class QueryGraphDumpStage {
		PreOptimization,   ///< The whole current graph, as recorded during compilation.
		PostOptimization,  ///< The graph after reduceOptimizeGraph, i.e. what gets persisted.
	};

	/**
	 * @brief Optional passes that make a dumped query graph smaller and easier to read. They only
	 * change the dump, never the graph itself.
	 */
	struct QueryGraphDumpPasses {
		/// Give input nodes readable names (`Source Code Input`, `Module Structure Input`) and
		/// call every unstable node `Unstable Node`.
		bool rename = false;
		/// Remove duplicated edges and module tree bookkeeping nodes, and merge source code inputs
		/// that only one node depends on. Implies #rename.
		bool simplify = false;
	};

	/**
	 * @brief Write the current query graph as a self-contained JSON document, for inspection by
	 * external tools (see `dev/scripts/query_graph/`).
	 * @param passes Passes to run on the graph just before it is written.
	 * @note Experimental and meant for debugging; the format is not persisted anywhere.
	 * @warning Must not be called while queries are executing.
	 */
	void dumpQueryGraphAsJson(
		QueryGraphDumpStage stage, std::ostream& out, QueryGraphDumpPasses passes = {}
	);

	/**
	 * @brief Paths of the files dumpQueryGraphsToDirectory() wrote, or why it could not write them.
	 */
	using QueryGraphDumpResult = std::expected<std::vector<std::filesystem::path>, std::string>;

	/**
	 * @brief Write both dumpQueryGraphAsJson() stages into @p output_dir, as
	 * `query_graph_pre_opt.json` and `query_graph_post_opt.json`, creating the directory if needed.
	 * @param passes Passes to run on both graphs just before they are written.
	 * @return The paths of the written files, or a message saying what could not be written.
	 * @warning Must not be called while queries are executing.
	 */
	[[nodiscard]] QueryGraphDumpResult dumpQueryGraphsToDirectory(
		const std::filesystem::path& output_dir, QueryGraphDumpPasses passes = {}
	);

	/**
	 * @brief Delete on-disk caches of previous-graph nodes that were not merged into the current
	 * graph (e.g. because their query hash changed): they will be dropped from the serialized
	 * graph, so their disk cache would otherwise be orphaned forever.
	 * @return The number of orphaned query caches removed.
	 */
	u64 deleteOrphanedDiskCaches();

	/**
	 * @brief Set the previous compilation metadata from serialized bytes.
	 * Must be called after setPreviousGraphFromRawBytes.
	 * @param metadata_raw_bytes Raw bytes of serialized metadata storage.
	 */
	void setPreviousMetadataFromRawBytes(std::span<const std::byte> metadata_raw_bytes);

	/**
	 * @brief Serialize the current metadata storage for persistence on disk.
	 * @return Serialized metadata as raw bytes.
	 */
	[[nodiscard]] std::vector<byte> serializeMetadata();

	enum class MetadataStorageKind { Current, Previous };

	/**
	 * @brief Get all metadata of a specific type from all nodes in the previous compilation.
	 *
	 * Efficiently iterates through all previous nodes once, collecting metadata of the given type.
	 *
	 * @tparam MetadataT The metadata type to retrieve (must derive from BaseMetadata)
	 * @return std::vector<MetadataInfo<MetadataT>> Metadata with associated InputData.
	 *         Returns empty vector if no previous metadata exists
	 *         or no metadata of this type was found.
	 */
	template<typename MetadataT, MetadataStorageKind Kind>
	requires std::derived_from<MetadataT, internal::BaseMetadata> [[nodiscard]]
	std::vector<MetadataInfo<MetadataT>> getMetadataFromAllNodesImpl() {
		auto state = ::query::internal::ContextAccess::getState();

		base::Optional<CRef<query::internal::MetadataStorage>> storage;
		if constexpr (Kind == MetadataStorageKind ::Previous)
			storage = state->getPreviousMetadataStorage();
		else
			storage = state->getMetadataStorage();

		if (!storage.has_value()) return {};

		auto internal_result = storage.value()->template getMetadataFromAllNodes<MetadataT>();

		std::vector<MetadataInfo<MetadataT>> result;
		result.reserve(internal_result.size());

		for (const auto& info: internal_result) {
			result.push_back(MetadataInfo<MetadataT>{
				.input_data = InputData(info.node_id.q_id, info.node_id.hash.val),
				.value      = info.value,
			});
		}

		return result;
	}

	template<typename MetadataT>
	auto getMetadataFromAllPrevNodes() {
		return getMetadataFromAllNodesImpl<MetadataT, MetadataStorageKind::Previous>();
	}

	template<typename MetadataT>
	auto getMetadataFromAllCurrentNodes() {
		return getMetadataFromAllNodesImpl<MetadataT, MetadataStorageKind::Current>();
	}

	/**
	 * @brief Check if previous metadata exists.
	 * @note this is used mostly for assertions.
	 */
	bool prevMetadataExists();

}  // namespace query::external
