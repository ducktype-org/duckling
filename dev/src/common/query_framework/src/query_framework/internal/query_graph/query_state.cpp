#include "query_state.hpp"


#include "base/str/str_utils.hpp"
#include <base/types/ints.hpp>
#include <algorithm>
#include <base/collections/maps.hpp>
#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

#include <query_framework/internal/query_data/query_data.hpp>
#include <query_framework/internal/query_data/query_id.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/q_stats/q_stats.hpp>
#include <time_stats/time_stats.hpp>

#include <iostream>
#include <queue>
#include <unordered_set>
#include <utility>
#include <vector>

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

	QueryState::PrevColor QueryState::redGreenSweep(NodeID start_node) {
		// measure time spent in red-green sweep:
		timer::AddToTime _(&total_red_green_sweep_time);

		// No previous compilation graph -> cannot decide incremental reuse, mark as needs recompute
		if (!previous.has_value()) return PrevColor::Red;

		const auto& prev_graph  = previous->graph;
		auto&       node_colors = previous->node_colors;

		// If the node does not exist in the previous graph -> needs recomputation
		if (!prev_graph.nodeExists(start_node)) return PrevColor::Red;

		// If color is already known for this node, return it
		if (auto it = node_colors.find(start_node); it != node_colors.end()) return it->second;

		// Iterative DFS (post-order) over previous graph starting from start_node.
		// A node becomes Green iff all its direct dependencies are Green; otherwise Red.
		// Leaf nodes without an assigned color are marked Green by default.
		struct Frame {
			NodeID node;
			usize  idx;  // next child index to process
		};

		std::vector<Frame> stack;

		IF_BUILD_TYPE_DEV(std::unordered_set<NodeID> in_stack);

		stack.push_back(Frame{ .node = start_node, .idx = 0 });

		// This is used to detect back-edges (cycles) in the previous graph
		// The previous graph should be acyclic, but we just check it to PANIC if not
		IF_BUILD_TYPE_DEV(in_stack.insert(start_node);)

		while (!stack.empty()) {
			auto& frame = stack.back();
			auto  node  = frame.node;

			// If already colored (via another path), just pop and continue
			if (node_colors.contains(node)) {
				IF_BUILD_TYPE_DEV(in_stack.erase(node);)

				stack.pop_back();
				continue;
			}

			// At this point, node must exist in previous graph because its a child of an existing node
			CORE_ASSERT(prev_graph.node_deps.contains(node), "Node should exist in previous graph");

			const auto& deps = prev_graph.node_deps.at(node);

			// If node has no entry or no deps -> treat as leaf; mark Green if not colored yet
			// If node is not colored that means node is not input, so we can safely mark it Green
			if (deps.empty()) {
				node_colors.insert_or_assign(node, PrevColor::Green);

				IF_BUILD_TYPE_DEV(in_stack.erase(node);)

				stack.pop_back();
				continue;
			}

			// Process children one by one ensuring post-order coloring
			if (frame.idx < deps.size()) {
				const NodeID& child = deps[frame.idx++];

				// If child's color is known already, continue to next child
				if (node_colors.contains(child)) continue;

				// Push child for processing
				stack.push_back(Frame{ .node = child, .idx = 0 });

				IF_BUILD_TYPE_DEV(
					auto instert_result = in_stack.insert(child); CORE_ASSERT(
						instert_result.second,
						"Cycle detected in previous query graph during red-green sweep"
					);
				)

				continue;
			}

			// All children processed. Determine this node's color from its direct dependencies.
			bool all_green = true;
			for (const auto& c: deps) {
				auto itc = node_colors.find(c);
				if (itc == node_colors.end() || itc->second != PrevColor::Green) {
					all_green = false;
					break;
				}
			}
			node_colors.insert_or_assign(node, all_green ? PrevColor::Green : PrevColor::Red);

			IF_BUILD_TYPE_DEV(in_stack.erase(node);)

			stack.pop_back();
		}

		return node_colors.at(start_node);
	}

	NodeID QueryState::remapUnstableOrUnregisteredNodes(NodeID node) {
		static base::VectorMap<QueryID, QueryID> old_to_new;

		if (node.q_id.registered() && node.q_id.getData().usesStableHashing()) return node;

		// Here the QueryID is either unregistered or uses unstable hashing, so we do remapping

		if (auto existing = old_to_new.atMaybe(node.q_id); existing.has_value())
			return { **existing, node.hash };

		QueryData dummy_query_data(
			QueryKind::Dummy, "Dummy from previous graph created during deserialization", {}
		);
		QueryID new_qid = registerQuery(dummy_query_data);
		old_to_new.put(node.q_id, new_qid);
		return { new_qid, node.hash };
	}

	void QueryState::mergePreviousGraphIntoCurrentGraph(NodeID start_node) {
		// measure time spent in graph merges:
		timer::AddToTime _(&total_graph_merge_time);


		// NodeID with unstable hash might have diferent ID and graph in previous graph
		// So merging from such NodeID is not allowed
		CORE_ASSERT(
			start_node.q_id.getData().usesStableHashing(),
			"Cannot merge previous graph starting from QueryID that does not have stable hash"
		);

		// If there is no previous compilation graph, there's nothing to merge
		if (!previous.has_value()) return;

		// If the start node exists in the current graph -> it's already merged or recomputed
		if (query_graph.node_deps.contains(start_node)) return;

		const auto& prev_graph = previous->graph;

		// If the start node does not exist in previous graph, nothing to merge
		if (!prev_graph.nodeExists(start_node)) return;

		// Iterative DFS to copy nodes and their dependencies from previous graph
		struct Frame {
			NodeID node;
		};

		std::vector<Frame> stack;

		stack.push_back(Frame{ .node = start_node });

		while (!stack.empty()) {
			const auto frame = stack.back();
			stack.pop_back();

			const NodeID node = frame.node;

			// If we already created this node in current graph, skip
			if (query_graph.node_deps.contains(node)) continue;

			// Insert the node with an empty dependency list first (ensures parent exists for
			// addDependency)
			query_graph.node_deps.emplace(node, std::vector<NodeID>{});

			// Retrieve dependencies from previous graph; if none -> it's a leaf, keep empty deps
			CORE_ASSERT(
				prev_graph.node_deps.contains(node), "Node to merge should exist in previous graph"
			);
			const auto& prev_deps = prev_graph.node_deps.at(node);

			// For each child, add the dependency edge and ensure the child will be processed
			for (const auto& child: prev_deps) {
				// Add edge in current graph (child doesn't have to exist yet)
				query_graph.addDependency(node, child);

				// If child is not in current graph yet, schedule it for creation
				if (!query_graph.node_deps.contains(child)) stack.push_back(Frame{ .node = child });
			}
		}
	}

	void QueryState::reduceOptimizeGraph() {
		// measure time spent in graph optimization:
		time_stats::TrackCategoryTime track_time(time_stats::TimeCategories::PSTConstruction);

		// Sanity check: ensure current query graph does not contain duplicate edges
		std::vector<std::pair<NodeID, NodeID>> duplicate_edges;
		for (const auto& [node, deps]: query_graph.node_deps) {
			std::unordered_set<NodeID> seen_children;
			for (const auto& child: deps) {
				if (!seen_children.insert(child).second) duplicate_edges.emplace_back(node, child);
			}
		}
		if (!duplicate_edges.empty()) {
			std::cerr << "Duplicate child edges detected in query graph:\n";
			for (const auto& [parent, child]: duplicate_edges) {
				std::cerr << "  parent=" << parent.q_id.getData().name
					  << " child=" << child.q_id.getData().name << '\n';
			}
			CORE_ASSERT(false, "Duplicate edges detected while optimizing query graph");
		}

		// First create Map NodeID -> usize to optimise feature algorithm than can operate on usize IDs and work on
		// plain vectors instead of hash maps
		base::HashMap<NodeID, usize> node_to_idx;
		std::vector<NodeID> idx_to_node;

		for (const auto& [node, _] : query_graph.node_deps) {
			node_to_idx.emplace(node, idx_to_node.size());
			idx_to_node.push_back(node);
		}

		const usize node_count = idx_to_node.size();

		// The opt graph will be represented as adjacency list of usize IDs
		std::vector<std::vector<usize>> opt_graph(node_count);

		// We also need to keep a parent map to be able to traverse back the graph
		std::vector<std::vector<usize>> parent_map(node_count);

		// Number of childs for each node
		std::vector<u64> number_of_childs(node_count, 0);

		// Number of parents for each node
		std::vector<u64> number_of_parents(node_count, 0);

		// Map to keep track of stable nodes
		std::vector<bool> is_stable_node(node_count, false);

		// Map to keep track of removed nodes
		std::vector<bool> removed(node_count, false);

		// Map to keep track of touched nodes during optimization
		std::vector<bool> touched(node_count, false);

		// build the parent map and opt graph
		for (const auto& [node, deps] : query_graph.node_deps) {
			const usize node_idx = node_to_idx.at(node);

			// Set the number of childs
			number_of_childs[node_idx] = deps.size();

			// Record if this node is stable
			is_stable_node[node_idx] = node.q_id.getData().usesStableHashing();

			auto& node_children = opt_graph[node_idx];
			node_children.reserve(deps.size());

			// Add the childs to the opt graph and update their parent map
			for (const auto& dep : deps) {
				const usize dep_idx = node_to_idx.at(dep);
				node_children.push_back(dep_idx);
				parent_map[dep_idx].push_back(node_idx);
				number_of_parents[dep_idx] += 1;
			}
		}

		// OPTIMIZATION ALGORITHM GOES HERE
		// Step 1: Remove all unstable nodes that have 0 parents
		std::queue<usize> to_remove_no_parents;

		// Add unstable nodes with 0 parents to processing queue
		for (usize node_idx = 0; node_idx < node_count; ++node_idx) {
			if (number_of_parents[node_idx] == 0 && !is_stable_node[node_idx]) {
				removed[node_idx] = true;
				to_remove_no_parents.push(node_idx);
			}
		}

		// Process nodes with 0 parents
		while (!to_remove_no_parents.empty()) {
			usize current = to_remove_no_parents.front();
			to_remove_no_parents.pop();

			// Assert that current node is non-stable
			CORE_ASSERT(!is_stable_node[current], "Current node should be non-stable");

			// Current not should have been already removed
			CORE_ASSERT(removed[current], "Current node should be marked as removed");

			// Process childs - decrease their number of parents
			for (auto child : opt_graph[current]) {
				// Decrease number of parents for the child
				auto& child_parents = number_of_parents[child];
				child_parents -= 1;

				// Set the child as touched
				touched[child] = true;

				// If child is unstable and has 0 parents, add it to processing queue
				if (child_parents == 0 && !is_stable_node[child] && !removed[child]) {
					to_remove_no_parents.push(child);
					removed[child] = true;
				}
			}
		}

		std::queue<usize> to_remove_no_childs;

		// Remove removed_nodes from the graph
		for (usize node_idx = 0; node_idx < node_count; ++node_idx) {

			// If node is removed remove it from the graph
			if (removed[node_idx]) {
				opt_graph[node_idx].clear();
				parent_map[node_idx].clear();
				continue;
			}

			// IF node is not removed but touched, we need to filter its parents
			if (touched[node_idx]) {
				auto& parents = parent_map[node_idx];
				auto parents_removed_range = std::ranges::remove_if(
					parents,
					[&](usize parent_idx) {
						return removed[parent_idx];
					}
				);
				parents.erase(
					parents_removed_range.begin(),
					parents_removed_range.end()
				);

				// Clear the touched flag
				touched[node_idx] = false;

				// assert that the actuall number of parents matches the stored one
				CORE_ASSERT(
					parents.size() == number_of_parents[node_idx],
					"Number of parents mismatch after removal"
				);
			}

			// For next step if node its unstable and has 0 childs add it to processing queue
			const auto num_childs = number_of_childs[node_idx];
			if (num_childs == 0 && !is_stable_node[node_idx]) {
				removed[node_idx] = true;
				to_remove_no_childs.push(node_idx);
			}
		}

		// Step 2: Remove all unstable nodes that have 0 childs
		while (!to_remove_no_childs.empty()) {
			const usize current = to_remove_no_childs.front();
			to_remove_no_childs.pop();
			// Assert that current node is non-stable
			CORE_ASSERT(!is_stable_node[current], "Current node should be non-stable");
			// Current node should have been already marked as removed
			CORE_ASSERT(removed[current], "Current node should be marked as removed");

			// Process parents - decrease their number of childs
			for (auto parent : parent_map[current]) {
				// Decrease number of childs for the parent
				auto& parent_childs = number_of_childs[parent];
				parent_childs -= 1;
				// Set the parent as touched
				touched[parent] = true;
				// If parent is unstable and has 0 childs, add it to processing queue
				if (parent_childs == 0 && !is_stable_node[parent] && !removed[parent]) {
					to_remove_no_childs.push(parent);
					removed[parent] = true;
				}
			}
		}

		// Remove nodes marked as removed from the graph
		for (usize node_idx = 0; node_idx < node_count; ++node_idx) {
			// If node is removed remove it from the graph
			if (removed[node_idx]) {
				opt_graph[node_idx].clear();
				parent_map[node_idx].clear();
				continue;
			}

			// If node is not removed but touched, we need to filter its childs
			if (touched[node_idx]) {
				auto& childs = opt_graph[node_idx];
				auto childs_removed_range = std::ranges::remove_if(
					childs,
					[&](usize child_idx) {
						return removed[child_idx];
					}
				);
				childs.erase(
					childs_removed_range.begin(),
					childs_removed_range.end()
				);

				// Clear the touched flag
				touched[node_idx] = false;

				// assert that the actuall number of childs matches the stored one
				CORE_ASSERT(
					childs.size() == number_of_childs[node_idx],
					"Number of childs mismatch after removal"
				);
			}
		}

		// STEP 3: Remove unstable nodes that have only 1 parent
		std::vector<std::unordered_set<usize>> actual_parent(node_count);
		
		std::queue<usize> to_process;

		std::vector<bool> processed(node_count, false);

		// Initialize actual parents for every non-removed node
		for (usize node_idx = 0; node_idx < node_count; ++node_idx) {
			if (removed[node_idx]) continue;

			for (auto parent: parent_map[node_idx]) {
				auto inserted = actual_parent[node_idx].insert(parent).second;
				// ASSERT that parent is not removed
				CORE_ASSERT(!removed[parent], "Parent of non-removed node cannot be removed");

				//Asert that insertion was successful
				CORE_ASSERT(inserted, "Parent cannot be inserted twice");
			}
			// We start processing the graph from all roots
			if (actual_parent[node_idx].empty()) {
				to_process.push(node_idx);
			}

			for (auto child: opt_graph[node_idx]) {
				CORE_ASSERT(
					!removed[child],
					"Child node cannot be removed before optimization"
				);
			}
		}

		// Check consistency of actual_parent map
		for (usize node_idx = 0; node_idx < node_count; ++node_idx) {
			if (removed[node_idx]) continue;

			// Check if every child have current node as parent
			for (auto child: opt_graph[node_idx]) {
				CORE_ASSERT(
					actual_parent[child].contains(node_idx),
					"Actual parent map is inconsistent with the graph"
				);
			}
		}

		// Invariant: All parents of are current node are processed correctly
		// And current node have only correct living parents in actual_parent map
		// Also current node has either more than 1 parent or is stable
		// The childs of current node aren't processed yet
		// We remove unstable childs that have only 1 parent (current), and connect children of removed children to current
		// This do not increrase the number of edges in the graph, but removes unnecessary nodes
		// Also all nodes in to_process queue are not removed 
		// Also all nodes in to_process queue are not processed yet and are either stable or have more than 1 parent
		while(!to_process.empty()){
			usize current = to_process.front();
			to_process.pop();

			if (processed[current]) continue;
			processed[current] = true;

			// This node should exist in the opt graph (not removed)
			CORE_ASSERT(!removed[current], base::strConcat(
				"Node in processing queue cannot be removed node: ", std::to_string(current),
				" Number of parents: ", std::to_string(actual_parent[current].size()),
				" Is stable: ", is_stable_node[current] ? "true" : "false",
				" Number of childs: ", std::to_string(number_of_childs[current])
			));

			auto& childs = opt_graph[current];

			// Iterete over childs and search for children that have only 1 parent (current)
			// They can be safety removed
			std::queue<usize> childs_to_process;

			for (auto child : childs)
				childs_to_process.push(child);

			while(!childs_to_process.empty()){
				usize child = childs_to_process.front();
				childs_to_process.pop();

				// Assert that child is not current
				CORE_ASSERT(child != current, "Cycle detected during optimization");

				// Check the number of parents
				const auto parent_count = actual_parent[child].size();

				// If child has only 1 parent, and its unstable we can remove it
				if (parent_count == 1 && !is_stable_node[child]) {
					// Set the child as removed

					// Assert that child is not already removed
					CORE_ASSERT(!removed[child], "Child cannot be already removed at this stage");
					removed[child] = true;
					// change the number of childs of current
					number_of_childs[current] -= 1;

					// For each grandchild: add edge from current to grandchild
					for (auto grandchild : opt_graph[child]) {
						// Add edge from current to grandchild
						auto& grandchild_parents = actual_parent[grandchild];
						grandchild_parents.erase(child);
						bool inserted = grandchild_parents.insert(current).second;
						if (inserted) {
							childs.push_back(grandchild);
							number_of_childs[current] += 1;
							// Since the grandchild is a new child now add it to processing queue
							childs_to_process.push(grandchild);
						}
					}
				} else {
					// Child cannot be removed, so we shedule it for processing
					to_process.push(child);
				}
			}

			// If we have only one child and current is unstable we can remove current too
			// And conect its only child to all its parents
			// Since this will not increase the number of edges in the graph
			if (!is_stable_node[current] && number_of_childs[current] == 1) {
				usize only_child = childs[childs.size() - 1]; // The only child left is the last one

				// Assert that only child is not removed
				CORE_ASSERT(
					removed[only_child] == false,
					"Only child cannot be removed at this stage. Cos it must have more then 1 parent or be stable"
				);

				// Mark current as removed
				removed[current] = true;

				// Remove current from the child's parent set and add all living parents of current
				auto& only_child_parents = actual_parent[only_child];
				only_child_parents.erase(current);
				for (auto parent: actual_parent[current]) {
					auto inserted = only_child_parents.insert(parent).second;
					if (inserted) opt_graph[parent].push_back(only_child);
				}
			}
		}

		// Then remove all removed nodes from the graph
		for (usize node_idx = 0; node_idx < node_count; ++node_idx) {
			// If node is marked as removed, remove it from the graph
			if (removed[node_idx]) {
				opt_graph[node_idx].clear();
				continue;
			}

			// Remove removed childs from the node children list
			auto& childs = opt_graph[node_idx];
			auto childs_removed_range = std::ranges::remove_if(
				childs,
				[&](usize child_idx) {
					return removed[child_idx];
				}
			);
			childs.erase(childs_removed_range.begin(), childs_removed_range.end());
		}

		// here the opt_graph is optimized, we need to rebuild the query_graph from it
		base::HashMap<NodeID, std::vector<NodeID>> new_query_graph_deps;
		for (usize node_idx = 0; node_idx < node_count; ++node_idx) {
			if (removed[node_idx]) continue;
			const auto& deps = opt_graph[node_idx];
			NodeID node      = idx_to_node[node_idx];
			std::vector<NodeID> dep_vec;
			dep_vec.reserve(deps.size());
			for (const auto& dep_idx : deps) {
				NodeID dep_node = idx_to_node[dep_idx];
				dep_vec.push_back(dep_node);
			}
			new_query_graph_deps.emplace(node, std::move(dep_vec));
		}

		query_graph.node_deps = std::move(new_query_graph_deps);
	}

}  // namespace query::internal
