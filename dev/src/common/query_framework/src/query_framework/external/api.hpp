#pragma once

#include <base/types/bit256.hpp>

#include <query_framework/query_hash.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/internal/query_data/query_id.hpp>

#include <span>
#include <cstddef>
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
		query::QueryStableHash              hash;

		InputData(query::internal::QueryID q_id, query::QueryStableHash hash): q_id(q_id), hash(hash) {}
	};

	/**
	 * Set the previous query graph and mark previous graph input nodes (Input/SideInput)
	 * as Green/Red based on provided input hashes.
	 * This function wires external input knowledge into query internals.
	 */
	void setPreviousGraph(query::internal::QueryGraph&& graph, std::vector<InputData>&& inputs);

	/**
	 * Wrapper to deserialize a query graph from raw bytes without exposing internals in callers.
	 */
	query::internal::QueryGraph deserialize(std::span<const std::byte> data);

}  // namespace query::external
