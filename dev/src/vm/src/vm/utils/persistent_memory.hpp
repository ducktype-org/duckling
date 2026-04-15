#pragma once

#include "base/except/exceptions.hpp"
#include <base/collections/maps.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include <vm/utils/bijective_map.hpp>

#include <algorithm>
#include <bit>
#include <deque>
#include <ranges>
#include <vector>

namespace vm::persistent::detail {
	STRONG_TYPEDEF_INT(MemoryStateID, u64);
}

STRONGLY_TYPED_INT_STD_HASH(vm::persistent::detail::MemoryStateID)

namespace vm::persistent::detail {
	class Memory {
		static constexpr auto SENTINEL = MemoryStateID{ 0 };

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

		detail::BijectiveMap<NodeEntry, MemoryStateID, NodeEntryH> node_entries{};
		detail::BijectiveMap<LeafEntry, MemoryStateID, LeafEntryH> leaf_entries{};

		base::HashMap<MemoryStateID, RootEntry> root_info{};
		MemoryStateID                           next_node_id = MemoryStateID{ 1 };

		constexpr void validateState(MemoryStateID state) const {
			if (root_info.contains(state)) {
				auto [size, height, offset] = root_info[state];

				CORE_ASSERT(size <= (1 << height), "root's size is too large");
				CORE_ASSERT(offset % (1 << height) == 0, "offset is not on multiple of 2^k");
				return;
			}

			if (!leaf_entries.atRightOpt(state).has_value())
				throw std::invalid_argument("got invalid state");
		}

		constexpr RootEntry getRootInfo(MemoryStateID state) const {
			validateState(state);

			if (auto opt_entry = root_info.atMaybeCopy(state); opt_entry.has_value())
				return opt_entry.value();

			if (auto val = leaf_entries.atRightOpt(state); val)
				return { .size = 1UL, .height = 0UL, .offset = val->idx };

			throw std::invalid_argument("invalid state id");
		}

		constexpr void validateIdx(MemoryStateID state, usize idx) const {
			auto [size, height, offset] = getRootInfo(state);

			if (idx < offset || idx >= offset + (1 << height))
				throw std::invalid_argument("got idx out of state bounds");

			return;
		}

		constexpr MemoryStateID nodeFromChildren(MemoryStateID left, MemoryStateID right) {
			CORE_ASSERT(
				left != SENTINEL || right != SENTINEL, "cannot get a father of two sentinels"
			);

			auto children       = NodeEntry{ .left = left, .right = right };
			auto [is_new, node] = node_entries.emplaceByLeft(children, next_node_id);

			if (!is_new) return node;

			auto [sizeL, heightL, offsetL] = getRootInfo(left);
			auto [sizeR, heightR, offsetR] = getRootInfo(left);

			CORE_ASSERT(
				heightL == heightR || left == SENTINEL || right == SENTINEL,
				"Children are of different hights"
			);
			auto new_height = heightL + 1;

			CORE_ASSERT(
				offsetL >> new_height == offsetR >> new_height || left == SENTINEL
					|| right == SENTINEL,
				"nodes have different offset"
			);
			auto new_offset = (offsetL | offsetR) >> new_height;

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
			auto leaf           = LeafEntry{ .idx = idx, .value = var_id };
			auto [is_new, node] = leaf_entries.emplaceByLeft(leaf, next_node_id);

			if (is_new) next_node_id++;

			return node;
		}

		[[nodiscard]]
		constexpr MemoryStateID getChild(MemoryStateID state, Dir dir) const {
			validateState(state);

			if (leaf_entries.atRightOpt(state).has_value()) return SENTINEL;

			auto& entry = node_entries.atRight(state);
			return (dir == Dir::Left) ? entry.left : entry.right;
		}

		[[nodiscard]]
		constexpr MemoryStateID getNodeAt(MemoryStateID state, usize idx) const {
			auto [_, height, offset] = getRootInfo(state);

			CORE_ASSERT(
				offset <= idx && idx < offset + (1 << height), "idx asked was out of state bounds"
			);

			if (height != 0)
				for (u64 max_bit = 1 << (height - 1); max_bit; max_bit /= 2) {
					auto& entry = node_entries.atRight(state);
					state       = (idx & max_bit) ? entry.right : entry.left;
				}


			return state;
		}

		[[nodiscard]]
		constexpr Path getPathTo(MemoryStateID state, usize idx) const {
			CORE_ASSERT(root_info.contains(state), "no info about the root");
			auto [size, offset, height] = root_info[state];

			CORE_ASSERT(offset % (1 << height) == 0, "The offset is not multple of power 2^k");
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
			auto [_, height, offset] = getRootInfo(state);
			CORE_ASSERT(state != SENTINEL, "Tryig to get path in empty state");

			usize                      idx   = offset;
			auto                       node  = state;
			std::vector<MemoryStateID> trace = { state };

			for (u64 i = 0; i < height; i++) {
				auto& [left, right] = node_entries.atRight(node);
				CORE_ASSERT(
					left != SENTINEL || right != SENTINEL,
					"when going down to the leaves, at least one of children is not empty"
				);

				if (left == SENTINEL) idx += (1 << (height - 1 - i));
				node = (left == SENTINEL) ? right : left;

				trace.emplace_back(node);
			}

			return Path{
				.idx   = idx,
				.trace = trace,
			};
		}

		[[nodiscard]]
		constexpr Path getRightMostPath(MemoryStateID state) const {
			auto [size, height, offset] = getRootInfo(state);
			CORE_ASSERT(state != SENTINEL, "Tryig to get path in empty state");

			usize                      idx   = offset;
			auto                       node  = state;
			std::vector<MemoryStateID> trace = { state };

			for (u64 i = 0; i < height; i++) {
				auto& [left, right] = node_entries.atRight(node);
				CORE_ASSERT(
					left != SENTINEL || right != SENTINEL,
					"when going down to the leaves, at least one of children is not empty"
				);

				if (right != SENTINEL) idx += (1 << (height - 1 - i));
				node = (right == SENTINEL) ? left : right;

				trace.emplace_back(node);
			}

			return Path{
				.idx   = idx,
				.trace = trace,
			};
		}

		bool pathForward(Path& path) const {
			if (path.trace.size() <= 1) return false;

			const auto orig_size = path.trace.size();
			const auto orig_idx  = path.idx;
			usize      mask      = 1;

			path.trace.pop_back();

			for (; path.trace.size(); path.trace.pop_back(), mask <<= 1) {
				MemoryStateID node_id = path.trace.back();
				NodeEntry     entry   = node_entries.atRight(node_id);

				if (path.idx & mask)
					path.idx ^= mask;
				else if (entry.right != SENTINEL)
					break;
			}

			if (path.trace.size() == 0) return false;

			CORE_ASSERT((path.idx & mask) == 0, "This bit has to be off");
			path.idx ^= mask;

			while (mask > 0) {
				Dir dir = (path.idx & mask) ? Dir::Right : Dir::Left;

				MemoryStateID son = getChild(path.trace.back(), dir);
				Dir dir_son       = (getChild(son, Dir::Left) == SENTINEL) ? Dir::Right : Dir::Left;

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
				NodeEntry     entry   = node_entries.atRight(node_id);

				if ((path.idx & mask) == 0) continue;

				path.idx ^= mask;
				if (entry.left != SENTINEL) break;
			}

			if (path.trace.size() == 0) return false;

			CORE_ASSERT((path.idx & mask) == 0, "This bit has to be off");

			while (mask > 0) {
				Dir dir = (path.idx & mask) ? Dir::Right : Dir::Left;

				MemoryStateID son = getChild(path.trace.back(), dir);
				Dir dir_son = (getChild(son, Dir::Right) == SENTINEL) ? Dir::Left : Dir::Right;

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
					MemoryStateID left     = (idx_first % 2) ? SENTINEL : state_first;
					MemoryStateID right    = (idx_first % 2) ? state_first : SENTINEL;

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
			auto [_, curr_height, offset] = getRootInfo(state);

			CORE_ASSERT(curr_height <= height, "node has to be at smaller height then desired");

			for (; curr_height < height; curr_height++) {
				MemoryStateID right = (offset & (1 << height)) ? state : SENTINEL;
				MemoryStateID left  = (offset & (1 << height)) ? SENTINEL : state;

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

	public:
		constexpr MemoryStateID setMultiple(
			MemoryStateID state, std::deque<std::pair<usize, usize>> vals
		) {
			auto [_, height, offset] = getRootInfo(state);
			if (!vals.size()) return state;

			std::ranges::sort(vals);

			for (usize i = 0; i + 1 < vals.size(); i++)
				if (vals[i] == vals[i + 1])
					throw std::invalid_argument("repeating idx at vals to set");

			if (state == SENTINEL) {
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
				auto [_, _height, _offset] = getRootInfo(root);
				CORE_ASSERT(_height == height && offset == _offset, "sth went terribly wrong");

				while (in_bounds.size() && in_bounds.front().first < offset) in_bounds.pop_front();

				if (!in_bounds.size()) return root;
				if (in_bounds.front().first >= offset + (1 << height)) return root;

				if (auto [idx, state] = in_bounds.front(); height == 0) {
					CORE_ASSERT(idx == offset, "Deduction has failed me, dear Watson");
					return state;
				}

				auto [left, right] = node_entries.atRight(root);
				left               = self(left, height - 1, offset);
				right              = self(right, height - 1, offset + (1 << (height - 1)));

				CORE_ASSERT(left != SENTINEL || right != SENTINEL, "One path down is required");
				return nodeFromChildren(left, right);
			};

			to_merge.emplace_back(orig_offset, lambda(state, height, offset));

			while (vals.size()) {
				usize curr_offset = (vals.front().first >> height);
				to_merge.emplace_back(curr_offset, combineStates(getGroup(curr_offset)));
			}

			for (auto& [idx, state]: to_merge) {
				auto [_, _height, _offset] = getRootInfo(state);
				CORE_ASSERT(_height <= height, "We should never go past the hight of original");
				state = ensureHeightAtLeast(state, height);
			}

			return combineStates(to_merge);
		}

		MemoryStateID eraseMultiple(MemoryStateID state, std::deque<usize> idxs) {
			auto [_, height, offset] = getRootInfo(state);
			if (!idxs.size()) return state;

			std::ranges::sort(idxs);

			auto path_left  = getLeftMostPath(state);
			auto path_right = getRightMostPath(state);

			CORE_ASSERT(
				path_left.trace.size() == path_right.trace.size()
					&& path_left.trace.size() == height && path_left.idx <= path_right.idx,
				"This should always be the case"
			);

			for (; idxs.size() && idxs.front() < path_left.idx; idxs.pop_front());
			for (; idxs.size() && idxs.back() > path_right.idx; idxs.pop_back());

			auto work_copy = idxs;

			for (; work_copy.size() && work_copy.front() == path_left.idx; work_copy.pop_front())
				if (!pathForward(path_left)) return SENTINEL;

			for (; work_copy.size() && work_copy.back() == path_right.idx; work_copy.pop_back())
				CORE_ASSERT(
					!pathBackward(path_right), "there must be at least one el oustide of idxs"
				);

			auto [common, __] = getLCA(path_left, path_right);

			auto lambda = [&](
							  this auto&& self, MemoryStateID root, usize height, usize offset
						  ) -> MemoryStateID {
				auto [_, _height, _offset] = getRootInfo(root);
				CORE_ASSERT(_height == height && offset == _offset, "sth went terribly wrong");

				while (idxs.size() && idxs.front() < offset) idxs.pop_front();

				if (!idxs.size()) return root;
				if (idxs.front() >= offset + (1 << height)) return root;
				if (height == 0) return SENTINEL;

				auto [left, right] = node_entries.atRight(root);
				left               = self(left, height - 1, offset);
				right              = self(right, height - 1, offset + (1 << (height - 1)));

				if (left == SENTINEL && right == SENTINEL) return SENTINEL;

				return nodeFromChildren(left, right);
			};

			auto [___, final_height, final_offset] = getRootInfo(common);

			return lambda(common, final_height, final_offset);
		}

		MemoryStateID slice(MemoryStateID state, usize left_bound, usize right_bound) {
			validateIdx(state, left_bound);
			validateIdx(state, right_bound);

			if (state == SENTINEL) return SENTINEL;

			auto left_path  = getPathTo(state, left_bound);
			auto right_path = getPathTo(state, right_bound);

			if (nodeAtHeight(left_path, 0) == SENTINEL) {
				bool advanced = pathForward(left_path);
				if (!advanced) return SENTINEL;
			}

			if (nodeAtHeight(right_path, 0) == SENTINEL) {
				bool decrement = pathBackward(right_path);
				if (!decrement) return SENTINEL;
			}

			if (left_path.idx > right_path.idx) return SENTINEL;

			auto [lca, lca_height] = getLCA(left_path, right_path);

			if (lca_height == 0) return lca;

			MemoryStateID left_node  = nodeAtHeight(left_path, 0);
			MemoryStateID right_node = nodeAtHeight(right_path, 0);

			for (usize height = 1; height < lca_height; height++) {
				MemoryStateID left{}, right{};
				usize         mask = (1 << (height - 1));

				left = right = left_node;
				if (left_path.idx & mask)
					left = SENTINEL;
				else
					right = getChild(nodeAtHeight(left_path, height), Dir::Right);

				left_node = nodeFromChildren(left, right);

				left = right = right_node;
				if (right_path.idx & mask)
					left = getChild(nodeAtHeight(right_path, height), Dir::Left);
				else
					right = SENTINEL;

				right_node = nodeFromChildren(left, right);
			}

			return nodeFromChildren(left_node, right_node);
		}

		base::Optional<usize> access(MemoryStateID state, usize idx) const {
			validateIdx(state, idx);

			auto node = getNodeAt(state, idx);

			CORE_ASSERT(
				leaf_entries.atRightOpt(node).has_value(), "expected that the node will be leaf"
			);

			return leaf_entries.atRight(node).value;
		}

		MemoryStateID erase(MemoryStateID state, usize idx) {
			return eraseMultiple(state, { idx });
		}

		MemoryStateID set(MemoryStateID state, usize idx, usize val) {
			return setMultiple(state, { { idx, val } });
		}

		[[nodiscard]]
		usize size(MemoryStateID state) const {
			return getRootInfo(state).size;
		}

		bool active(MemoryStateID state, usize idx) const {
			validateIdx(state, idx);

			return getNodeAt(state, idx) == SENTINEL;
		}

		[[nodiscard]]
		MemoryStateID getEmpty() const {
			return SENTINEL;
		}

		Memory() {
			root_info.put(
				SENTINEL,
				RootEntry{
					.size   = 0,
					.height = 63,
					.offset = 0,
				}
			);
			node_entries.emplaceByLeft(NodeEntry{ .left = SENTINEL, .right = SENTINEL }, SENTINEL);
		}
	};
}
