#include "query_graph.hpp"

#include "node_id.hpp"

#include <base/types/bit256.hpp>
#include <base/types/ints.hpp>  // IWYU pragma: export

#include <cstring>
#include <iomanip>
#include <ostream>
#include <queue>
#include <ranges>
#include <set>
#include <vector>

namespace query::internal {

	QueryGraph::DependencyStatus QueryGraph::addDependency(NodeID from, NodeID to) {
		CORE_ASSERT(
			node_deps.contains(from), "Node not found in dep graph, call the given query first."
		);
		node_deps.at(from).emplace_back(to);

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
		using HVType  = decltype(NodeID::hash.val.data);
		using QIDType = decltype(QueryID::val);

		constexpr usize node_id_size
			= sizeof(QIDType) + sizeof(HVType);  // Size of NodeID (q_id and hash)

		std::vector<byte> buffer;

		// Calculate the total size of the serialized data
		usize total_size = sizeof(usize);  // map_size
		for (const auto& [node, deps]: node_deps) {
			total_size += node_id_size;
			total_size += sizeof(usize);  // deps_size
			total_size += deps.size() * node_id_size;
		}
		buffer.reserve(total_size);

		auto write = [&](const auto& value) -> void {
			using T = std::decay_t<decltype(value)>;

			auto serialized_value = std::bit_cast<std::array<byte, sizeof(T)>>(value);
			buffer.insert(buffer.end(), serialized_value.begin(), serialized_value.end());
		};

		auto write_node_id = [&](const NodeID& node) -> void {
			write(node.q_id.val);
			write(node.hash.val.data);
		};

		// Serialize the size of the node_deps map
		write(node_deps.size());

		// Serialize each entry in the map
		for (const auto& [node, deps]: node_deps) {
			write_node_id(node);

			// Serialize the dependencies vector size
			write(deps.size());

			// Serialize each dependency (NodeID)
			for (const auto& dep: deps) write_node_id(dep);
		}

		return buffer;
	}

	QueryGraph QueryGraph::deserialize(std::span<const byte> data) {
		using HType   = decltype(NodeID::hash.val);
		using HVType  = decltype(NodeID::hash.val.data);
		using QIDType = decltype(QueryID::val);
		// Compile-time check to ensure HVType and QIDType are trivial
		static_assert(std::is_trivial_v<HVType>, "HVType must be a trivial type.");
		static_assert(std::is_trivial_v<QIDType>, "QIDType must be a trivial type.");

		constexpr usize node_id_size
			= sizeof(QIDType) + sizeof(HVType);  // Size of NodeID (q_id and hash)

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

		// Deserialize the size of the node_deps map
		usize map_size = 0;
		read(map_size);

		// Deserialize each entry in the map
		for (usize i = 0; i < map_size; ++i) {
			NodeID node = read_node_id();

			// Deserialize the dependencies vector size
			usize deps_size = 0;
			read(deps_size);

			if (deps_size * node_id_size + offset > data_size)
				throw std::out_of_range("Buffor size exceeded during deserialization");

			// Deserialize each dependency (NodeID)
			std::vector<NodeID> deps;
			deps.reserve(deps_size);

			for (usize j = 0; j < deps_size; ++j) deps.emplace_back(read_node_id());

			// Add the deserialized entry to the graph
			auto [it, inserted] = graph.node_deps.emplace(node, std::move(deps));
			if (!inserted)
				CORE_PANIC("Duplicate node detected during deserialization");
		}

		// Check here oif offset is equal to data_size
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
}
