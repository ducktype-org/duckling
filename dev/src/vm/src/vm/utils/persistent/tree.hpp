#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/defer.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/pointers/ref.hpp>

#include <vm/utils/bijective_map.hpp>

#include <algorithm>
#include <bit>
#include <deque>
#include <functional>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace vm::persistent::detail {
	STRONG_TYPEDEF_INT(NodeID, usize);
}

STRONGLY_TYPED_INT_STD_HASH(vm::persistent::detail::NodeID)

namespace vm::persistent::detail {
	class SegmentTree;

	template<typename T>
	concept RebuildRes = std::is_same_v<T, void> || std::is_same_v<T, NodeID>;

	template<typename SelfT, typename ResT>
	concept ValidSignature
		= RebuildRes<ResT> && std::is_same_v<SegmentTree, std::remove_cvref_t<SelfT>>
	   && (std::is_same_v<void, ResT> || !std::is_const_v<SelfT>);

	class SegmentTree final {
		using posT = usize;
		using valT = usize;

		using idxT                        = posT;
		constexpr static usize POS_T_SIZE = 8 * sizeof(posT);

		static_assert(
			std::is_integral_v<posT> && std::is_unsigned_v<posT>, "posT must be an unsigned integer"
		);
		static_assert(
			std::is_integral_v<valT> && std::is_unsigned_v<valT>, "valT must be an unsigned integer"
		);

	public:
		constexpr static NodeID EMPTY    = NodeID{ 0 };
		constexpr static posT   ROOT_POS = 1;
		constexpr static idxT   IDX_END  = idxT(1) << (POS_T_SIZE - 1);

		enum class Dir { Left, Rght };

	private:
		constexpr static posT OFFSET_MASK = (posT(-1) >> 1);
		constexpr static posT LEAF_MASK   = posT(1) << (POS_T_SIZE - 1);
		constexpr static posT TOP_BIT     = posT(1) << (POS_T_SIZE - 1);

		struct ChildEntry {
			NodeID left_child;
			NodeID right_child;

			bool operator==(const ChildEntry&) const = default;
		};

		// Both sides must be hashable for use with BijectiveMap.
		struct ChildEntryH {
			usize operator()(const ChildEntry& h) const {
				return (std::hash<NodeID>{}(h.left_child) << 1)
				     ^ std::hash<NodeID>{}(h.right_child);
			}
		};

		struct LeafEntry {
			idxT idx;
			valT value;

			bool operator==(const LeafEntry&) const = default;
		};

		// Both sides must be hashable for use with BijectiveMap.
		struct LeafEntryH {
			usize operator()(const LeafEntry& h) const {
				return (std::hash<usize>{}(h.idx) << 1) ^ std::hash<usize>{}(h.value);
			}
		};

		/**
		 * @brief Metadata for a branch node, including its size, position, and bounds.
		 */
		struct BranchEntry {
			usize size;
			posT  position;
			idxT  left_bound;
			idxT  right_bound;
		};

		/**
		 * @brief Returns the height of a tree position.
		 * @note Leaf positions have height zero.
		 * @note The empty position has a height greater than every non-empty position.
		 */
		static constexpr usize heightFromPos(posT pos) { return usize(std::countl_zero(pos)); }

		/**
		 * @brief Returns the smallest possible leaf index in a subtree.
		 * @note The empty position has offset zero.
		 */
		static constexpr idxT offsetFromPos(posT pos) {
			auto h = heightFromPos(pos);
			if (h >= POS_T_SIZE) return 0;

			auto shifted = (pos << h);
			CORE_ASSERT(shifted & TOP_BIT, "After the shift, top bit must be set");

			auto ans = shifted & OFFSET_MASK;
			CORE_ASSERT(ans < IDX_END, "Returned idx must be smaller than end of idxs");

			return ans;
		}

		/**
		 * @brief Returns the height of the lowest common ancestor of two positions.
		 * @note If one position is zero, returns the height of the other position.
		 * @note If both positions are zero, returns `POS_T_SIZE`.
		 */
		static constexpr usize getLCAHeight(posT pos_1, posT pos_2) {
			if (pos_1 > pos_2) std::swap(pos_1, pos_2);
			auto h_2 = heightFromPos(pos_2);

			if (pos_1 == 0) return h_2;
			CORE_ASSERT(pos_1 && pos_2, "At this point, both positions are non-empty");

			auto h_1 = heightFromPos(pos_1);
			CORE_ASSERT(h_1 >= h_2, "At this point, pos_1 should be higher than pos_2");

			pos_2 >>= (h_1 - h_2);
			CORE_ASSERT(
				std::bit_width(pos_1) == std::bit_width(pos_2),
				"At this point, both positions must be at the same depth"
			);

			auto ans = (usize) std::bit_width(pos_1 ^ pos_2);
			CORE_ASSERT(
				(pos_1 >> ans) == (pos_2 >> ans),
				"Since both are at the same height, they must be equal if we shift by partial "
				"result"
			);

			ans += h_1;
			return ans;
		}

		/**
		 * @brief Returns the position of the lowest common ancestor of two positions.
		 * @note If one position is zero, returns the other position.
		 */
		static constexpr posT getLCAPos(posT pos_1, posT pos_2) {
			if (pos_1 == 0 && pos_2 == 0) return 0;
			if (pos_1 == 0) std::swap(pos_1, pos_2);

			CORE_ASSERT(pos_1 != 0, "At this point, pos_1 cannot be 0");

			usize lca_h = getLCAHeight(pos_1, pos_2);
			usize h_1   = heightFromPos(pos_1);
			CORE_ASSERT(lca_h >= h_1, "Height of lca must be >= than height of first node");

			return pos_1 >> (lca_h - h_1);
		}

		/**
		 * @brief Returns whether a position is in another position's subtree.
		 * @note A zero child position is always considered to be in the subtree.
		 * @note A zero root position returns false unless the child position is also zero.
		 */
		static constexpr bool inSubtree(posT maybe_child, posT root) {
			return (root == getLCAPos(root, maybe_child));
		}

		static constexpr bool isLeafPos(posT pos) { return pos & TOP_BIT; }

		static constexpr posT getChildPos(posT pos, Dir dir) {
			CORE_ASSERT(!isLeafPos(pos), "Leaf doesn't have any children");
			idxT modifier = (dir == Dir::Rght) ? 1 : 0;
			return (pos << 1) | modifier;
		}

		static constexpr bool inSubtree(posT maybe_child, posT root, Dir dir) {
			return inSubtree(maybe_child, getChildPos(root, dir));
		}

		static constexpr Dir dirToChild(posT child, posT ancestor) {
			CORE_ASSERT(inSubtree(child, ancestor), "it must be a child");
			CORE_ASSERT(child != ancestor, "Ancestor is not it's own child");

			return inSubtree(child, ancestor, Dir::Rght) ? Dir::Rght : Dir::Left;
		}

		static constexpr posT elevate(posT pos, usize height_diff) { return pos >> height_diff; }

		static constexpr posT elevateTo(posT pos, usize desired_height) {
			CORE_ASSERT(pos != 0, "We don't want the position to be 0");
			CORE_ASSERT(desired_height < POS_T_SIZE, "we cannot go above the root");

			auto current_height = heightFromPos(pos);
			CORE_ASSERT(current_height <= desired_height, "we should only elevate upwards");
			return elevate(pos, desired_height - current_height);
		}

		static constexpr posT getSiblingPos(posT pos) {
			CORE_ASSERT(pos != 0, "We don't want the position to be 0");
			return pos ^ 1;
		}

		/**
		 * @brief Returns the first node whose subtree fits within a range.
		 */
		static constexpr posT firstNodeForRange(idxT left_idx, idxT right_idx) {
			CORE_ASSERT(left_idx < right_idx, "Received wrong invalid/empty");
			CORE_ASSERT(right_idx <= IDX_END, "Expecting a valid interval");
			CORE_ASSERT((left_idx & TOP_BIT) == 0, "Follows from previous assertions");

			auto left_pos   = left_idx | TOP_BIT;
			auto max_height = (usize) std::bit_width(left_pos & (-left_pos));

			CORE_ASSERT(
				max_height > 0, "There must be at least one 1 in binary representation of left_pos"
			);
			CORE_ASSERT(
				max_height <= POS_T_SIZE, "General limit on the height of node is size of address"
			);
			CORE_ASSERT(
				(left_idx == 0) == (max_height == POS_T_SIZE),
				"max_height == size of address if and only if left_idx == 0"
			);

			{
				usize lsb_idx = max_height - 1;
				posT  mask    = 1;
				mask <<= lsb_idx;
				CORE_ASSERT(mask & left_pos, "max_height digit from right is 1");

				mask -= 1;
				CORE_ASSERT(
					(mask & left_pos) == 0, "all bits with index smaller than (max_height - 1) are 0"
				);
			}

			auto right_guard = right_idx & OFFSET_MASK;
			CORE_ASSERT(
				(right_guard != right_idx) == (right_idx == IDX_END),
				"we modify right_guard only when right_idx == IDX_END"
			);

			auto height_of_diff = (usize) std::bit_width(left_idx ^ right_guard);

			CORE_ASSERT(height_of_diff > 0, "indexes must differ on at least one position");
			CORE_ASSERT(height_of_diff < POS_T_SIZE, "the difference cannot be at the top bit");

			{
				usize diff_idx = height_of_diff - 1;
				posT  diff_bit = 1;
				diff_bit <<= diff_idx;
				CORE_ASSERT(
					(diff_bit & left_idx) != (diff_bit & right_guard),
					"left idx and right_guard must differ at diff_idx"
				);
				CORE_ASSERT(
					((diff_bit & left_idx) != 0) == (right_idx == IDX_END),
					"The only case when left_idx has 1 at diff_idx is when right_idx was end"
				);
				CORE_ASSERT(
					left_idx >> height_of_diff == right_guard >> height_of_diff,
					"After shifting by the height of difference, both left_idx and right guard are "
					"the same"
				);
			}

			usize final_height = std::min(height_of_diff, max_height) - 1;

			return (left_pos >> final_height);
		}

		/**
		 * @brief Returns the tree positions responsible for the range `[left_idx, right_idx)`.
		 */
		static constexpr std::deque<posT> getPosForRange(std::pair<idxT, idxT> arg) {
			auto [left_idx, right_idx] = arg;
			CORE_ASSERT(left_idx < right_idx, "Received wrong invalid/empty");
			CORE_ASSERT(right_idx <= IDX_END, "Expecting a valid interval");

			std::deque<posT> ans = {};

			while (left_idx < right_idx) {
				posT pos = firstNodeForRange(left_idx, right_idx);
				ans.emplace_back(pos);
				auto height = heightFromPos(pos);

				CORE_ASSERT(
					height < POS_T_SIZE, "height for valid nodes is smaller than size of address"
				);

				left_idx += (idxT(1) << height);
			}

			return ans;
		}

		BijectiveMap<ChildEntry, NodeID, ChildEntryH> child_entries{};
		BijectiveMap<LeafEntry, NodeID, LeafEntryH>   leaf_entries{};

		base::HashMap<NodeID, BranchEntry> branch_info{};
		NodeID                             next_node_id = NodeID{ 1 };

	public:
		/**
		 * @brief Iterator for traversing the active leaves of a tree.
		 */
		struct Path {
			std::deque<NodeID>      trace;
			base::CRef<SegmentTree> mem;

			void enforceValidState() const {
				CORE_ASSERT(trace.size(), "there must be a leaf on the path");
				CORE_ASSERT(isLeafPos(mem->getPos(trace.front())), "we go down to leaf");
			}

			/**
			 * @brief Moves the path to the next active leaf in the given direction.
			 * @param move_dir Direction in which to move.
			 * @param skip Number of active leaves to skip.
			 * @return Whether an active leaf was found.
			 * @note The path may initially point to an inactive index.
			 */
			[[nodiscard("When is false, the path must be discarded")]]
			bool moveToValid(Dir move_dir, usize skip = 0) {
				enforceValidState();
				auto prev = trace.front();
				trace.pop_front();
				auto prev_pos = mem->getPos(prev);

				while (trace.size()) {
					auto next = trace.front();
					trace.pop_front();
					auto next_pos = mem->getPos(next);

					defer(std::tie(prev, prev_pos) = std::tie(next, next_pos););

					if (dirToChild(prev_pos, next_pos) == move_dir) continue;

					auto [left_child, right_child] = mem->getChildren(next);

					auto considered = move_dir == Dir::Left ? left_child : right_child;

					if (auto size = mem->getSize(considered); size <= skip) {
						skip -= size;
						continue;
					}

					trace.push_front(next);
					trace.push_front(considered);
					break;
				}

				if (trace.size() == 0) return false;

				while (!mem->isLeaf(trace.front())) {
					auto tip                       = trace.front();
					auto [left_child, right_child] = mem->getChildren(tip);

					auto considered = move_dir == Dir::Rght ? left_child : right_child;

					if (auto size = mem->getSize(considered); size <= skip) {
						skip -= size;
						trace.push_front(move_dir == Dir::Left ? left_child : right_child);
						continue;
					}

					trace.push_front(considered);
				}

				return true;
			}

			/**
			 * @brief Returns the value at the leaf to which the path points.
			 */
			[[nodiscard]]
			valT getValue() const {
				enforceValidState();
				return mem->getValueOfLeaf(trace.front());
			}

			[[nodiscard]]
			idxT getIdx() const {
				enforceValidState();
				return mem->getIndexOfLeaf(trace.front());
			}
		};

		bool isLeaf(NodeID state) const { return leaf_entries.atRightOpt(state).has_value(); }

		bool knows(NodeID state) const { return branch_info.atMaybeCopy(state).has_value(); }

		/**
		 * @brief Returns the number of active leaves in a subtree.
		 */
		usize getSize(NodeID state) const {
			auto entry = *branch_info.atMaybeCopy(state);
			CORE_ASSERT((entry.size == 0) == (state == EMPTY), "empty state iff empty size");
			return entry.size;
		}

		/**
		 * @brief Returns the position of a node in the tree.
		 */
		posT getPos(NodeID state) const { return branch_info.atMaybeCopy(state)->position; }

		/**
		 * @brief Returns the bounds of the active leaves in a subtree.
		 */
		std::pair<idxT, idxT> getRange(NodeID state) const {
			auto entry = *branch_info.atMaybeCopy(state);
			auto l = entry.left_bound, r = entry.right_bound;
			CORE_ASSERT(l <= r, "Branch bounds should have a valid range");
			CORE_ASSERT((state == EMPTY) == (l == r), "empty range iff empty state");
			CORE_ASSERT(r <= IDX_END, "Right bound must be smaller than end of idxs");
			return { l, r };
		}

		/**
		 * @brief Returns the value held by a leaf.
		 */
		valT getValueOfLeaf(NodeID leaf) const { return leaf_entries.atRight(leaf).value; }

		idxT getIndexOfLeaf(NodeID leaf) const { return leaf_entries.atRight(leaf).idx; }

		std::pair<NodeID, NodeID> getChildren(NodeID node) const {
			if_opt_some(child_entries.atRightOpt(node), [left COMMA right]) {
				return { left, right };
			}
			if_opt_some(leaf_entries.atRightOpt(node), _) { return { EMPTY, EMPTY }; }
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Returns a branch node with the given children.
		 * @note Returns an existing node when the same children have already been stored.
		 */
		NodeID emplaceBranch(NodeID left, NodeID right) {
			if (left == EMPTY) return right;
			if (right == EMPTY) return left;

			auto pos_left = getPos(left), pos_right = getPos(right);
			auto lca_pos = getLCAPos(pos_left, pos_right);
			CORE_ASSERT(pos_right != 1 && pos_left != 1, "top node cannot be ever passed");
			CORE_ASSERT(
				inSubtree(pos_left, lca_pos, Dir::Left) && inSubtree(pos_right, lca_pos, Dir::Rght),
				"we should always get the nodes in the right subtrees"
			);

			auto children       = ChildEntry{ .left_child = left, .right_child = right };
			auto [is_new, node] = child_entries.emplaceByLeft(children, next_node_id);

			if (!is_new) return node;

			next_node_id++;
			branch_info.emplace(
				node,
				BranchEntry{
					.size        = getSize(left) + getSize(right),
					.position    = lca_pos,
					.left_bound  = getRange(left).first,
					.right_bound = getRange(right).second,
				}
			);

			return node;
		}

		/**
		 * @brief Returns a leaf node for an index and value.
		 * @note Returns an existing node when the same index and value have already been stored.
		 */
		NodeID emplaceLeaf(idxT idx, valT var_id) {
			CORE_ASSERT(idx < IDX_END, "idx of the leaf must be small enough");
			auto leaf_entry = LeafEntry{
				.idx   = idx,
				.value = var_id,
			};

			auto [is_new, node] = leaf_entries.emplaceByLeft(leaf_entry, next_node_id);
			if (!is_new) return node;

			next_node_id++;
			branch_info.emplace(
				node,
				BranchEntry{
					.size        = 1,
					.position    = idx | TOP_BIT,
					.left_bound  = idx,
					.right_bound = idx + 1,
				}
			);

			return node;
		}

		/**
		 * @brief Callbacks used when merging two trees.
		 * @note The result type determines whether the operation rebuilds or only reads the trees.
		 */
		template<RebuildRes ResT>
		struct MergeBuilder {
			std::function<ResT(NodeID, posT)> only_1 = [](NodeID id, posT) -> ResT {
				if constexpr (std::is_same_v<ResT, NodeID>) return id;
			};

			std::function<ResT(NodeID, posT)> only_2 = [](NodeID id, posT) -> ResT {
				if constexpr (std::is_same_v<ResT, NodeID>) return id;
			};

			std::function<ResT(NodeID, posT)> the_same = [](NodeID id, posT) -> ResT {
				if constexpr (std::is_same_v<ResT, NodeID>) return id;
			};

			std::function<ResT(idxT, valT, valT)> conflicts = [](idxT, valT, valT) -> ResT {
				throw std::invalid_argument("conflicts present");
			};

			template<typename... ArgT>
			base::Optional<NodeID> invoke(auto MergeBuilder::* member, ArgT&&... args) const {
				if constexpr (std::is_same_v<ResT, NodeID>)
					return (this->*member)(std::forward<ArgT>(args)...);
				else {
					(this->*member)(std::forward<ArgT>(args)...);
					return std::nullopt;
				}
			}
		};

		/**
		 * @brief Callbacks used when processing ranges of a tree.
		 * @note The result type determines whether the operation rebuilds or only reads the tree.
		 */
		template<RebuildRes ResT>
		struct RangeBuilder {
			std::function<ResT(NodeID, posT)> in_range = [](NodeID id, posT) -> ResT {
				if constexpr (std::is_same_v<ResT, NodeID>) return id;
			};

			std::function<ResT(NodeID, posT)> out_of_range = [](NodeID id, posT) -> ResT {
				if constexpr (std::is_same_v<ResT, NodeID>) return id;
			};
		};

		using LeafBuilder = std::function<NodeID(usize, base::Optional<usize>)>;

		/**
		 * @brief Merges two tree states using callbacks for differences and conflicts.
		 */
		template<RebuildRes ResT, typename SelfT>
		ResT rebuildFromTwo(
			this SelfT& st, NodeID root_1, NodeID root_2, MergeBuilder<ResT> merge_policy
		) requires ValidSignature<SelfT, ResT> {
			using bldT                        = MergeBuilder<ResT>;
			static constexpr bool RECONSTRUCT = std::is_same_v<ResT, NodeID>;

			auto make_branch = [&](auto left, auto right) -> base::Optional<NodeID> {
				if constexpr (RECONSTRUCT)
					return st.emplaceBranch(*left, *right);
				else
					return std::nullopt;
			};

			auto helper = [&](this auto& self, posT cur_pos, NodeID node_1, NodeID node_2) {
				const auto pos_1 = st.getPos(node_1);
				const auto pos_2 = st.getPos(node_2);

				const auto lca_pos = getLCAPos(pos_1, pos_2);
				CORE_ASSERT(inSubtree(lca_pos, cur_pos), "invariant of the recursive call");

				if (node_1 == node_2) return merge_policy.invoke(&bldT::the_same, node_1, pos_1);

				if (isLeafPos(cur_pos))
					return merge_policy.invoke(
						&bldT::conflicts,
						offsetFromPos(cur_pos),
						st.getValueOfLeaf(node_1),
						st.getValueOfLeaf(node_2)
					);


				base::Optional<NodeID> left = EMPTY, right = EMPTY;

				const auto pos_l = getChildPos(cur_pos, Dir::Left),
						   pos_r = getChildPos(cur_pos, Dir::Rght);

				if (lca_pos != cur_pos) {
					const auto dir_lca = dirToChild(lca_pos, cur_pos);


					left  = dir_lca == Dir::Left
					          ? self(pos_l, node_1, node_2)
					          : merge_policy.invoke(&bldT::the_same, EMPTY, pos_l);
					right = dir_lca == Dir::Rght
					          ? self(pos_r, node_1, node_2)
					          : merge_policy.invoke(&bldT::the_same, EMPTY, pos_r);

					return make_branch(left, right);
				}

				if (pos_1 == cur_pos && pos_2 == cur_pos) {
					const auto [left_1, right_1] = st.getChildren(node_1);
					const auto [left_2, right_2] = st.getChildren(node_2);

					left  = self(pos_l, left_1, left_2);
					right = self(pos_r, right_1, right_2);

					return make_branch(left, right);
				}

				if (pos_1 != lca_pos && pos_2 != lca_pos) {
					const auto dir_1 = dirToChild(pos_1, cur_pos);

					left  = dir_1 == Dir::Left ? merge_policy.invoke(&bldT::only_1, node_1, pos_l)
					                           : merge_policy.invoke(&bldT::only_2, node_2, pos_l);
					right = dir_1 == Dir::Rght ? merge_policy.invoke(&bldT::only_1, node_1, pos_r)
					                           : merge_policy.invoke(&bldT::only_2, node_2, pos_r);

					return make_branch(left, right);
				}

				if (pos_1 == lca_pos) {
					const auto [left_1, right_1] = st.getChildren(node_1);
					const auto dir_2             = dirToChild(pos_2, cur_pos);

					left = dir_2 == Dir::Left ? self(pos_l, left_1, node_2)
					                          : merge_policy.invoke(&bldT::only_1, left_1, pos_l);

					right = dir_2 == Dir::Rght ? self(pos_r, right_1, node_2)
					                           : merge_policy.invoke(&bldT::only_1, right_1, pos_r);

					return make_branch(left, right);
				}

				if (pos_2 == lca_pos) {
					const auto [left_2, right_2] = st.getChildren(node_2);
					const auto dir_1             = dirToChild(pos_1, cur_pos);

					left  = dir_1 == Dir::Left ? self(pos_l, node_1, left_2)
					                           : merge_policy.invoke(&bldT::only_2, left_2, pos_l);
					right = dir_1 == Dir::Rght ? self(pos_r, node_1, right_2)
					                           : merge_policy.invoke(&bldT::only_2, right_2, pos_r);

					return make_branch(left, right);
				}

				CORE_UNREACHABLE();
			};

			const auto pos_1 = st.getPos(root_1);
			const auto pos_2 = st.getPos(root_2);

			if (root_1 == EMPTY && root_2 == EMPTY) return merge_policy.the_same(EMPTY, ROOT_POS);
			if (root_1 == EMPTY) return merge_policy.only_2(root_2, pos_2);
			if (root_2 == EMPTY) return merge_policy.only_1(root_1, pos_1);

			auto res = helper(getLCAPos(pos_1, pos_2), root_1, root_2);

			if constexpr (RECONSTRUCT) return *res;
		}

		/**
		 * @brief Applies callbacks to multiple disjoint half-open ranges.
		 * @note At least one range is required, and each range must satisfy `l < r`.
		 */
		template<RebuildRes ResT, typename SelfT>
		ResT rebuildRanges(
			this SelfT&                              st,
			NodeID                                   root,
			const std::deque<std::pair<idxT, idxT>>& ranges,
			const RangeBuilder<ResT>&                range_constructor
		) requires ValidSignature<SelfT, ResT> {
			static constexpr bool RECONSTRUCT = std::is_same_v<ResT, NodeID>;

			CORE_ASSERT(ranges.size(), "We require at least one range");
			for (auto [l, r]: ranges) CORE_ASSERT(l < r, "Interval must be valid & non-empty");

			for (usize i = 0; i + 1 < ranges.size(); i++) {
				auto [l1, r1] = ranges.at(i);
				auto [l2, r2] = ranges.at(i + 1);

				CORE_ASSERT(r1 <= l2, "The intervals have to be disjoint and in order");
			}

			idxT left_idx  = ranges.front().first;
			idxT right_idx = ranges.back().second;

			if (root) {
				auto [range_l, range_r] = st.getRange(root);
				left_idx                = std::min(left_idx, range_l);
				right_idx               = std::max(right_idx, range_r);
			}

			CORE_ASSERT(left_idx < right_idx, "The full range must be valid & non-empty");
			CORE_ASSERT(right_idx <= IDX_END, "Last range must finish before the end of idxs");

			posT left_pos  = left_idx | LEAF_MASK;
			posT right_pos = (right_idx - 1) | LEAF_MASK;

			std::deque<posT> nodes_to_visit = ranges | std::views::transform(getPosForRange)
			                                | std::views::join | std::ranges::to<std::deque>();


			auto helper = [&](this auto&& self, posT cur_pos, NodeID node) -> ResT {
				if (nodes_to_visit.empty()) return range_constructor.out_of_range(node, cur_pos);

				auto& to_visit = nodes_to_visit.front();

				if (!inSubtree(to_visit, cur_pos))
					return range_constructor.out_of_range(node, cur_pos);

				if (cur_pos == to_visit) {
					nodes_to_visit.pop_front();
					return range_constructor.in_range(node, cur_pos);
				}

				NodeID left_n = EMPTY, right_n = EMPTY;

				if (node != EMPTY) {
					auto node_pos = st.getPos(node);
					if (cur_pos == node_pos) {
						std::tie(left_n, right_n) = st.getChildren(node);
					} else {
						auto  dir       = dirToChild(node_pos, cur_pos);
						auto& to_change = dir == Dir::Left ? left_n : right_n;
						to_change       = node;
					}
				}

				if constexpr (RECONSTRUCT) {
					auto new_left  = self(getChildPos(cur_pos, Dir::Left), left_n);
					auto new_right = self(getChildPos(cur_pos, Dir::Rght), right_n);
					return st.emplaceBranch(new_left, new_right);
				} else {
					self(getChildPos(cur_pos, Dir::Left), left_n);
					self(getChildPos(cur_pos, Dir::Rght), right_n);
				}
			};

			auto lca_pos = getLCAPos(left_pos, right_pos);

			return helper(lca_pos, root);
		}

		/**
		 * @brief Applies callbacks to a single half-open range.
		 */
		template<RebuildRes ResT, typename SelfT>
		ResT rebuildRange(
			this SelfT&               st,
			NodeID                    root,
			idxT                      left_idx,
			idxT                      right_idx,
			const RangeBuilder<ResT>& range_constructor
		) requires ValidSignature<SelfT, ResT> {
			return st.rebuildRanges(root, { { left_idx, right_idx } }, range_constructor);
		}

		/**
		 * @brief Applies callbacks to a sorted collection of indices.
		 * @note This operation modifies the tree; use `Path` to traverse it without modification.
		 */
		NodeID reconstructLeaves(
			NodeID root, const std::deque<idxT>& idxs, const LeafBuilder& constructor
		) {
			std::deque<std::pair<idxT, idxT>> ranges{};
			for (auto idx: idxs) {
				CORE_ASSERT(idx < IDX_END, "it has to be valid idx");
				ranges.emplace_back(idx, idx + 1);
			}
			usize idx = 0;

			auto reconstructor = RangeBuilder<NodeID>{
				.in_range = [&](NodeID id, posT pos) -> NodeID {
					CORE_ASSERT(pos & LEAF_MASK, "expecting a leaf");
					idxT offset = offsetFromPos(pos);
					CORE_ASSERT(idxs.at(idx) == offset, "I iterate exactly over the idxs");
					idx++;

					base::Optional<valT> val = std::nullopt;
					if (id) val = getValueOfLeaf(id);

					return constructor(offset, val);
				},
				.out_of_range = [](NodeID id, posT) { return id; },
			};
			return rebuildRanges(root, ranges, reconstructor);
		}

		/**
		 * @brief Returns the child of a node in the given direction.
		 */
		[[nodiscard]]
		NodeID getChild(Dir dir, NodeID root) const {
			if_opt_some(child_entries.atRightOpt(root), children) {
				auto [left, right] = children;
				return (dir == Dir::Left) ? left : right;
			}

			CORE_ASSERT(
				leaf_entries.atRightOpt(root).has_value(), "if not a root, node, has to be a leaf"
			);
			return EMPTY;
		}

		/**
		 * @brief Returns a path from a root node to an index.
		 */
		[[nodiscard]]
		base::Optional<Path> getPathTo(
			NodeID root, idxT idx, base::Optional<Dir> opt_dir = std::nullopt
		) const {
			std::deque<NodeID> trace = {};

			auto [left_r, right_r] = getRange(root);

			if (idx < left_r || right_r <= idx) return std::nullopt;

			for (NodeID node = root; true;) {
				trace.push_front(node);

				if (isLeaf(node)) {
					CORE_ASSERT(getIndexOfLeaf(node) == idx, "invariant");
					break;
				}

				auto [child_l, child_r] = getChildren(node);

				auto [begin_l, end_l] = getRange(child_l);
				auto [begin_r, end_r] = getRange(child_r);

				if (end_l <= idx && idx < begin_r) {
					if_opt_none(opt_dir) return std::nullopt;

					idx = (*opt_dir == Dir::Left) ? end_l - 1 : begin_r;
				}

				if (begin_l <= idx && idx < end_l) {
					node = child_l;
					continue;
				}
				CORE_ASSERT(begin_r <= idx && idx < end_r, "someone has to");
				node = child_r;
			}

			return Path{
				.trace = trace,
				.mem   = this,
			};
		}

		/**
		 * @brief Constructs an empty segment tree.
		 */
		SegmentTree() {
			branch_info.put(
				EMPTY,
				BranchEntry{
					.size        = 0,
					.position    = 0,
					.left_bound  = 0,
					.right_bound = 0,
				}
			);
			child_entries.emplaceByLeft(
				ChildEntry{ .left_child = EMPTY, .right_child = EMPTY }, EMPTY
			);
		}
	};
}
