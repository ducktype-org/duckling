#pragma once

#include "base/except/exceptions.hpp"
#include <base/collections/maps.hpp>
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
#include <vector>

namespace vm::persistent::detail {
	STRONG_TYPEDEF_INT(MemoryStateID, usize);
}

STRONGLY_TYPED_INT_STD_HASH(vm::persistent::detail::MemoryStateID)

namespace vm::persistent::detail {
	class MemoryStateView;

	class Memory {
		static constexpr auto EMPTY = MemoryStateID{ 0 };
		friend MemoryStateView;

		struct NodeEntry {
			MemoryStateID left;
			MemoryStateID right;

			bool operator==(const NodeEntry&) const = default;
		};

		using NodeEntryH = decltype([](const NodeEntry& h) -> usize {
			return (std::hash<MemoryStateID>{}(h.left) << 1) ^ std::hash<MemoryStateID>{}(h.right);
		});

		struct LeafEntry {
			usize idx;
			usize value;

			bool operator==(const LeafEntry&) const = default;
		};

		using LeafEntryH = decltype([](const LeafEntry& h) -> usize {
			return (std::hash<usize>{}(h.idx) << 1) ^ std::hash<usize>{}(h.value);
		});

		struct RootEntry {
			usize size;
			usize height;
			usize offset;
		};

		enum class Dir { Left, Right };

		struct Path {
			usize                      idx;
			std::vector<MemoryStateID> trace;
		};

		detail::BijectiveMap<NodeEntry, MemoryStateID, NodeEntryH> child_entries{};
		detail::BijectiveMap<LeafEntry, MemoryStateID, LeafEntryH> leaf_entries{};

		base::HashMap<MemoryStateID, RootEntry> root_info{};
		MemoryStateID                           next_node_id = MemoryStateID{ 1 };

		constexpr std::pair<usize, usize> getHeightOffset(MemoryStateID state) const {
			if_opt_some(root_info.atMaybeCopy(state), entry) {
				return {
					entry.height,
					entry.offset,
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

		constexpr usize getSize(MemoryStateID state) const {
			if_opt_some(root_info.atMaybeCopy(state), entry) { return entry.size; }

			if_opt_some(leaf_entries.atRightOpt(state), _) { return 1UL; }

			CORE_UNREACHABLE();
		}

		constexpr void validateState(MemoryStateID state) const {
			if (!root_info.contains(state) && !leaf_entries.atRightOpt(state))
				throw std::invalid_argument("got invalid state");

			auto size             = getSize(state);
			auto [height, offset] = getHeightOffset(state);

			CORE_ASSERT(size <= (1 << height), "root's size is too large");
			CORE_ASSERT(
				height >= 64 || offset % (1 << height) == 0, "offset is not on multiple of 2^k"
			);
		}

		constexpr void validateIdx(MemoryStateID state, usize idx) const {
			auto [height, offset] = getHeightOffset(state);

			if (idx < offset || idx >= offset + (1 << height))
				throw std::invalid_argument("got idx out of state bounds");
		}

		constexpr MemoryStateID nodeFromChildren(MemoryStateID left, MemoryStateID right) {
			CORE_ASSERT(left || right, "cannot get a father of two sentinels");

			auto children       = NodeEntry{ .left = left, .right = right };
			auto [is_new, node] = child_entries.emplaceByLeft(children, next_node_id);

			if (!is_new) return node;

			auto sizeL = getSize(left);
			auto sizeR = getSize(right);

			auto [heightL, offsetL] = getHeightOffset(left);
			auto [heightR, offsetR] = getHeightOffset(right);

			CORE_ASSERT(
				heightL == heightR || left == EMPTY || right == EMPTY,
				"Children are of different hights"
			);
			auto new_height = heightL + 1;
			offsetL >>= new_height;
			offsetR >>= new_height;

			CORE_ASSERT(
				offsetL == offsetR || left == EMPTY || right == EMPTY, "nodes have different offset"
			);
			auto new_offset = offsetL | offsetR;

			CORE_ASSERT(
				sizeL <= (1 << heightL) && sizeL <= (1 << heightR),
				"nodes hold moe values than possible"
			);
			auto new_size = sizeL + sizeR;

			root_info.emplace(
				node,
				RootEntry{
					.size   = new_size,
					.height = new_height,
					.offset = new_offset,
				}
			);
			next_node_id++;

			return node;
		}

		constexpr MemoryStateID nodeFromIdxVar(usize idx, usize var_id) {
			auto leaf = LeafEntry{
				.idx   = idx,
				.value = var_id,
			};

			auto [is_new, node] = leaf_entries.emplaceByLeft(leaf, next_node_id);

			if (is_new) next_node_id++;

			return node;
		}

		[[nodiscard]]
		constexpr MemoryStateID getChild(MemoryStateID state, Dir dir) const {
			if_opt_some(child_entries.atRightOpt(state), children) {
				auto [left, right] = children;
				return (dir == Dir::Left) ? left : right;
			}

			CORE_ASSERT(
				leaf_entries.atRightOpt(state).has_value(), "if not a root, node, has to be a leaf"
			);

			return EMPTY;
		}

		[[nodiscard]]
		constexpr MemoryStateID getLeaf(MemoryStateID state, usize idx) const {
			auto [height, offset] = getHeightOffset(state);

			if (offset > idx || idx >= offset + (1 << height)) return EMPTY;

			usize mask = (1 << height);
			for (usize i = 0; i < height; i++) {
				mask >>= 1;
				auto& entry = child_entries.atRight(state);
				state       = (idx & mask) ? entry.right : entry.left;
			}

			return state;
		}

		[[nodiscard]]
		constexpr Path getPathTo(MemoryStateID state, usize idx) const {
			auto [height, offset] = getHeightOffset(state);

			CORE_ASSERT(
				height >= 64 || offset % (1 << height) == 0, "The offset is not multple of power 2^k"
			);
			CORE_ASSERT(idx >= offset && idx < offset + (1 << height), "The idx was out of bounds!");

			std::vector<MemoryStateID> ans  = { state };
			auto                       node = state;
			usize                      mask = (1 << height);

			for (usize i = 0; i < height; i++) {
				mask >>= 1;
				Dir dir = (idx & mask) ? Dir::Right : Dir::Left;
				node    = getChild(node, dir);
				ans.emplace_back(node);
			}

			return Path{
				.idx   = idx,
				.trace = ans,
			};
		}

		[[nodiscard]]
		constexpr MemoryStateID nodeAtHeight(const Path& path, usize height) const {
			return path.trace.at(path.trace.size() - 1 - height);
		}

		[[nodiscard]]
		constexpr Path getLeftMostPath(MemoryStateID state) const {
			auto [height, offset] = getHeightOffset(state);
			CORE_ASSERT(state != EMPTY, "Tryig to get path in empty state");

			usize                      idx   = offset;
			auto                       node  = state;
			std::vector<MemoryStateID> trace = { state };

			for (usize i = 0; i < height; i++) {
				auto& [left, right] = child_entries.atRight(node);
				CORE_ASSERT(
					left || right,
					"when going down to the leaves, at least one of children is not empty"
				);

				if (left) idx += (1 << (height - 1 - i));
				node = left ? left : right;

				trace.emplace_back(node);
			}

			return Path{
				.idx   = idx,
				.trace = trace,
			};
		}

		[[nodiscard]]
		constexpr Path getRightMostPath(MemoryStateID state) const {
			auto [height, offset] = getHeightOffset(state);
			CORE_ASSERT(state != EMPTY, "Tryig to get path in empty state");

			usize                      idx   = offset;
			auto                       node  = state;
			std::vector<MemoryStateID> trace = { state };

			for (usize i = 0; i < height; i++) {
				auto& [left, right] = child_entries.atRight(node);
				CORE_ASSERT(
					left || right,
					"when going down to the leaves, at least one of children is not empty"
				);

				if (right) idx += (1 << (height - 1 - i));
				node = right ? right : left;

				trace.emplace_back(node);
			}

			return Path{
				.idx   = idx,
				.trace = trace,
			};
		}

		bool pathForward(Path& path, usize skip = 0) const {
			if (path.trace.size() <= 1) return false;

			const auto orig_size = path.trace.size();
			const auto orig_idx  = path.idx;
			usize      mask      = 1;

			path.trace.pop_back();

			for (; path.trace.size(); path.trace.pop_back(), mask <<= 1) {
				MemoryStateID node_id = path.trace.back();
				auto     [_, right]   = child_entries.atRight(node_id);

				if (path.idx & mask)
					path.idx ^= mask;

				if (!right)
					continue;

				if (auto amount = getSize(right); amount <= skip)
					skip -= amount;
				else 
					break;
			}

			if (path.trace.size() == 0) return false;

			CORE_ASSERT((path.idx & mask) == 0, "This bit has to be off");
			path.idx ^= mask;

			while (mask > 0) {
				Dir dir = (path.idx & mask) ? Dir::Right : Dir::Left;

				MemoryStateID son     = getChild(path.trace.back(), dir);
				Dir           dir_son = getChild(son, Dir::Left) ? Dir::Left : Dir::Right;

				path.trace.emplace_back(son);

				mask >>= 1;
				CORE_ASSERT((path.idx & mask) == 0, "None of the bits should be on by default");
				if (dir_son == Dir::Right) path.idx ^= mask;
			}

			CORE_ASSERT(path.trace.size() == orig_size, "path should remain the same length");
			CORE_ASSERT(path.idx > orig_idx, "idx should increas");

			return true;
		}

		bool pathBackward(Path& path) const {
			if (path.trace.size() <= 1) return false;

			const auto orig_size = path.trace.size();
			const auto orig_idx  = path.idx;
			usize      mask      = 1;

			path.trace.pop_back();

			for (; path.trace.size(); path.trace.pop_back(), mask <<= 1) {
				MemoryStateID node_id = path.trace.back();
				NodeEntry     entry   = child_entries.atRight(node_id);

				if ((path.idx & mask) == 0) continue;

				path.idx ^= mask;
				if (entry.left) break;
			}

			if (path.trace.size() == 0) return false;

			CORE_ASSERT((path.idx & mask) == 0, "This bit has to be off");

			while (mask > 0) {
				Dir dir = (path.idx & mask) ? Dir::Right : Dir::Left;

				MemoryStateID son     = getChild(path.trace.back(), dir);
				Dir           dir_son = getChild(son, Dir::Right) ? Dir::Right : Dir::Left;

				path.trace.emplace_back(son);

				mask >>= 1;
				CORE_ASSERT((path.idx & mask) == 0, "Trailing bits should be off");
				if (dir_son == Dir::Right) path.idx ^= mask;
			}

			CORE_ASSERT(path.trace.size() == orig_size, "path should remain the same length");
			CORE_ASSERT(path.idx > orig_idx, "idx should increas");

			return true;
		}

		MemoryStateID combineStates(std::deque<std::pair<usize, MemoryStateID>> states) {
			CORE_ASSERT(states.size(), "expecting non-empty set of states");

			for (usize i = 0; i + 1 < states.size(); i++)
				CORE_ASSERT(states[i].first < states[i + 1].first, "states should be increasing");

			while (states.size() > 1) {
				std::deque<std::pair<usize, MemoryStateID>> next_layer;

				while (states.size()) {
					auto [idx_first, state_first] = states.front();
					states.pop_front();

					usize         next_idx = idx_first / 2;
					MemoryStateID left     = (idx_first % 2) ? EMPTY : state_first;
					MemoryStateID right    = (idx_first % 2) ? state_first : EMPTY;

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

			return states.at(0).second;
		}

		MemoryStateID ensureHeightAtLeast(MemoryStateID state, usize height) {
			auto [curr_height, offset] = getHeightOffset(state);

			CORE_ASSERT(curr_height <= height, "node has to be at smaller height then desired");

			for (; curr_height < height; curr_height++) {
				MemoryStateID right = (offset & (1 << height)) ? state : EMPTY;
				MemoryStateID left  = (offset & (1 << height)) ? EMPTY : state;

				state = nodeFromChildren(left, right);
			}

			return state;
		}

		std::pair<MemoryStateID, usize> getLCA(const Path& path_1, const Path& path_2) {
			CORE_ASSERT(
				path_1.trace.size() == path_2.trace.size(), "This should always be the case"
			);
			auto height = path_1.trace.size();

			auto new_height = (usize) std::bit_width(path_1.idx ^ path_2.idx);
			CORE_ASSERT(new_height < height, "new heihgt is a valid idx for the paths");

			MemoryStateID common = nodeAtHeight(path_1, new_height);

			CORE_ASSERT(common == nodeAtHeight(path_2, new_height), "the element must be common");

			CORE_ASSERT(
				new_height == 0
					|| (nodeAtHeight(path_1, new_height - 1)
			            != nodeAtHeight(path_2, new_height - 1)),
				"paths diverge"
			);

			return { common, new_height };
		}

		std::vector<MemoryStateID> getSubNodesAtHeight(
			MemoryStateID state, usize desired_height
		) const {
			auto [root_height, _] = getHeightOffset(state);
			CORE_ASSERT(
				root_height >= desired_height, "cannot get subnodes at height bigger than mine"
			);

			std::vector<MemoryStateID> ans = {};

			auto lambda = [&](this auto&& self, MemoryStateID root, usize height) -> void {
				if (root == EMPTY) return;
				if (height == desired_height) ans.emplace_back(root);

				if_opt_some(child_entries.atRightOpt(root), children) {
					auto [left, right] = children;

					self(left, height - 1);
					self(right, height - 1);
				}

				CORE_UNREACHABLE();
			};

			lambda(state, root_height);

			return ans;
		}

	public:
		constexpr MemoryStateID setMultiple(
			MemoryStateID state, std::deque<std::pair<usize, usize>> vals
		) {
			validateState(state);

			auto [height, offset] = getHeightOffset(state);
			if (!vals.size()) return state;

			std::ranges::sort(vals);

			for (usize i = 0; i + 1 < vals.size(); i++)
				if (vals[i] == vals[i + 1])
					throw std::invalid_argument("repeating idx at vals to set");

			if (state == EMPTY) {
				std::deque<std::pair<usize, MemoryStateID>> all = {};
				for (auto [idx, var_id]: vals) all.emplace_back(idx, nodeFromIdxVar(idx, var_id));
				return combineStates(all);
			}

			usize orig_offset = offset >> height;

			auto getGroup = [&](usize offset_prefix) {
				std::deque<std::pair<usize, MemoryStateID>> group = {};
				for (auto [idx, var_id]: vals)
					if (idx >> height == offset_prefix)
						group.emplace_back(idx, nodeFromIdxVar(idx, var_id));
					else
						break;

				for (usize i = 0; i < group.size(); i++) vals.pop_front();

				return group;
			};

			std::deque<std::pair<usize, MemoryStateID>> to_merge = {};

			while (vals.size()) {
				usize curr_offset = (vals.front().first >> height);
				if (curr_offset >= orig_offset) break;

				to_merge.emplace_back(curr_offset, combineStates(getGroup(curr_offset)));
			}

			auto in_bounds = getGroup(orig_offset);

			auto lambda = [&](
							  this auto&& self, MemoryStateID root, usize height, usize offset
						  ) -> MemoryStateID {
				auto [_height, _offset] = getHeightOffset(root);
				CORE_ASSERT(_height == height && offset == _offset, "sth went terribly wrong");

				while (in_bounds.size() && in_bounds.front().first < offset) in_bounds.pop_front();

				if (!in_bounds.size()) return root;
				if (in_bounds.front().first >= offset + (1 << height)) return root;

				if (auto [idx, state] = in_bounds.front(); height == 0) {
					CORE_ASSERT(idx == offset, "Deduction has failed me, dear Watson");
					return state;
				}

				auto [left, right] = child_entries.atRight(root);
				left               = self(left, height - 1, offset);
				right              = self(right, height - 1, offset + (1 << (height - 1)));

				CORE_ASSERT(left || right, "One path down is required");
				return nodeFromChildren(left, right);
			};

			to_merge.emplace_back(orig_offset, lambda(state, height, offset));

			while (vals.size()) {
				usize curr_offset = (vals.front().first >> height);
				to_merge.emplace_back(curr_offset, combineStates(getGroup(curr_offset)));
			}

			for (auto& [idx, state]: to_merge) {
				auto [_height, _offset] = getHeightOffset(state);
				CORE_ASSERT(_height <= height, "We should never go past the hight of original");
				state = ensureHeightAtLeast(state, height);
			}

			return combineStates(to_merge);
		}

		MemoryStateID eraseMultiple(MemoryStateID state, std::deque<usize> idxs) {
			validateState(state);

			auto [height, offset] = getHeightOffset(state);
			if (idxs.empty()) return state;

			std::ranges::sort(idxs);

			auto path_left  = getLeftMostPath(state);
			auto path_right = getRightMostPath(state);

			CORE_ASSERT(
				path_left.trace.size() == path_right.trace.size()
					&& path_left.trace.size() == height && path_left.idx <= path_right.idx,
				"This should always be the case"
			);

			for (; !idxs.empty() && idxs.front() < path_left.idx; idxs.pop_front());
			for (; !idxs.empty() && idxs.back() > path_right.idx; idxs.pop_back());

			auto work_copy = idxs;

			for (; !work_copy.empty() && work_copy.front() == path_left.idx; work_copy.pop_front())
				if (!pathForward(path_left)) return EMPTY;

			for (; !work_copy.empty() && work_copy.back() == path_right.idx; work_copy.pop_back())
				CORE_ASSERT(
					!pathBackward(path_right), "there must be at least one el oustide of idxs"
				);

			auto [common, __] = getLCA(path_left, path_right);

			auto lambda = [&](
							  this auto&& self, MemoryStateID root, usize height, usize offset
						  ) -> MemoryStateID {
				auto [_height, _offset] = getHeightOffset(root);
				CORE_ASSERT(_height == height && offset == _offset, "sth went terribly wrong");

				while (!idxs.empty() && idxs.front() < offset) idxs.pop_front();

				if (idxs.empty()) return root;
				if (idxs.front() >= offset + (1 << height)) return root;
				if (height == 0) return EMPTY;

				if_opt_some(child_entries.atRightOpt(root), children) {
					auto [left, right] = children;
					left               = self(left, height - 1, offset);
					right              = self(right, height - 1, offset + (1 << (height - 1)));

					return (left || right) ? nodeFromChildren(left, right) : EMPTY;
				}

				CORE_UNREACHABLE();
			};

			auto [final_height, final_offset] = getHeightOffset(common);

			return lambda(common, final_height, final_offset);
		}

		void pruneHistory(std::vector<MemoryStateID> desired) {
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

		std::vector<std::pair<usize, usize>> toVec(MemoryStateID state) const {
			validateState(state);

			auto leaves = getSubNodesAtHeight(state, 0);

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

		auto getDiff(MemoryStateID stateL, MemoryStateID stateR) const {
			validateState(stateL);
			validateState(stateR);

			auto [heightL, offsetL] = getHeightOffset(stateL);
			auto [heightR, offsetR] = getHeightOffset(stateR);

			bool change = (heightL < heightR);

			if (heightL < heightR)
				std::swap(std::tie(heightL, offsetL, stateL), std::tie(heightR, offsetR, stateR));

			using helper = std::function<void(MemoryStateID)>;
			std::vector<std::tuple<usize, base::Optional<usize>, base::Optional<usize>>> ans = {};

			helper addLeft = [&](MemoryStateID state) {
				auto vec = toVec(state);

				for (auto [idx, val]: vec) ans.emplace_back(idx, val, std::nullopt);
			};

			helper addRight = [&](MemoryStateID state) {
				auto vec = toVec(state);

				for (auto [idx, val]: vec) ans.emplace_back(idx, std::nullopt, val);
			};

			std::vector<MemoryStateID>    equiv       = getSubNodesAtHeight(stateL, heightR);
			base::Optional<MemoryStateID> counterpart = std::nullopt;

			for (auto node: equiv) {
				auto [_, offset] = getHeightOffset(node);

				if (offset == offsetR) {
					counterpart = node;
					continue;
				}

				addLeft(node);
			}

			auto detailDiff
				= [&](this auto&& self, MemoryStateID node_1, MemoryStateID node_2) -> void {
				auto [_height_1, _offset_1] = getHeightOffset(node_1);
				auto [_height_2, _offset_2] = getHeightOffset(node_2);

				CORE_ASSERT(node_1 || node_2, "one of the nodes has to be non-empty");
				CORE_ASSERT(
					_height_1 == _height_2 && _offset_1 == _offset_2,
					"both nodes are responsible for the same memory region"
				);

				if (node_1 == node_2) return;

				if (!node_1) {
					addRight(node_2);
					return;
				}

				if (!node_2) {
					addLeft(node_1);
					return;
				}

				match_optional(leaf_entries.atRightOpt(node_1)) {
					opt_none {
						CORE_ASSERT(
							!leaf_entries.atRightOpt(node_1).has_value(),
							"both nodes must be leaves or not"
						);
						CORE_ASSERT(
							child_entries.atRightOpt(node_1).has_value()
								&& child_entries.atRightOpt(node_2).has_value(),
							"both nodes need to have children"
						);
						auto [left_1, right_1] = child_entries.atRight(node_1);
						auto [left_2, right_2] = child_entries.atRight(node_2);

						self(left_1, left_2);
						self(right_1, right_2);
					}
					opt_some(leaf_entry1) {
						if_opt_some(leaf_entries.atRightOpt(node_2), leaf_entry2) {
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

			match_optional(counterpart) {
				opt_none { addRight(stateR); }
				opt_some(mirrorR) { detailDiff(mirrorR, stateR); }
			}

			if (change)
				for (auto& [_, val_1, val_2]: ans) std::swap(val_1, val_2);

			return ans;
		}

		MemoryStateID slice(MemoryStateID state, usize left_bound, usize right_bound) {
			validateState(state);

			validateIdx(state, left_bound);
			validateIdx(state, right_bound);

			if (state == EMPTY) return EMPTY;

			auto left_path  = getPathTo(state, left_bound);
			auto right_path = getPathTo(state, right_bound);

			if (!nodeAtHeight(left_path, 0)) {
				bool advanced = pathForward(left_path);
				if (!advanced) return EMPTY;
			}

			if (!nodeAtHeight(right_path, 0)) {
				bool decrement = pathBackward(right_path);
				if (!decrement) return EMPTY;
			}

			if (left_path.idx > right_path.idx) return EMPTY;

			auto [lca, lca_height] = getLCA(left_path, right_path);

			if (lca_height == 0) return lca;

			MemoryStateID left_node  = nodeAtHeight(left_path, 0);
			MemoryStateID right_node = nodeAtHeight(right_path, 0);

			for (usize height = 1; height < lca_height; height++) {
				MemoryStateID left{}, right{};
				usize         mask = (1 << (height - 1));

				left = right = left_node;
				if (left_path.idx & mask)
					left = EMPTY;
				else
					right = getChild(nodeAtHeight(left_path, height), Dir::Right);

				left_node = nodeFromChildren(left, right);

				left = right = right_node;
				if (right_path.idx & mask)
					left = getChild(nodeAtHeight(right_path, height), Dir::Left);
				else
					right = EMPTY;

				right_node = nodeFromChildren(left, right);
			}

			return nodeFromChildren(left_node, right_node);
		}

		base::Optional<usize> access(MemoryStateID state, usize idx) const {
			validateState(state);
			validateIdx(state, idx);

			auto node = getLeaf(state, idx);

			if_opt_some(leaf_entries.atRightOpt(node), leaf_entry) { return leaf_entry.value; }

			return std::nullopt;
		}

		MemoryStateID erase(MemoryStateID state, usize idx) {
			return eraseMultiple(state, { idx });
		}

		MemoryStateID set(MemoryStateID state, usize idx, usize val) {
			return setMultiple(state, { { idx, val } });
		}

		[[nodiscard]]
		usize size(MemoryStateID state) const {
			validateState(state);
			return getSize(state);
		}

		bool active(MemoryStateID state, usize idx) const {
			validateState(state);
			return getLeaf(state, idx) == EMPTY;
		}

		[[nodiscard]]
		MemoryStateID getEmpty() const {
			return EMPTY;
		}

		Memory() {
			root_info.put(
				EMPTY,
				RootEntry{
					.size   = 0,
					.height = 64,
					.offset = 0,
				}
			);
			child_entries.emplaceByLeft(NodeEntry{ .left = EMPTY, .right = EMPTY }, EMPTY);
		}
	};

	class MemoryStateView {
		MemoryStateID id;
		const Memory& mem;

		class Iterator {
			base::Optional<Memory::Path> maybe_path;
			const Memory&                mem;

		public:
			Iterator& operator++() {
				if_opt_some(maybe_path, path) {
					bool success = mem.pathForward(path);
					if (!success) maybe_path = std::nullopt;
				}
				return *this;
			}

			Iterator operator++(int) {
				auto cpy = *this;
				(*this)++;
				return cpy;
			}

			Iterator& operator--() {
				if_opt_some(maybe_path, path) {
					bool success = mem.pathBackward(path);
					if (!success) maybe_path = std::nullopt;
				}
				return *this;
			}

			Iterator operator--(int) {
				auto cpy = *this;
				(*this)--;
				return cpy;
			}

	}

	public:
		  MemoryStateView(const Memory& mem, MemoryStateID id):
		  id{ id },
		  mem{ mem } {}

		[[nodiscard]]
		base::Optional<usize> atMaybe(usize idx) const {
			return mem.access(id, idx);
		}

		[[nodiscard]]
		usize size() const {
			return mem.size(id);
		}

		[[nodiscard]]
		std::vector<std::pair<usize, usize>> toVec() const {
			return mem.toVec(id);
		}

		[[nodiscard]]
		auto diff(const MemoryStateView& oth) const {
			return mem.getDiff(id, oth.id);
		}

		usize operator[](usize idx) const { return *mem.access(id, idx); }

		[[nodiscard]]
		bool contains(usize idx) const {
			return mem.active(id, idx);
		}
	};
}
