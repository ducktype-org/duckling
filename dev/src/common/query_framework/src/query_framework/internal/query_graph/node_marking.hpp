#pragma once

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
}
