#include "query_graph.hpp"

#include "node_id.hpp"

#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/bit256.hpp>
#include <base/types/ints.hpp>  // IWYU pragma: export

#include <query_framework/module_flags/module_flags.hpp>

#include <cstring>
#include <iomanip>
#include <ostream>
#include <queue>
#include <ranges>
#include <set>
#include <unordered_set>
#include <vector>

namespace query::internal {

	QueryGraph::QueryGraph():
		  node_deps(base::makeBox<concurrent::ConHashMap<NodeID, ChildrenData>>()),
		  node_reverse_deps(base::makeBox<concurrent::ConHashMap<NodeID, std::vector<NodeID>>>()) {}

	void QueryGraph::addDependency(NodeID from, NodeID to) {
		CORE_ASSERT(
			node_deps->contains(from), "Node not found in dep graph, call the given query first."
		);

		if (track_reverse_graph) {
			CORE_ASSERT(
				enable_incremental_compilation,
				"Reverse graph tracking should only be enabled in incremental compilation mode"
			);
			node_reverse_deps->maybePutAndUpdate(
				to,
				std::vector<NodeID>{},
				[from](Ref<std::vector<NodeID>> deps) { deps->emplace_back(from); }
			);
		}

		auto children_data = node_deps->atMaybe(from).value();
		auto children      = children_data->getHolder();
		children->push_back(to);
	}

	std::vector<NodeID> QueryGraph::getNodeDeps(internal::NodeID node_id) const {
		CORE_ASSERT(
			node_deps->contains(node_id), "Node not found in dep graph, call the given query first."
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
				node_deps->contains(visited_node_id),
				"Node not found in dep graph. Node ID: ",
				visited_node_id.q_id.getData().name,
				" Key: ",
				visited_node_id.hash.val.toStringHex()
			);

			const auto node            = node_deps->atMaybe(visited_node_id).value();
			auto       children_holder = node->getHolder();
			for (auto dep: *children_holder)
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
			node_deps->contains(node_id),
			"Node not found in dep graph when requesting direct dependencies."
		);
		auto  children_data   = node_deps->atMaybe(node_id).value();
		auto  children_holder = children_data->getHolder();
		auto& children        = *children_holder;
		children_holder.release();
		return children;
	}

	void QueryGraph::debugPrint(std::ostream& out) const {
		out << "Dep Graph: \n";
		std::vector<NodeID> all_nodes;
		for (const auto& [k, _]: *node_deps) all_nodes.push_back(k);
		debugPrintNodes(all_nodes, out);
	}

	void QueryGraph::debugPrintForDrawing(std::ostream& out) const {
		out << "Dep Graph: \n";
		out << node_deps->size() << "\n";

		std::map<NodeID, u64> index;
		u64                   id = 0;
		for (auto& [k, _]: *node_deps) {
			index[k] = id++;
			out << id << " " << k.q_id.getData().name << "\n";
		}

		for (auto& [k, v]: *node_deps) {
			auto children_holder = v.getHolder();
			for (auto& dep: *children_holder) out << index[k] << " " << index[dep] << "\n";
		}
	}

	void QueryGraph::debugPrintNodes(const std::vector<NodeID>& nodes, std::ostream& out) const {
		std::string spacing(25, ' ');
		for (const auto& n: nodes) {
			auto        children_data   = node_deps->atMaybe(n).value();
			auto        children_holder = children_data->getHolder();
			const auto& node_data_entry = *children_holder;
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
		if (node_deps->size() != other.node_deps->size()) return false;

		for (auto& [node, deps]: *node_deps) {
			auto it = other.node_deps->atMaybe(node);
			if (!it.has_value()) return false;

			auto deps_holder       = deps.getHolder();
			auto other_deps_holder = it.value()->getHolder();

			if (*deps_holder != *other_deps_holder) return false;
		}

		for (auto& [node, deps]: *other.node_deps) {
			auto it = node_deps->atMaybe(node);
			if (!it.has_value()) return false;

			auto deps_holder       = deps.getHolder();
			auto other_deps_holder = it.value()->getHolder();

			bool are_same = *deps_holder == *other_deps_holder;
			if (!are_same) return false;
		}

		return true;
	}

	std::vector<byte> QueryGraph::serialize() const {
		std::vector<NodeID> nodes;
		nodes.reserve(node_deps->size());
		base::HashMap<NodeID, usize> node_to_index;
		node_to_index.reserve(node_deps->size());

		usize next_index = 0;
		for (const auto& [node, _]: *node_deps) {
			node_to_index.emplace(node, next_index++);
			nodes.push_back(node);
		}

		const usize node_count = nodes.size();

		std::vector<std::vector<usize>> adjacency(node_count);
		for (const auto& node: nodes) {
			auto& deps        = *node_deps->atMaybe(node).value();
			auto  deps_holder = deps.getHolder();

			auto& out = adjacency.at(node_to_index.at(node));
			out.reserve(deps_holder->size());
			for (const auto& dep: *deps_holder) {
				CORE_ASSERT(
					node_to_index.contains(dep),
					"Dependency node missing from graph during serialization."
				);
				out.push_back(node_to_index.at(dep));
			}
		}

		return serializeReducedGraph(ReducedGraphData{ .nodes     = std::move(nodes),
		                                               .adjacency = std::move(adjacency) });
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

			std::vector<NodeID> children;
			children.reserve(deps_size);

			// Deserialize each dependency index
			for (usize j = 0; j < deps_size; ++j) {
				usize dep_index = 0;
				read(dep_index);
				if (dep_index >= nodes.size())
					throw std::out_of_range("Dependency index out of range during deserialization");
				children.emplace_back(nodes.at(dep_index));
			}

			auto key_value_pair
				= graph.node_deps->maybePut(nodes.at(node_index), std::move(children));
			if (key_value_pair == nullptr)
				CORE_PANIC("Duplicate node detected during deserialization");
		}

		// Ensure that the entire buffer was consumed
		CORE_ASSERT(offset == data_size, "Deserialization did not consume the entire buffer");

		return graph;
	}

	std::vector<NodeID> QueryGraph::getAllNodes() const {
		std::vector<NodeID> nodes;
		nodes.reserve(node_deps->size());
		for (const auto& [node, _]: *node_deps) nodes.push_back(node);
		return nodes;
	}

	bool QueryGraph::hasDependencies(const NodeID& node_id) const {
		auto it = node_deps->atMaybe(node_id);
		if (!it.has_value()) return false;
		auto& deps        = *it.value();
		auto  deps_holder = deps.getHolder();
		return !deps_holder->empty();
	}

	QueryGraph::Dependents QueryGraph::getDependentNodes(const std::vector<NodeID>& start_nodes
	) const {
		CORE_ASSERT(
			track_reverse_graph,
			"Reverse graph tracking must be enabled to erase nodes based on dependencies."
		);

		std::vector<NodeID>        queue{ start_nodes.begin(), start_nodes.end() };
		std::unordered_set<NodeID> visited;

		while (not queue.empty()) {
			auto node = queue.back();
			queue.pop_back();

			if (visited.contains(node)) continue;
			visited.insert(node);

			if_opt_some(node_reverse_deps->atMaybe(node), its_reverse_deps) {
				for (auto& new_node: *its_reverse_deps) queue.push_back(new_node);
			}
		}

		return QueryGraph::Dependents{ .dependents_recursive = { visited.begin(), visited.end() } };
	}

	void QueryGraph::eraseNodes(const QueryGraph::Dependents& nodes_to_erase) {
		CORE_ASSERT(
			track_reverse_graph,
			"Reverse graph tracking must be enabled to erase nodes based on dependencies."
		);


		for (const auto& node: nodes_to_erase.dependents_recursive) {
			// A(input) <- B <- C
			//        D <--┘
			// deps(B) = {A, D}
			// rev_deps(A) = {B}
			// rev_deps(D) = {B}

			// Some inputs may have no dependencies at all when in Language Server mode (e.g. no
			// queries were executed between reparsings).
			if_opt_some(node_deps->atMaybe(node), node_deps_children_data) {
				auto removed_node_deps = node_deps_children_data->getHolder();

				for (const auto& dep: *removed_node_deps) {
					if_opt_some(node_reverse_deps->atMaybe(dep), its_reverse_deps) {
						std::erase(*its_reverse_deps, node);
					}
				}
			}

			node_deps->erase(node);
			node_reverse_deps->erase(node);
		}
	}
}
