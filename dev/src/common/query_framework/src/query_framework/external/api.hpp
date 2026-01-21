#pragma once

#include <base/types/bit256.hpp>

#include <query_framework/internal/query_data/query_id.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/query_hash.hpp>

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

}  // namespace query::external
