#include "persistent_memory.hpp"

#include "base/collections/optional.hpp"
#include "base/except/exceptions.hpp"
#include "base/types/ints.hpp"
#include <base/collections/maps.hpp>
#include <base/extend_cpp/defer.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include <vm/utils/bijective_map.hpp>

#include <algorithm>
#include <bit>
#include <deque>
#include <functional>
#include <optional>
#include <ranges>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace vm::persistent::detail {


	std::pair<usize, usize> Memory::getHeightOffset(MemoryStateID state) const {
		if_opt_some(root_info.atMaybeCopy(state), entry) {
			auto pos    = entry.position;
			auto height = usize(64 - std::bit_width(pos));
			CORE_ASSERT(!(pos & LEAF_MASK), "top bit would imply that node is leaf");

			return {
				height,
				(pos << height) & ROOT_MASK,
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

	usize Memory::getSize(MemoryStateID state) const {
		if_opt_some(root_info.atMaybeCopy(state), entry) { return entry.size; }
		if_opt_some(leaf_entries.atRightOpt(state), _) { return 1UL; }
		CORE_UNREACHABLE();
	}

	usize Memory::getPos(MemoryStateID state) const {
		if_opt_some(root_info.atMaybeCopy(state), entry) { return entry.position; }
		if_opt_some(leaf_entries.atRightOpt(state), entry) { return LEAF_MASK | entry.idx; }
		CORE_UNREACHABLE();
	}

	std::pair<usize, usize> Memory::getRange(MemoryStateID state) const {
		if_opt_some(root_info.atMaybeCopy(state), entry) {
			return { entry.left_bound, entry.right_bound };
		}
		if_opt_some(leaf_entries.atRightOpt(state), entry) { return { entry.idx, entry.idx + 1 }; }
		CORE_UNREACHABLE();
	}

	usize Memory::getValue(MemoryStateID leaf) const {
		if_opt_some(leaf_entries.atRightOpt(leaf), entry) { return entry.value; }
		CORE_UNREACHABLE();
	}

	void Memory::validateRoot(MemoryStateID root) const {
		if (!root_info.contains(root) && !leaf_entries.atRightOpt(root))
			throw std::invalid_argument("got invalid state");

		auto size             = getSize(root);
		auto [height, offset] = getHeightOffset(root);
		CORE_ASSERT(size <= (1 << height), "root's size is too large");
	}

	MemoryStateID Memory::nodeFromChildren(MemoryStateID left, MemoryStateID right) {
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

	MemoryStateID Memory::nodeFromIdxVar(usize idx, usize var_id) {
		auto leaf_entry = LeafEntry{
			.idx   = idx,
			.value = var_id,
		};

		auto [is_new, node] = leaf_entries.emplaceByLeft(leaf_entry, next_node_id);
		if (is_new) next_node_id++;

		return node;
	}

	MemoryStateID Memory::getChild(Dir dir, MemoryStateID root) const {
		if_opt_some(child_entries.atRightOpt(root), children) {
			auto [left, right] = children;
			return (dir == Dir::Left) ? left : right;
		}

		CORE_ASSERT(
			leaf_entries.atRightOpt(root).has_value(), "if not a root, node, has to be a leaf"
		);
		return EMPTY;
	}

	MemoryStateID Memory::reconstructIdxs(
		MemoryStateID root, std::deque<usize> idxs, LeafBuilder constructor
	) {
		if (!idxs.size()) return root;
		std::ranges::sort(idxs);
		Path iter = getPathTo(root, idxs.front());

		bool  found_diff = false;
		auto  curr_leaf = EMPTY, prev_leaf = EMPTY;
		usize curr_idx = 0, prev_idx = idxs.front();

		auto takeUntilDiffefent = [&](std::deque<usize>& dq) {
			usize last_idx = prev_idx;
			for (; dq.size(); dq.pop_front()) {
				curr_idx = dq.front();
				iter.moveBy(curr_idx - last_idx);
				CORE_ASSERT(iter.trace.size(), "trace must be non-empty");
				auto old_leaf = iter.trace.at(0);
				auto new_leaf = constructor(curr_idx, getValue(old_leaf));
				if (new_leaf != old_leaf) {
					curr_leaf = new_leaf;
					break;
				}
				last_idx = curr_idx;
			}
			CORE_ASSERT(found_diff || dq.empty(), "when we didn't find anything dq is empty");
		};

		takeUntilDiffefent(idxs);
		if (!found_diff) return root;
		prev_leaf = curr_leaf;
		prev_idx  = idxs.front();
		idxs.pop_front();

		std::deque<MemoryStateID> on_left = {}, on_right = {};

		auto extendNeighs = [&](usize height) {
			CORE_ASSERT(on_left.size() == on_right.size(), "paranoid assert this is required");
			while (on_left.size() <= height) on_left.emplace_back(EMPTY);
			while (on_right.size() <= height) on_right.emplace_back(EMPTY);
		};

		auto reconstructLocalNeighs = [&](usize height) {
			CORE_ASSERT(on_left.size() == on_right.size(), "paranoid assert this is required");
			CORE_ASSERT(height + 1 <= iter.trace.size(), "paranoid assertion");
			CORE_ASSERT(height <= on_left.size() && height <= on_right.size(), "paranoid assertion");

			for (usize i = 0; i < height; i++) {
				auto state  = iter.trace.at(i + 1);
				on_left[i]  = EMPTY;
				on_right[i] = EMPTY;

				usize mask = (1 << i);
				if (iter.idx & mask)
					on_left[i] = getChild(Dir::Left, state);
				else
					on_right[i] = getChild(Dir::Right, state);
			}
		};

		extendNeighs(iter.trace.size());
		reconstructLocalNeighs(iter.trace.size());

		std::deque<usize> before = {}, after = {};
		auto [root_offset, root_height] = getHeightOffset(root);

		for (; idxs.size() && idxs.front() < root_offset + (1 << root_height); idxs.pop_front())
			before.emplace_back(idxs.front());
		for (; idxs.size(); idxs.pop_front()) after.emplace_back(idxs.front());

		auto elevateNode
			= [&](MemoryStateID node, usize idx, usize height, usize start_height = 0) {
				  CORE_ASSERT(
					  start_height == getHeightOffset(node).first,
					  "sanity assert - we always use the right node"
				  );
				  for (usize i = start_height; i < height; i++)
					  if (idx & (1 << i))
						  node = nodeFromChildren(on_left[i], node);
					  else
						  node = nodeFromChildren(node, on_right[i]);

				  return node;
			  };

		auto updateNextDiff = [&](std::deque<usize>& dq) {
			found_diff = false;
			takeUntilDiffefent(dq);
			if (!found_diff) return;

			auto new_idx = dq.front();
			dq.pop_front();

			usize height = getLCAHeight(new_idx, prev_idx);
			extendNeighs(height);
			on_left[height] = elevateNode(curr_leaf, prev_idx, height);
			reconstructLocalNeighs(height);

			prev_leaf = curr_leaf;
			prev_idx  = curr_idx;
		};

		auto processQueue = [&](std::deque<usize>& dq) {
			while (dq.size()) updateNextDiff(dq);
		};

		processQueue(before);

		if (!root && prev_idx < root_offset) {
			updateNextDiff(after);

			if (found_diff) {
				usize height = getLCAHeight(root_offset, prev_idx);
				CORE_ASSERT(root_height < height, "We need to go outsude current scope");
				CORE_ASSERT(
					on_left.size() == on_right.size() && on_left.size() <= height,
					"we neet to have high enough neighs"
				);
				for (usize i = root_height; i < height; i++)
					CORE_ASSERT(
						on_left[i] == EMPTY && on_right[i] == EMPTY,
						"This is expected outside the root's scopr"
					);
				on_left[height - 1] = elevateNode(root, root_offset, height - 1, root_height);
			}
		}

		processQueue(after);

		while (on_left.size() && !on_left.back() && !on_right.back()) {
			on_left.pop_back();
			on_right.pop_back();
		}

		return elevateNode(prev_leaf, prev_idx, on_left.size());
	}

	MemoryStateID Memory::rebuildFromTwo(
		MemoryStateID root_1, MemoryStateID root_2, MergeBuilder merge_policy
	) {
		auto [height_1, offset_1] = getHeightOffset(root_1);
		auto [height_2, offset_2] = getHeightOffset(root_2);

		bool change = (height_1 < height_2);

		if (change) {
			std::swap(
				std::tie(height_1, offset_1, root_1, merge_policy.only_1),
				std::tie(height_2, offset_2, root_2, merge_policy.only_2)
			);
			merge_policy.confilicts = [&](usize idx, usize val_1, usize val_2) {
				return merge_policy.confilicts(idx, val_2, val_1);
			};
		}

		auto detailMerge = [&, mem = this](
							   this auto&& self, MemoryStateID node_1, MemoryStateID node_2
						   ) -> MemoryStateID {
			CORE_ASSERT(
				mem->getPos(node_1) == mem->getPos(node_2) || !node_1 || !node_2,
				"both nodes are responsible for the same memory region"
			);

			if (node_1 == node_2) return merge_policy.the_same(node_1);
			if (!node_1) return merge_policy.only_2(node_2);
			if (!node_2) return merge_policy.only_1(node_1);

			if_opt_some(mem->leaf_entries.atRightOpt(node_1), leaf_entry1) {
				auto maybe_entry2 = mem->leaf_entries.atRightOpt(node_2);
				CORE_ASSERT(maybe_entry2.has_value(), "both must be leaves");
				auto leaf_entry2    = *maybe_entry2;
				auto [idx_1, val_1] = leaf_entry1;
				auto [idx_2, val_2] = leaf_entry2;
				CORE_ASSERT(idx_1 == idx_2, "leaves must be of the same index");
				CORE_ASSERT(
					val_1 != val_2, "if values were the same, we would handle this in diff edgecase"
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

			return mem->nodeFromChildren(self(left_1, left_2), self(right_1, right_2));
		};

		return EMPTY;
	}

	usize Memory::getLCAHeight(usize idx_1, usize idx_2) {
		return (usize) std::bit_width(idx_1 ^ idx_2);
	}

	MemoryStateID Memory::getLeaf(MemoryStateID root, usize idx) const {
		auto [height, offset] = getHeightOffset(root);

		if (offset > idx || idx >= offset + (1 << height)) return EMPTY;

		usize mask = (1 << height);
		for (usize i = 0; i < height; i++) {
			mask >>= 1;
			auto [left, right] = child_entries.atRight(root);
			root               = (idx & mask) ? right : left;
		}

		return root;
	}

	Memory::Path Memory::getPathTo(MemoryStateID root, usize idx) const {
		auto [height, offset] = getHeightOffset(root);

		std::deque<MemoryStateID> trace = { root };

		if (idx < offset || idx >= offset + (1 << height))
			trace = { EMPTY };
		else {
			MemoryStateID node = root;
			usize         mask = (1 << height);

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

	[[nodiscard]]
	Memory::Path Memory::getEndPath(Dir end_dir, MemoryStateID root) const {
		CORE_ASSERT(root != EMPTY, "Tryig to get path in empty state");

		auto [l, r] = getRange(root);
		CORE_ASSERT(
			r > 0, "paranod assert - unless root is empty, no state will have right bound equal 0"
		);

		if (end_dir == Dir::Left) return getPathTo(root, l);
		if (end_dir == Dir::Right) return getPathTo(root, r - 1);
		CORE_UNREACHABLE();
	}

	bool Memory::Path::moveToValid(Dir move_dir, usize skip) {
		CORE_ASSERT(trace.size(), "there must be a leaf on the path");

		usize mask = 1;
		trace.pop_front();

		for (; trace.size(); trace.pop_front(), mask <<= 1) {
			MemoryStateID node_id = trace.front();

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

	void Memory::Path::moveBy(usize by) {
		auto [height, offset] = mem->getHeightOffset(root);
		auto new_idx          = idx + by;
		auto to_change        = (usize) std::bit_width(idx ^ new_idx);

		while (trace.size() < to_change) trace.emplace_back(EMPTY);

		if (new_idx < offset || new_idx >= offset + (1 << height)) {
			if (trace.size() > height)
				for (usize i = height; i > 0 && trace.at(i - 1) != EMPTY; i--)
					trace.at(i - 1) = EMPTY;

			idx = new_idx;
			return;
		}

		if (idx < offset || idx >= offset + (1 << height)) {
			auto path = mem->getPathTo(root, new_idx);
			for (usize i = 0; i < path.trace.size(); i++) trace[i] = path.trace[i];
			idx = new_idx;
			return;
		}

		for (usize i = 0; i < to_change; i++) trace.pop_front();

		CORE_ASSERT(trace.size(), "paranoid assert - The root must be at the path");

		for (usize height = to_change; height > 0; height--) {
			usize mask = (1 << (height - 1));
			Dir   dir  = (new_idx & mask) ? Dir::Right : Dir::Left;
			auto  root = trace.front();

			trace.emplace_front(mem->getChild(dir, root));
		}
		idx = new_idx;
	}

	MemoryStateID Memory::buildCommonRoot(std::deque<std::pair<usize, MemoryStateID>> states) {
		CORE_ASSERT(states.size(), "expecting non-empty set of states");

		while (states.size() > 1) {
			CORE_ASSERT(states.front().second, "all states should be not-empty");

			for (usize i = 0; i + 1 < states.size(); i++) {
				auto idx                    = states[i].first;
				auto [idx_next, state_next] = states[i + 1];

				CORE_ASSERT(idx < idx_next, "states should have increasing indexes");
				CORE_ASSERT(state_next, "all states should be not-empty");
			}

			std::deque<std::pair<usize, MemoryStateID>> next_layer = {};

			while (states.size()) {
				auto [idx_first, state_first] = states.front();
				states.pop_front();

				usize         next_idx = idx_first / 2;
				auto left     = (idx_first % 2) ? EMPTY : state_first;
				auto right    = (idx_first % 2) ? state_first : EMPTY;

				if (!states.size() || (idx_first % 2) == 1) {
					next_layer.emplace_back(next_idx, nodeFromChildren(left, right));
					continue;
				}

				auto [idx_follow, state_follow] = states.front();
				if (idx_follow == idx_first + 1) {
					right = state_follow;
					states.pop_front();
				}

				next_layer.emplace_back(next_idx, nodeFromChildren(left, right));
			}

			states = next_layer;
		}

		CORE_ASSERT(states.size() == 1, "There has to be one final root");
		CORE_ASSERT(states.at(0).second, "The final root should not be zero");

		return states.at(0).second;
	}

	MemoryStateID Memory::elevateRoot(MemoryStateID root, usize height) {
		auto [curr_height, offset] = getHeightOffset(root);

		CORE_ASSERT(height < 64, "height should be smaller than bit-length of idx-type");
		CORE_ASSERT(curr_height <= height, "node has to be at smaller height then desired");
		CORE_ASSERT(root, "paranoid assert - EMPTY should fail on first assertion");

		for (; curr_height < height; curr_height++) {
			auto right = (offset & (1 << height)) ? root : EMPTY;
			auto left  = (offset & (1 << height)) ? EMPTY : root;

			root = nodeFromChildren(left, right);
		}

		return root;
	}

	std::pair<MemoryStateID, usize> Memory::getLCA(const Path& path_1, const Path& path_2) {
		CORE_ASSERT(path_1.trace.size() == path_2.trace.size(), "This should always be the case");
		auto height = path_1.trace.size();

		usize new_height = getLCAHeight(path_1.idx, path_2.idx);
		CORE_ASSERT(new_height < height, "new heihgt is a valid idx for the paths");

		MemoryStateID common = path_1.trace.at(new_height);
		CORE_ASSERT(common == path_2.trace.at(new_height), "the element must be common");
		CORE_ASSERT(
			new_height == 0 || (path_1.trace.at(new_height - 1) != path_2.trace.at(new_height - 1)),
			"paths diverge"
		);

		return { common, new_height };
	}

	std::deque<MemoryStateID> Memory::getSubNodesAtHeight(
		MemoryStateID root, usize desired_height
	) const {
		auto [root_height, _] = getHeightOffset(root);
		CORE_ASSERT(root_height >= desired_height, "cannot get subnodes at height bigger than mine");

		std::deque<MemoryStateID> ans = {};

		auto lambda = [&, mem = this](this auto&& self, MemoryStateID state, usize height) -> void {
			if (state == EMPTY) return;
			if (height == desired_height) ans.emplace_back(state);

			if_opt_some(mem->child_entries.atRightOpt(state), children) {
				auto [left, right] = children;

				self(left, height - 1);
				self(right, height - 1);
			}

			CORE_UNREACHABLE();
		};

		lambda(root, root_height);

		return ans;
	}

	MemoryStateID Memory::setMultiple(MemoryStateID root, std::deque<std::pair<usize, usize>> vals) {
		validateRoot(root);

		auto [root_height, root_offset] = getHeightOffset(root);
		if (!vals.size()) return root;

		std::ranges::sort(vals);

		for (usize i = 0; i + 1 < vals.size(); i++)
			if (vals[i].first == vals[i + 1].first)
				throw std::invalid_argument("repeating idx at vals to set");

		std::deque<usize> idxs = {};

		for (auto [idx, val]: vals) idxs.emplace_back(idx);

		return reconstructIdxs(root, idxs, LeafBuilder{ [&](usize idx, base::Optional<usize>) {
								   CORE_ASSERT(vals.size(), "vals cannot suddenly get empty");
								   auto [_idx, val] = vals.front();
								   vals.pop_front();
								   CORE_ASSERT(_idx == idx, "sth got weird at reconstruction level");
								   return nodeFromIdxVar(idx, val);
							   } });
	}

	MemoryStateID Memory::eraseMultiple(MemoryStateID root, std::deque<usize> idxs) {
		validateRoot(root);

		auto [root_height, root_offset] = getHeightOffset(root);
		if (idxs.empty()) return root;

		std::ranges::sort(idxs);

		auto [left, right] = getRange(root);

		for (; !idxs.empty() && idxs.front() < left; idxs.pop_front());
		for (; !idxs.empty() && idxs.back() >= right; idxs.pop_back());

		return reconstructIdxs(root, idxs, LeafBuilder{ [&](usize, base::Optional<usize>) {
								   return EMPTY;
							   } });
	}

	void Memory::pruneHistory(std::vector<MemoryStateID> desired) {
		std::unordered_set<MemoryStateID> stay{ EMPTY };

		while (desired.size()) {
			std::vector<MemoryStateID> dfs_queue = { desired.back() };
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

		std::unordered_set<MemoryStateID> nodes{};
		std::unordered_set<MemoryStateID> leafs{};

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

	std::vector<std::pair<usize, usize>> Memory::toVec(MemoryStateID root) const {
		validateRoot(root);

		auto leaves = getSubNodesAtHeight(root, 0);

		std::vector<std::pair<usize, usize>> ans = {};

		for (auto leaf: leaves) {
			if_opt_some(leaf_entries.atRightOpt(leaf), entry) {
				ans.emplace_back(entry.idx, entry.value);
				continue;
			}
			CORE_UNREACHABLE();
		}

		return ans;
	}

	Memory::diffResT Memory::getDiff(MemoryStateID root_1, MemoryStateID root_2) const {
		validateRoot(root_1);
		validateRoot(root_2);

		auto [height_1, offset_1] = getHeightOffset(root_1);
		auto [height_2, offset_2] = getHeightOffset(root_2);

		bool change = (height_1 < height_2);

		if (height_1 < height_2)
			std::swap(std::tie(height_1, offset_1, root_1), std::tie(height_2, offset_2, root_2));

		using helper = std::function<void(MemoryStateID)>;
		std::vector<std::tuple<usize, base::Optional<usize>, base::Optional<usize>>> ans = {};

		helper add_left = [&](MemoryStateID state) {
			auto vec = toVec(state);

			for (auto [idx, val]: vec) ans.emplace_back(idx, val, std::nullopt);
		};

		helper add_right = [&](MemoryStateID state) {
			auto vec = toVec(state);

			for (auto [idx, val]: vec) ans.emplace_back(idx, std::nullopt, val);
		};

		std::deque<MemoryStateID>     equiv       = getSubNodesAtHeight(root_1, height_2);
		base::Optional<MemoryStateID> counterpart = std::nullopt;

		while (equiv.size()) {
			auto node = equiv.front();
			equiv.pop_front();
			auto [_, offset] = getHeightOffset(node);

			if (offset >= offset_2) break;

			add_left(node);
		}

		auto detailDiff =
			[&, mem = this](this auto&& self, MemoryStateID node_1, MemoryStateID node_2) -> void {
			auto [_height_1, _offset_1] = mem->getHeightOffset(node_1);
			auto [_height_2, _offset_2] = mem->getHeightOffset(node_2);

			CORE_ASSERT(node_1 || node_2, "one of the nodes has to be non-empty");
			CORE_ASSERT(
				_height_1 == _height_2 && _offset_1 == _offset_2,
				"both nodes are responsible for the same memory region"
			);

			if (node_1 == node_2) return;

			if (!node_1) {
				add_right(node_2);
				return;
			}

			if (!node_2) {
				add_left(node_1);
				return;
			}

			match_optional(mem->leaf_entries.atRightOpt(node_1)) {
				opt_none {
					CORE_ASSERT(
						!mem->leaf_entries.atRightOpt(node_1).has_value(),
						"both nodes must be leaves or not"
					);
					CORE_ASSERT(
						mem->child_entries.atRightOpt(node_1).has_value()
							&& mem->child_entries.atRightOpt(node_2).has_value(),
						"both nodes need to have children"
					);
					auto [left_1, right_1] = mem->child_entries.atRight(node_1);
					auto [left_2, right_2] = mem->child_entries.atRight(node_2);

					self(left_1, left_2);
					self(right_1, right_2);
				}
				opt_some(leaf_entry1) {
					if_opt_some(mem->leaf_entries.atRightOpt(node_2), leaf_entry2) {
						auto [idx_1, val_1] = leaf_entry1;
						auto [idx_2, val_2] = leaf_entry2;

						CORE_ASSERT(idx_1 == idx_2, "leaves must be of the same index");

						ans.emplace_back(idx_1, val_1, val_2);

						return;
					}

					CORE_UNREACHABLE();
				}
			}
		};

		if (equiv.size() && getHeightOffset(equiv.front()).second == offset_2) {
			detailDiff(equiv.front(), root_2);
			equiv.pop_front();
		} else {
			add_right(root_2);
		}

		while (equiv.size()) {
			auto node = equiv.front();
			equiv.pop_front();
			auto [_, offset] = getHeightOffset(node);

			CORE_ASSERT(offset > offset_2, "all the indexes at this point should be bigger");

			add_left(node);
		}

		if (change)
			for (auto& [_, val_1, val_2]: ans) std::swap(val_1, val_2);

		return ans;
	}

	MemoryStateID Memory::merge(MemoryStateID root_1, MemoryStateID root_2, ConflictPolicy policy) {
		validateRoot(root_1);
		validateRoot(root_2);

		auto [height_1, offset_1] = getHeightOffset(root_1);
		auto [height_2, offset_2] = getHeightOffset(root_2);

		bool change = (height_1 < height_2);

		if (height_1 < height_2)
			std::swap(std::tie(height_1, offset_1, root_1), std::tie(height_2, offset_2, root_2));

		std::deque<MemoryStateID> equiv = getSubNodesAtHeight(root_1, height_2);

		std::deque<std::pair<usize, MemoryStateID>> to_merge = {};

		while (equiv.size()) {
			auto node = equiv.front();
			equiv.pop_front();
			auto [_, offset] = getHeightOffset(node);

			if (offset >= offset_2) break;

			to_merge.emplace_back(offset >> height_2, node);
		}

		auto detailMerge = [&, mem = this](
							   this auto&& self, MemoryStateID node_1, MemoryStateID node_2
						   ) -> MemoryStateID {
			auto [_height_1, _offset_1] = mem->getHeightOffset(node_1);
			auto [_height_2, _offset_2] = mem->getHeightOffset(node_2);

			CORE_ASSERT(
				_height_1 == _height_2 && _offset_1 == _offset_2,
				"both nodes are responsible for the same memory region"
			);

			if (node_1 == node_2) return node_1;
			if (!node_1) return node_2;
			if (!node_2) return node_1;

			match_optional(mem->leaf_entries.atRightOpt(node_1)) {
				opt_none {
					CORE_ASSERT(
						!mem->leaf_entries.atRightOpt(node_1).has_value(),
						"both nodes must be leaves or not"
					);
					CORE_ASSERT(
						mem->child_entries.atRightOpt(node_1).has_value()
							&& mem->child_entries.atRightOpt(node_2).has_value(),
						"both nodes need to have children"
					);
					auto [left_1, right_1] = mem->child_entries.atRight(node_1);
					auto [left_2, right_2] = mem->child_entries.atRight(node_2);

					return mem->nodeFromChildren(self(left_1, left_2), self(right_1, right_2));
				}
				opt_some(leaf_entry1) {
					if_opt_some(mem->leaf_entries.atRightOpt(node_2), leaf_entry2) {
						auto [idx_1, val_1] = leaf_entry1;
						auto [idx_2, val_2] = leaf_entry2;

						CORE_ASSERT(idx_1 == idx_2, "leaves must be of the same index");

						return policy(idx_1, val_1, val_2);
					}

					CORE_UNREACHABLE();
				}
			}

			CORE_UNREACHABLE();
		};

		if (equiv.size() && getHeightOffset(equiv.front()).second == offset_2) {
			auto counterpart = equiv.front();
			equiv.pop_front();
			if (change) std::swap(counterpart, root_2);
			to_merge.emplace_back(offset_2 >> height_2, detailMerge(counterpart, root_2));
		} else {
			to_merge.emplace_back(offset_2 >> height_2, root_2);
		}

		while (equiv.size()) {
			auto node = equiv.front();
			equiv.pop_front();
			auto [_, offset] = getHeightOffset(node);

			CORE_ASSERT(offset > offset_2, "all the indexes at this point should be bigger");

			to_merge.emplace_back(offset >> height_2, node);
		}

		return buildCommonRoot(to_merge);
	}

	/**
	 * @brief getting a memory of all the statee in [left_bound, right_bound) range
	 */
	MemoryStateID Memory::slice(MemoryStateID root, usize left_idx, usize right_idx) {
		validateRoot(root);

		if (root == EMPTY) return EMPTY;

		if (left_idx > right_idx) throw std::invalid_argument("left idx bigger than right idx");

		auto left_path  = getPathTo(root, left_idx);
		auto right_path = getPathTo(root, right_idx - 1);

		if (!right_path.trace.at(0)) {
			bool decrement = right_path.moveToValid(Dir::Left);
			if (!decrement) return EMPTY;
		}

		if (!left_path.trace.at(0)) {
			bool advanced = left_path.moveToValid(Dir::Right);
			if (!advanced) return EMPTY;
		}

		if (left_path.idx > right_path.idx) return EMPTY;

		auto [lca, lca_height] = getLCA(left_path, right_path);

		if (lca_height == 0) return lca;

		MemoryStateID left_node  = left_path.trace.at(0);
		MemoryStateID right_node = right_path.trace.at(0);

		for (usize curr_height = 1; curr_height < lca_height; curr_height++) {
			MemoryStateID left{}, right{};
			usize         mask = (1 << (curr_height - 1));

			left  = left_node;
			right = left_node;
			if (left_path.idx & mask)
				left = EMPTY;
			else
				right = getChild(Dir::Right, left_path.trace.at(curr_height));

			left_node = nodeFromChildren(left, right);

			left  = right_node;
			right = right_node;
			if (right_path.idx & mask)
				left = getChild(Dir::Left, right_path.trace.at(curr_height));
			else
				right = EMPTY;

			right_node = nodeFromChildren(left, right);

			CORE_ASSERT(left_node && right_node, "both of the nodes on paths should be non-empty");
		}

		return nodeFromChildren(left_node, right_node);
	}

	/**
	 * @brief deallocate lements from [left-idx, right_idx) interval
	 */
	MemoryStateID Memory::eraseRange(MemoryStateID root, usize left_idx, usize right_idx) {
		validateRoot(root);

		if (left_idx > right_idx) throw std::invalid_argument("left idx bigger than right idx");

		if (root == EMPTY) return EMPTY;

		auto path_l = getEndPath(Dir::Left, root);
		auto path_r = getEndPath(Dir::Right, root);

		if (left_idx <= path_l.idx && path_r.idx < right_idx) return EMPTY;

		if (left_idx <= path_l.idx && path_l.idx < right_idx)
			return slice(root, right_idx, path_r.idx);

		if (left_idx <= path_r.idx && path_r.idx < right_idx)
			return slice(root, path_l.idx, left_idx);

		auto lambda = [&, mem = this](this auto&& self, MemoryStateID state) -> MemoryStateID {
			auto [height, offset] = mem->getHeightOffset(state);

			if (right_idx <= offset) return state;
			if (offset + (1 << height) <= left_idx) return state;

			if (left_idx <= offset && offset + (1 << height) <= right_idx) return EMPTY;

			if_opt_some(mem->child_entries.atRightOpt(state), children) {
				auto [left, right] = children;
				left               = self(left);
				right              = self(right);

				return mem->nodeFromChildren(left, right);
			}

			CORE_UNREACHABLE();
		};

		return lambda(root);
	}

	base::Optional<usize> Memory::access(MemoryStateID root, usize idx) const {
		validateRoot(root);

		auto node = getLeaf(root, idx);

		if_opt_some(leaf_entries.atRightOpt(node), leaf_entry) { return leaf_entry.value; }

		return std::nullopt;
	}

	MemoryStateID Memory::erase(MemoryStateID root, usize idx) {
		return eraseMultiple(root, { idx });
	}

	MemoryStateID Memory::set(MemoryStateID root, usize idx, usize val) {
		return setMultiple(root, { { idx, val } });
	}

	usize Memory::size(MemoryStateID root) const {
		validateRoot(root);
		return getSize(root);
	}

	bool Memory::active(MemoryStateID root, usize idx) const {
		validateRoot(root);
		return getLeaf(root, idx) == EMPTY;
	}

	MemoryStateID Memory::getEmpty() const { return EMPTY; }

	Memory::Memory() {
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

	MemoryIterator& MemoryIterator::operator++() {
		if_opt_some(maybe_path, path) {
			bool success = path.moveToValid(Memory::Dir::Right);
			if (!success) maybe_path = std::nullopt;
		}
		return *this;
	}

	MemoryIterator MemoryIterator::operator++(int) {
		auto cpy = *this;
		++(*this);
		return cpy;
	}

	MemoryIterator& MemoryIterator::operator--() {
		if_opt_some(maybe_path, path) {
			bool success = path.moveToValid(Memory::Dir::Left);
			if (!success) maybe_path = std::nullopt;
		}
		return *this;
	}

	MemoryIterator MemoryIterator::operator--(int) {
		auto cpy = *this;
		--(*this);
		return cpy;
	}

	MemoryIterator::MemoryIterator(const Memory& mem, MemoryStateID root, usize idx) {
		mem.validateRoot(root);
		auto path = mem.getPathTo(root, idx);

		if (path.trace.size() && path.trace.at(0) != Memory::EMPTY) maybe_path = path;
	}

	MemoryStateView::MemoryStateView(const Memory& mem, MemoryStateID id): id{ id }, mem{ mem } {}

	[[nodiscard]]
	base::Optional<usize> MemoryStateView::atMaybe(usize idx) const {
		return mem.access(id, idx);
	}

	[[nodiscard]]
	usize MemoryStateView::size() const {
		return mem.size(id);
	}

	[[nodiscard]]
	std::vector<std::pair<usize, usize>> MemoryStateView::toVec() const {
		return mem.toVec(id);
	}

	[[nodiscard]]
	auto MemoryStateView::diff(const MemoryStateView& oth) const {
		return mem.getDiff(id, oth.id);
	}

	usize MemoryStateView::operator[](usize idx) const { return *mem.access(id, idx); }

	[[nodiscard]]
	bool MemoryStateView::contains(usize idx) const {
		return mem.active(id, idx);
	}
}
