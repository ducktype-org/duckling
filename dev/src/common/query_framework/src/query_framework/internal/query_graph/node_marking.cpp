#include "node_marking.hpp"

#include <base/except/exceptions.hpp>

#include <query_framework/external/api.hpp>  // for query::external::InputData definition
#include <query_framework/internal/context_access.hpp>
#include <query_framework/internal/query_graph/node_id.hpp>
#include <query_framework/internal/query_graph/query_graph.hpp>

#include <algorithm>
#include <utility>

namespace query::internal {

	template<typename NodePresentCallback, typename NodeRemovedCallback>
	void callForEveryRemovedInput(
		NodePresentCallback                     present_callback,
		NodeRemovedCallback                     removed_callback,
		CRef<QueryGraph>                        prev_graph,
		std::vector<query::external::InputData> new_inputs
	) {
		// Sort inputs by (hash, q_id)
		std::ranges::sort(
			new_inputs,
			[](const query::external::InputData& a, const query::external::InputData& b) {
				if (a.hash == b.hash) return a.q_id.asInt() < b.q_id.asInt();
				return a.hash < b.hash;
			}
		);

		// Collect previous nodes of interest (Input and SideInput)
		auto                all_nodes = prev_graph->getAllNodes();
		std::vector<NodeID> prev_inputs;
		prev_inputs.reserve(all_nodes.size());
		for (const auto& node: all_nodes) {
			// You should map unregistered nodes to Dummy first for correctness of query framework
			// The function qury_state::remapUnstableOrUnregisteredNodes does it already
			CORE_ASSERT(node.q_id.registered(), "Node from previous graph must be registered.");

			if (node.q_id.getData().kind != QueryKind::SideInput
			    && node.q_id.getData().kind != QueryKind::Input) {
				continue;
			}

			CORE_ASSERT(
				node.q_id.getData().usesStableHashing(),
				"Side/Input nodes must have stable hashes: ",
				node.q_id.getData().name
			);

			CORE_ASSERT(
				!prev_graph->hasDependencies(node),
				"Input nodes should not have dependencies: ",
				node.q_id.getData().name
			);

			prev_inputs.push_back(node);
		}

		// Sort previous nodes by (hash, q_id)
		std::ranges::sort(prev_inputs, [](const NodeID& a, const NodeID& b) {
			if (a.hash.val == b.hash.val) return a.q_id.asInt() < b.q_id.asInt();
			return a.hash.val < b.hash.val;
		});

		// Two-pointer merge-like pass to mark colors
		usize i = 0;  // index into inputs
		usize j = 0;  // index into prev_inputs

		auto cmp_pair = [](const base::Bit256& h1, u64 id1, const base::Bit256& h2, u64 id2) {
			if (h1 == h2) return id1 < id2;
			return h1 < h2;
		};

		while (i < new_inputs.size() && j < prev_inputs.size()) {
			const auto& in   = new_inputs[i];
			const auto& node = prev_inputs[j];

			if (in.hash == node.hash.val && in.q_id.asInt() == node.q_id.asInt()) {
				present_callback(node);
				++i;
				++j;
			} else if (cmp_pair(in.hash, in.q_id.asInt(), node.hash.val, node.q_id.asInt())) {
				// input < node: advance inputs
				++i;
			} else {
				// node < input: mark as red and advance nodes
				removed_callback(node);
				++j;
			}
		}

		// Remaining nodes are red
		for (; j < prev_inputs.size(); ++j) removed_callback(prev_inputs[j]);
	}

	void markPreviousGraphNodesInputs(std::vector<query::external::InputData> inputs) {
		auto state      = ContextAccess::getState();
		auto maybe_prev = state->getPreviousGraph();
		CORE_ASSERT(maybe_prev.has_value(), "Previous graph is not set");
		auto prev_graph       = maybe_prev.value();
		auto present_callback = [&](const NodeID& node) {
			state->setPrevNodeColor(node, QueryState::PrevColor::Green);
		};
		auto removed_callback = [&](const NodeID& node) {
			state->setPrevNodeColor(node, QueryState::PrevColor::Red);
		};
		callForEveryRemovedInput(present_callback, removed_callback, prev_graph, std::move(inputs));
	}

	std::vector<NodeID> findInputsRemovedFromCurrentGraph(
		std::vector<query::external::InputData> inputs
	) {
		auto                state      = ContextAccess::getState();
		auto&               prev_graph = state->getGraph();
		std::vector<NodeID> removed_inputs;
		auto                present_callback = [&](const NodeID&) {
            // do nothing
		};
		auto removed_callback = [&](const NodeID& node) { removed_inputs.push_back(node); };
		callForEveryRemovedInput(present_callback, removed_callback, &prev_graph, std::move(inputs));
		return removed_inputs;
	}
}  // namespace query::internal
