// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <query_framework/internal/query_graph/node_id.hpp>

#include <vector>

namespace query::external {
	struct InputData;
}

namespace query::internal {

	/**
	 * Marks the input nodes in the previous query graph Red / Green based on the provided input
	 * data. This function is used for incremental compilation.
	 */
	void markPreviousGraphNodesInputs(std::vector<query::external::InputData> inputs);

	/**
	 * @brief Get the NodeIDs of the input nodes in the current graph
	 * that are not present in the new inputs.
	 *
	 * For example if the current graph had input nodes {1,2}
	 * and we call this function with new inputs {2,3},
	 * it should return the NodeID corresponding to input {1}.
	 *
	 * @note This is for incremental Language Server.
	 * @warning Should not be executed concurrently with query multi-thread execution.
	 *
	 * @param new_inputs All new inputs given to the compiler.
	 */
	std::vector<NodeID> findRemovedInputsFromCurrentGraph(
		std::vector<query::external::InputData> new_inputs
	);

	/**
	 * @brief Same as @p findRemovedInputsFromCurrentGraph but compares the new inputs
	 * with a selected set of previous inputs.
	 *
	 * It is used by the Language Server if we only want to invalidate
	 * input's gathered from one source file and keep all other source files intact.
	 */
	std::vector<NodeID> findRemovedInputsFromSelectedInputs(
		const std::vector<external::InputData>& selected_inputs,
		std::vector<query::external::InputData> new_inputs
	);
}
