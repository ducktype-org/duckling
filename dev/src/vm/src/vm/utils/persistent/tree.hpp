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
#include <optional>
#include <ranges>
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
		struct BranchEntry {
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

		static constexpr posT firstNodeForRange(idxT left_idx, idxT right_idx) {
			CORE_ASSERT(left_idx <= right_idx, "Received wrong interval");
			CORE_ASSERT(right_idx <= IDX_END, "Expecting a valid interval");

			auto right_guard = right_idx & OFFSET_MASK;  // to handle right_idx == IDX_END
			auto left_pos    = left_idx | TOP_BIT;

			// max_height == height of lsb or TOP_BIT when left_idx == 0
			auto max_height     = (usize) std::bit_width(left_pos & (-left_pos));
			auto height_of_diff = (usize) std::bit_width(left_idx ^ right_guard);

			usize final_height = std::min(height_of_diff, max_height) - 1;

			return (left_pos >> final_height);
		}

		/**
		 * @brief Get the tree psitions of nodes responsible for range [lefft_idx, right_idx)
		 */
		static constexpr std::deque<posT> getPosForRange(idxT left_idx, idxT right_idx) {
			CORE_ASSERT(left_idx <= right_idx, "Received wrong interval");
			CORE_ASSERT(right_idx <= IDX_END, "Expecting a valid interval");

			std::deque<posT> ans = {};

			while (left_idx < right_idx) {
				posT pos = firstNodeForRange(left_idx, right_idx);
				ans.emplace_back(pos);
				auto height = heightFromPos(pos);
				left_idx += (idxT(1) << height);
			}

			return ans;
		}

		/**
		 * @brief Helper struct for moving around th tree, with built-in support for tree-rebuilding
		 * @note this is to avoid non-trivial recursion and make a more generic code
		 * @note reconstruction will only happen if the segTreeT is not const-qualified
		 * @tparam segTreeT underlying inner type of the ptr to memory. Passed explicitly to
		 * determine qualifiers
		 */
		template<typename segTreeT>
		requires SameWNoQual<SegmentTree, segTreeT> struct ReconstructCtx {
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
						node = mem->lazyMergeTwoNodes(left, right);
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
					auto before = getSiblingsOnSide(Dir::Left, upto_here);
					for (auto el: before) ans.emplace_back(el);
				}

				usize idx = ans.size();
				ans.emplace_back(node_pos, node);

				{
					auto after = getSiblingsOnSide(Dir::Right, upto_here);
					for (auto el: after) ans.emplace_back(el);
				}

				return { idx, ans };
			}

			/**
			 * @brief Gives all the nodes which are in subtree of given position and preceed/follow
			 * (depending on dir parameter) currently tracked node in pre-order traverse of tree
			 * @note Think about returned list as a list of nodes which had to be processed
			 * before/after node if we were doing recursive operations from given node to currently
			 * tracked
			 * @note nodes are given in the order from left to right (just as if the call was
			 * recursive)
			 * @note related to inOrder
			 */
			[[nodiscard]]
			std::deque<std::pair<posT, NodeID>> getSiblingsOnSide(Dir dir, posT upto_here) const {
				auto cur_pos = node_pos;

				std::deque<std::pair<posT, NodeID>> ans = {};
				CORE_ASSERT(
					inSubtree(upto_here, root_pos), "target root must be my in root subtree"
				);

				for (auto sibling: siblings) {
					if (cur_pos == upto_here) break;

					CORE_ASSERT(
						inSubtree(cur_pos, upto_here),
						"current position must remain inside the subtree of target root"
					);

					if (bool(cur_pos & 1) == (dir == Dir::Left))
						ans.emplace_front(cur_pos ^ 1, sibling);
					cur_pos >>= 1;
				}

				if (dir != Dir::Right) std::ranges::reverse(ans);

				return ans;
			}
		};

		BijectiveMap<ChildEntry, NodeID, ChildEntryH> child_entries{};
		BijectiveMap<LeafEntry, NodeID, LeafEntryH>   leaf_entries{};

		base::HashMap<NodeID, BranchEntry> root_info{};
		NodeID                             next_node_id = NodeID{ 1 };

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
		void validateNode(NodeID root) const {
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
		NodeID constructBranch(NodeID left, NodeID right) {
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
					BranchEntry{
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
		NodeID constructLeaf(idxT idx, valT var_id) {
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

					return mem->lazyMergeTwoNodes(rec_left, rec_right);
				} else {
					self(pos_left, left_1, left_2);
					self(pos_right, right_1, right_2);
					return;
				}
			};

			auto lca = getLCAPos(pos_1, pos_2);

			ReconstructCtx<BaseT> neigh = {
				.mem      = &obj,
				.root_pos = lca,
				.node_pos = lca,
				.node     = EMPTY,
				.siblings = {},
			};

			auto handle_list = [&](const auto& list,
			                       usize       special_idx,
			                       posT        special_pos,
			                       NodeID      special_val,
			                       bool        special_skip) {
				for (usize i = 0; i < list.size(); i++) {
					auto [pos, node] = list.at(i);
					bool special     = (i == special_idx);

					if (special) {
						CORE_ASSERT(pos == special_pos, "This is our node");
						if (special_skip) continue;
					}

					NodeID corresponding = special ? special_val : EMPTY;

					if constexpr (!RECONSTRUCT)
						detail_merge(pos, node, corresponding);
					else {
						usize relative_h = heightFromPos(pos) - heightFromPos(special_pos);
						CORE_ASSERT(
							relative_h < neigh.siblings.size(),
							"Height diff must refer to some sibling"
						);
						auto& dst = special ? neigh.node : neigh.siblings.at(relative_h);
						dst       = detail_merge(pos, node, corresponding);
					}
				}
			};

			neigh.moveNodeTo(pos_1);
			neigh.node                    = root_1;
			auto [node_1_idx, in_order_1] = neigh.inOrder(lca);
			in_order_1.pop_back();

			handle_list(in_order_1, node_1_idx, pos_1, EMPTY, inSubtree(pos_2, pos_1));

			neigh.moveNodeTo(pos_2);
			auto [node_2_idx, in_order_2] = neigh.inOrder(lca);

			if (!inSubtree(pos_2, pos_1)) {
				CORE_ASSERT(in_order_2.size() >= 2, "There must be sth going on");
				CORE_ASSERT(node_2_idx > 0, "second node must have at least one sibling on left");
				in_order_2.pop_front();
				node_2_idx--;
			}

			handle_list(in_order_2, node_2_idx, pos_2, root_2, false);

			if constexpr (RECONSTRUCT) {
				neigh.moveNodeTo(lca);
				return neigh.node;
			}
		}

		/**
		 * @brief helper function for modifying a multiple ranges in memory
		 * @note can be mutable or unmutable, depending of return type of range builder
		 */
		template<typename ResT, typename SelfT>
		ResT rebuildRanges(
			this SelfT&                              st,
			NodeID                                   root,
			const std::deque<std::pair<idxT, idxT>>& ranges,
			const RangeBuilder<ResT>&                range_constructor
		) requires ValidSignature<SelfT, ResT> {
			using BaseT = std::conditional_t<
				std::is_const_v<std::remove_reference_t<SelfT>>,
				const SegmentTree,
				SegmentTree>;

			auto&& obj = static_cast<BaseT&>(st);

			static constexpr bool RECONSTRUCT = std::is_same_v<ResT, NodeID>;

			CORE_ASSERT(ranges.size(), "we need at least one range");
			for (auto [l, r]: ranges) CORE_ASSERT(l < r, "Interval must be valid & non-empty");

			for (usize i = 0; i + 1 < ranges.size(); i++) {
				auto [l1, r1] = ranges.at(i);
				auto [l2, r2] = ranges.at(i + 1);

				CORE_ASSERT(r1 <= l2, "the intervals have to be disjoint");
			}

			idxT left_idx  = ranges.front().first;
			idxT right_idx = ranges.back().second;

			CORE_ASSERT(right_idx <= IDX_END, "last range must finish before the memory end");

			if (root) {
				auto [height, offset] = obj.getHeightOffset(root);
				CORE_ASSERT(height < POS_T_SIZE, "This is always the case for normal nodes");

				left_idx  = std::min(left_idx, offset);
				right_idx = std::max(right_idx, offset + (usize(1) << height));
			}

			posT left_pos  = left_idx | LEAF_MASK;
			posT right_pos = (right_idx - 1) | LEAF_MASK;

			auto lca_pos = getLCAPos(left_pos, right_pos);

			ReconstructCtx<BaseT> neigh = {
				.mem      = &obj,
				.root_pos = lca_pos,
				.node_pos = lca_pos,
				.node     = EMPTY,
				.siblings = {},
			};

			neigh.moveNodeTo(obj.getPos(root));
			neigh.node = root;

			auto handle_range = [&](idxT l, idxT r) {
				if (l == r) return;
				auto range_nodes = getPosForRange(l, r);
				CORE_ASSERT(
					neigh.node_pos == range_nodes.front(),
					"Currently tracked position must be at the starting position for current range"
				);

				for (; range_nodes.size(); range_nodes.pop_front()) {
					auto pos = range_nodes.front();
					neigh.moveNodeTo(pos);

					if constexpr (RECONSTRUCT)
						neigh.node = range_constructor.in_range(neigh.node, pos);
					else
						range_constructor.in_range(neigh.node, pos);
				}
			};

			auto handle_list = [&](const std::deque<std::pair<posT, NodeID>>& list) {
				for (auto [pos, node]: list)
					if constexpr (RECONSTRUCT) {
						usize relative_h = heightFromPos(pos) - heightFromPos(neigh.node_pos);
						CORE_ASSERT(
							relative_h < neigh.siblings.size(),
							"Height diff must refer particular sibling"
						);
						neigh.siblings.at(relative_h) = range_constructor.out_of_range(node, pos);
					} else
						range_constructor.out_of_range(node, pos);
			};

			{
				auto [l1, r1]  = ranges.front();
				auto begin_pos = firstNodeForRange(l1, r1);
				neigh.moveNodeTo(begin_pos);

				handle_list(neigh.getSiblingsOnSide(Dir::Left, lca_pos));
				handle_range(l1, r1);
			}

			using namespace std::views;
			for (auto [l, r]: ranges | drop(1)) {
				auto begin_pos = firstNodeForRange(l, r);
				auto lca       = getLCAPos(neigh.node_pos, begin_pos);

				CORE_ASSERT(
					inSubtree(neigh.node_pos, lca) && inSubtree(begin_pos, lca),
					"this must always be the case (one is in left subtree other in right)"
				);
				CORE_ASSERT(
					!inSubtree(neigh.node_pos, begin_pos) && !inSubtree(begin_pos, neigh.node_pos),
					"one position cannot be a predecessor of other"
				);

				auto list_1 = neigh.getSiblingsOnSide(Dir::Right, lca);
				list_1.pop_back();
				handle_list(list_1);

				neigh.moveNodeTo(begin_pos);

				auto list_2 = neigh.getSiblingsOnSide(Dir::Left, lca);
				list_2.pop_front();
				handle_list(list_2);

				handle_range(l, r);
			}


			handle_list(neigh.getSiblingsOnSide(Dir::Right, lca_pos));
			neigh.moveNodeTo(lca_pos);

			if constexpr (RECONSTRUCT) return neigh.node;
		}

		/**
		 * @brief helper function for modifying a single range of memory
		 * @note can be mutable or unmutable, depending of return type of range constructor
		 */
		template<typename ResT, typename SelfT>
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
		 * @brief helper function for atomically modifying the certain idxs
		 * @note idxs must be sorted from left to right
		 * @note this function is never const (use Path for iterating over unmutable memory)
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
		 * @brief Lazy merge of two roots
		 * @note when one of the roor is empty, other is returned
		 * @note requires that the root cannot contain each other
		 */
		NodeID lazyMergeTwoNodes(NodeID root_1, NodeID root_2) {
			auto reduce = [&](NodeID node) -> NodeID {
				if (!node) return node;
				while (true) {
					if_opt_some(child_entries.atRightOpt(node), children) {
						if (children.right_child && children.left_child) return node;

						node = children.right_child != EMPTY ? children.right_child
						                                     : children.left_child;
						continue;
					}

					CORE_ASSERT(leaf_entries.atRightOpt(node).has_value(), "we should be in leaf");
					return node;
				}
			};

			if (!root_1) return reduce(root_2);
			if (!root_2) return reduce(root_1);

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
					root_1 = constructBranch(EMPTY, root_1);
				else
					root_1 = constructBranch(root_1, EMPTY);

			for (; (pos_2 >> 1) != lca; pos_2 >>= 1)
				if (pos_2 & 1)
					root_2 = constructBranch(EMPTY, root_2);
				else
					root_2 = constructBranch(root_2, EMPTY);

			CORE_ASSERT(pos_2 == pos_1 + 1, "after all those operations they should be siblings");

			return constructBranch(root_1, root_2);
		}

	public:
		/**
		 * @brief Construct a new Segment Tree object
		 */
		SegmentTree() {
			root_info.put(
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
