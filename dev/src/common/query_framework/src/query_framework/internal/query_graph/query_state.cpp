#include "query_state.hpp"

#include <time_stats/time_stats.hpp>

#include <base/collections/maps.hpp>
#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>
#include <base/types/ints.hpp>

#include <logger/logger.hpp>
#include <query_framework/internal/query_data/query_data.hpp>
#include <query_framework/internal/query_data/query_id.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/q_stats/q_stats.hpp>

#include <algorithm>
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

		// If color is already known for this node, return it
		// At this point the node don't need to be in previous graph (can be moved to actual graph
		// in merging) Moved nodes always have their color known already
		if (auto it = node_colors.find(start_node); it != node_colors.end()) return it->second;

		// If the node does not exist in the previous graph -> needs recomputation
		if (!prev_graph.nodeExists(start_node)) return PrevColor::Red;

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

			// At this point, node must exist in previous graph because its a child of an existing
			// node and aren't colored yet So the cannot be merged yet
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

		auto& prev_graph = previous->graph;

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

			// Node may already have been merged if it was scheduled multiple times (e.g. duplicate
			// deps) This can happen when some Node has multiple parents in the previous graph
			if (query_graph.node_deps.contains(node)) continue;

			CORE_ASSERT(
				prev_graph.node_deps.contains(node), "Node to merge should exist in previous graph"
			);

			// Node must have a color assigned already
			CORE_ASSERT(
				previous->node_colors.contains(node),
				"Node to merge should have color assigned in previous graph"
			);

			// Retrieve dependencies from previous graph; if none -> keep empty deps in current graph
			// Node should exist in previous graph at this point (because its not in current graph yet)

			auto prev_it = prev_graph.node_deps.find(node);
			CORE_ASSERT(
				prev_it != prev_graph.node_deps.end(),
				"Failed to find node in previous graph during merge"
			);

			auto prev_deps = std::move(prev_it->second);
			prev_graph.node_deps.erase(prev_it);

			auto [it, inserted] = query_graph.node_deps.emplace(node, std::move(prev_deps));
			CORE_ASSERT(inserted, "Node should not exist in current graph during merge");
			const auto& deps = it->second;

			for (const auto& child: deps) stack.push_back(Frame{ .node = child });
		}
	}

	QueryGraph::ReducedGraphData QueryState::reduceOptimizeGraph(const QueryGraph& graph) const {
		// measure time spent in graph optimization:
		time_stats::TrackCategoryTime track_time(time_stats::TimeCategories::GraphOptimization);

		const auto& node_deps = graph.node_deps;

		// First create Map NodeID -> usize to optimise feature algorithm than can operate on usize
		// IDs and work on plain vectors instead of hash maps
		base::HashMap<NodeID, usize> node_to_idx;
		std::vector<NodeID>          idx_to_node;

		for (const auto& [node, _]: node_deps) {
			node_to_idx.emplace(node, idx_to_node.size());
			idx_to_node.push_back(node);
		}

		const usize node_count = idx_to_node.size();

		// Map to keep track of touched nodes during optimization
		// All values ​​in this map should always be set to false unless we are in the middle of
		// some algorithm
		std::vector<bool> touched(node_count, false);

		// Map to keep track of removed nodes during optimization
		std::vector<bool> removed(node_count, false);

		// A function that allows you to lazily delete and deduplicate vertices. Runs in O(n)
		auto deduplicate_or_remove
			= [&removed,
		       &touched](std::vector<usize>& vec, const bool deduplicate, const bool remove) {
				  auto range = std::ranges::remove_if(
					  vec,
					  [&removed, &touched, remove, deduplicate](usize child) {
						  if (remove && removed[child]) return true;
						  if (deduplicate && touched[child])
							  return true;
						  else if (deduplicate)
							  touched[child] = true;
						  return false;
					  }
				  );
				  vec.erase(range.begin(), range.end());
				  // Reset touched map
				  if (deduplicate)
					  for (auto child: vec) touched[child] = false;
			  };

		// The opt graph will be represented as adjacency list of usize IDs
		std::vector<std::vector<usize>> opt_graph(node_count);

		// We also need to keep a parent map to be able to traverse back the graph
		std::vector<std::vector<usize>> parent_map(node_count);

		// Some algorithms may rely solely on the number of children or parents, and only delete
		// them after the operation is complete. These maps should be kept up to date. Number of
		// childs for each node
		std::vector<u64> number_of_childs(node_count, 0);

		// Number of parents for each node
		std::vector<u64> number_of_parents(node_count, 0);

		// Map to keep track of nodes that needs to be preserved during the optimization
		std::vector<bool> is_preserve_node(node_count, false);

		const bool log_incremental
			= ::logger::enable_dev_logs
		   && ::logger::isCategoryEnabled(::logger::DevLogCategories::Incremental);

		const auto log_original_graph = [&](std::string_view phase) {
			if (!log_incremental) return;
			u64 edge_count = 0;
			for (const auto& [_, deps]: node_deps) edge_count += deps.size();
			CORE_DEV_LOG(
				Incremental,
				"[reduceOptGraph] Number of Nodes (",
				phase,
				"): ",
				node_deps.size(),
				", Number of Edges: ",
				edge_count
			);
		};

		const auto log_reduced_graph = [&](std::string_view phase, const auto& adjacency) {
			if (!log_incremental) return;
			u64 edge_count = 0;
			for (const auto& deps: adjacency) edge_count += deps.size();
			CORE_DEV_LOG(
				Incremental,
				"[reduceOptGraph] Number of Nodes (",
				phase,
				"): ",
				adjacency.size(),
				", Number of Edges: ",
				edge_count
			);
		};

		log_original_graph("Before optimization");

		// build the parent map and opt graph
		for (const auto& [node, deps]: node_deps) {
			const usize node_idx = node_to_idx.at(node);

			// Record if this node needs to be preserved
			is_preserve_node[node_idx] = node.q_id.getData().tags.preserve_on_disk;

			// Convert dependencies to index space and deduplicate using sort+unique
			std::vector<usize> child_indices;
			child_indices.reserve(deps.size());
			for (const auto& dep: deps) child_indices.push_back(node_to_idx.at(dep));

			// Deduplicate dependencies
			deduplicate_or_remove(child_indices, true, false);

			// Store deduplicated children
			auto& node_children = opt_graph[node_idx];
			node_children       = std::move(child_indices);

			// Set the number of childs after deduplication
			number_of_childs[node_idx] = node_children.size();

			// Update parent map using deduplicated children
			for (const auto dep_idx: node_children) {
				parent_map[dep_idx].push_back(node_idx);
				number_of_parents[dep_idx] += 1;
			}
		}

		// OPTIMIZATION ALGORITHM GOES HERE
		// Step 1: Remove all unstable nodes that have 0 parents (recursively)
		// Queue for nodes to process
		std::vector<usize> to_remove_no_parents;

		// Add unstable nodes with 0 parents to processing queue
		for (usize node_idx = 0; node_idx < node_count; ++node_idx) {
			if (number_of_parents[node_idx] == 0 && !is_preserve_node[node_idx]) {
				// note that marking node as remove do not remove it from the graph yet
				// It can be lasy removed later after processing
				removed[node_idx] = true;
				to_remove_no_parents.push_back(node_idx);
			}
		}

		// Process roots
		while (!to_remove_no_parents.empty()) {
			usize current = to_remove_no_parents.back();
			to_remove_no_parents.pop_back();

			// Assert that current node is not preserved
			CORE_ASSERT(!is_preserve_node[current], "Current node should not be preserved");

			// Current node should have been marked as removed already
			CORE_ASSERT(removed[current], "Current node should be marked as removed");

			// Process childs - decrease their number of parents
			for (auto child: opt_graph[current]) {
				// Decrease number of parents for the child
				auto& child_parents = number_of_parents[child];
				child_parents -= 1;

				// Set the child as touched
				touched[child] = true;

				// If child do not need to be preserved and has 0 parents, add it to processing queue
				if (child_parents == 0 && !is_preserve_node[child] && !removed[child]) {
					to_remove_no_parents.push_back(child);
					removed[child] = true;
				}
			}
		}

		// This is the queue for next step
		// Declared here to avoid unnecessary loops thru the graph
		std::vector<usize> to_remove_no_childs;

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
				// Remove the parents that have been marked as removed during step 1
				deduplicate_or_remove(parent_map[node_idx], false, true);

				// Clear the touched flag
				touched[node_idx] = false;

				// assert that the actuall number of parents matches the stored one
				CORE_ASSERT(
					parent_map[node_idx].size() == number_of_parents[node_idx],
					"Number of parents mismatch after removal"
				);
			}

			// For next step if node has 0 childs add it to processing queue
			const auto num_childs = number_of_childs[node_idx];
			if (num_childs == 0 && !is_preserve_node[node_idx]) {
				removed[node_idx] = true;
				to_remove_no_childs.push_back(node_idx);
			}
		}

		// Step 2: Remove all unstable nodes that have 0 childs
		while (!to_remove_no_childs.empty()) {
			const usize current = to_remove_no_childs.back();
			to_remove_no_childs.pop_back();

			// Assert that current node is non-stable
			CORE_ASSERT(!is_preserve_node[current], "Current node should be non-stable");
			// Current node should have been already marked as removed
			CORE_ASSERT(removed[current], "Current node should be marked as removed");

			// Process parents - decrease their number of childs
			for (auto parent: parent_map[current]) {
				// Decrease number of childs for the parent
				auto& parent_childs = number_of_childs[parent];
				parent_childs -= 1;
				// Set the parent as touched
				touched[parent] = true;
				// If parent is unstable and has 0 childs, add it to processing queue
				if (parent_childs == 0 && !is_preserve_node[parent] && !removed[parent]) {
					to_remove_no_childs.push_back(parent);
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

				// Remove the childs that have been marked as removed during step 2
				deduplicate_or_remove(childs, false, true);

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
		// Queue for nodes to process we process from roots to leaves
		// REQUIREMENT: THERE CANNOT BE DUPLICATES IN PARENTS, and the parent map cannot contain
		// deleted nodes It is good if the children are also not deleted, but it is not necessary

		for (int i = 0; i < 2; ++i) {
			std::vector<usize> to_process;
			std::vector<bool>  sheduled_to_process(node_count, false);
			usize              to_process_head = 0;

			// Add all roots to processing queue
			for (usize node_idx = 0; node_idx < node_count; ++node_idx) {
				if (!removed[node_idx] && parent_map[node_idx].empty()) {
					to_process.push_back(node_idx);
					sheduled_to_process[node_idx] = true;
					// Node should be preserved at this point
					CORE_ASSERT(
						is_preserve_node[node_idx],
						"Root nodes must be preserved at this point of optimization"
					);
				}
			}

			// Map to keep track of original childs of current node during processing
			// To not add same parent multiple times
			std::vector<bool> was_original_child(node_count, false);

			// Invariant: All parents of are current node are processed correctly
			// And current node have all FINAL living parents in parent_map
			// current node is not removen and will not be removed
			// Also the number_of_parents[current] have the correct value of FINAL living parents
			// Also current node has either more than 1 parent or it must be preserved
			// The childs of current node aren't processed yet
			// We remove unstable childs that have only 1 parent (current), and connect children of
			// removed children to current This do not increrase the number of edges in the graph,
			// but removes unnecessary nodes We also remove nodes that after processing have only 1
			// child and are not need to be preserved by connecting their only child to all their
			// parents This also do not increase the number of edges in the graph, but decreases the
			// number of nodes
			while (to_process_head < to_process.size()) {
				usize current = to_process[to_process_head++];

				// The node can be already removed via adoption after being scheduled
				if (removed[current]) continue;

				// Scheduled to process must be true
				CORE_ASSERT(
					sheduled_to_process[current],
					"Node in processing queue must be scheduled to process"
				);

				// Asert invariant holds
				CORE_ASSERT(
					is_preserve_node[current] || number_of_parents[current] > 1,
					base::strConcat(
						"Node in processing queue must be preserve node or have more than 1 "
						"parent: ",
						std::to_string(current),
						" Number of parents: ",
						std::to_string(number_of_parents[current]),
						" Needs to be preserved: ",
						is_preserve_node[current] ? "true" : "false",
						" Number of childs: ",
						std::to_string(number_of_childs[current])
					)
				);

				auto& childs = opt_graph[current];

				// Iterete over childs and search for children that have only 1 parent (current)
				// They can be safety removed
				std::vector<usize> childs_to_process;

				const usize original_child_count = childs.size();

				for (auto child: childs) {
					was_original_child[child] = true;
					childs_to_process.push_back(child);
				}

				while (!childs_to_process.empty()) {
					usize child = childs_to_process.back();
					childs_to_process.pop_back();
					// Assert that child is not current
					CORE_ASSERT(child != current, "Cycle detected during optimization");

					// If child is removed skip it
					if (removed[child]) continue;

					// Check the number of parents
					const auto parent_count = number_of_parents[child];

					// If child has only 1 parent, and its unstable we can remove it
					if (parent_count == 1 && !is_preserve_node[child]) {
						// Set the child as removed

						removed[child] = true;
						// change the number of childs of current
						number_of_childs[current] -= 1;

						// For each grandchild: add edge from current to grandchild
						for (auto grandchild: opt_graph[child]) {
							// Decrease number of parents for grandchild
							number_of_parents[grandchild] -= 1;

							// Add edge from current to grandchild (if not already present)
							// That can happen if grandchild is also child of current or grandchild
							// was processed before
							if (!was_original_child[grandchild]
							    && parent_map[grandchild][parent_map[grandchild].size() - 1]
							           != current) {
								number_of_childs[current] += 1;
								number_of_parents[grandchild] += 1;
								parent_map[grandchild].push_back(current);
								childs.push_back(grandchild);
							}

							// This grandchild might also have only 1 parent, and it is a child of
							// current now So we need to process it too
							childs_to_process.push_back(grandchild);
						}
					} else if (!sheduled_to_process[child]) {
						// Child cannot be removed, so we schedule it for processing
						to_process.push_back(child);
						sheduled_to_process[child] = true;
					}
				}

				// Clear was_original_child flags
				for (usize j = 0; j < original_child_count; ++j)
					was_original_child[childs[j]] = false;
			}

			// Then remove all removed nodes from the graph
			for (usize node_idx = 0; node_idx < node_count; ++node_idx) {
				// If node is marked as removed, remove it from the graph
				if (removed[node_idx]) {
					if (i == 0) opt_graph[node_idx].clear();
					parent_map[node_idx].clear();
					continue;
				}
				// Remove removed childs from the node children list
				if (i == 0) deduplicate_or_remove(opt_graph[node_idx], false, true);
				// Remove removed parents from the parent map
				deduplicate_or_remove(parent_map[node_idx], false, true);
			}

			std::swap(opt_graph, parent_map);
			std::swap(number_of_childs, number_of_parents);
		}

		// END OF THE OPTIMIZATION ALGORITHM

		// Here we need create a compacted version of the graph without removed nodes
		// And return it
		// Build a compact mapping for remaining nodes
		constexpr usize    INVALID_IDX = std::numeric_limits<usize>::max();
		std::vector<usize> old_to_new(node_count, INVALID_IDX);
		std::vector<usize> old_to_new_reverse;
		usize              kept_nodes = 0;
		for (usize node_idx = 0; node_idx < node_count; ++node_idx) {
			if (removed[node_idx]) continue;
			old_to_new_reverse.push_back(node_idx);
			old_to_new[node_idx] = kept_nodes++;
		}

		std::vector<NodeID>             new_idx_to_node;
		std::vector<std::vector<usize>> new_opt_graph(kept_nodes);
		for (usize mapped_idx = 0; mapped_idx < kept_nodes; ++mapped_idx) {
			const usize old_idx = old_to_new_reverse[mapped_idx];
			new_idx_to_node.push_back(idx_to_node[old_idx]);
		}

		for (usize node_idx = 0; node_idx < node_count; ++node_idx) {
			if (removed[node_idx]) continue;
			const auto&        deps = opt_graph[node_idx];
			std::vector<usize> new_dep_vec;
			new_dep_vec.reserve(deps.size());
			for (const auto& dep_idx: deps) new_dep_vec.push_back(old_to_new[dep_idx]);
			new_opt_graph[old_to_new[node_idx]] = std::move(new_dep_vec);
		}

		log_reduced_graph("After optimization", new_opt_graph);
		return { std::move(new_idx_to_node), std::move(new_opt_graph) };
	}

}  // namespace query::internal
