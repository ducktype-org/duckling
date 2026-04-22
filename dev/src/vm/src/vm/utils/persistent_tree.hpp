#pragma once

#include "base/collections/optional.hpp"
#include "base/except/exceptions.hpp"
#include "base/pointers/ref.hpp"
#include <base/collections/maps.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include <vm/utils/bijective_map.hpp>

#include <bit>
#include <deque>
#include <functional>
#include <stdexcept>
#include <vector>

namespace vm::persistent::detail {
	STRONG_TYPEDEF_INT(NodeID, usize);
}

STRONGLY_TYPED_INT_STD_HASH(vm::persistent::detail::NodeID)

namespace vm::persistent::detail {

	class SegmentTree {
		constexpr static auto  EMPTY     = NodeID{ 0 };
		constexpr static usize ROOT_MASK = (usize(-1) >> 1);
		constexpr static usize LEAF_MASK = ~ROOT_MASK;

		struct ChildEntry {
			NodeID left_child;
			NodeID right_child;

			bool operator==(const ChildEntry&) const = default;
		};

		// required for use of BijectiveMap (both sides mus be hashable)
		using ChildEntryH = decltype([](const ChildEntry& h) -> usize {
			return (std::hash<NodeID>{}(h.left_child) << 1) ^ std::hash<NodeID>{}(h.right_child);
		});

		struct LeafEntry {
			usize idx;
			usize value;

			bool operator==(const LeafEntry&) const = default;
		};

		// required for use of BijectiveMap (both sides mus be hashable)
		using LeafEntryH = decltype([](const LeafEntry& h) -> usize {
			return (std::hash<usize>{}(h.idx) << 1) ^ std::hash<usize>{}(h.value);
		});

		struct RootEntry {
			usize size;
			usize position;
			usize left_bound;
			usize right_bound;
		};

		/**
		 * @brief Helper class for determining directions. Used for readability
		 */
		enum class Dir { Left, Right };

		static constexpr Dir othDir(Dir dir) { return dir == Dir::Left ? Dir::Right : Dir::Left; }

		static constexpr usize heightFromPos(usize pos) { return usize(64 - std::bit_width(pos)); }

		static constexpr usize offsetFromPos(usize pos) {
			return (pos << heightFromPos(pos)) & ROOT_MASK;
		}

		static constexpr usize getLCAHeight(usize pos_1, usize pos_2) {
			if (pos_1 > pos_2) std::swap(pos_1, pos_2);
			if (pos_1 == 0) return pos_2;
			auto h_1 = heightFromPos(pos_1), h_2 = heightFromPos(pos_2);
			CORE_ASSERT(h_1 >= h_2, "pos_1 should be higher tahn pos_2");
			pos_2 >>= (h_1 - h_2);
			return h_1 + (usize) std::bit_width(pos_1 ^ pos_2);
		}

		static constexpr usize getLCAPos(usize pos_1, usize pos_2) {
			if (pos_1 == 0) std::swap(pos_1, pos_2);
			return pos_1 >> (getLCAHeight(pos_1, pos_2) - heightFromPos(pos_1));
		}

		static constexpr bool inSubtree(usize maybe_child, usize root) {
			return (maybe_child == getLCAPos(root, maybe_child));
		}

		static constexpr std::deque<usize> getPosInRange(usize left_idx, usize right_idx) {
			CORE_ASSERT(left_idx <= right_idx, "Received wrong interval");
			CORE_ASSERT(
				(left_idx & LEAF_MASK) == 0 && (right_idx & LEAF_MASK) == 0,
				"leaf bit must be off"
			);

			left_idx |= LEAF_MASK;
			right_idx |= LEAF_MASK;

			std::deque<usize> left_ans = {}, right_ans = {}, ans = {};

			while (left_idx < right_idx) {
				if (left_idx % 2 == 1) {
					left_ans.push_back(left_idx);
					left_idx++;
				}
				if (right_idx % 2 == 1) {
					right_idx--;
					right_ans.push_front(right_idx);
				}
				left_idx /= 2;
				right_idx /= 2;
			}

			ans.insert(ans.end(), left_ans.begin(), left_ans.end());
			ans.insert(ans.end(), right_ans.begin(), right_ans.end());

			return ans;
		}

		struct SurroundingNeigh {
			base::Ref<SegmentTree> mem;
			usize                  root_pos;
			usize                  node_pos;
			NodeID                 node;

			std::deque<NodeID> siblings;

			void moveNodeTo(usize desired_pos);
			[[nodiscard]]
			std::pair<usize, std::deque<std::pair<usize, NodeID>>> inOrder(usize upto_here) const;
		};

		/**
		 * @brief struture used to iterate over the MemoryStateView
		 */
		struct Path {
			usize                   idx;
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
			bool moveToValid(Dir move_dir, usize skip = 0);
		};

		using _ConflictPolicy = std::function<NodeID(usize, usize, usize)>;

		detail::BijectiveMap<ChildEntry, NodeID, ChildEntryH> child_entries{};
		detail::BijectiveMap<LeafEntry, NodeID, LeafEntryH>   leaf_entries{};

		base::HashMap<NodeID, RootEntry> root_info{};
		NodeID                           next_node_id = NodeID{ 1 };

	protected:
		std::pair<usize, usize> getHeightOffset(NodeID state) const;
		usize                   getSize(NodeID state) const;
		usize                   getPos(NodeID state) const;
		std::pair<usize, usize> getRange(NodeID state) const;
		usize                   getValue(NodeID state) const;


		void validateRoot(NodeID root) const;

		NodeID nodeFromChildren(NodeID left, NodeID right);
		NodeID nodeFromIdxVar(usize idx, usize var_id);

		struct MergeBuilder {
			std::function<NodeID(NodeID, usize)> only_1 =
				[]([[maybe_unused]] NodeID id, [[maybe_unused]] usize pos) -> NodeID { return id; };

			std::function<NodeID(NodeID, usize)> only_2 =
				[]([[maybe_unused]] NodeID id, [[maybe_unused]] usize pos) -> NodeID { return id; };

			std::function<NodeID(NodeID, usize)> the_same =
				[]([[maybe_unused]] NodeID id, [[maybe_unused]] usize pos) -> NodeID { return id; };

			_ConflictPolicy confilicts = [](usize, usize, usize) -> NodeID {
				throw std::invalid_argument("conflicts present");
			};
		};

		struct RangeBuilder {
			std::function<NodeID(NodeID, usize)> in_range =
				[]([[maybe_unused]] NodeID id, [[maybe_unused]] usize pos) -> NodeID { return id; };

			std::function<NodeID(NodeID, usize)> out_of_range =
				[]([[maybe_unused]] NodeID id, [[maybe_unused]] usize pos) -> NodeID { return id; };
			
		};
		using LeafBuilder  = std::function<NodeID(usize, base::Optional<usize>)>;

		NodeID reconstructIdxs(NodeID root, std::deque<usize> idxs, LeafBuilder leaf_constructor);
		NodeID rebuildFromTwo(NodeID root_1, NodeID root_2, MergeBuilder merge_policy);
		NodeID rebuildWithRange(
			NodeID root, usize left_idx, usize right_idx, RangeBuilder range_constructor
		);

		[[nodiscard]]
		NodeID getChild(Dir dir, NodeID root) const;

		[[nodiscard]]
		Path getPathTo(NodeID root, usize idx) const;

		void pruneHistory(std::vector<NodeID> desired);

		NodeID mergeTwoRoots(NodeID root_1, NodeID root_2);

	public:
		using ConflictPolicy = _ConflictPolicy;

		SegmentTree();
	};
}
