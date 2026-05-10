#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/pointers/ref.hpp>

#include <vm/utils/bijective_map.hpp>

#include <algorithm>
#include <bit>
#include <deque>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <vector>

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
		= RebuildRes<ResT> && (std::is_same_v<void, ResT> || !std::is_const_v<SelfT>);

	template<typename T1, typename T2>
	concept SameWNoQual = std::is_same_v<std::remove_cvref_t<T1>, std::remove_cvref_t<T2>>;

	class SegmentTree {
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
		constexpr static auto EMPTY   = NodeID{ 0 };
		constexpr static idxT IDX_END = idxT(1) << (POS_T_SIZE - 1);

		enum class Dir { Left, Right };

	private:
		constexpr static posT OFFSET_MASK = (posT(-1) >> 1);
		constexpr static posT LEAF_MASK   = posT(1) << (POS_T_SIZE - 1);
		constexpr static posT TOP_BIT     = posT(1) << (POS_T_SIZE - 1);

		struct ChildEntry {
			NodeID left_child;
			NodeID right_child;

			bool operator==(const ChildEntry&) const = default;
		};

		// required for use of BijectiveMap (both sides must be hashable)
		using ChildEntryH = decltype([](const ChildEntry& h) -> usize {
			return (std::hash<NodeID>{}(h.left_child) << 1) ^ std::hash<NodeID>{}(h.right_child);
		});

		struct LeafEntry {
			idxT idx;
			valT value;

			bool operator==(const LeafEntry&) const = default;
		};

		// required for use of BijectiveMap (both sides must be hashable)
		using LeafEntryH = decltype([](const LeafEntry& h) -> usize {
			return (std::hash<usize>{}(h.idx) << 1) ^ std::hash<usize>{}(h.value);
		});

		/**
		 * @brief helds no of actiVe nodes, position in the tree and idx of leftmost and rightmost
		 * leaf
		 */
		struct RootEntry {
			usize size;
			posT  position;
			idxT  left_bound;
			idxT  right_bound;
		};

		/**
		 * @brief helper function for getting other direction
		 */
		static constexpr Dir othDir(Dir dir) { return dir == Dir::Left ? Dir::Right : Dir::Left; }

		/**
		 * @brief helper function for getting height of tree position
		 * @note leaf nodes get height 0
		 * @note empty position has position higher than everyone else
		 */
		static constexpr usize heightFromPos(posT pos) {
			return POS_T_SIZE - usize(std::bit_width(pos));
		}

		/**
		 * @brief helper function for getting smallest possible idx of leaf in subtree of given tree
		 * position
		 * @note empty position has offset 0
		 */
		static constexpr idxT offsetFromPos(posT pos) {
			auto h = heightFromPos(pos);
			if (h >= POS_T_SIZE) return 0;
			return (pos << h) & OFFSET_MASK;
		}

		/**
		 * @brief helper function for getting height of lca for two tree positions
		 */
		static constexpr usize getLCAHeight(posT pos_1, posT pos_2) {
			if (pos_1 > pos_2) std::swap(pos_1, pos_2);
			auto h_2 = heightFromPos(pos_2);
			if (pos_1 == 0) return h_2;
			auto h_1 = heightFromPos(pos_1);
			CORE_ASSERT(h_1 >= h_2, "pos_1 should be higher tahn pos_2");
			pos_2 >>= (h_1 - h_2);
			return h_1 + (usize) std::bit_width(pos_1 ^ pos_2);
		}

		/**
		 * @brief helper function for getting position of lca for two tree positions
		 */
		static constexpr posT getLCAPos(posT pos_1, posT pos_2) {
			if (pos_1 == 0) std::swap(pos_1, pos_2);
			usize lca_h = getLCAHeight(pos_1, pos_2);
			usize h_1   = heightFromPos(pos_1);
			return pos_1 >> (lca_h - h_1);
		}

		/**
		 * @brief helper function for determining if one position in subtree of another
		 */
		static constexpr bool inSubtree(posT maybe_child, posT root) {
			return (root == getLCAPos(root, maybe_child));
		}

		/**
		 * @brief Get the tree psitions of nodes responsible for range [lefft_idx, right_idx)
		 */
		static constexpr std::deque<posT> getPosInRange(idxT left_idx, idxT right_idx) {
			CORE_ASSERT(left_idx <= right_idx, "Received wrong interval");
			CORE_ASSERT(right_idx <= IDX_END, "Expecting a valid interval");

			std::deque<posT> ans = {};

			auto right_guard = right_idx & OFFSET_MASK;  // to handle right_idx == IDX_END

			while (left_idx < right_idx) {
				auto left_pos = left_idx | TOP_BIT;

				// max_height == height of lsb or TOP_BIT when left_idx == 0
				auto max_height     = (usize) std::bit_width(left_pos & (-left_pos));
				auto height_of_diff = (usize) std::bit_width(left_idx ^ right_guard);

				usize final_height = std::min(height_of_diff, max_height) - 1;

				posT pos = (left_pos >> final_height);
				ans.emplace_back(pos);
				left_idx += (idxT(1) << final_height);
			}

			return ans;
		}

		/**
		 * @brief Helper struct for moving around th tree, with built-in support for tree-rebuilding
		 * @note this is to avoid non-trivial recursion and make a more generic code
		 * @note reconstruction will only happen if the segTreeT is not const-qualified
		 * @tparam segTreeT underlying inner type of the ptr to memory. Passed explicitly to determine qualifiers
		 */
		template<typename segTreeT>
		requires SameWNoQual<SegmentTree, segTreeT> struct SurroundingNeigh {
			segTreeT* mem;
			posT      root_pos{};
			posT      node_pos{};
			NodeID    node{};

			std::deque<NodeID> siblings{};
			std::deque<NodeID> ancestors{};

			/**
			 * @brief moves the tracked node to desired position
			 * @note when moving upwards, children are reconstructed by lazily merging
			 */
			void moveNodeTo(posT desired_pos) {
				static constexpr bool RECONSTRUCT = !std::is_const_v<segTreeT>;
				if (!desired_pos || desired_pos == node_pos) return;
				CORE_ASSERT(inSubtree(desired_pos, root_pos), "we should be within root's subtree");

				usize lca_h       = getLCAHeight(desired_pos, node_pos);
				usize h           = heightFromPos(node_pos);
				usize height_diff = lca_h - h;
				for (usize i = (desired_pos == root_pos ? 0 : 1); i < height_diff; i++) {
					if constexpr (RECONSTRUCT) {
						auto left = node, right = siblings.front();
						if (node_pos & 1) std::swap(left, right);
						node = mem->lazyMergeTwoRoots(left, right);
					} else {
						node = ancestors.front();
						ancestors.pop_front();
					}
					siblings.pop_front();
					node_pos >>= 1;
				}

				if (!inSubtree(desired_pos, node_pos)) {
					CORE_ASSERT(siblings.size(), "We need to have at least one sibling");
					std::swap(node, siblings.front());
					node_pos ^= 1;
				}

				CORE_ASSERT(inSubtree(desired_pos, node_pos), "we are above desired pos");

				while (node_pos != desired_pos) {
					posT left_pos  = node_pos << 1;
					posT right_pos = (node_pos << 1 | 1);

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

					if constexpr (!RECONSTRUCT) ancestors.emplace_front(node);

					if (inSubtree(desired_pos, left_pos)) {
						node_pos = left_pos;
						node     = left;
						siblings.emplace_front(right);
					} else {
						node_pos = right_pos;
						node     = right;
						siblings.emplace_front(left);
					}
				}
			}

			/**
			 * @brief returns a list of nodes and positions of the nodes which would habe been touch
			 * if there have been executed a recursive call from given node to tracked position and
			 * the idx of the currently tracked node
			 * @note nodes are given in the order from left to right (just as if the call was
			 * recursive)
			 */
			[[nodiscard]]
			std::pair<usize, std::deque<std::pair<posT, NodeID>>> inOrder(posT upto_here) const {
				std::deque<std::pair<posT, NodeID>> ans = {};
				{
					auto before = beforeNode(upto_here);
					for (auto el: before) ans.emplace_back(el);
				}

				usize idx = ans.size();
				ans.emplace_back(node_pos, node);

				{
					auto after = afterNode(upto_here);
					for (auto el: after) ans.emplace_back(el);
				}

				return { idx, ans };
			}

			/**
			 * @brief Gives all the nodes which are in subtree of given position and preceed
			 * currently tracked node in pre-order traverse of tree
			 * @note Think about returned list as a list of nodes which had to be processed before
			 * node if we were doing recursive operations from given node to currently tracked
			 * @note nodes are given in the order from left to right (just as if the call was
			 * recursive)
			 * @note related to inOrder
			 */
			[[nodiscard]]
			std::deque<std::pair<posT, NodeID>> beforeNode(posT upto_here) const {
				auto cur_pos = node_pos;

				std::deque<std::pair<posT, NodeID>> ans = {};
				CORE_ASSERT(
					inSubtree(upto_here, root_pos), "target root must be my in root subtree"
				);

				for (auto it = siblings.begin(); cur_pos != upto_here; it++, cur_pos >>= 1) {
					CORE_ASSERT(
						inSubtree(cur_pos, upto_here), "I need to have a path to the target root"
					);
					CORE_ASSERT(it != siblings.end(), "there is a sibling on this level");

					if (cur_pos & 1) ans.emplace_front(cur_pos ^ 1, *it);
				}

				return ans;
			}

			/**
			 * @brief Gives all the nodes which are in subtree of given position and follow
			 * currently tracked node in pre-order traverse of tree
			 * @note Think about returned list as a list of nodes which had to be processed adter
			 * node if we were doing recursive operations from given node to currently tracked
			 * @note nodes are given in the order from left to right (just as if the call was
			 * recursive)
			 * @note related to inOrder
			 */
			[[nodiscard]]
			std::deque<std::pair<posT, NodeID>> afterNode(usize upto_here) const {
				auto cur_pos = node_pos;

				std::deque<std::pair<posT, NodeID>> ans = {};
				CORE_ASSERT(
					inSubtree(upto_here, root_pos), "target root must be my in root subtree"
				);

				for (auto it = siblings.begin(); cur_pos != upto_here; it++, cur_pos >>= 1) {
					CORE_ASSERT(
						inSubtree(cur_pos, upto_here), "I need to have a path to the target root"
					);
					CORE_ASSERT(it != siblings.end(), "there is a sibling on this level");

					if ((cur_pos & 1) == 0) ans.emplace_back(cur_pos ^ 1, *it);
				}

				return ans;
			}
		};

		BijectiveMap<ChildEntry, NodeID, ChildEntryH> child_entries{};
		BijectiveMap<LeafEntry, NodeID, LeafEntryH>   leaf_entries{};

		base::HashMap<NodeID, RootEntry> root_info{};
		NodeID                           next_node_id = NodeID{ 1 };

	protected:
		/**
		 * @brief struture used to iterate over the unmutable memory
		 */
		struct Path {
			idxT                    idx;
			std::deque<NodeID>      trace;
			base::CRef<SegmentTree> mem;
			NodeID                  root;

			/**
			 * @brief moves Path to the leaf, which is present in Memory
			 * @param move_dir the direction in which we seek the leaf
			 * @param skip number of leaves to skip
			 * @return boolean, if the was successful (whethter leaf was found)
			 * @note works even if Path was pointing to idx which wasn't active in `root` state of
			 * Memory
			 */
			bool moveToValid(Dir move_dir, usize skip = 0) {
				CORE_ASSERT(trace.size(), "there must be a leaf on the path");

				posT mask = 1;
				trace.pop_front();

				for (; trace.size(); trace.pop_front(), mask <<= 1) {
					NodeID node_id = trace.front();

					usize orig_idx = idx;
					if (idx & mask) idx ^= mask;
					if (bool(orig_idx & mask) == (move_dir == Dir::Right)) continue;

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

			/**
			 * @brief checks whether path points to an active idx
			 */
			[[nodiscard]]
			bool pointsToValid() const {
				return trace.at(0) != EMPTY;
			}

			/**
			 * @brief returns a value of at the idx to which path points or empty optional if idx is
			 * not active
			 */
			[[nodiscard]]
			base::Optional<valT> getValue() const {
				if (trace.at(0) == EMPTY)
					return std::nullopt;
				else
					return mem->getValueOfLeaf(trace.at(0));
			}
		};

		/**
		 * @brief Get the height and the smallest idx of the leaf which could potentially be in the
		 * subtree
		 */
		std::pair<usize, idxT> getHeightOffset(NodeID state) const {
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

		/**
		 * @brief Return number of active leaves in subtree
		 */
		usize getSize(NodeID state) const {
			if_opt_some(root_info.atMaybeCopy(state), entry) { return entry.size; }
			if_opt_some(leaf_entries.atRightOpt(state), _) { return 1UL; }
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Return position in the tree of given node
		 */
		posT getPos(NodeID state) const {
			if_opt_some(root_info.atMaybeCopy(state), entry) { return entry.position; }
			if_opt_some(leaf_entries.atRightOpt(state), entry) { return LEAF_MASK | entry.idx; }
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Return idx of leftmost and rightmost idx of active leavs in fiven subtree
		 */
		std::pair<idxT, idxT> getRange(NodeID state) const {
			if_opt_some(root_info.atMaybeCopy(state), entry) {
				return { entry.left_bound, entry.right_bound };
			}
			if_opt_some(leaf_entries.atRightOpt(state), entry) {
				return { entry.idx, entry.idx + 1 };
			}
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Return the value held by leaf
		 */
		valT getValueOfLeaf(NodeID leaf) const {
			if_opt_some(leaf_entries.atRightOpt(leaf), entry) { return entry.value; }
			CORE_UNREACHABLE();
		}

		/**
		 * @brief Checks if the id is a valid root of some tree
		 */
		void validateRoot(NodeID root) const {
			if (!root_info.contains(root) && !leaf_entries.atRightOpt(root))
				throw std::invalid_argument("got invalid state");

			if (root == EMPTY) return;
			auto size             = (idxT) getSize(root);
			auto [height, offset] = getHeightOffset(root);
			CORE_ASSERT(height < POS_T_SIZE, "all non-empty roots need to have valid height");
			CORE_ASSERT(size <= (idxT(1) << height), "root's size is too large");
		}

		/**
		 * @brief emplaces a new node from given children.
		 * @note node can be either constructed, or returned previous (depeding if there already was
		 * node with given subtrees)
		 */
		NodeID nodeFromChildren(NodeID left, NodeID right) {
			auto pos_left = getPos(left), pos_right = getPos(right);
			CORE_ASSERT(pos_right != 1 && pos_left != 1, "top node cannot be ever passed");
			CORE_ASSERT(
				(pos_left >> 1) == (pos_right >> 1) || left == EMPTY || right == EMPTY,
				"Children are of different hights"
			);

			auto children       = ChildEntry{ .left_child = left, .right_child = right };
			auto [is_new, node] = child_entries.emplaceByLeft(children, next_node_id);

			idxT left_bound = 0, right_bound = 0;
			if (!left) {
				auto [l, r] = getRange(right);
				left_bound  = l;
				right_bound = r;
			} else if (!right) {
				auto [l, r] = getRange(left);
				left_bound  = l;
				right_bound = r;
			} else {
				left_bound  = getRange(left).first;
				right_bound = getRange(right).second;
			}

			if (is_new) {
				root_info.emplace(
					node,
					RootEntry{
						.size        = getSize(left) + getSize(right),
						.position    = (pos_left | pos_right) >> 1,
						.left_bound  = left_bound,
						.right_bound = right_bound,
					}
				);
				next_node_id++;
			}

			return node;
		}

		/**
		 * @brief emplaces a new leaf from idx and held valie.
		 * @note node can be either constructed, or returned previous (depeding if there already was
		 * node with given idx and value)
		 */
		NodeID nodeFromIdxVar(idxT idx, valT var_id) {
			auto leaf_entry = LeafEntry{
				.idx   = idx,
				.value = var_id,
			};

			auto [is_new, node] = leaf_entries.emplaceByLeft(leaf_entry, next_node_id);
			if (is_new) next_node_id++;

			return node;
		}

		/**
		 * @brief helper structure dedicated for merging two trees
		 * @note used either for rebuilding or const operations (depending on result type)
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

			std::function<ResT(idxT, valT, valT)> confilicts = [](idxT, valT, valT) -> ResT {
				throw std::invalid_argument("conflicts present");
			};
		};

		/**
		 * @brief helper structure dedicated for handling ranges
		 * @note used either for rebuilding or const operations (depending on result type)
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
		 * @brief helper function for atomically modifying the certain idxs
		 * @note idxs must be sorted from left to right
		 * @note this function is never const (use Path for iterating over unmutable memory)
		 */
		NodeID reconstructIdxs(NodeID root, std::deque<idxT> idxs, const LeafBuilder& constructor) {
			if (idxs.empty()) return root;

			for (auto& idx: idxs)
				CORE_ASSERT((LEAF_MASK & idx) == 0, "we must get valid leaf values");
			std::ranges::sort(idxs);
			for (usize i = 1; i < idxs.size(); i++)
				CORE_ASSERT(idxs[i - 1] < idxs[i], "no duplicates");

			auto [height, offset] = getHeightOffset(root);

			auto left_bound = idxs.front(), right_bound = idxs.back();

			if (root) {
				left_bound  = std::min(left_bound, offset);
				right_bound = std::max(right_bound, offset + (usize(1) << height) - 1);
			}

			const auto old_root_pos = getPos(root);
			const auto new_root_pos = getLCAPos(LEAF_MASK | left_bound, LEAF_MASK | right_bound);

			SurroundingNeigh<SegmentTree> neigh = {
				.mem      = this,
				.root_pos = new_root_pos,
				.node_pos = new_root_pos,
				.node     = EMPTY,
				.siblings = {},
			};

			std::deque<idxT> left = {}, right = {};
			for (; idxs.size(); idxs.pop_front())
				if (idxs.front() < offset)
					left.emplace_back(idxs.front());
				else
					right.emplace_back(idxs.front());

			for (auto idx: left) {
				neigh.moveNodeTo(idx | LEAF_MASK);
				neigh.node = constructor(idx, std::nullopt);
			}
			neigh.moveNodeTo(old_root_pos);
			neigh.node = root;

			for (auto idx: right) {
				neigh.moveNodeTo(idx | LEAF_MASK);
				base::Optional<usize> val = std::nullopt;
				if (neigh.node) val = getValueOfLeaf(neigh.node);
				neigh.node = constructor(idx, val);
			}

			neigh.moveNodeTo(new_root_pos);
			CORE_ASSERT(neigh.siblings.empty(), "all my siblings should have been handled");

			return neigh.node;
		}

		/**
		 * @brief helper function for merging two instances of the memory
		 * @note can be mutable or unmutable, depending of return type of merge poliscy (hence use
		 * of this deduction)
		 */
		template<typename ResT, typename SelfT>
		ResT rebuildFromTwo(
			this SelfT& st, NodeID root_1, NodeID root_2, MergeBuilder<ResT> merge_policy
		) requires ValidSignature<SelfT, ResT> {
			using BaseT = std::conditional_t<
				std::is_const_v<std::remove_reference_t<SelfT>>,
				const SegmentTree,
				SegmentTree>;

			auto& obj = static_cast<BaseT&>(st);

			static constexpr bool RECONSTRUCT = std::is_same_v<ResT, NodeID>;

			CORE_ASSERT(root_1 && root_2, "Both of the states must be non-empty");

			auto pos_1 = obj.getPos(root_1);
			auto pos_2 = obj.getPos(root_2);

			if (offsetFromPos(pos_2) < offsetFromPos(pos_1)) {
				std::swap(pos_1, pos_2);
				std::swap(root_1, root_2);
				std::swap(merge_policy.only_1, merge_policy.only_2);
				merge_policy.confilicts = [orig_strat = merge_policy.confilicts](
											  idxT idx, valT val_1, valT val_2
										  ) -> ResT { return orig_strat(idx, val_2, val_1); };
			}

			auto detail_merge =
				[&, mem = &obj](this auto&& self, posT pos, NodeID node_1, NodeID node_2) -> ResT {
				CORE_ASSERT(
					mem->getPos(node_1) == mem->getPos(node_2) || !node_1 || !node_2,
					"both nodes are responsible for the same memory region"
				);

				if (node_1 == node_2) return merge_policy.the_same(node_1, pos);
				if (!node_1) return merge_policy.only_2(node_2, pos);
				if (!node_2) return merge_policy.only_1(node_1, pos);

				if_opt_some(mem->leaf_entries.atRightOpt(node_1), leaf_entry1) {
					CORE_ASSERT(
						pos & LEAF_MASK, "leaves have a designated bit on in their position"
					);
					auto maybe_entry2 = mem->leaf_entries.atRightOpt(node_2);
					CORE_ASSERT(maybe_entry2.has_value(), "both must be leaves");
					auto leaf_entry2    = *maybe_entry2;
					auto [idx_1, val_1] = leaf_entry1;
					auto [idx_2, val_2] = leaf_entry2;
					CORE_ASSERT(idx_1 == idx_2, "leaves must be of the same index");
					CORE_ASSERT(
						val_1 != val_2,
						"if values were the same, we would handle this in prev edgecase"
					);

					return merge_policy.confilicts(idx_1, val_1, val_2);
				}

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
				auto pos_left = pos << 1, pos_right = ((pos << 1) | 1);

				if constexpr (RECONSTRUCT) {
					auto rec_left  = self(pos_left, left_1, left_2);
					auto rec_right = self(pos_right, right_1, right_2);

					CORE_ASSERT(
						!rec_left || inSubtree(mem->getPos(rec_left), pos_left), "stay in subtree"
					);
					CORE_ASSERT(
						!rec_right || inSubtree(mem->getPos(rec_right), pos_right), "stay in subtree"
					);

					return mem->lazyMergeTwoRoots(rec_left, rec_right);
				} else {
					self(pos_left, left_1, left_2);
					self(pos_right, right_1, right_2);
					return;
				}
			};

			auto lca = getLCAPos(pos_1, pos_2);

			SurroundingNeigh<BaseT> neigh = {
				.mem      = &obj,
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
				usize height_1 = heightFromPos(pos_1);

				for (usize i = 0; i < node_1_idx; i++) {
					auto [pos, node_1] = in_order.front();
					in_order.pop_front();

					CORE_ASSERT(node_1 == EMPTY, "all before the leftmost must be empty");

					if constexpr (RECONSTRUCT)
						neigh.siblings.at(heightFromPos(pos) - height_1)
							= detail_merge(pos, EMPTY, EMPTY);
					else
						detail_merge(pos, EMPTY, EMPTY);
				}

				if (!inSubtree(pos_2, pos_1)) {
					CORE_ASSERT(in_order.size(), "There must be node on the list");
					auto [pos, node_1] = in_order.front();
					in_order.pop_front();

					if constexpr (RECONSTRUCT)
						neigh.node = detail_merge(pos, node_1, EMPTY);
					else
						detail_merge(pos, node_1, EMPTY);
				}

				for (; in_order.size(); in_order.pop_front()) {
					auto [pos, node_1] = in_order.front();
					CORE_ASSERT(node_1 == EMPTY, "all between two roots must be empty");


					if constexpr (RECONSTRUCT)
						neigh.siblings.at(heightFromPos(pos) - height_1)
							= detail_merge(pos, EMPTY, EMPTY);
					else
						detail_merge(pos, EMPTY, EMPTY);
				}
			}

			{
				neigh.moveNodeTo(pos_2);
				neigh.node                  = root_2;
				auto [node_2_idx, in_order] = neigh.inOrder(lca);
				usize height_2              = heightFromPos(pos_2);

				if (!inSubtree(pos_2, pos_1)) {
					CORE_ASSERT(in_order.size() >= 2, "There must be sth going on");
					CORE_ASSERT(
						node_2_idx > 0, "second node must have at least one sibling on left"
					);
					in_order.pop_front();
					node_2_idx--;
				}

				for (usize i = 0; i < node_2_idx; i++) {
					auto [pos, node_1] = in_order.front();
					in_order.pop_front();

					if constexpr (RECONSTRUCT)
						neigh.siblings.at(heightFromPos(pos) - height_2)
							= detail_merge(pos, node_1, EMPTY);
					else
						detail_merge(pos, node_1, EMPTY);
				}

				{
					auto [pos, node_1] = in_order.front();
					in_order.pop_front();
					CORE_ASSERT(pos == pos_2, "This is our node");

					if constexpr (RECONSTRUCT)
						neigh.node = detail_merge(pos, node_1, root_2);
					else
						detail_merge(pos, node_1, root_2);
				}

				for (; in_order.size(); in_order.pop_front()) {
					auto [pos, node_1] = in_order.front();

					if constexpr (RECONSTRUCT)
						neigh.siblings.at(heightFromPos(pos) - height_2)
							= detail_merge(pos, node_1, EMPTY);
					else
						detail_merge(pos, node_1, EMPTY);
				}
			}

			if constexpr (RECONSTRUCT) {
				neigh.moveNodeTo(lca);
				return neigh.node;
			}
		}

		/**
		 * @brief helper function for modifying a single range of memory
		 * @note can be mutable or unmutable, depending of return type of merge poliscy (hence use
		 * of this deduction)
		 */
		template<typename ResT, typename SelfT>
		ResT rebuildWithRange(
			this SelfT&       st,
			NodeID             root,
			idxT               left_idx,
			idxT               right_idx,
			RangeBuilder<ResT> range_constructor
		) requires ValidSignature<SelfT, ResT> {
			using BaseT = std::conditional_t<
				std::is_const_v<std::remove_reference_t<SelfT>>,
				const SegmentTree,
				SegmentTree>;

			auto&& obj = static_cast<BaseT&>(st);

			static constexpr bool RECONSTRUCT = std::is_same_v<ResT, NodeID>;

			CORE_ASSERT(left_idx < right_idx, "Interval must be non-empty");
			CORE_ASSERT(right_idx <= IDX_END, "idxs must be small enough");
			auto range_nodes = getPosInRange(left_idx, right_idx);
			CORE_ASSERT(range_nodes.size(), "when range non-empty, there must be some nodes");

			if (root) {
				auto [height, offset] = obj.getHeightOffset(root);

				left_idx  = std::min(left_idx, offset);
				right_idx = std::max(right_idx, offset + (usize(1) << height));
			}

			posT left_pos  = left_idx | LEAF_MASK;
			posT right_pos = (right_idx - 1) | LEAF_MASK;

			auto lca_pos = getLCAPos(left_pos, right_pos);

			SurroundingNeigh<BaseT> neigh = {
				.mem      = &obj,
				.root_pos = lca_pos,
				.node_pos = lca_pos,
				.node     = EMPTY,
				.siblings = {},
			};

			neigh.moveNodeTo(obj.getPos(root));
			neigh.node = root;

			usize first_height = heightFromPos(range_nodes.front());
			usize last_height  = heightFromPos(range_nodes.back());

			neigh.moveNodeTo(range_nodes.front());
			{
				auto list = neigh.beforeNode(lca_pos);
				for (auto [pos, node]: list) {
					CORE_ASSERT(heightFromPos(pos) >= first_height, "All the siblings are above");

					if constexpr (RECONSTRUCT)
						neigh.siblings.at(heightFromPos(pos) - first_height)
							= range_constructor.out_of_range(node, pos);
					else
						range_constructor.out_of_range(node, pos);
				}
			}

			for (; range_nodes.size(); range_nodes.pop_front()) {
				auto pos = range_nodes.front();
				neigh.moveNodeTo(pos);

				if constexpr (RECONSTRUCT)
					neigh.node = range_constructor.in_range(neigh.node, pos);
				else
					range_constructor.in_range(neigh.node, pos);
			}

			{
				auto list = neigh.afterNode(lca_pos);

				for (auto [pos, node]: list) {
					CORE_ASSERT(heightFromPos(pos) >= last_height, "All the siblings are above");

					if constexpr (RECONSTRUCT) {
						usize height_diff = heightFromPos(pos) - last_height;
						CORE_ASSERT(
							height_diff < neigh.siblings.size(),
							"Height diff must refer particular sibling"
						);
						neigh.siblings.at(height_diff) = range_constructor.out_of_range(node, pos);
					} else
						range_constructor.out_of_range(node, pos);
				}
			}

			neigh.moveNodeTo(lca_pos);

			if constexpr (RECONSTRUCT) return neigh.node;
		}

		/**
		 * @brief Gets a child of given root in particular direction
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
		 * @brief returns a path from particular root to the idx
		 */
		[[nodiscard]]
		Path getPathTo(NodeID root, idxT idx) const {
			auto [height, offset] = getHeightOffset(root);

			std::deque<NodeID> trace = { root };

			if (idx < offset || idx >= offset + (idxT(1) << height))
				trace = { EMPTY };
			else {
				NodeID node = root;
				idxT   mask = (idxT(1) << height);

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

		/**
		 * @brief draft impl of potentially removing excessive nodes from segemnt tree, apart from
		 * desired
		 * @warning NOT TESTED
		 */
		void pruneHistory(std::vector<NodeID> desired) {
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

		/**
		 * @brief Lazy merge of two roots
		 * @note when one of the roor is empty, other is returned
		 * @note requires that the root cannot contain each other
		 */
		NodeID lazyMergeTwoRoots(NodeID root_1, NodeID root_2) {
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
			CORE_ASSERT(
				offsetFromPos(pos_1) + (idxT(1) << heightFromPos(pos_1)) <= offsetFromPos(pos_2),
				"pos_1 has to be on the left to pos_2"
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

			CORE_ASSERT(pos_2 == pos_1 + 1, "after all those operations they should be siblings");

			return nodeFromChildren(root_1, root_2);
		}

	public:
		/**
		 * @brief Construct a new Segment Tree object
		 */
		SegmentTree() {
			root_info.put(
				EMPTY,
				RootEntry{
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
