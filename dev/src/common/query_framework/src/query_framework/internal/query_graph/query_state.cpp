#include "query_state.hpp"

#include <iostream>

namespace query::internal {
	void QueryState::setEntry(NodeID node, NodeID from) {
		query_stack_size++;

		if (node_data.contains(node)) {
			if (node_data.at(node).color == Color::Visiting) {
				// Detect and print the cycle
				std::cerr << "Cycle detected in dependency graph: \n";
				NodeID              current = from;
				std::vector<NodeID> cycle;

				cycle.push_back(node);
				while (current != node && node_data.contains(current)) {
					cycle.push_back(current);
					current = node_data.at(current).parent;
				}
				cycle.push_back(node);

				query_graph.debugPrintNodes(cycle, std::cerr);
				throw base::NotYetImplemented("Query Cycle!");
			}
		}
		node_data.insert_or_assign(node, NodeData(Color::Visiting, from));
		query_graph.node_deps.insert_or_assign(node, std::vector<NodeID>{});
	}

	void QueryState::setExit(NodeID node) {
		CORE_ASSERT(query_stack_size > 0, "Query exit called on empty call stack");
		query_stack_size--;

		node_data.at(node).color = Color::Done;
	}

	base::Optional<base::CRef<QueryGraph>> QueryState::getPreviousGraph() const {
		if (!previous.has_value()) return base::Optional<base::CRef<QueryGraph>>{};
		return &previous.value().graph;
	}

	u64 QueryState::queryStackSize() const { return query_stack_size; }

	void QueryState::setPrevNodeColor(internal::NodeID node, PrevColor color) {
		CORE_ASSERT(previous.has_value(), "PreviousCompilation is not set when setting node color");
		previous->node_colors.insert_or_assign(node, color);
	}

	base::CRef<base::HashMap<NodeID, QueryState::PrevColor>> QueryState::getPreviousNodeColors(
	) const {
		CORE_ASSERT(
			previous.has_value(), "PreviousCompilation is not set when accessing node colors"
		);
		return &previous.value().node_colors;
	}

	void QueryState::setPreviousGraph(QueryGraph&& graph) {
		CORE_ASSERT(!previous.has_value(), "Previous graph is already set");
		previous.emplace(std::move(graph));
	}
}
