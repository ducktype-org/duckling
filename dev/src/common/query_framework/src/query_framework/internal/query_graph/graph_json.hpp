/**
 * @file graph_json.hpp
 * @brief Human- and tool-friendly JSON dump of a query graph, used for inspecting the graph
 * before and after the incremental-compilation graph optimization.
 */
#pragma once

#include "query_graph.hpp"

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace query::internal {

	/**
	 * @brief Short category of a node, used by graph drawing tools.
	 * @return `"input"` for Input and SideInput queries, otherwise `"stable"` or `"unstable"`
	 * depending on the hashing used by the query.
	 */
	[[nodiscard]] std::string_view nodeCategoryName(const NodeID& node);

	/**
	 * @brief One node of a DumpGraph, i.e. exactly what gets written for it in the JSON document.
	 */
	struct DumpNode {
		std::string        query_name;  ///< Name of the query, never changed by the passes.
		std::string        name;        ///< Displayed name, the passes may rename it.
		u64                query_id = 0;
		std::string_view   kind;
		std::string_view   category;
		bool               preserved = false;
		std::string        hash;
		std::vector<usize> deps;  ///< Indices into DumpGraph::nodes.
	};

	/**
	 * @brief Index-based copy of a query graph that the dump passes can rewrite before it is
	 * written as JSON.
	 */
	struct DumpGraph {
		std::vector<DumpNode> nodes;
	};

	/**
	 * @brief Builds a DumpGraph from @p graph, with nodes sorted by NodeID so that the output is
	 * deterministic, and dependency indices following that order.
	 */
	[[nodiscard]] DumpGraph makeDumpGraph(const QueryGraph::ReducedGraphData& graph);

	/**
	 * @brief Which passes run on a DumpGraph before it is written.
	 */
	struct DumpPasses {
		bool rename            = false;  ///< Run renameDumpGraphNodes().
		bool simplify          = false;  ///< Run renameDumpGraphNodes(), then simplifyDumpGraph().
		bool remove_dead_nodes = false;  ///< Run removeDeadDumpNodes(), after the passes above.
	};

	/**
	 * @brief Rename pass: gives input nodes readable names and hides unstable query names.
	 * @details `PSTAccessSideInput` becomes `Source Code Input`; the module structure side inputs
	 * (`QueryModuleSideInput`, `QueryModuleChildSideInput`, `QuerySubmoduleCountSideInput`) become
	 * `Module Structure Input`; `QueryFileSideInput` becomes `File Structure Input`; every node of
	 * the `unstable` category becomes `Unstable Node`.
	 */
	void renameDumpGraphNodes(DumpGraph& graph);

	/**
	 * @brief Simplify pass: makes the graph smaller while keeping its overall shape.
	 * @details In this order: removes duplicated edges; removes `QuerySubmoduleCountSideInput`
	 * and `QuerySubmodules` nodes together with their edges; then, for every node that depends
	 * on two or more `PSTAccessSideInput` nodes that no other node depends on, replaces those
	 * inputs with a single `<name> (repeated N times)` node. Nodes are matched by their query name,
	 * so the pass works the same with and without the rename pass.
	 */
	void simplifyDumpGraph(DumpGraph& graph);

	/**
	 * @brief Dead node pass: removes every non-input node without dependencies, recursively, so
	 * a node whose dependencies all got removed goes too.
	 * @details Such a node never depends on an input, so it can never be invalidated. Input nodes
	 * are always kept: they are where the graph starts, and removing them too would remove every
	 * node of the graph.
	 */
	void removeDeadDumpNodes(DumpGraph& graph);

	/**
	 * @brief Writes @p graph to @p out as a self-contained JSON document.
	 * @details Every node has an `index`, its `name`, `query_id`, `kind`, `category`, `preserved`
	 * flag, key `hash` and the list of its dependencies (`deps`) as indices into `nodes`.
	 * @param stage Free-form label stored in the document (e.g. "pre_optimization").
	 */
	void writeDumpGraphAsJson(const DumpGraph& graph, std::string_view stage, std::ostream& out);

	/**
	 * @brief Writes @p graph to @p out as a self-contained JSON document, after running the
	 * requested @p passes on it. See makeDumpGraph() and writeDumpGraphAsJson().
	 * @param stage Free-form label stored in the document (e.g. "pre_optimization").
	 */
	void writeReducedGraphAsJson(
		const QueryGraph::ReducedGraphData& graph,
		std::string_view                    stage,
		std::ostream&                       out,
		DumpPasses                          passes = {}
	);
}
