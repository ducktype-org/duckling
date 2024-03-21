#include "dep_graph.hpp"
#include <base/maps.hpp>
#include <vector>
#include <iostream>

template <>
struct std::hash<::query::NodeID> {
	std::size_t operator()(const ::query::NodeID& key) const {
		auto l = key.q_id;
		auto r = key.hash.val;
		
		// this is questionable:
		return l * 9223372036854775783UL + r;
	}
};

namespace query {

	constexpr bool operator==(const NodeID& l, const NodeID& r) {
		return l.q_id == r.q_id and l.hash.val == r.hash.val;
	}

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
			if (node_data.contains(node)) {
				if (node_data.at(node).color == Color::Visiting) {
					// @TODO: cycle mark
					std::cerr << "Dep graph at cycle: \n";
					debugPrint();
					throw base::NotYetImplemented("Query Cycle!");
				}
			}
			node_data.insert_or_assign(node, NodeData{Color::Visiting, {} });
		}
		
		DependencyStatus addDependency(NodeID from, NodeID to) {
			node_data.at(from).dependencies.emplace_back(to);

			// @TODO: see if cycle was created inside dep and propagate as if I was cyclic
			return DependencyStatus::OK;
		}
		
		void setExit(NodeID node) {
			node_data.at(node).color = Color::Done;
		}


		void debugPrint() {
			// @TODO: optional pratty key printing

			std::cerr << "Dep Graph: \n";
			for (auto& [k, v]: node_data) {
				std::cerr << "    ";
				std::cerr << "Query " << k.q_id << ", Key " << k.hash.val << "  <--- ";
				for (auto& dep: v.dependencies) {
					std::cerr << "(Q: " << dep.q_id << ", " << "K: " << dep.hash.val << ")"; 
					std::cerr << ", ";
				}
				std::cerr << "\n";
			}
		}

	}
}
