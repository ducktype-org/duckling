#include "query_graph.hpp"

#include <iomanip>
#include <iostream>
#include <ostream>
#include <queue>
#include <ranges>
#include <set>
#include <vector>

namespace query::detail {

	QueryGraph::DependencyStatus QueryGraph::addDependency(NodeID from, NodeID to) {
		CORE_ASSERT(
			node_deps.contains(from), "Node not found in dep graph, call the given query first."
		);
		node_deps.at(from).emplace_back(to);

		// @TODO: see if cycle was created inside dep and propagate as if I was cyclic
		return DependencyStatus::OK;
	}

	std::vector<NodeID> QueryGraph::getNodeDeps(detail::NodeID node_id) const {
		CORE_ASSERT(
			node_deps.contains(node_id), "Node not found in dep graph, call the given query first."
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

			const auto& node = node_deps.at(visited_node_id);
			for (auto& dep: node)
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
		for (auto& [k, v]: node_deps) {
			out << "    ";
			out << "> Query - " << std::setw(5) << std::left;
			out << k.q_id.asInt() << std::setw(30) << std::left << "\"" << k.q_id.getData().name
				<< "\"";
			out << " Key " << k.hash.val << " :=>\n";
			for (auto& dep: v) {
				out << spacing << "(Q: " << "\"" << dep.q_id.getData().name << "\", "
					<< "K: " << dep.hash.val << "),\n";
			}
			if (!v.empty()) out << '\n';
		}
	}

	void QueryGraph::debugPrintForDrawing(std::ostream& out) const {
		out << "Dep Graph: \n";
		out << node_deps.size() << "\n";

		std::map<NodeID, u64> index;
		u64                   id = 0;
		for (auto& [k, v]: node_deps) index[k] = id++;
		for (auto& [k, v]: node_deps)
			for (auto& dep: v) out << index[k] << " " << index[dep] << "\n";
	}

	std::vector<uint8_t> QueryGraph::serialize() const {
			std::vector<uint8_t> buffer;

			size_t total_size = sizeof(size_t); // map_size
			for (const auto& [node, deps] : node_deps) {
				total_size += sizeof(u64) * 2; // q_id + hash_val
				total_size += sizeof(size_t); // deps_size
				total_size += deps.size() * (sizeof(u64) * 2);
			}
			buffer.reserve(total_size);

			// Serialize the size of the node_deps map
			size_t map_size = node_deps.size();
			buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&map_size),
						  reinterpret_cast<const uint8_t*>(&map_size) + sizeof(map_size));

			// Serialize each entry in the map
			for (const auto& [node, deps] : node_deps) {
				// Serialize NodeID (q_id and hash)
				auto q_id = node.q_id.asInt();
				buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&q_id),
							  reinterpret_cast<const uint8_t*>(&q_id) + sizeof(q_id));

				auto hash_val = node.hash.val;
				buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&hash_val),
							  reinterpret_cast<const uint8_t*>(&hash_val) + sizeof(hash_val));

				// Serialize the dependencies vector size
				size_t deps_size = deps.size();
				buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&deps_size),
							  reinterpret_cast<const uint8_t*>(&deps_size) + sizeof(deps_size));

				// Serialize each dependency (NodeID)
				for (const auto& dep : deps) {
					auto dep_q_id = dep.q_id.asInt();
					buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&dep_q_id),
								  reinterpret_cast<const uint8_t*>(&dep_q_id) + sizeof(dep_q_id));

					auto dep_hash_val = dep.hash.val;
					buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&dep_hash_val),
								  reinterpret_cast<const uint8_t*>(&dep_hash_val) + sizeof(dep_hash_val));
				}
			}

			return buffer;
		}

	QueryGraph QueryGraph::deserialize(const std::vector<uint8_t>& data) {
			QueryGraph graph;
			size_t offset = 0;

			// Deserialize the size of the node_deps map
			size_t map_size;
			std::memcpy(&map_size, data.data() + offset, sizeof(map_size));
			offset += sizeof(map_size);

			// Deserialize each entry in the map
			for (size_t i = 0; i < map_size; ++i) {
				// Deserialize NodeID (q_id and hash)
				u64 q_id;
				std::memcpy(&q_id, data.data() + offset, sizeof(q_id));
				offset += sizeof(q_id);

				u64 hash;
				std::memcpy(&hash, data.data() + offset, sizeof(hash.val));
				offset += sizeof(hash);

				NodeID node{.q_id=q_id, .hash={hash}};

				// Deserialize the dependencies vector size
				size_t deps_size;
				std::memcpy(&deps_size, data.data() + offset, sizeof(deps_size));
				offset += sizeof(deps_size);

				// Deserialize each dependency (NodeID)
				std::vector<NodeID> deps;
				for (size_t j = 0; j < deps_size; ++j) {
					u64 dep_q_id;
					std::memcpy(&dep_q_id, data.data() + offset, sizeof(dep_q_id));
					offset += sizeof(dep_q_id);

					u64 dep_hash;
					std::memcpy(&dep_hash.val, data.data() + offset, sizeof(dep_hash.val));
					offset += sizeof(dep_hash.val);

					deps.emplace_back(NodeID{.q_id=dep_q_id, .hash={dep_hash}});
				}

				// Add the deserialized entry to the graph
				graph.node_deps[node] = std::move(deps);
			}

			return graph;
		}
}
