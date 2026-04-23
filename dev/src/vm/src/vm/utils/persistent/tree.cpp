#include "tree.hpp"

#include "base/collections/optional.hpp"
#include "base/except/exceptions.hpp"
#include "base/types/ints.hpp"
#include <base/collections/maps.hpp>
#include <base/extend_cpp/defer.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include <vm/utils/bijective_map.hpp>

#include <algorithm>
#include <deque>
#include <functional>
#include <optional>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace vm::persistent::detail {


	std::pair<usize, usize> SegmentTree::getHeightOffset(NodeID state) const {
		if_opt_some(root_info.atMaybeCopy(state), entry) {
			auto pos = entry.position;
			CORE_ASSERT(!(pos & LEAF_MASK), "top bit would imply that node is leaf");

			return {
				heightFromPos(pos),
				offsetFromPos(pos),
			};
		}
		if_opt_some(leaf_entries.atRightOpt(state), val) {
			return {
				0UL,
				val.idx,
			};
		}
		CORE_UNREACHABLE();
	}

	usize SegmentTree::getSize(NodeID state) const {
		if_opt_some(root_info.atMaybeCopy(state), entry) { return entry.size; }
		if_opt_some(leaf_entries.atRightOpt(state), _) { return 1UL; }
		CORE_UNREACHABLE();
	}

	usize SegmentTree::getPos(NodeID state) const {
		if_opt_some(root_info.atMaybeCopy(state), entry) { return entry.position; }
		if_opt_some(leaf_entries.atRightOpt(state), entry) { return LEAF_MASK | entry.idx; }
		CORE_UNREACHABLE();
	}

	std::pair<usize, usize> SegmentTree::getRange(NodeID state) const {
		if_opt_some(root_info.atMaybeCopy(state), entry) {
			return { entry.left_bound, entry.right_bound };
		}
		if_opt_some(leaf_entries.atRightOpt(state), entry) { return { entry.idx, entry.idx + 1 }; }
		CORE_UNREACHABLE();
	}

	usize SegmentTree::getValue(NodeID leaf) const {
		if_opt_some(leaf_entries.atRightOpt(leaf), entry) { return entry.value; }
		CORE_UNREACHABLE();
	}

	void SegmentTree::validateRoot(NodeID root) const {
		if (!root_info.contains(root) && !leaf_entries.atRightOpt(root))
			throw std::invalid_argument("got invalid state");

		auto size             = getSize(root);
		auto [height, offset] = getHeightOffset(root);
		CORE_ASSERT(size <= (1 << height), "root's size is too large");
	}

	NodeID SegmentTree::nodeFromChildren(NodeID left, NodeID right) {
		auto pos_left = getPos(left), pos_right = getPos(right);
		CORE_ASSERT(pos_right != 1 && pos_left != 1, "top node cannot be ever passed");
		CORE_ASSERT(
			(pos_left >> 1) == (pos_right >> 1) || left == EMPTY || right == EMPTY,
			"Children are of different hights"
		);

		auto children       = ChildEntry{ .left_child = left, .right_child = right };
		auto [is_new, node] = child_entries.emplaceByLeft(children, next_node_id);

		if (is_new) {
			root_info.emplace(
				node,
				RootEntry{
					.size        = getSize(left) + getSize(right),
					.position    = (pos_left | pos_right) >> 1,
					.left_bound  = getRange(left).first,
					.right_bound = getRange(right).second,
				}
			);
			next_node_id++;
		}

		return node;
	}

	NodeID SegmentTree::nodeFromIdxVar(usize idx, usize var_id) {
		auto leaf_entry = LeafEntry{
			.idx   = idx,
			.value = var_id,
		};

		auto [is_new, node] = leaf_entries.emplaceByLeft(leaf_entry, next_node_id);
		if (is_new) next_node_id++;

		return node;
	}

	NodeID SegmentTree::getChild(Dir dir, NodeID root) const {
		if_opt_some(child_entries.atRightOpt(root), children) {
			auto [left, right] = children;
			return (dir == Dir::Left) ? left : right;
		}

		CORE_ASSERT(
			leaf_entries.atRightOpt(root).has_value(), "if not a root, node, has to be a leaf"
		);
		return EMPTY;
	}

	NodeID SegmentTree::mergeTwoRoots(NodeID root_1, NodeID root_2) {
		if (!root_1) return root_2;
		if (!root_2) return root_1;

		auto pos_1 = getPos(root_1);
		auto pos_2 = getPos(root_2);

		CORE_ASSERT(
			!inSubtree(pos_1, pos_2) && !inSubtree(pos_2, pos_1),
			"none of the roots can be other's ancestors"
		);

		auto lca = getLCAPos(pos_1, pos_2);
		CORE_ASSERT(
			inSubtree(pos_1, lca) && inSubtree(pos_2, lca), "Both positions are in LCA's subtree"
		);

		for (; (pos_1 >> 1) != lca; pos_1 >>= 1)
			if (pos_1 & 1)
				root_1 = nodeFromChildren(EMPTY, root_1);
			else
				root_1 = nodeFromChildren(root_1, EMPTY);

		for (; (pos_2 >> 1) != lca; pos_2 >>= 1)
			if (pos_2 & 1)
				root_2 = nodeFromChildren(EMPTY, root_2);
			else
				root_2 = nodeFromChildren(root_2, EMPTY);

		CORE_ASSERT(pos_2 == (pos_1 | 1), "after all those operations they should be siblings");
		CORE_ASSERT(pos_2 != pos_1, "they should be separate");
		CORE_ASSERT(getPos(root_1) == pos_1, "pos_1 should match");
		CORE_ASSERT(getPos(root_2) == pos_2, "pos_2 should match");

		return nodeFromChildren(root_1, root_2);
	}

	void SegmentTree::SurroundingNeigh::moveNodeTo(usize desired_pos) {
		if (!desired_pos || desired_pos == node_pos) return;
		CORE_ASSERT(inSubtree(desired_pos, root_pos), "we are within root's subtree");

		usize height_diff = getLCAHeight(desired_pos, node_pos) - heightFromPos(node_pos);
		for (usize i = (desired_pos == root_pos ? 0 : 1); i < height_diff; i++) {
			node = mem->mergeTwoRoots(siblings.back(), node);
			siblings.pop_back();
			node_pos >>= 1;
		}

		if (desired_pos != root_pos) {
			CORE_ASSERT(siblings.size(), "We need to have at least one sibling");
			std::swap(node, siblings.back());
			node_pos ^= 1;
		}

		CORE_ASSERT(inSubtree(desired_pos, node_pos), "we are above desired pos");

		while (node_pos != desired_pos) {
			auto left_pos  = node_pos << 1;
			auto right_pos = (node_pos << 1 | 1);

			NodeID left = EMPTY, right = EMPTY;
			CORE_ASSERT(
				inSubtree(desired_pos, left_pos) != inSubtree(desired_pos, right_pos),
				"one of children can handle desired pos"
			);

			if (node_pos == mem->getPos(node)) {
				left  = mem->getChild(Dir::Left, node);
				right = mem->getChild(Dir::Right, node);
			} else if (inSubtree(desired_pos, left_pos))
				left = node;
			else
				right = node;

			if (inSubtree(desired_pos, left_pos)) {
				node_pos = left_pos;
				node     = left;
				siblings.emplace_back(right);
			} else {
				node_pos = right_pos;
				node     = right;
				siblings.emplace_back(left);
			}
		}
	}

	std::pair<usize, std::deque<std::pair<usize, NodeID>>> SegmentTree::SurroundingNeigh::inOrder(
		usize upto_here
	) const {
		usize idx     = 0;
		auto  cur_pos = node_pos;

		std::deque<std::pair<usize, NodeID>> list = { { cur_pos, node } };
		CORE_ASSERT(inSubtree(upto_here, root_pos), "target root must be my in root subtree");

		for (auto it = siblings.rbegin(); cur_pos != upto_here; it++) {
			CORE_ASSERT(inSubtree(cur_pos, upto_here), "I need to have a path to the root");
			CORE_ASSERT(it != siblings.rend(), "there is a sibling on this level");

			std::pair<usize, NodeID> cur_sibling = { cur_pos ^ 1, *it };

			if (cur_pos & 1) {
				list.emplace_front(cur_sibling);
				idx++;
			} else
				list.emplace_back(cur_sibling);

			cur_pos >>= 1;
		}

		CORE_ASSERT(list.size(), "list must have at least cur node");

		return { idx, list };
	}

	NodeID SegmentTree::reconstructIdxs(
		NodeID root, std::deque<usize> idxs, LeafBuilder constructor
	) {
		if (idxs.empty()) return root;

		for (auto& idx: idxs) CORE_ASSERT((LEAF_MASK & idx) == 0, "we must get valid leaf values");
		std::ranges::sort(idxs);
		for (usize i = 1; i < idxs.size(); i++) CORE_ASSERT(idxs[i - 1] < idxs[i], "no duplicates");

		auto [height, offset] = getHeightOffset(root);

		auto left_bound = idxs.front(), right_bound = idxs.back();

		if (root) {
			left_bound  = std::min(left_bound, offset);
			right_bound = std::max(right_bound, offset + (1 << height) - 1);
		}

		auto old_root_pos = getPos(root);
		auto new_root_pos = getLCAPos(LEAF_MASK | left_bound, LEAF_MASK | right_bound);

		SurroundingNeigh neigh = {
			.mem      = this,
			.root_pos = new_root_pos,
			.node_pos = new_root_pos,
			.node     = EMPTY,
			.siblings = {},
		};

		std::deque<usize> left = {}, right = {};
		for (; idxs.size(); idxs.pop_front())
			if (idxs.front() < offset)
				left.emplace_back(idxs.front());
			else
				right.emplace_back(idxs.front());

		for (auto idx: left) {
			neigh.moveNodeTo(idx | LEAF_MASK);
			neigh.node = constructor(offset, std::nullopt);
		}
		neigh.moveNodeTo(old_root_pos);
		neigh.node = root;

		for (auto idx: right) {
			neigh.moveNodeTo(idx | LEAF_MASK);
			base::Optional<usize> val = std::nullopt;
			if (neigh.node) val = getValue(neigh.node);
			neigh.node = constructor(offset, val);
		}

		neigh.moveNodeTo(new_root_pos);
		CORE_ASSERT(neigh.siblings.empty(), "all my siblings should have been handled");

		return neigh.node;
	}

	NodeID SegmentTree::rebuildFromTwo(NodeID root_1, NodeID root_2, MergeBuilder merge_policy) {
		CORE_ASSERT(root_1 && root_2, "Both of the states must be non-empty");

		auto pos_1 = getPos(root_1);
		auto pos_2 = getPos(root_2);

		if (offsetFromPos(pos_2) < offsetFromPos(pos_1)) {
			std::swap(pos_1, pos_2);
			std::swap(root_1, root_2);
			std::swap(merge_policy.only_1, merge_policy.only_1);
			merge_policy.confilicts
				= [orig_strat = merge_policy.confilicts](usize idx, usize val_1, usize val_2) {
					  return orig_strat(idx, val_2, val_1);
				  };
		}

		auto detail_merge
			= [&, mem = this](this auto&& self, usize pos, NodeID node_1, NodeID node_2) -> NodeID {
			CORE_ASSERT(
				mem->getPos(node_1) == mem->getPos(node_2) || !node_1 || !node_2,
				"both nodes are responsible for the same memory region"
			);

			if (node_1 == node_2) return merge_policy.the_same(node_1, pos);
			if (!node_1) return merge_policy.only_2(node_2, pos);
			if (!node_2) return merge_policy.only_1(node_1, pos);

			if_opt_some(mem->leaf_entries.atRightOpt(node_1), leaf_entry1) {
				CORE_ASSERT(pos & LEAF_MASK, "leaves have a designated bit on in their position");
				auto maybe_entry2 = mem->leaf_entries.atRightOpt(node_2);
				CORE_ASSERT(maybe_entry2.has_value(), "both must be leaves");
				auto leaf_entry2    = *maybe_entry2;
				auto [idx_1, val_1] = leaf_entry1;
				auto [idx_2, val_2] = leaf_entry2;
				CORE_ASSERT(idx_1 == idx_2, "leaves must be of the same index");
				CORE_ASSERT(
					val_1 != val_2, "if values were the same, we would handle this in prev edgecase"
				);

				return merge_policy.confilicts(idx_1, val_1, val_2);
			}

			CORE_ASSERT(
				!mem->leaf_entries.atRightOpt(node_1).has_value(), "both nodes must be leaves or not"
			);
			CORE_ASSERT(
				mem->child_entries.atRightOpt(node_1).has_value()
					&& mem->child_entries.atRightOpt(node_2).has_value(),
				"both nodes need to have children"
			);
			auto [left_1, right_1] = mem->child_entries.atRight(node_1);
			auto [left_2, right_2] = mem->child_entries.atRight(node_2);
			auto pos_left = pos << 1, pos_right = ((pos << 1) | 1);

			auto rec_left  = self(pos_left, left_1, left_2);
			auto rec_right = self(pos_right, right_1, right_2);

			CORE_ASSERT(!rec_left || inSubtree(mem->getPos(rec_left), pos_left), "stay in subtree");
			CORE_ASSERT(
				!rec_right || inSubtree(mem->getPos(rec_right), pos_right), "stay in subtree"
			);

			return mem->mergeTwoRoots(rec_left, rec_right);
		};

		auto lca = getLCAPos(pos_1, pos_2);

		SurroundingNeigh neigh = {
			.mem      = this,
			.root_pos = lca,
			.node_pos = lca,
			.node     = EMPTY,
			.siblings = {},
		};

		{
			neigh.moveNodeTo(pos_1);
			neigh.node                  = root_1;
			auto [node_1_idx, in_order] = neigh.inOrder(lca);
			in_order.pop_back();

			for (usize i = 0; i < node_1_idx; i++) {
				auto [pos, node_1] = in_order.front();
				in_order.pop_front();
				CORE_ASSERT(node_1 == EMPTY, "all before the leftmost must be empty");
				detail_merge(pos, EMPTY, EMPTY);
			}

			if (!inSubtree(pos_2, pos_1)) {
				CORE_ASSERT(in_order.size(), "There must be node on the list");
				auto [pos, node_1] = in_order.front();
				in_order.pop_front();
				detail_merge(pos, node_1, EMPTY);
			}

			for (; in_order.size(); in_order.pop_front()) {
				auto [pos, node_1] = in_order.front();
				CORE_ASSERT(node_1 == EMPTY, "all between two roots must be empty");
				detail_merge(pos, EMPTY, EMPTY);
			}
		}

		{
			neigh.moveNodeTo(pos_2);
			neigh.node                  = root_2;
			auto [node_2_idx, in_order] = neigh.inOrder(lca);

			if (!inSubtree(pos_2, pos_1)) {
				CORE_ASSERT(in_order.size() >= 2, "There must be sth going on");
				CORE_ASSERT(node_2_idx > 0, "second node must have at least one sibling on left");
				in_order.pop_front();
				node_2_idx--;
			}

			for (usize i = 0; i < node_2_idx; i++) {
				auto [pos, node_1] = in_order.front();
				in_order.pop_front();
				detail_merge(pos, node_1, EMPTY);
			}

			{
				auto [pos, node_1] = in_order.front();
				in_order.pop_front();
				CORE_ASSERT(pos == pos_2, "This is our node");
				detail_merge(pos, node_1, root_2);
			}

			for (; in_order.size(); in_order.pop_front()) {
				auto [pos, node_1] = in_order.front();
				detail_merge(pos, node_1, EMPTY);
			}
		}

		neigh.moveNodeTo(lca);
		return neigh.node;
	}

	SegmentTree::Path SegmentTree::getPathTo(NodeID root, usize idx) const {
		auto [height, offset] = getHeightOffset(root);

		std::deque<NodeID> trace = { root };

		if (idx < offset || idx >= offset + (1 << height))
			trace = { EMPTY };
		else {
			NodeID node = root;
			usize  mask = (1 << height);

			for (usize i = 0; i < height; i++) {
				mask >>= 1;
				Dir dir = (idx & mask) ? Dir::Right : Dir::Left;
				node    = getChild(dir, node);
				trace.emplace_front(node);
			}
		}

		return Path{
			.idx   = idx,
			.trace = trace,
			.mem   = this,
			.root  = root,
		};
	}

	bool SegmentTree::Path::moveToValid(Dir move_dir, usize skip) {
		CORE_ASSERT(trace.size(), "there must be a leaf on the path");

		usize mask = 1;
		trace.pop_front();

		for (; trace.size(); trace.pop_front(), mask <<= 1) {
			NodeID node_id = trace.front();

			if (idx & mask) idx ^= mask;
			if ((idx & mask) == (move_dir == Dir::Right)) continue;

			auto right = mem->getChild(move_dir, node_id);
			if (auto amount = mem->getSize(right); amount <= skip)
				skip -= amount;
			else
				break;
		}

		if (trace.size() == 0) return false;
		CORE_ASSERT((idx & mask) == 0, "This bit has to be off");
		if (move_dir == Dir::Right) idx ^= mask;

		while (mask > 0) {
			Dir dir = (idx & mask) ? Dir::Right : Dir::Left;

			auto son = mem->getChild(dir, trace.front());
			trace.emplace_front(son);

			mask >>= 1;
			CORE_ASSERT((idx & mask) == 0, "Trailing bits should be off");

			auto grandchild = mem->getChild(othDir(move_dir), son);
			if (usize size = mem->getSize(grandchild); size <= skip) {
				skip -= size;
				grandchild = mem->getChild(move_dir, son);
				idx ^= mask;
			}

			CORE_ASSERT(
				(grandchild || !mask),
				"the only case when grandchild is empty id for last iteeration"
			);
		}

		return true;
	}

	NodeID SegmentTree::rebuildWithRange(
		NodeID root, usize left_idx, usize right_idx, RangeBuilder range_constructor
	) {
		CORE_ASSERT(left_idx < right_idx, "Interval must be non-empty");
		CORE_ASSERT(
			(left_idx & LEAF_MASK) == 0 && ((right_idx - 1) & LEAF_MASK) == 0,
			"idxs must be small enough"
		);
		auto left_pos = left_idx, right_pos = (right_idx - 1);

		if (root) {
			auto [offset, height] = getHeightOffset(root);

			left_pos  = std::min(left_pos, offset);
			right_pos = std::max(right_pos, (offset + (1 << height) - 1));
		}

		left_pos |= LEAF_MASK;
		right_pos |= LEAF_MASK;

		auto lca_pos = getLCAPos(left_pos, right_pos);

		SurroundingNeigh neigh = {
			.mem      = this,
			.root_pos = lca_pos,
			.node_pos = lca_pos,
			.node     = EMPTY,
			.siblings = {},
		};

		neigh.moveNodeTo(getPos(root));
		neigh.node = root;

		auto range_nodes = getPosInRange(left_idx, right_idx);
		CORE_ASSERT(range_nodes.size(), "when range non-empty, there must be some nodes");

		usize first_height = heightFromPos(range_nodes.front());
		usize last_height  = heightFromPos(range_nodes.back());

		neigh.moveNodeTo(range_nodes.front());
		{
			auto [idx, list] = neigh.inOrder(lca_pos);
			for (usize i = 0; i < idx; i++) {
				auto [pos, node] = list.front();
				list.pop_front();
				CORE_ASSERT(heightFromPos(pos) >= first_height, "All the siblings are above");
				auto relative_h = heightFromPos(pos) - first_height;
				CORE_ASSERT(relative_h < neigh.siblings.size(), "We can in fact change the node");
				neigh.siblings[relative_h] = range_constructor.out_of_range(node, pos);
			}
		}

		for (; range_nodes.size(); range_nodes.pop_front()) {
			auto pos = range_nodes.front();
			neigh.moveNodeTo(pos);
			neigh.node = range_constructor.in_range(neigh.node, pos);
		}

		{
			auto [idx, list] = neigh.inOrder(lca_pos);
			for (usize i = 0; i <= idx; i++) list.pop_front();

			for (; list.size(); list.pop_front()) {
				auto [pos, node] = list.front();
				list.pop_front();
				CORE_ASSERT(heightFromPos(pos) >= last_height, "All the siblings are above");
				auto relative_h = heightFromPos(pos) - last_height;
				CORE_ASSERT(relative_h < neigh.siblings.size(), "We can in fact change the node");
				neigh.siblings[relative_h] = range_constructor.out_of_range(node, pos);
			}
		}

		neigh.moveNodeTo(lca_pos);
		return neigh.node;
	}

	void SegmentTree::pruneHistory(std::vector<NodeID> desired) {
		std::unordered_set<NodeID> stay{ EMPTY };

		while (desired.size()) {
			std::vector<NodeID> dfs_queue = { desired.back() };
			desired.pop_back();

			while (dfs_queue.size()) {
				auto front = dfs_queue.back();
				dfs_queue.pop_back();

				if (stay.contains(front)) continue;

				stay.insert(front);

				if_opt_some(child_entries.atRightOpt(front), children) {
					auto [left, right] = children;
					dfs_queue.push_back(left);
					dfs_queue.push_back(right);
				}
			}
		}

		std::unordered_set<NodeID> nodes{};
		std::unordered_set<NodeID> leafs{};

		for (auto node_id: stay) {
			if_opt_some(child_entries.atRightOpt(node_id), _) {
				nodes.insert(node_id);
				continue;
			}

			if_opt_some(leaf_entries.atRightOpt(node_id), _) {
				leafs.insert(node_id);
				continue;
			}

			CORE_UNREACHABLE();
		}

		for (auto root: nodes) root_info.erase(root);

		child_entries.pruneByRight(nodes);
		leaf_entries.pruneByRight(leafs);
	}

	SegmentTree::SegmentTree() {
		root_info.put(
			EMPTY,
			RootEntry{
				.size        = 0,
				.position    = 0,
				.left_bound  = 0,
				.right_bound = 0,
			}
		);
		child_entries.emplaceByLeft(ChildEntry{ .left_child = EMPTY, .right_child = EMPTY }, EMPTY);
	}

}
