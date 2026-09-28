#include "graph_json.hpp"

#include <base/str/str_utils.hpp>

#include <algorithm>
#include <array>
#include <numeric>
#include <ranges>
#include <string>
#include <vector>

namespace query::internal {

	namespace {
		std::string_view queryKindName(QueryKind kind) {
			switch (kind) {
			case QueryKind::Normal:
				return "Normal";
			case QueryKind::SideInput:
				return "SideInput";
			case QueryKind::Input:
				return "Input";
			case QueryKind::Dummy:
				return "Dummy";
			}
			return "Unknown";
		}

		constexpr std::string_view PST_ACCESS_SIDE_INPUT = "PSTAccessSideInput";

		constexpr std::array<std::string_view, 3> MODULE_STRUCTURE_INPUTS{
			"QueryModuleSideInput",
			"QueryModuleChildSideInput",
			"QuerySubmoduleCountSideInput",
		};

		/**
		 * @brief Drops the nodes marked in @p removed, with every edge to them, and renumbers
		 * the rest. The relative order of the nodes, and so of each deps list, is kept.
		 */
		void removeDumpNodes(DumpGraph& graph, const std::vector<bool>& removed) {
			std::vector<usize> new_index(graph.nodes.size(), 0);
			usize              kept_count = 0;
			for (usize i = 0; i < graph.nodes.size(); ++i)
				if (not removed[i]) new_index[i] = kept_count++;

			std::vector<DumpNode> kept;
			kept.reserve(kept_count);
			for (usize i = 0; i < graph.nodes.size(); ++i) {
				if (removed[i]) continue;
				auto node = std::move(graph.nodes[i]);
				std::erase_if(node.deps, [&removed](usize dep) { return removed[dep]; });
				for (auto& dep: node.deps) dep = new_index[dep];
				kept.push_back(std::move(node));
			}
			graph.nodes = std::move(kept);
		}
	}

	std::string_view nodeCategoryName(const NodeID& node) {
		const auto& data = node.q_id.getData();
		if (data.isInputQuery()) return "input";
		return data.usesStableHashing() ? "stable" : "unstable";
	}

	DumpGraph makeDumpGraph(const QueryGraph::ReducedGraphData& graph) {
		CORE_ASSERT(
			graph.nodes.size() == graph.adjacency.size(), "Reduced graph data is inconsistent"
		);

		// Sort nodes so that two dumps of the same graph are byte-identical.
		std::vector<usize> order(graph.nodes.size());
		std::ranges::iota(order, usize{ 0 });
		std::ranges::sort(order, [&graph](usize l, usize r) {
			return graph.nodes[l] < graph.nodes[r];
		});

		std::vector<usize> new_index(graph.nodes.size());
		for (usize i = 0; i < order.size(); ++i) new_index[order[i]] = i;

		DumpGraph dump;
		dump.nodes.reserve(order.size());
		for (usize old_index: order) {
			const auto& node = graph.nodes[old_index];
			const auto& data = node.q_id.getData();

			DumpNode dump_node{
				.query_name = std::string(data.name),
				.name       = std::string(data.name),
				.query_id   = node.q_id.asInt(),
				.kind       = queryKindName(data.kind),
				.category   = nodeCategoryName(node),
				.preserved  = data.tags.preserve_in_graph,
				.hash       = node.hash.val.toStringHex(),
				.deps       = {},
			};
			dump_node.deps.reserve(graph.adjacency[old_index].size());
			for (usize dep: graph.adjacency[old_index]) {
				CORE_ASSERT(dep < graph.nodes.size(), "Dependency index out of range");
				dump_node.deps.push_back(new_index[dep]);
			}
			std::ranges::sort(dump_node.deps);
			dump.nodes.push_back(std::move(dump_node));
		}
		return dump;
	}

	void renameDumpGraphNodes(DumpGraph& graph) {
		for (auto& node: graph.nodes)
			if (node.query_name == PST_ACCESS_SIDE_INPUT)
				node.name = "Source Code Input";
			else if (std::ranges::contains(MODULE_STRUCTURE_INPUTS, node.query_name))
				node.name = "Module Structure Input";
			else if (node.category == "unstable")
				node.name = "Unstable Node";
	}

	void simplifyDumpGraph(DumpGraph& graph) {
		// 1. Duplicated edges. Deps are kept sorted, so duplicates are next to each other.
		for (auto& node: graph.nodes) {
			std::ranges::sort(node.deps);
			const auto duplicates = std::ranges::unique(node.deps);
			node.deps.erase(duplicates.begin(), duplicates.end());
		}

		// 2. Module tree bookkeeping nodes that only add noise to the picture.
		std::vector<bool> removed(graph.nodes.size(), false);
		for (usize i = 0; i < graph.nodes.size(); ++i) {
			const auto& name = graph.nodes[i].query_name;
			removed[i]       = name == "QuerySubmoduleCountSideInput" || name == "QuerySubmodules";
		}
		removeDumpNodes(graph, removed);

		// 3. Source code inputs that only one node depends on are merged into one node.
		std::vector<usize> dependents(graph.nodes.size(), 0);
		for (const auto& node: graph.nodes)
			for (usize dep: node.deps) ++dependents[dep];

		removed.assign(graph.nodes.size(), false);
		for (const auto& node: graph.nodes) {
			std::vector<usize> own_inputs;
			for (usize dep: node.deps)
				if (graph.nodes[dep].query_name == PST_ACCESS_SIDE_INPUT && dependents[dep] == 1)
					own_inputs.push_back(dep);
			if (own_inputs.size() < 2) continue;

			// Keep the first input as the merged node, it takes over the edges of the others.
			auto& merged = graph.nodes[own_inputs.front()];
			for (usize other: own_inputs | std::views::drop(1)) {
				removed[other]         = true;
				const auto& other_deps = graph.nodes[other].deps;
				merged.deps.insert(merged.deps.end(), other_deps.begin(), other_deps.end());
			}
			std::ranges::sort(merged.deps);
			const auto duplicates = std::ranges::unique(merged.deps);
			merged.deps.erase(duplicates.begin(), duplicates.end());
			merged.name += " times " + std::to_string(own_inputs.size());
		}
		removeDumpNodes(graph, removed);
	}

	void writeDumpGraphAsJson(const DumpGraph& graph, std::string_view stage, std::ostream& out) {
		out << "{\n";
		out << R"(  "stage": ")" << base::escapeString(stage) << "\",\n";
		out << R"(  "nodes": [)";
		for (usize i = 0; i < graph.nodes.size(); ++i) {
			const auto& node = graph.nodes[i];
			out << (i == 0 ? "\n" : ",\n");
			out << R"(    { "index": )" << i;
			out << R"(, "name": ")" << base::escapeString(node.name) << '"';
			out << R"(, "query_id": )" << node.query_id;
			out << R"(, "kind": ")" << node.kind << '"';
			out << R"(, "category": ")" << node.category << '"';
			out << R"(, "preserved": )" << (node.preserved ? "true" : "false");
			out << R"(, "hash": ")" << node.hash << '"';
			out << R"(, "deps": [)";
			for (usize d = 0; d < node.deps.size(); ++d)
				out << (d == 0 ? "" : ", ") << node.deps[d];
			out << "] }";
		}
		out << (graph.nodes.empty() ? "]\n" : "\n  ]\n");
		out << "}\n";
	}

	void writeReducedGraphAsJson(
		const QueryGraph::ReducedGraphData& graph,
		std::string_view                    stage,
		std::ostream&                       out,
		DumpPasses                          passes
	) {
		auto dump = makeDumpGraph(graph);
		if (passes.rename || passes.simplify) renameDumpGraphNodes(dump);
		if (passes.simplify) simplifyDumpGraph(dump);
		writeDumpGraphAsJson(dump, stage, out);
	}
}
