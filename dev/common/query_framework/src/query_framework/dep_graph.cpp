#pragma once

#include "dep_graph.hpp"
#include <base/maps.hpp>
#include <vector>

// @TODO: nodeid hash

namespace query {

	namespace dep_graph {

		enum class Color {
			Visiting,
			Done,
		};

		struct NodeData {
			Color color;
			std::vector<NodeID> dependencies;
		};

		namespace {
			base::HashMap<NodeID, NodeData> node_data;
		}


		void setEntry(NodeID node) {
			// Check for cycle!
			if (node_data.contains(node)) {
				if (node_data.at(node).color == Color::Visiting) {
					throw base::NotYetImplemented("Cycle!");
				}
			}
			node_data.insert_or_assign(node, {Color::Visiting, {} });
		}
		
		DependencyStatus addDependency(NodeID from, NodeID to) {
			node_data.at(from).dependencies.emplace_back(to);
		}
		
		void setExit(NodeID node) {
			// @TODO
		}

	}
}
