// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

namespace query {
	/**
	 * Whether the query statistics collection is enabled or not.
	 * This is a compile-time constant for performance reasons.
	 */
	constexpr bool USE_STATS = true;


	/**
	 * @brief This flag is used to enable the query graph.
	 * The query graph is needed for incremental compilation and for the Language Server.
	 * It is currently enabled by default
	 */
	extern constinit bool enable_query_graph;

	/**
	 * @brief Enables or disables tracking of the reverse graph of dependencies in the QueryGraph.
	 * This flag is used to track the reverse graph of dependencies in the QueryGraph.
	 * It is used by the Language Server to find the dependent nodes of a given node and invalidate
	 * them without having to traverse the whole graph.
	 * @note the enable_query_graph flag must be enabled when using this
	 */
	void setTrackReverseGraph(bool value);

	/**
	 * @brief Returns whether tracking of the reverse graph of dependencies in the QueryGraph is
	 * enabled.
	 */
	bool getTrackReverseGraph();
}
