#include "dep_graph.hpp"
#include "query_impl.hpp"

#include <base/maps.hpp>
#include <iomanip>
#include <vector>
#include <iostream>

using query::detail::NodeID;

template<>
struct std::hash<NodeID> {
	std::size_t operator()(const NodeID& key) const {
		auto l = key.q_id;
		auto r = key.hash.val;

		// this is questionable:
		return l.asInt() * 9'223'372'036'854'775'783UL + r;
	}
};

namespace query::detail {

	constexpr bool operator==(const NodeID& l, const NodeID& r) {
		return l.q_id.asInt() == r.q_id.asInt() and l.hash.val == r.hash.val;
	}

	namespace dep_graph {

		enum class Color {
			Visiting,
			Done,
		};

		struct NodeData {
			Color               color;
			std::vector<NodeID> dependencies;

			/**
			 * @brief NodeID of last node "calling" this query.
			 * Should only hold value when color==Visiting.
			 * Used for cycle recovery.
			 */
			NodeID parent;
		};

		namespace {
			base::HashMap<NodeID, NodeData> node_data;
		}

		void setEntry(NodeID node, NodeID from) {
			if (node_data.contains(node)) {
				if (node_data.at(node).color == Color::Visiting) {
					// @TODO: cycle mark
					std::cerr << "Dep graph at cycle: \n";
					debugPrint();
					throw base::NotYetImplemented("Query Cycle!");
				}
			}
			node_data.insert_or_assign(node, NodeData{ Color::Visiting, {}, from });
		}

		DependencyStatus addDependency(NodeID from, NodeID to) {
			node_data.at(from).dependencies.emplace_back(to);

			// @TODO: see if cycle was created inside dep and propagate as if I was cyclic
			return DependencyStatus::OK;
		}

		void setExit(NodeID node) { node_data.at(node).color = Color::Done; }

		void debugPrint() {
			std::cerr << "Dep Graph: \n";
			std::string spacing(25, ' ');
			for (auto& [k, v]: node_data) {
				std::cerr << "    ";
				std::cerr << "> Query - " << std::setw(5) << std::left;
				std::cerr << k.q_id.asInt() << std::setw(30) << std::left
						  << "\"" + k.q_id.getName() + "\"";
				std::cerr << " Key " << k.hash.val << " :=>\n";
				for (auto& dep: v.dependencies) {
					std::cerr << spacing << "(Q: "
							  << "\"" << dep.q_id.getName() << "\", "
							  << "K: " << dep.hash.val << "),\n";
				}
				if (!v.dependencies.empty()) std::cerr << '\n';
			}
		}

	}
}
