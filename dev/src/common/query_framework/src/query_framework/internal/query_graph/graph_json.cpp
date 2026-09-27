#include "graph_json.hpp"

#include <base/str/str_utils.hpp>

#include <algorithm>
#include <numeric>
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
	}

	std::string_view nodeCategoryName(const NodeID& node) {
		const auto& data = node.q_id.getData();
		if (data.isInputQuery()) return "input";
		return data.usesStableHashing() ? "stable" : "unstable";
	}

	void writeReducedGraphAsJson(
		const QueryGraph::ReducedGraphData& graph, std::string_view stage, std::ostream& out
	) {
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

		out << "{\n";
		out << R"(  "stage": ")" << base::escapeString(stage) << "\",\n";
		out << R"(  "nodes": [)";
		for (usize i = 0; i < order.size(); ++i) {
			const auto& node = graph.nodes[order[i]];
			const auto& data = node.q_id.getData();

			std::vector<usize> deps;
			deps.reserve(graph.adjacency[order[i]].size());
			for (usize dep: graph.adjacency[order[i]]) {
				CORE_ASSERT(dep < graph.nodes.size(), "Dependency index out of range");
				deps.push_back(new_index[dep]);
			}
			std::ranges::sort(deps);

			out << (i == 0 ? "\n" : ",\n");
			out << R"(    { "index": )" << i;
			out << R"(, "name": ")" << base::escapeString(data.name) << '"';
			out << R"(, "query_id": )" << node.q_id.asInt();
			out << R"(, "kind": ")" << queryKindName(data.kind) << '"';
			out << R"(, "category": ")" << nodeCategoryName(node) << '"';
			out << R"(, "preserved": )" << (data.tags.preserve_in_graph ? "true" : "false");
			out << R"(, "hash": ")" << node.hash.val.toStringHex() << '"';
			out << R"(, "deps": [)";
			for (usize d = 0; d < deps.size(); ++d) out << (d == 0 ? "" : ", ") << deps[d];
			out << "] }";
		}
		out << (order.empty() ? "]\n" : "\n  ]\n");
		out << "}\n";
	}
}
