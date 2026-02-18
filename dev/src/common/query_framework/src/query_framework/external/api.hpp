#pragma once

#include <base/types/bit256.hpp>

#include <query_framework/internal/query_data/query_id.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/utils/query_hash.hpp>

#include <cstddef>
#include <span>
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
	};

	/**
	 * Set the previous query graph and mark previous graph input nodes (Input/SideInput)
	 * as Green/Red based on provided input hashes.
	 * This function wires external input knowledge into query internals.
	 * @param graph_raw_bytes Raw bytes of serialized previous query graph.
	 * @param inputs Vector of input data (QueryID + hash) used in previous compilation.
	 */
	void setPreviousGraphFromRawBytes(
		std::span<const std::byte> graph_raw_bytes, std::vector<InputData>&& inputs
	);


	/**
	 * @brief Takes new input data for the current compilation and invalidates queries
	 * that depend on the inputs not present in the new input set.
	 * The query invalidation involves removing them from the graph, erasing their cache entries,
	 * erasing their diagnostics and all the state that need to be erased when a query is invalidated.
	 *
	 * @param new_inputs Vector of input data (QueryID + hash) used in current compilation.
	 * @param previous_inputs_opt Optional vector of input data. If provided, the function will only
	 * search the provided previous inputs for invalidation, (otherwise it will search all previous
	 * inputs from the whole graph). Used when we know the rest of the previous inputs are the same
	 * as the new ones.
	 */
	void invalidateQueries(
		std::vector<InputData>&&               new_inputs,
		base::Optional<std::vector<InputData>> previous_inputs_opt = {}
	);

	/**
	 * @brief Optimize and serialize the current query graph for persistence on disk.
	 */
	[[nodiscard]] std::vector<byte> optAndSerializeQueryGraph();

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


}  // namespace query::external
