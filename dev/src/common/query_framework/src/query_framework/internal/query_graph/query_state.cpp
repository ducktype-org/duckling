#include "query_state.hpp"

#include "base/preproc/utils.hpp"
#include "base/types/ints.hpp"
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

		// First create Map NodeID -> usize to optimise feature algorithm than can operate on usize IDs and work on 
		// VectorMap instead of HashMap thanks to that
		base::HashMap<NodeID, usize> node_to_idx;
		std::vector<NodeID> 	   idx_to_node;

		for (const auto& [node, _] : query_graph.node_deps) {
			node_to_idx.emplace(node, idx_to_node.size());
			idx_to_node.push_back(node);
		}

		// The opt graph will be represented as adjacency list of usize IDs
		base::VectorMap<usize, std::vector<usize>> opt_graph;

		// We also need to keep a parent map to be able to traverse back the graph
		base::VectorMap<usize, std::vector<usize>> parent_map;

		// Number of childs for each node
		base::VectorMap<usize, u64> number_of_childs;

		// Number of parents for each node
		base::VectorMap<usize, u64> number_of_parents;

		// Map to keep track of stable nodes
		base::VectorMap<usize, bool> is_stable_node;

		// Map to keep track of removed nodes
		base::VectorMap<usize, bool> removed;

		// Map to keep track of touched nodes during optimization
		base::VectorMap<usize, bool> touched;

		// build the parent map and opt graph
		for (const auto& [node, deps] : query_graph.node_deps) {
			usize node_idx = node_to_idx.at(node);

			// Set the number of childs
			number_of_childs.emplace(node_idx, deps.size());

			// Record if this node is stable
			is_stable_node.emplace(node_idx, node.q_id.getData().usesStableHashing());

			removed.emplace(node_idx, false);
			touched.emplace(node_idx, false);

			// Ad this node to parent map (if not added yet)
			if (!number_of_parents.contains(node_idx)){
				number_of_parents.emplace(node_idx, 0);
				parent_map.emplace(node_idx, std::vector<usize>{});
			}

			// Add this node to the opt graph
			std::vector<usize> dep_vec;
			dep_vec.reserve(deps.size());
			opt_graph.emplace(node_idx, std::move(dep_vec));

			// Add the childs to the opt graph and update their parent map

			for (const auto& dep : deps) {
				usize dep_idx = node_to_idx.at(dep);

				// Add the child to the opt graph
				opt_graph.atMaybe(node_idx)->get()->push_back(dep_idx);

				// Add this node as parent to the child and increment number of parents
				if(!parent_map.contains(dep_idx)){
					parent_map.emplace(dep_idx, std::vector<usize>{});
					number_of_parents.emplace(dep_idx, 0);
				}

				parent_map.atMaybe(dep_idx)->get()->push_back(node_idx);
				*number_of_parents.atMaybe(dep_idx)->get() += 1;
			}
		}

		// OPTIMIZATION ALGORITHM GOES HERE
		// Step 1: Remove all unstable nodes that have 0 parents
		std::queue<usize> to_remove_no_parents;

		// Add unstable nodes with 0 parents to processing queue
		for (usize node_idx = 0; node_idx < idx_to_node.size(); ++node_idx) {
			auto num_parents_opt = *number_of_parents.atMaybe(node_idx)->get();

			if (num_parents_opt == 0 && !*is_stable_node.atMaybe(node_idx)->get()) {
				*removed.atMaybe(node_idx)->get() = true;
				to_remove_no_parents.push(node_idx);
			}
		}

		// Process nodes with 0 parents
		while (!to_remove_no_parents.empty()) {
			usize current = to_remove_no_parents.front();
			to_remove_no_parents.pop();

			// Assert that current node is non-stable
			CORE_ASSERT(!*is_stable_node.atMaybe(current)->get(), "Current node should be non-stable");

			// Current not should have been already removed
			CORE_ASSERT(*removed.atMaybe(current)->get(), "Current node should be marked as removed");

			// Process childs - decrease their number of parents
			for (auto child : *opt_graph.atMaybe(current)->get()) {
				// Decrease number of parents for the child
				auto child_parents = number_of_parents.atMaybe(child)->get();
				*child_parents -= 1;

				// Set the child as touched
				*touched.atMaybe(child)->get() = true;

				// If child is unstable and has 0 parents, add it to processing queue
				if (*child_parents == 0 && !*is_stable_node.atMaybe(child)->get() && !*removed.atMaybe(child)->get()) {
					to_remove_no_parents.push(child);
					*removed.atMaybe(child)->get() = true;
				}
			}
		}

		std::queue<usize> to_remove_no_childs;

		// Remove removed_nodes from the graph
		for (usize node_idx = 0; node_idx < idx_to_node.size(); ++node_idx) {

			// If node is removed remove it from the graph
			if (*removed.atMaybe(node_idx)->get()) {
				// Remove from opt graph
				opt_graph.erase(node_idx);
				// Remove from parent map
				parent_map.erase(node_idx);
				continue;
			}

			// IF node is not removed but touched, we need to filter its parents
			if (*touched.atMaybe(node_idx)->get()){
				auto& parents = *parent_map.atMaybe(node_idx)->get();
				auto parents_removed_range = std::ranges::remove_if(
					parents,
					[&](usize parent_idx) {
						return *removed.atMaybe(parent_idx)->get();
					}
				);
				parents.erase(
					parents_removed_range.begin(),
					parents_removed_range.end()
				);

				// Clear the touched flag
				*touched.atMaybe(node_idx)->get() = false;

				// assert that the actuall number of parents matches the stored one
				CORE_ASSERT(
					parents.size() == *number_of_parents.atMaybe(node_idx)->get(),
					"Number of parents mismatch after removal"
				);
			}

			// For next step if node its unstable and has 0 childs add it to processing queue
			auto num_childs_opt = *number_of_childs.atMaybe(node_idx)->get();
			if (num_childs_opt == 0 && !*is_stable_node.atMaybe(node_idx)->get()) {
				*removed.atMaybe(node_idx)->get() = true;
				to_remove_no_childs.push(node_idx);
			}
		}

		// Step 2: Remove all unstable nodes that have 0 childs
		while (!to_remove_no_childs.empty()) {
			usize current = to_remove_no_childs.front();
			to_remove_no_childs.pop();
			// Assert that current node is non-stable
			CORE_ASSERT(!*is_stable_node.atMaybe(current)->get(), "Current node should be non-stable");
			// Current node should have been already marked as removed
			CORE_ASSERT(*removed.atMaybe(current)->get(), "Current node should be marked as removed");

			// Process parents - decrease their number of childs
			for (auto parent : *parent_map.atMaybe(current)->get()) {
				// Decrease number of childs for the parent
				auto parent_childs = number_of_childs.atMaybe(parent)->get();
				*parent_childs -= 1;
				// Set the parent as touched
				*touched.atMaybe(parent)->get() = true;
				// If parent is unstable and has 0 childs, add it to processing queue
				if (*parent_childs == 0 && !*is_stable_node.atMaybe(parent)->get() && !*removed.atMaybe(parent)->get()) {
					to_remove_no_childs.push(parent);
					*removed.atMaybe(parent)->get() = true;
				}
			}
		}

		// Remove nodes marked as removed from the graph
		for (usize node_idx = 0; node_idx < idx_to_node.size(); ++node_idx) {
			// If node is removed remove it from the graph
			if (*removed.atMaybe(node_idx)->get()) {
				// If node already removed continue
				if(opt_graph.contains(node_idx) == false) continue;
				// Remove from opt graph
				opt_graph.erase(node_idx);
				// Remove from parent map
				parent_map.erase(node_idx);
				continue;
			}

			// If node is not removed but touched, we need to filter its childs
			if (*touched.atMaybe(node_idx)->get()){
				auto& childs = *opt_graph.atMaybe(node_idx)->get();
				auto childs_removed_range = std::ranges::remove_if(
					childs,
					[&](usize child_idx) {
						return *removed.atMaybe(child_idx)->get();
					}
				);
				childs.erase(
					childs_removed_range.begin(),
					childs_removed_range.end()
				);

				// Clear the touched flag
				*touched.atMaybe(node_idx)->get() = false;

				// assert that the actuall number of childs matches the stored one
				CORE_ASSERT(
					childs.size() == *number_of_childs.atMaybe(node_idx)->get(),
					"Number of childs mismatch after removal"
				);
			}
		}

		// STEP 3: Remove unstable nodes that have only 1 parent

		// HELPER lambda to update the living parents of given node
		// This function will modyfy the parent map
		// IF the parent is not living then it will call itself recurslively to find living parents of the parent
		// But this function will do this in iterative way to avoid stack overflows on large graphs

	}

}  // namespace query::internal
