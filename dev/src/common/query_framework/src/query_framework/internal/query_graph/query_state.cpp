#include "query_state.hpp"

#include <concurrent/base/locks/assert_lock.hpp>
#include <diagnostic_interactive/logger.hpp>
#include <time_stats/time_stats.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/defer.hpp>
#include <base/pointers/ref.hpp>
#include <base/str/str_utils.hpp>
#include <base/types/ints.hpp>

#include <logger/logger.hpp>
#include <query_framework/internal/query_data/query_data.hpp>
#include <query_framework/internal/query_data/query_id.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>
#include <query_framework/module_flags/module_flags.hpp>
#include <query_framework/q_stats/q_stats.hpp>

#include <algorithm>
#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
	// Small local wrappers to make intent explicit while staying on top of std::vector.
	template<typename T>
	struct VectorMap final {
		std::vector<T> data;

		VectorMap() = default;

		explicit VectorMap(usize size): data(size) {}

		VectorMap(usize size, const T& value): data(size, value) {}

		void assign(usize size, const T& value) { data.assign(size, value); }

		[[nodiscard]] usize size() const { return data.size(); }

		decltype(auto) operator[](usize idx) { return data[idx]; }

		decltype(auto) operator[](usize idx) const { return data[idx]; }
	};

	template<typename T>
	struct VectorStack final {
		std::vector<T> data;

		VectorStack() = default;

		explicit VectorStack(usize size): data(size) {}

		VectorStack(usize size, const T& value): data(size, value) {}

		void push(const T& value) { data.push_back(value); }

		void push(T&& value) { data.push_back(std::move(value)); }

		[[nodiscard]] bool empty() const { return data.empty(); }

		void clear() { data.clear(); }

		[[nodiscard]] usize size() const { return data.size(); }

		T& top() { return data.back(); }

		[[nodiscard]] const T& top() const { return data.back(); }

		T pop() {
			T value = std::move(data.back());
			data.pop_back();
			return value;
		}
	};

	template<typename T>
	struct VectorQueue final {
		std::vector<T> data;
		usize          head = 0;

		VectorQueue() = default;

		explicit VectorQueue(usize size): data(size) {}

		VectorQueue(usize size, const T& value): data(size, value) {}

		void push(const T& value) { data.push_back(value); }

		void push(T&& value) { data.push_back(std::move(value)); }

		[[nodiscard]] bool empty() const { return head >= data.size(); }

		[[nodiscard]] usize size() const { return data.size() - head; }

		void clear() {
			data.clear();
			head = 0;
		}

		T pop() {
			T value = std::move(data[head]);
			++head;
			return value;
		}
	};
}

namespace query::internal {


	const QueryGraph& QueryState::getGraph() const {
		CORE_ASSERT(query::enable_query_graph, "Query graph must be enabled to access the graph.");
		return query_graph;
	}

	QueryGraph& QueryState::getGraphMutable() {
		CORE_ASSERT(query::enable_query_graph, "Query graph must be enabled to access the graph.");
		return query_graph;
	}

	void QueryState::addGraphNode(NodeID node_id) {
		if (query::enable_query_graph) {
			CORE_ASSERT(
				!query_graph.node_deps->contains(node_id), "Node already exists in the graph"
			);
			query_graph.node_deps->put(node_id, QueryGraph::ChildrenData{});
		}
	}

	void QueryState::addSideInputNode(NodeID node_id) {
		if (query::enable_query_graph) {
			CORE_ASSERT(node_id.q_id.getData().isInputQuery(), "Node is not an input query");
			query_graph.node_deps->maybePut(node_id, QueryGraph::ChildrenData{});
		}
	}

	void QueryState::addDependency(NodeID from, NodeID to) {
		if (query::enable_query_graph) query_graph.addDependency(from, to);
	}

	base::Optional<CRef<QueryGraph>> QueryState::getPreviousGraph() const {
		if (!previous.has_value()) return base::Optional<CRef<QueryGraph>>{};
		return &previous.value().graph;
	}

	Ref<ActiveGraph> QueryState::getActiveGraph() noexcept { return &active_graph; }

	Ref<TaskPool> QueryState::getTaskPool() const {
		static TaskPool task_pool;
		return &task_pool;
	}

	u64 QueryState::activeQueryCount() const { return active_graph.size(); }

	void QueryState::setPrevNodeColor(internal::NodeID node, PrevColor color) {
		CORE_ASSERT(previous.has_value(), "PreviousCompilation is not set when setting node color");
		CORE_ASSERT(
			!previous->node_colors->contains(node),
			"Node color is already set in previous compilation"
		);
		previous->node_colors->put(node, color);
	}

	CRef<concurrent::ConHashMap<NodeID, QueryState::PrevColor>> QueryState::getPreviousNodeColors(
	) const {
		CORE_ASSERT(
			previous.has_value(), "PreviousCompilation is not set when accessing node colors"
		);
		return previous.value().node_colors.ref();
	}

	void QueryState::setPreviousGraph(QueryGraph&& graph) {
		// This should be called only once per compilation
		static concurrent::AssertLock lock;
		lock.lock();

		CORE_ASSERT(!previous.has_value(), "Previous graph is already set");
		previous.emplace(std::move(graph));

		lock.unlock();
	}

	void QueryState::setPreviousMetadata(MetadataStorage&& metadata) {
		// This should be called only once per compilation
		static concurrent::AssertLock lock;
		lock.lock();

		CORE_ASSERT(previous.has_value(), "Previous graph must be set before setting metadata");
		CORE_ASSERT(previous->metadata.empty(), "Previous metadata is already set!");
		previous->metadata.emplace(std::move(metadata));

		lock.unlock();
	}

	CRef<MetadataStorage> QueryState::getMetadataStorage() const { return &metadata_storage; }

	base::Optional<CRef<MetadataStorage>> QueryState::getPreviousMetadataStorage() const {
		if (!previous.has_value() || !previous->metadata.has_value()) return {};
		return CRef<MetadataStorage>(&previous->metadata.value());
	}

	QueryState::PrevColor QueryState::redGreenSweep(NodeID start_node) {
		// @TODO: #2007 Remove this mutex and make it truly thread-safe.
		static std::mutex red_green_sweep_mutex;

		std::scoped_lock lock(red_green_sweep_mutex);

		// measure time spent in red-green sweep:
		timer::AddToTimeAtomic _(&total_red_green_sweep_time);

		// No previous compilation graph -> cannot decide incremental reuse, mark as needs recompute
		if (!previous.has_value()) return PrevColor::Red;

		const auto& prev_graph  = previous->graph;
		auto&       node_colors = previous->node_colors;

		// If color is already known for this node, return it
		// At this point the node don't need to be in previous graph (can be moved to actual graph
		// in merging) Moved nodes always have their color known already
		auto prev_color_opt = node_colors->atMaybe(start_node);
		if (prev_color_opt.has_value()) return **prev_color_opt;

		// If the node does not exist in the previous graph -> needs recomputation
		if (!prev_graph.nodeExists(start_node)) return PrevColor::Red;

		// Iterative DFS (post-order) over previous graph starting from start_node.
		// A node becomes Green iff all its direct dependencies are Green; otherwise Red.
		// Leaf nodes without an assigned color are marked Green by default.
		struct Frame {
			NodeID node;
			usize  idx;  // next child index to process
		};

		VectorStack<Frame> stack;

		IF_BUILD_TYPE_DEV(std::unordered_set<NodeID> in_stack);

		stack.push(Frame{ .node = start_node, .idx = 0 });

		// This is used to detect back-edges (cycles) in the previous graph
		// The previous graph should be acyclic, but we just check it to PANIC if not
		IF_BUILD_TYPE_DEV(in_stack.insert(start_node);)

		while (!stack.empty()) {
			auto& frame = stack.top();
			auto  node  = frame.node;

			// If already colored (via another path), just pop and continue
			if (node_colors->contains(node)) {
				IF_BUILD_TYPE_DEV(in_stack.erase(node);)

				stack.pop();
				continue;
			}

			// At this point, node must exist in previous graph because its a child of an existing
			// node and aren't colored yet So the cannot be merged yet
			CORE_ASSERT(prev_graph.node_deps->contains(node), "Node should exist in previous graph");

			auto& deps        = *prev_graph.node_deps->atMaybe(node).value();
			auto  deps_holder = deps.getHolder();

			// If node has no entry or no deps -> treat as leaf; mark Green if not colored yet
			// If node is not colored that means node is not input, so we can safely mark it Green
			if (deps_holder->empty()) {
				node_colors->putOrAssign(node, PrevColor::Green);

				IF_BUILD_TYPE_DEV(in_stack.erase(node);)

				stack.pop();
				continue;
			}

			// Process children one by one ensuring post-order coloring
			if (frame.idx < deps_holder->size()) {
				const NodeID& child = (*deps_holder)[frame.idx++];

				// If child's color is known already, continue to next child
				// This is necessary for merging and sweeping algorithm work concurrently
				if (node_colors->contains(child)) continue;

				// Push child for processing
				stack.push(Frame{ .node = child, .idx = 0 });

				IF_BUILD_TYPE_DEV(
					auto insert_result = in_stack.insert(child); CORE_ASSERT(
						insert_result.second,
						"Cycle detected in previous query graph during red-green sweep"
					);
				)

				continue;
			}

			// All children processed. Determine this node's color from its direct dependencies.
			bool all_green = true;
			for (const auto& c: *deps_holder) {
				auto itc = node_colors->atMaybe(c);
				if (!itc.has_value() || *itc.value() != PrevColor::Green) {
					all_green = false;
					break;
				}
			}
			CORE_ASSERT(
				!node_colors->contains(node), "Node color should not be set before processing"
			);
			node_colors->put(node, all_green ? PrevColor::Green : PrevColor::Red);

			IF_BUILD_TYPE_DEV(in_stack.erase(node);)

			stack.pop();
		}

		return *node_colors->atMaybe(start_node).value();
	}

	bool dummyEraseFunction(QueryStableHash) { return false; }

	NodeID QueryState::remapUnstableOrUnregisteredNodes(NodeID node) {
		static base::VectorMap<QueryID, QueryID> old_to_new;

		if (node.q_id.registered() && node.q_id.getData().usesStableHashing()) return node;

		// Here the QueryID is either unregistered or uses unstable hashing, so we do remapping

		if (auto existing = old_to_new.atMaybe(node.q_id); existing.has_value())
			return { **existing, node.hash };


		QueryData dummy_query_data(
			QueryKind::Dummy,
			"Dummy from previous graph created during deserialization",
			{},
			{ .erase_function = dummyEraseFunction }
		);
		QueryID new_qid = registerQuery(dummy_query_data);
		old_to_new.put(node.q_id, new_qid);
		return { new_qid, node.hash };
	}

	void QueryState::mergePreviousGraphIntoCurrentGraph(NodeID start_node) {
		// @TODO: #2007 Remove this mutex and make it trully thread-safe.
		static std::mutex merge_mutex;

		std::scoped_lock lock(merge_mutex);

		// measure time spent in graph merges:
		timer::AddToTimeAtomic _(&total_graph_merge_time);


		// NodeID with unstable hash might have diferent ID and graph in previous graph
		// So merging from such NodeID is not allowed
		CORE_ASSERT(
			start_node.q_id.getData().usesStableHashing(),
			"Cannot merge previous graph starting from QueryID that does not have stable hash"
		);

		// If there is no previous compilation graph, there's nothing to merge
		if (!previous.has_value()) return;

		// If the start node exists in the current graph -> it's already merged or recomputed
		if (query_graph.node_deps->contains(start_node)) return;

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
			if (query_graph.node_deps->contains(node)) continue;

			CORE_ASSERT(
				prev_graph.node_deps->contains(node), "Node to merge should exist in previous graph"
			);

			// Node must have a color assigned already
			CORE_ASSERT(
				previous->node_colors->contains(node),
				"Node to merge should have color assigned in previous graph"
			);

			// Check if color is green
			CORE_ASSERT(
				*previous->node_colors->atMaybe(node).value() == PrevColor::Green,
				"Node to merge should be green"
			);

			// Retrieve dependencies from previous graph; if none -> keep empty deps in current graph
			// Node should exist in previous graph at this point (because its not in current graph yet)

			auto prev_it = prev_graph.node_deps->atMaybe(node);
			CORE_ASSERT(prev_it.has_value(), "Failed to find node in previous graph during merge");

			// Get holder only to hold the assert lock
			auto node_deps_holder = prev_it.value()->getHolder();

			auto prev_deps = node_deps_holder.moveFrom();  // implicit release.
			prev_graph.node_deps->erase(node);

			auto key_value_pair = query_graph.node_deps->maybePut(node, std::move(prev_deps));
			// If the node is an input query, it may already exist in the current graph (added via
			// addSideInputNode). This is because addSideInputNode does not try to merge the input
			// node from the previous graph, but just adds it to the current graph if it does not
			// exist yet. The same applies to queries with preserve_in_graph = true but with
			// can_be_loaded_from_disk == false: they need to be recomputed, but might contain
			// metadata that needs to be loaded from disk. This exception is limited to
			// stable-hashed nodes: a preserved node without stable hashing is remapped to a unique
			// dummy id, so it can never already be present in the current graph.
			CORE_ASSERT(
				key_value_pair != nullptr || node.q_id.getData().isInputQuery()
					|| (node.q_id.getData().tags.preserve_in_graph
			            and !node.q_id.getData().tags.can_be_loaded_from_disk
			            and node.q_id.getData().usesStableHashing()),
				"Node should not exist in current graph during merge"
			);

			// Merge metadata for nodes with preserve_in_graph = true
			if (node.q_id.getData().tags.preserve_in_graph && previous->metadata.has_value()) {
				auto extracted_opt = previous->metadata->extract(node);
				if (extracted_opt.has_value()) {
					// The metadata might already exist in the current graph (the node may have been
					// recomputed in this compilation), so we use maybeEmplace to avoid overwriting
					// it. A query with the same key always produces the same effect, including the
					// same metadata, so keeping either copy is equivalent and there is nothing to
					// merge here.
					metadata_storage.maybeEmplace(std::move(extracted_opt).value());
				}
			}

			// The key-value pair may be null for any node that is already present in the current
			// graph (for example an input query or a preserve_in_graph query added concurrently on
			// another worker). In that case there is nothing more to merge for this node.
			if (key_value_pair == nullptr) continue;

			auto current_deps_holder = key_value_pair->value.getHolder();

			for (const auto& child: *current_deps_holder) stack.push_back(Frame{ .node = child });
		}
	}

	QueryGraph::ReducedGraphData QueryState::reduceOptimizeGraph(const QueryGraph& graph) const {
		// measure time spent in graph optimization:
		time_stats::TrackCategoryTime track_time(time_stats::TimeCategories::GraphOptimization);

		const auto& node_deps = graph.node_deps;
		using LocalNodeID     = usize;


		// First create Map NodeID -> usize to optimize future algorithms that can operate on usize
		// IDs and work on plain vectors instead of hash maps
		base::HashMap<NodeID, LocalNodeID> node_to_idx;
		std::vector<NodeID>                idx_to_node;

		for (const auto& [node, _]: *node_deps) {
			node_to_idx.emplace(node, idx_to_node.size());
			idx_to_node.push_back(node);
		}

		const usize node_count = idx_to_node.size();

		// Map to keep track of touched nodes during optimization
		// All values in this map should always be set to false unless we are in the middle of
		// some algorithm
		VectorMap<bool> touched(node_count, false);

		// Map to keep track of removed nodes during optimization
		VectorMap<bool> removed(node_count, false);

		// Map used only for deduplication inside deduplicate_or_remove.
		VectorMap<bool> seen_for_deduplicate_or_remove(node_count, false);

		// A function that lazily deletes and/or deduplicates entries in-place. Runs in O(n).
		// - deduplicate: removes duplicates within the vector
		// - remove: removes entries already marked as removed
		auto deduplicate_or_remove = [&removed, &seen_for_deduplicate_or_remove](
										 std::vector<LocalNodeID>& vec,
										 const bool                deduplicate,
										 const bool                remove
									 ) {
			auto range = std::ranges::remove_if(
				vec,
				[&removed, &seen_for_deduplicate_or_remove, remove, deduplicate](LocalNodeID child) {
					if (remove && removed[child]) return true;
					if (deduplicate && seen_for_deduplicate_or_remove[child])
						return true;
					else if (deduplicate)
						seen_for_deduplicate_or_remove[child] = true;
					return false;
				}
			);
			vec.erase(range.begin(), range.end());
			// Reset seen map
			if (deduplicate)
				for (auto child: vec) seen_for_deduplicate_or_remove[child] = false;
		};

		// The opt graph will be represented as adjacency list of LocalNodeID IDs
		VectorMap<std::vector<LocalNodeID>> opt_graph(node_count, {});

		// We also need to keep a parent map to be able to traverse back the graph
		VectorMap<std::vector<LocalNodeID>> parent_map(node_count, {});

		// Some algorithms may rely solely on the number of children or parents, and only delete
		// them after the operation is complete. These maps should be kept up to date. Number of
		// children for each node
		VectorMap<u64> number_of_children(node_count, 0);

		// Number of parents for each node
		VectorMap<u64> number_of_parents(node_count, 0);

		// Map to keep track of nodes that need to be preserved during the optimization.
		// This must be filled correctly before any log_reduced_graph calls.
		VectorMap<bool> is_preserve_node(node_count, false);

		// Optional logging of graph size before/after optimization (dev logs).
		const bool log_incremental
			= ::logger::enable_dev_logs
		   && ::logger::isCategoryEnabled(::logger::DevLogCategories::Incremental);

		const auto log_original_graph = [&](std::string_view phase) {
			if (!log_incremental) return;
			u64   edge_count      = 0;
			usize preserved_nodes = 0;
			for (const auto& [node, deps]: *node_deps) {
				auto deps_holder = deps.getHolder();
				edge_count += deps_holder->size();
				if (node.q_id.getData().tags.preserve_in_graph) ++preserved_nodes;
			}
			CORE_DEV_LOG(
				Incremental,
				"[reduceOptGraph] Number of Nodes (",
				phase,
				"): ",
				node_deps->size(),
				", Number of Preserved Nodes: ",
				preserved_nodes,
				", Number of Edges: ",
				edge_count,
				"\n"
			);
		};

		const auto log_reduced_graph = [&](std::string_view phase,
		                                   const auto&      adjacency,
		                                   const auto&      preserved_map,
		                                   const auto&      kept_old_indices) {
			if (!log_incremental) return;
			u64   edge_count      = 0;
			usize preserved_nodes = 0;
			for (const auto& deps: adjacency) edge_count += deps.size();
			for (const auto old_idx: kept_old_indices)
				if (preserved_map[old_idx]) ++preserved_nodes;
			CORE_DEV_LOG(
				Incremental,
				"[reduceOptGraph] Number of Nodes (",
				phase,
				"): ",
				adjacency.size(),
				", Number of Preserved Nodes: ",
				preserved_nodes,
				", Number of Edges: ",
				edge_count,
				"\n"
			);
		};

		log_original_graph("Before optimization");

		// build the parent map and opt graph
		for (const auto& [node, deps]: *node_deps) {
			const usize node_idx = node_to_idx.at(node);

			// Record if this node needs to be preserved
			is_preserve_node[node_idx] = node.q_id.getData().tags.preserve_in_graph;

			// Convert dependencies to index space and deduplicate
			std::vector<LocalNodeID> child_indices;
			auto                     deps_holder = deps.getHolder();
			child_indices.reserve(deps_holder->size());
			for (const auto& dep: *deps_holder) child_indices.push_back(node_to_idx.at(dep));

			// Deduplicate dependencies
			deduplicate_or_remove(child_indices, true, false);

			// Store deduplicated children
			auto& node_children = opt_graph[node_idx];
			node_children       = std::move(child_indices);

			// Set the number of children after deduplication
			number_of_children[node_idx] = node_children.size();

			// Update parent map using deduplicated children
			for (const auto dep_idx: node_children) {
				parent_map[dep_idx].push_back(node_idx);
				number_of_parents[dep_idx] += 1;
			}
		}

		// OPTIMIZATION ALGORITHM GOES HERE
		// Step 1a: Remove all unstable nodes that have 0 parents (recursively)
		// Step 1b: Remove all unstable nodes that have 0 dependencies (recursively)
		// Queue for nodes to process

		VectorStack<LocalNodeID> to_remove_no_parents;

		// Add unstable nodes with 0 parents to processing queue
		for (LocalNodeID node_idx = 0; node_idx < node_count; ++node_idx) {
			if (number_of_parents[node_idx] == 0 && !is_preserve_node[node_idx]) {
				// Note that marking a node as removed does not remove it from the graph yet.
				// It can be lazily removed later after processing.
				removed[node_idx] = true;
				to_remove_no_parents.push(node_idx);
			}
		}

		// Two passes: first use the original direction (roots), then transpose to remove leaves.
		for (int i = 0; i < 2; ++i) {
			// Process roots
			while (!to_remove_no_parents.empty()) {
				LocalNodeID current = to_remove_no_parents.pop();

				// Assert that current node is not preserved
				CORE_ASSERT(!is_preserve_node[current], "Current node should not be preserved");

				// Current node should have been marked as removed already
				CORE_ASSERT(removed[current], "Current node should be marked as removed");

				// Process children - decrease their number of parents
				for (auto child: opt_graph[current]) {
					// Decrease number of parents for the child
					auto& child_parents = number_of_parents[child];
					child_parents -= 1;

					// Set the child as touched
					touched[child] = true;

					// If child does not need to be preserved and has 0 parents, add it to
					// processing queue
					if (child_parents == 0 && !is_preserve_node[child] && !removed[child]) {
						to_remove_no_parents.push(child);
						removed[child] = true;
					}
				}
			}

			if (i == 0) {
				// First pass: clean up and seed the next pass immediately to save time.
				// Remove removed_nodes from the graph
				for (LocalNodeID node_idx = 0; node_idx < node_count; ++node_idx) {
					// If node is removed remove it from the graph
					if (removed[node_idx]) {
						opt_graph[node_idx].clear();
						parent_map[node_idx].clear();
						continue;
					}

					// If node is not removed but touched, we need to filter its parents
					if (touched[node_idx]) {
						// Remove the parents that have been marked as removed during step 1
						deduplicate_or_remove(parent_map[node_idx], false, true);

						// Clear the touched flag
						touched[node_idx] = false;

						// Assert that the actual number of parents matches the stored one
						CORE_ASSERT(
							parent_map[node_idx].size() == number_of_parents[node_idx],
							"Number of parents mismatch after removal"
						);
					}

					// For the next step, if node has 0 children add it to processing queue
					const auto num_children = number_of_children[node_idx];
					if (num_children == 0 && !is_preserve_node[node_idx]) {
						removed[node_idx] = true;
						to_remove_no_parents.push(node_idx);
					}
				}
			}

			// Swap to run the same logic from the opposite direction (roots vs. leaves).
			std::swap(opt_graph, parent_map);
			std::swap(number_of_children, number_of_parents);
		}

		// For the next step we need a processing queue
		VectorQueue<LocalNodeID> to_process;
		VectorMap<bool>          scheduled_to_process(node_count, false);

		// Remove nodes marked as removed from the graph
		for (usize node_idx = 0; node_idx < node_count; ++node_idx) {
			// If node is removed remove it from the graph
			if (removed[node_idx]) {
				opt_graph[node_idx].clear();
				parent_map[node_idx].clear();
				continue;
			}

			// If node is not removed but touched, we need to filter its children
			if (touched[node_idx]) {
				auto& children = opt_graph[node_idx];

				// Remove the children that have been marked as removed during step 1
				deduplicate_or_remove(children, false, true);

				// Clear the touched flag
				touched[node_idx] = false;

				// Assert that the actual number of children matches the stored one
				CORE_ASSERT(
					children.size() == number_of_children[node_idx],
					"Number of children mismatch after removal"
				);
			}

			// For the next step, if node has 0 parents add it to processing queue
			if (parent_map[node_idx].empty()) {
				// Node is root, it must be preserved
				CORE_ASSERT(
					is_preserve_node[node_idx],
					"Root nodes must be preserved at this point of optimization"
				);
				to_process.push(node_idx);
				scheduled_to_process[node_idx] = true;
			}
		}

		// STEP 2a: Remove unstable nodes that have only 1 parent
		// STEP 2b: Remove unstable nodes that have only 1 child
		// Queue for nodes to process; we process from roots to leaves
		// REQUIREMENT: THERE CANNOT BE DUPLICATES IN PARENTS, and the parent map cannot contain
		// deleted nodes. SAME for CHILDREN, since we reverse the graph for the step 2b.
		// This is ensured by the previous steps and the deduplication calls.

		// Two passes: remove nodes with a single parent, then transpose to remove nodes with single
		// child
		for (int i = 0; i < 2; ++i) {
			// Map to keep track of original children of current node during processing
			// To not add same parent multiple times
			VectorMap<bool> was_original_child(node_count, false);

			// This is important since we process nodes with all their parents processed
			VectorMap<usize> number_of_processed_parents(node_count, 0);

			IF_BUILD_TYPE_DEV(VectorMap<bool> processed(node_count, false);)

			// Declare children to process stack
			// It is declared here to avoid reallocations
			VectorStack<LocalNodeID> children_to_process;

			// Invariant: 1. All parents of the current node are processed correctly by this
			// algorithm
			// 2. Current node has > 1 living parents or it is a preserved node
			// 3. Current node is not removed and will not be removed. This is important since we
			// connect grandchildren to current
			// 4. The parents of current migt have been removed already, but the
			// number_of_parents[current] must cointain number of living parents Also the
			// number_of_processed_parents[current] must be correct and contains the number of
			// living processed parents. Point 3 must hold for correctness and linear complexity.
			// 5. All chindren of current are unprocessed. So they weren't removed yet. This point
			// is equivalent to point 1. Algorithm flow: We remove unstable children that have only
			// 1 parent (current), and connect their children (grandchildren of current) to current
			// directly. This might create new children for current that have only 1 parent
			// (current), se we also need to process them. This continues until no more children can
			// be removed. If the children cannot be removed, we schedule them for processing later.
			// If current node has not all parents processed yet, we skip it for now and the last
			// processed parent will re-schedule it. This is required to keep the invariant correct.
			// The algorithm works in amortized linear time since each node is processed only once,
			// and each edge is processed only once.

			while (!to_process.empty()) {
				usize current = to_process.pop();

				// The node can be already removed via adoption after being scheduled
				if (removed[current]) continue;

				// Scheduled to process must be true
				CORE_ASSERT(
					scheduled_to_process[current],
					"Node in processing queue must be scheduled to process"
				);

				// If not all parents are processed yet, skip for now
				// The last processed parent will re-schedule the node
				// We need to make sure the invariant holds and all parents are processed
				if (number_of_processed_parents[current] < number_of_parents[current]) {
					scheduled_to_process[current] = false;
					continue;
				}

				IF_BUILD_TYPE_DEV(
					CORE_ASSERT(
						!processed[current],
						"Node in processing queue must not be processed. Cycle exists in the graph"
					);

					processed[current] = true;
				)

				// Assert invariant holds
				CORE_ASSERT(
					is_preserve_node[current] || number_of_processed_parents[current] > 1,
					base::strConcat(
						"Node in processing queue must be a preserved node or have more than 1 "
						"parent: ",
						std::to_string(current),
						" Number of parents: ",
						std::to_string(number_of_parents[current]),
						" Needs to be preserved: ",
						is_preserve_node[current] ? "true" : "false",
						" Number of children: ",
						std::to_string(number_of_children[current])
					)
				);

				auto& children = opt_graph[current];

				// Iterate over children and search for children that have only 1 parent (current)
				// They can be safely removed

				const usize original_child_count = children.size();

				for (auto child: children) {
					was_original_child[child] = true;
					children_to_process.push(child);
					// Increase the number of processed parents for the child
					number_of_processed_parents[child] += 1;
				}

				while (!children_to_process.empty()) {
					usize child = children_to_process.pop();
					// Assert that child is not current
					CORE_ASSERT(child != current, "Cycle detected during optimization");

					// If child is removed skip it
					if (removed[child]) continue;

					// Child cannot be processed yet
					IF_BUILD_TYPE_DEV(CORE_ASSERT(
										  !processed[child],
										  "Child node must not be processed yet. This also means "
										  "that a cycle exists in the graph"
					);)

					// Check the number of parents
					const auto parent_count = number_of_parents[child];

					// If child has only 1 parent, and it's unstable we can remove it
					if (parent_count == 1 && !is_preserve_node[child]) {
						// Set the child as removed

						removed[child] = true;
						// Change the number of children of current
						number_of_children[current] -= 1;

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
								number_of_children[current] += 1;
								number_of_parents[grandchild] += 1;
								number_of_processed_parents[grandchild] += 1;
								parent_map[grandchild].push_back(current);
								children.push_back(grandchild);
							}

							// This grandchild might also have only 1 parent, and it is a child of
							// current now, so we need to process it too
							children_to_process.push(grandchild);
						}
					} else if (!scheduled_to_process[child]) {
						// Child cannot be removed, so we schedule it for processing
						to_process.push(child);
						scheduled_to_process[child] = true;
					}
				}

				// Clear was_original_child flags
				for (usize j = 0; j < original_child_count; ++j)
					was_original_child[children[j]] = false;
			}

			// Clear the vector Queue for re-use
			// This only saves memory
			to_process.clear();

			// Assert that all nodes are either processed or removed
			IF_BUILD_TYPE_DEV(for (usize node_idx = 0; node_idx < node_count; ++node_idx) {
				CORE_ASSERT(
					processed[node_idx] || removed[node_idx],
					"All nodes in the graph must be either processed or removed"
				);
			})

			if (i == 0) {
				// First pass: immediately seed leaves for the second pass to save time.
				scheduled_to_process.assign(node_count, false);
				// Then remove all removed nodes from the graph
				for (usize node_idx = 0; node_idx < node_count; ++node_idx) {
					// If node is marked as removed, remove it from the graph
					if (removed[node_idx]) {
						opt_graph[node_idx].clear();
						parent_map[node_idx].clear();
						continue;
					}
					// Remove removed children from the node children list
					deduplicate_or_remove(opt_graph[node_idx], false, true);
					// Remove removed parents from the parent map
					deduplicate_or_remove(parent_map[node_idx], false, true);

					// Add leaf nodes to processing queue for next iteration
					if (opt_graph[node_idx].empty()) {
						// Node is leaf, it must be preserved
						CORE_ASSERT(
							is_preserve_node[node_idx],
							"Leaf nodes must be preserved at this point of optimization"
						);
						if (!scheduled_to_process[node_idx]) {
							to_process.push(node_idx);
							scheduled_to_process[node_idx] = true;
						}
					}
				}
			}

			// Swap to re-use the same logic in the transposed direction.
			std::swap(opt_graph, parent_map);
			std::swap(number_of_children, number_of_parents);
		}

		// END OF THE OPTIMIZATION ALGORITHM

		// Here we need to create a compacted version of the graph without removed nodes
		// and return it
		// Build a compact mapping for remaining nodes
		constexpr usize        INVALID_IDX = std::numeric_limits<usize>::max();
		VectorMap<LocalNodeID> old_to_new(node_count, INVALID_IDX);
		std::vector<usize>     old_to_new_reverse;
		usize                  kept_nodes = 0;
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

			// Remove removed children from the node children list from the last optimization step
			deduplicate_or_remove(opt_graph[node_idx], false, true);

			const auto& deps = opt_graph[node_idx];

			std::vector<usize> new_dep_vec;
			new_dep_vec.reserve(deps.size());
			for (const auto& dep_idx: deps) new_dep_vec.push_back(old_to_new[dep_idx]);
			new_opt_graph[old_to_new[node_idx]] = std::move(new_dep_vec);
		}

		log_reduced_graph("After optimization", new_opt_graph, is_preserve_node, old_to_new_reverse);
		return { .nodes = std::move(new_idx_to_node), .adjacency = std::move(new_opt_graph) };
	}

	Ref<MetadataStorage> QueryState::getMetadataStorageMutable() { return &metadata_storage; }

	void QueryState::logDiagnosticForNode(NodeID node_id, Box<dia_int::MessageBase> diagnostic) {
		diagnostic_loggers.maybePutAndUpdate(
			node_id,
			makeBox<dia_int::Logger>(),
			[&](Ref<Box<dia_int::Logger>> logger) mutable {
				logger->refMut()->log(std::move(diagnostic));
			}
		);
	}

	void QueryState::logDiagnosticFromLoggerForNode(NodeID node_id, dia_int::Logger& src_logger) {
		diagnostic_loggers.maybePutAndUpdate(
			node_id,
			makeBox<dia_int::Logger>(),
			[&](Ref<Box<dia_int::Logger>> dst_logger) {
				dst_logger->refMut()->logFromLogger(src_logger);
			}
		);
	}

	void QueryState::clearDiagnosticForNode(NodeID node_id) { diagnostic_loggers.erase(node_id); }

	CRef<concurrent::ConHashMap<NodeID, Box<dia_int::Logger>>> QueryState::getDiagnosticLoggers(
	) const {
		return &diagnostic_loggers;
	}

	base::Optional<CRef<dia_int::Logger>> QueryState::getDiagnosticForNode(NodeID node_id) const {
		if (auto it = diagnostic_loggers.atMaybe(node_id); it.has_value()) return it.value()->ref();
		return {};
	}
}  // namespace query::internal
