#include "query_graph.hpp"

#include "node_id.hpp"

#include <algorithm>
#include <base/pointers/ref.hpp>
#include <base/types/bit256.hpp>
#include <base/types/ints.hpp>  // IWYU pragma: export

#include <cstring>
#include <iomanip>
#include <ostream>
#include <queue>
#include <ranges>
#include <set>
#include <stack>
#include <unordered_set>
#include <vector>

namespace query::internal {

	QueryGraph::DependencyStatus QueryGraph::addDependency(NodeID from, NodeID to) {
		CORE_ASSERT(
			node_deps.contains(from), "Node not found in dep graph, call the given query first."
		);
		node_deps.at(from).emplace_back(to);

		if (TRACK_REVERSE_GRAPH) {
			if (!node_reverse_deps.contains(to))
				node_reverse_deps.insert_or_assign(to, std::vector<NodeID>{});
			node_reverse_deps.at(to).emplace_back(from);
		}

		// @TODO: see if cycle was created inside dep and propagate as if I was cyclic
		return DependencyStatus::OK;
	}

	std::vector<NodeID> QueryGraph::getNodeDeps(internal::NodeID node_id) const {
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

			CORE_ASSERT(
				node_deps.contains(visited_node_id),
				"Node not found in dep graph. Node ID: ",
				visited_node_id.q_id.getData().name,
				" Key: ",
				visited_node_id.hash.val.toStringHex()
			);

			const auto& node = node_deps.at(visited_node_id);
			for (auto& dep: node)
				if (!visited.contains(dep)) queue.push(dep);
		}

		// make issue for query types (side input/input/standard/etc):
		// it would be cool to print only input ones, but for now we print all of them:

		return { visited.begin(), visited.end() };
	}

	std::vector<NodeID> QueryGraph::getNodeDepsFiltered(
		internal::NodeID node_id, QueryID dependency_id
	) const {
		return getNodeDeps(node_id) | std::views::filter([dependency_id](const NodeID& id) {
				   return id.q_id == dependency_id;
			   })
		     | std::ranges::to<std::vector<NodeID>>();
	}

	const std::vector<NodeID>& QueryGraph::getDirectDependencies(const NodeID& node_id) const {
		CORE_ASSERT(
			node_deps.contains(node_id),
			"Node not found in dep graph when requesting direct dependencies."
		);
		return node_deps.at(node_id);
	}

	void QueryGraph::debugPrint(std::ostream& out) const {
		out << "Dep Graph: \n";
		std::vector<NodeID> all_nodes;
		for (const auto& [k, _]: node_deps) all_nodes.push_back(k);
		debugPrintNodes(all_nodes, out);
	}

	void QueryGraph::debugPrintForDrawing(std::ostream& out) const {
		out << "Dep Graph: \n";
		out << node_deps.size() << "\n";

		std::map<NodeID, u64> index;
		u64                   id = 0;
		for (auto& [k, v]: node_deps) {
			index[k] = id++;
			out << id << " " << k.q_id.getData().name << "\n";
		}

		for (auto& [k, v]: node_deps)
			for (auto& dep: v) out << index[k] << " " << index[dep] << "\n";
	}

	void QueryGraph::debugPrintNodes(const std::vector<NodeID>& nodes, std::ostream& out) const {
		std::string spacing(25, ' ');
		for (const auto& n: nodes) {
			const auto& node_data_entry = node_deps.at(n);
			out << "    > Query - " << std::setw(5) << std::left;
			out << n.q_id.asInt() << "\"" << n.q_id.getData().name << "\"";
			out << " Key " << n.hash.val << " :=>\n";
			for (const auto& dep: node_data_entry) {
				out << spacing << "(Q: \"" << dep.q_id.getData().name << "\", "
					<< "K: " << dep.hash.val << "),\n";
			}
			if (!node_data_entry.empty()) out << '\n';
		}
	}

	bool QueryGraph::compare(const QueryGraph& other) const {
		if (node_deps.size() != other.node_deps.size()) return false;

		for (const auto& [node, deps]: node_deps) {
			auto it = other.node_deps.find(node);
			if (it == other.node_deps.end() || deps != it->second) return false;
		}

		for (const auto& [node, deps]: other.node_deps) {
			auto it = node_deps.find(node);
			if (it == node_deps.end() || deps != it->second) return false;
		}

		return true;
	}

	std::vector<byte> QueryGraph::serialize() const {
		const usize         node_count = node_deps.size();
		std::vector<NodeID> nodes;
		nodes.reserve(node_count);
		base::HashMap<NodeID, usize> node_to_index;
		node_to_index.reserve(node_count);

		usize next_index = 0;
		for (const auto& [node, _]: node_deps) {
			node_to_index.emplace(node, next_index++);
			nodes.push_back(node);
		}

		std::vector<std::vector<usize>> adjacency(node_count);
		for (const auto& node: nodes) {
			const auto& deps = node_deps.at(node);
			auto&       out  = adjacency.at(node_to_index.at(node));
			out.reserve(deps.size());
			for (const auto& dep: deps) {
				CORE_ASSERT(
					node_to_index.contains(dep),
					"Dependency node missing from graph during serialization."
				);
				out.push_back(node_to_index.at(dep));
			}
		}

		return serializeReducedGraph(
			ReducedGraphData{ .nodes = std::move(nodes), .adjacency = std::move(adjacency) }
		);
	}

	std::vector<byte> QueryGraph::serializeReducedGraph(ReducedGraphData reduced_graph) {
		using HVType  = decltype(NodeID::hash.val.data);
		using QIDType = decltype(QueryID::val);

		constexpr usize NODE_ID_SIZE
			= sizeof(QIDType) + sizeof(HVType);  // Size of NodeID (q_id and hash)

		auto& nodes     = reduced_graph.nodes;
		auto& adjacency = reduced_graph.adjacency;
		CORE_ASSERT(nodes.size() == adjacency.size(), "Reduced graph data is inconsistent");

		const usize node_count  = nodes.size();
		usize       total_edges = 0;
		for (const auto& deps: adjacency) total_edges += deps.size();

		std::vector<byte> buffer;

		// Calculate the total size of the serialized data
		usize total_size = sizeof(usize);  // node_count
		total_size += node_count * NODE_ID_SIZE;
		total_size += node_count * sizeof(usize);
		total_size += total_edges * sizeof(usize);
		buffer.reserve(total_size);

		auto write = [&](const auto& value) -> void {
			using T               = std::decay_t<decltype(value)>;
			auto serialized_value = std::bit_cast<std::array<byte, sizeof(T)>>(value);
			buffer.insert(buffer.end(), serialized_value.begin(), serialized_value.end());
		};

		auto write_node_id = [&](const NodeID& node) -> void {
			write(node.q_id.val);
			write(node.hash.val.data);
		};

		// Serialize the size of the node list
		write(node_count);
		for (const auto& node: nodes) write_node_id(node);

		for (usize idx = 0; idx < node_count; ++idx) {
			const auto& deps = adjacency.at(idx);
			write(deps.size());
			for (usize dep_idx: deps) {
				CORE_ASSERT(dep_idx < node_count, "Dependency index out of range in reduced graph");
				write(dep_idx);
			}
		}

		return buffer;
	}

	QueryGraph QueryGraph::deserialize(
		std::span<const byte> data, std::function<NodeID(NodeID)> node_mapper
	) {
		if (!node_mapper) node_mapper = [](NodeID node) { return node; };
		using HType   = decltype(NodeID::hash.val);
		using HVType  = decltype(NodeID::hash.val.data);
		using QIDType = decltype(QueryID::val);
		// Compile-time check to ensure HVType and QIDType are trivial
		static_assert(std::is_trivial_v<HVType>, "HVType must be a trivial type.");
		static_assert(std::is_trivial_v<QIDType>, "QIDType must be a trivial type.");

		QueryGraph  graph;
		usize       offset    = 0;
		const usize data_size = data.size();


		auto read = [&](auto& dest) -> void {
			using T = std::decay_t<decltype(dest)>;

			if (offset + sizeof(T) > data_size)
				throw std::out_of_range("Buffer size exceeded during deserialization");

			std::memcpy(&dest, data.data() + offset, sizeof(T));

			offset += sizeof(T);
		};

		auto read_node_id = [&]() -> NodeID {
			QIDType q_id = 0;
			read(q_id);

			HVType hash;
			read(hash);

			return NodeID(QueryID(q_id), { HType(hash) });
		};

		// Deserialize the size of the node list
		usize map_size = 0;
		read(map_size);

		std::vector<NodeID> nodes;
		nodes.reserve(map_size);
		// Deserialize each node entry
		for (usize i = 0; i < map_size; ++i) {
			NodeID raw_node = read_node_id();
			nodes.emplace_back(node_mapper(raw_node));
		}

		// Deserialize the adjacency lists
		for (usize node_index = 0; node_index < map_size; ++node_index) {
			usize deps_size = 0;
			read(deps_size);

			std::vector<NodeID> deps;
			deps.reserve(deps_size);

			// Deserialize each dependency index
			for (usize j = 0; j < deps_size; ++j) {
				usize dep_index = 0;
				read(dep_index);
				if (dep_index >= nodes.size())
					throw std::out_of_range("Dependency index out of range during deserialization");
				deps.emplace_back(nodes.at(dep_index));
			}

			auto [it, inserted] = graph.node_deps.emplace(nodes.at(node_index), std::move(deps));
			if (!inserted) CORE_PANIC("Duplicate node detected during deserialization");
		}

		// Ensure that the entire buffer was consumed
		CORE_ASSERT(offset == data_size, "Deserialization did not consume the entire buffer");

		return graph;
	}

	std::vector<NodeID> QueryGraph::getAllNodes() const {
		std::vector<NodeID> nodes;
		nodes.reserve(node_deps.size());
		for (const auto& [node, _]: node_deps) nodes.push_back(node);
		return nodes;
	}

	bool QueryGraph::hasDependencies(const NodeID& node_id) const {
		auto it = node_deps.find(node_id);
		return it != node_deps.end() && !it->second.empty();
	}

	std::vector<NodeID> QueryGraph::getDependentNodes(std::vector<NodeID> start_nodes) {
		CORE_ASSERT(TRACK_REVERSE_GRAPH, "Reverse graph tracking must be enabled to get dependent nodes.");
		std::queue<NodeID> queue{start_nodes.begin(), start_nodes.end()};
		std::unordered_set<NodeID> visited;

		while (not queue.empty()) {
			auto node = queue.front();
			queue.pop();

			if (visited.contains(node)) continue;
			visited.insert(node);

			for (auto& node: node_reverse_deps.at(node)) {
				queue.push(node);
			}
		}

		return { visited.begin(), visited.end() };
	}

	void QueryGraph::eraseNodes(const std::vector<NodeID>& nodes_to_erase) {
		CORE_ASSERT(TRACK_REVERSE_GRAPH, "Reverse graph tracking must be enabled to erase nodes.");

		for (const auto& node: nodes_to_erase) {
			node_deps.erase(node);

			// Erase the node from reverse dependencies of its dependencies
			const auto& reverse_deps = node_reverse_deps.at(node);
			for (const auto& dep: reverse_deps) {
				auto& deps = node_deps.at(dep);
				auto new_end = std::ranges::remove(deps, node);
				deps.erase(new_end.begin(), new_end.end()); 
			}

			node_reverse_deps.erase(node);
		}
	}
}
