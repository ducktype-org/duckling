#pragma once

namespace query {
	/**
	 * Whether the query statistics collection is enabled or not.
	 * This is a compile-time constant for performance reasons.
	 */
	constexpr bool USE_STATS = true;

	/**
	 * @brief This flag is used to track the reverse graph of dependencies in the QueryGraph.
	 * It is used by the Language Server to find the dependent nodes of a given node and invalidate
	 * them without having to traverse the whole graph.
	 */
	extern constinit bool track_reverse_graph;

	/**
	 * If set, incremental compilation is enabled.
	 * This flag is set during driver initialization based on user options.
	 * --no-incremental will disable it.
	 *
	 * @note This is currently set in initializeTheCompiler functions and used in driver::exit and
	 * in QueryFramework.
	 */
	extern constinit bool enable_incremental_compilation;
}
