#include "query_graph.hpp"

#include "../query_data/query_id.hpp"
#include "node_id.hpp"

#include <base/bit256.hpp>
#include <base/ints.hpp>  // IWYU pragma: export

#include <iomanip>
#include <iostream>
#include <ostream>
#include <queue>
#include <ranges>
#include <set>
#include <vector>

namespace {
	template<typename T, typename = void>
	struct has_serialization_methods: std::false_type {};

	template<typename T>
	struct has_serialization_methods<
		T,
		std::void_t<
			decltype(std::declval<T>().serialize()),
			decltype(T::deserialize(
				std::declval<const std::vector<uint8_t>&>(), std::declval<const usize&>()
			)),
			decltype(T::serializedSize())>>: std::true_type {};

	template<typename T>
	constexpr bool HAS_SERIALIZATION_METHODS_V = has_serialization_methods<T>::value;

	template<typename T>
	constexpr usize sizeOfType() {
		if constexpr (std::is_trivial_v<T>) {
			return sizeof(T);
		} else {
			static_assert(
				HAS_SERIALIZATION_METHODS_V<T>,
				"Type must either be trivial or have serialize, deserialize, and serializedSize "
				"methods."
			);
			return T::serializedSize();
		}
	}
}

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
		std::vector<NodeID> all_nodes;
		for (const auto& [k, _]: node_deps) all_nodes.push_back(k);
		debugPrintNodes(all_nodes, out);
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

	std::vector<uint8_t> QueryGraph::serialize() const {
		using HType   = decltype(NodeID::hash.val);
		using QIDType = decltype(QueryID::val);

		const usize node_id_size
			= sizeOfType<QIDType>() + sizeOfType<HType>();  // Size of NodeID (q_id and hash)

		std::vector<uint8_t> buffer;

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

			if constexpr (std::is_trivial_v<T>) {
				// For trivial types, use memcpy
				buffer.insert(
					buffer.end(),
					reinterpret_cast<const uint8_t*>(&value),
					reinterpret_cast<const uint8_t*>(&value) + sizeof(T)
				);
			} else {
				// For non-trivial types, use their serialize method
				auto serialized_data = value.serialize();
				buffer.insert(buffer.end(), serialized_data.begin(), serialized_data.end());
			}
		};

		auto write_node_id = [&](const NodeID& node) -> void {
			write(node.q_id.val);
			write(node.hash.val);
		};

		// Serialize the size of the node_deps map
		usize map_size = node_deps.size();
		write(map_size);

		// Serialize each entry in the map
		for (const auto& [node, deps]: node_deps) {
			write_node_id(node);

			// Serialize the dependencies vector size
			usize deps_size = deps.size();
			write(deps_size);

			// Serialize each dependency (NodeID)
			for (const auto& dep: deps) write_node_id(dep);
		}

		return buffer;
	}

	QueryGraph QueryGraph::deserialize(const std::vector<uint8_t>& data) {
		using HType   = decltype(NodeID::hash.val);
		using QIDType = decltype(QueryID::val);

		QueryGraph  graph;
		usize       offset    = 0;
		const usize data_size = data.size();
		const usize node_id_size
			= sizeOfType<QIDType>() + sizeOfType<HType>();  // Size of NodeID (q_id and hash)

		auto read = [&](auto& dest) -> void {
			using T = std::decay_t<decltype(dest)>;

			if constexpr (std::is_trivial_v<T>) {
				// For trivial types, use memcpy
				if (offset + sizeof(T) > data_size)
					throw std::out_of_range("Buffer size exceeded during deserialization");

				std::memcpy(&dest, data.data() + offset, sizeof(T));
				offset += sizeof(T);
			} else {
				// For non-trivial types, use their deserialize method
				if (offset + T::serializedSize() > data_size)
					throw std::out_of_range("Buffer size exceeded during deserialization");

				dest = T::deserialize(data, offset);
				offset += T::serializedSize();
			}
		};

		auto read_node_id = [&]() -> NodeID {
			QIDType q_id = 0;
			read(q_id);

			HType hash;
			read(hash);

			return NodeID{ .q_id = QueryID(q_id), .hash = { hash } };
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
			graph.node_deps.insert_or_assign(node, std::move(deps));
		}

		return graph;
	}
}
