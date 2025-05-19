#include "query_graph.hpp"

#include <iomanip>
#include <iostream>
#include <ostream>
#include <queue>
#include <ranges>
#include <set>
#include <vector>

namespace query::detail {
	void QueryGraph::setEntry(NodeID node, NodeID from) {
		query_stack_size++;

		if (node_data.contains(node)) {
			if (node_data.at(node).color == Color::Visiting) {
				// @TODO: cycle mark
				std::cerr << "Dep graph at cycle: \n";
				debugPrint(std::cerr);
				throw base::NotYetImplemented("Query Cycle!");
			}
		}
		node_data.insert_or_assign(
			node, NodeData{ .color = Color::Visiting, .dependencies = {}, .parent = from }
		);
	}

	QueryGraph::DependencyStatus QueryGraph::addDependency(NodeID from, NodeID to) {
		node_data.at(from).dependencies.emplace_back(to);

		// @TODO: see if cycle was created inside dep and propagate as if I was cyclic
		return DependencyStatus::OK;
	}

	void QueryGraph::setExit(NodeID node) {
		CORE_ASSERT(query_stack_size > 0, "Query exit called on empty call stack");
		query_stack_size--;

		node_data.at(node).color = Color::Done;
	}

	std::vector<NodeID> QueryGraph::getNodeDeps(detail::NodeID node_id) const {
		CORE_ASSERT(
			node_data.contains(node_id), "Node not found in dep graph, call the given query first."
		);

		// some simple bfs for now:
		std::set<NodeID>   visited;
		std::queue<NodeID> queue;
		queue.push(node_id);

		while (!queue.empty()) {
			auto visited_node_id = queue.front();
			queue.pop();

			if (visited.contains(visited_node_id)) continue;
			visited.insert(visited_node_id);

			const auto& node = node_data.at(visited_node_id);
			for (auto& dep: node.dependencies)
				if (!visited.contains(dep)) queue.push(dep);
		}

		// make issue for query types (side input/input/standard/etc):
		// it would be cool to print only input ones, but for now we print all of them:

		return { visited.begin(), visited.end() };
	}

	std::vector<NodeID> QueryGraph::getNodeDepsFiltered(
		detail::NodeID node_id, QueryID dependency_id
	) const {
		return getNodeDeps(node_id) | std::views::filter([dependency_id](const NodeID& id) {
				   return id.q_id == dependency_id;
			   })
		     | std::ranges::to<std::vector<NodeID>>();
	}

	void QueryGraph::debugPrint(std::ostream& out) const {
		out << "Dep Graph: \n";
		std::string spacing(25, ' ');
		for (auto& [k, v]: node_data) {
			out << "    ";
			out << "> Query - " << std::setw(5) << std::left;
			out << k.q_id.asInt() << std::setw(30) << std::left << "\"" << k.q_id.getData().name
				<< "\"";
			out << " Key " << k.hash.val << " :=>\n";
			for (auto& dep: v.dependencies) {
				out << spacing << "(Q: " << "\"" << dep.q_id.getData().name << "\", "
					<< "K: " << dep.hash.val << "),\n";
			}
			if (!v.dependencies.empty()) out << '\n';
		}
	}

	void QueryGraph::debugPrintForDrawing(std::ostream& out) const {
		out << "Dep Graph: \n";
		out << node_data.size() << "\n";

		std::map<NodeID, u64> index;
		u64                   id = 0;
		for (auto& [k, v]: node_data) index[k] = id++;
		for (auto& [k, v]: node_data)
			for (auto& dep: v.dependencies) out << index[k] << " " << index[dep] << "\n";
	}

}
