/**
 * @file graph_json.hpp
 * @brief Human- and tool-friendly JSON dump of a query graph, used for inspecting the graph
 * before and after the incremental-compilation graph optimization.
 */
#pragma once

#include "query_graph.hpp"

#include <ostream>
#include <string_view>

namespace query::internal {

	/**
	 * @brief Short category of a node, used by graph drawing tools.
	 * @return `"input"` for Input and SideInput queries, otherwise `"stable"` or `"unstable"`
	 * depending on the hashing used by the query.
	 */
	[[nodiscard]] std::string_view nodeCategoryName(const NodeID& node);

	/**
	 * @brief Writes @p graph to @p out as a self-contained JSON document.
	 * @details Nodes are sorted by NodeID so the output is deterministic. Every node has an
	 * `index`, its query `name`, `kind`, `category`, `preserved` flag, key `hash` and the list of
	 * its dependencies (`deps`) as indices into the `nodes` array.
	 * @param stage Free-form label stored in the document (e.g. "pre_optimization").
	 */
	void writeReducedGraphAsJson(
		const QueryGraph::ReducedGraphData& graph, std::string_view stage, std::ostream& out
	);
}
