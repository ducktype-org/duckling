#pragma once

namespace query {
	/**
	 * Whether the query statistics collection is enabled or not.
	 * This is a compile-time constant for performance reasons.
	 *
	 * @TODO: #2024 Query stats are not thread safe, so they are currently disabled by default.
	 * Change that.
	 */
	constexpr bool USE_STATS = false;

	/**
	 * @brief This flag is used to track the reverse graph of dependencies in the QueryGraph.
	 * It is used by the Langauge Server to find the dependent nodes of a given node and invalidate
	 * them without having to traverse the whole graph.
	 */
	inline bool track_reverse_graph = false;
}
