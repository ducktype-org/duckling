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
#include <type_traits>
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

	class SegmentTree {
	public:
		constexpr static auto  EMPTY   = NodeID{ 0 };
		constexpr static usize IDX_END = usize(1) << 63;

		enum class Dir { Left, Right };

	private:
		constexpr static usize OFFSET_MASK = (usize(-1) >> 1);
		constexpr static usize LEAF_MASK   = usize(1) << 63;
		constexpr static usize TOP_BIT     = usize(1) << 63;

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

		static constexpr Dir othDir(Dir dir) { return dir == Dir::Left ? Dir::Right : Dir::Left; }

		static constexpr usize heightFromPos(usize pos) { return usize(64 - std::bit_width(pos)); }

		static constexpr usize offsetFromPos(usize pos) {
			return (pos << heightFromPos(pos)) & OFFSET_MASK;
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

		/**
		 * @brief Get the nodes for range [l, r)
		 */
		static constexpr std::deque<usize> getPosInRange(usize left_idx, usize right_idx) {
			CORE_ASSERT(left_idx < right_idx, "Received wrong interval");
			CORE_ASSERT(right_idx <= IDX_END, "Expecting a valid interval");

			std::deque<usize> ans = {};

			auto right_guard = right_idx & OFFSET_MASK;  // to handle right_idx == IDX_END

			while (left_idx < right_idx) {
				auto left_guard = left_idx | TOP_BIT;

				// max_height == height of lsb or 64 when left_idx == 0
				auto max_height     = (usize) std::bit_width(left_guard & (-left_guard));
				auto height_of_diff = (usize) std::bit_width(left_idx ^ right_guard);

				usize final_height = std::min(height_of_diff, max_height) - 1;

				usize pos = (left_idx >> final_height) | (1 << (63 - final_height));
				ans.emplace_back(pos);
				left_idx += (1 << final_height);
			}

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

			[[nodiscard]]
			std::deque<std::pair<usize, NodeID>> beforeNode(usize upto_here) const;
			[[nodiscard]]
			std::deque<std::pair<usize, NodeID>> afterNode(usize upto_here) const;
		};

		BijectiveMap<ChildEntry, NodeID, ChildEntryH> child_entries{};
		BijectiveMap<LeafEntry, NodeID, LeafEntryH>   leaf_entries{};

		base::HashMap<NodeID, RootEntry> root_info{};
		NodeID                           next_node_id = NodeID{ 1 };

	protected:
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

		std::pair<usize, usize> getHeightOffset(NodeID state) const;
		usize                   getSize(NodeID state) const;
		usize                   getPos(NodeID state) const;
		std::pair<usize, usize> getRange(NodeID state) const;
		usize                   getValue(NodeID state) const;


		void validateRoot(NodeID root) const;

		NodeID nodeFromChildren(NodeID left, NodeID right);
		NodeID nodeFromIdxVar(usize idx, usize var_id);

		template<RebuildRes ResT>
		struct MergeBuilder {
			std::function<ResT(NodeID, usize)> only_1 = [](NodeID id, usize) -> ResT {
				if constexpr (std::is_same_v<ResT, NodeID>) return id;
			};

			std::function<ResT(NodeID, usize)> only_2 = [](NodeID id, usize) -> ResT {
				if constexpr (std::is_same_v<ResT, NodeID>) return id;
			};

			std::function<ResT(NodeID, usize)> the_same = [](NodeID id, usize) -> ResT {
				if constexpr (std::is_same_v<ResT, NodeID>) return id;
			};

			std::function<ResT(usize, usize, usize)> confilicts = [](usize, usize, usize) -> ResT {
				throw std::invalid_argument("conflicts present");
			};
		};

		template<RebuildRes ResT>
		struct RangeBuilder {
			std::function<ResT(NodeID, usize)> in_range = [](NodeID id, usize) -> ResT {
				if constexpr (std::is_same_v<ResT, NodeID>) return id;
			};

			std::function<ResT(NodeID, usize)> out_of_range = [](NodeID id, usize) -> ResT {
				if constexpr (std::is_same_v<ResT, NodeID>) return id;
			};
		};

		using LeafBuilder = std::function<NodeID(usize, base::Optional<usize>)>;

		NodeID reconstructIdxs(NodeID root, std::deque<usize> idxs, LeafBuilder leaf_constructor);

		template<typename ResT, typename SelfT>
		ResT rebuildFromTwo(
			this SelfT&& self, NodeID root_1, NodeID root_2, MergeBuilder<ResT> merge_policy
		) requires ValidSignature<SelfT, ResT>;

		template<typename ResT, typename SelfT>
		ResT rebuildWithRange(
			this SelfT&&       self,
			NodeID             root,
			usize              left_idx,
			usize              right_idx,
			RangeBuilder<ResT> range_constructor
		) requires ValidSignature<SelfT, ResT>;

		[[nodiscard]]
		NodeID getChild(Dir dir, NodeID root) const;

		[[nodiscard]]
		Path getPathTo(NodeID root, usize idx) const;

		void pruneHistory(std::vector<NodeID> desired);

		NodeID mergeTwoRoots(NodeID root_1, NodeID root_2);

	public:
		SegmentTree();
	};
}
