#pragma once

#include "base/collections/optional.hpp"
#include "base/except/exceptions.hpp"
#include "base/pointers/ref.hpp"
#include <base/collections/maps.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include <vm/utils/bijective_map.hpp>

#include <deque>
#include <functional>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace vm::persistent::detail {
	STRONG_TYPEDEF_INT(MemoryStateID, usize);
}

STRONGLY_TYPED_INT_STD_HASH(vm::persistent::detail::MemoryStateID)

namespace vm::persistent::detail {
	class MemoryStateView;
	class MemoryIterator;

	/**
	 * @brief Class implementing an abstract access to fully persistent memory - allows to
	 modificate any previous instance efficiently
	 * @note Implementation is based on persistent segement tree
	 * @note can be thought of as unordered_map<MemoryStateID, MemoryStateView>
	 */
	class Memory {
		constexpr static auto EMPTY = MemoryStateID{ 0 };
		friend MemoryStateView;
		friend MemoryIterator;

		constexpr static usize ROOT_MASK = (usize(-1) >> 1);
		constexpr static usize LEAF_MASK = ~ROOT_MASK;

		struct ChildEntry {
			MemoryStateID left_child;
			MemoryStateID right_child;

			bool operator==(const ChildEntry&) const = default;
		};

		// required for use of BijectiveMap (both sides mus be hashable)
		using ChildEntryH = decltype([](const ChildEntry& h) -> usize {
			return (std::hash<MemoryStateID>{}(h.left_child) << 1)
			     ^ std::hash<MemoryStateID>{}(h.right_child);
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

		/**
		 * @brief struture used to iterate over the MemoryStateView
		 * @note might be used in internal workings of the class Memory
		 * @warning DO NOT TOUCH!
		 */
		struct Path {
			usize                     idx;
			std::deque<MemoryStateID> trace;
			base::CRef<Memory>        mem;
			MemoryStateID             root;

			/**
			 * @brief moves Path to the leaf, which is present in Memory
			 * @param move_dir the direction in which we seek the leaf
			 * @param skip number of leaves to skip
			 * @return boolean, if the was successful (whethter leaf was found)
			 * @note works even if Path was pointing to idx which wasn't active in `root` state of
			 * Memory
			 */
			bool moveToValid(Dir move_dir, usize skip = 0);

			/**
			 * @brief moves iterator to the right by `step`. Doesn't care if the position is valid
			 * or not
			 */
			void moveBy(usize step);
		};

		using _ConflictPolicy = std::function<base::Optional<usize>(usize, usize, usize)>;

		detail::BijectiveMap<ChildEntry, MemoryStateID, ChildEntryH> child_entries{};
		detail::BijectiveMap<LeafEntry, MemoryStateID, LeafEntryH>   leaf_entries{};

		base::HashMap<MemoryStateID, RootEntry> root_info{};
		MemoryStateID                           next_node_id = MemoryStateID{ 1 };

		/**
		 * @brief Getters, allow to get information about the root of subtree representing `state`,
		 * such as it's height, position in tree overall, offset of subtree in leaf layer, number of
		 * active leaves and min, max idx of acative leaves
		 */
		std::pair<usize, usize> getHeightOffset(MemoryStateID state) const;
		usize                   getSize(MemoryStateID state) const;
		usize                   getPos(MemoryStateID state) const;
		std::pair<usize, usize> getRange(MemoryStateID state) const;

		/**
		 * @brief meethods for getting descendants of given root, either a direct child  or
		 * the leaf at given idx or all the descendants at particular height
		 * @note when root doesn't have a particular descendant, `getChild` and `getLeaf` methods
		 * will return EMPTY
		 */
		[[nodiscard]]
		MemoryStateID getChild(Dir dir, MemoryStateID root) const;
		[[nodiscard]]
		MemoryStateID getLeaf(MemoryStateID root, usize idx) const;
		[[nodiscard]]
		std::deque<MemoryStateID> getSubNodesAtHeight(MemoryStateID root, usize desired_height) const;


		// method for determining height of LCA for two leaves at idx_1 and idx_2. Used for readability
		static usize getLCAHeight(usize idx_1, usize idx_2);

		// checks if root is a valid state of memory
		void validateRoot(MemoryStateID root) const;

		/**
		 * @brief basic constuctors for the nodes in the tree. Node can be constructed as a leaf,
		 * via `nodeFromIdxVar` or as a node with children via `nodeFromChildren`
		 */
		MemoryStateID nodeFromChildren(MemoryStateID left, MemoryStateID right);
		MemoryStateID nodeFromIdxVar(usize idx, usize var_id);

		/**
		 * @brief Helper struct for keeping reconstruction when merging two memory states
		 */
		struct ReconstructPolicy {
			std::function<MemoryStateID(MemoryStateID)> only_left
				= [](MemoryStateID id) -> MemoryStateID { return id; };

			std::function<MemoryStateID(MemoryStateID)> only_right
				= [](MemoryStateID id) -> MemoryStateID { return id; };

			std::function<MemoryStateID(MemoryStateID)> the_same
				= [](MemoryStateID id) -> MemoryStateID { return id; };

			_ConflictPolicy confilicts = [](usize, usize, usize) -> base::Optional<usize> {
				throw std::invalid_argument("conflicts present");
			};
		};

		/**
		 * @brief abstract methods which implement atomic reconstruction of  memory, either by
		 * modifying indexes, merging two trees or changing given range
		 * @note in the long run all of methods which construct new states of memory will be
		 * wrappers for those functions
		 */
		MemoryStateID rebuildFromIdxs(
			MemoryStateID                       root,
			std::deque<usize>                   idxs,
			std::function<MemoryStateID(usize)> leaf_constructor
		);
		MemoryStateID rebuildFromTwo(
			MemoryStateID root_1, MemoryStateID root_2, ReconstructPolicy reconstruction_policy
		);
		MemoryStateID rebuildWithRange(
			MemoryStateID                               root,
			usize                                       left_idx,
			usize                                       right_idx,
			std::function<MemoryStateID(MemoryStateID)> rebuilder
		);

		/**
		 * @brief methods for getting paths for given memory state, either to a particular idx or to
		 * the first active leaf at given directio
		 */
		[[nodiscard]]
		Path getPathTo(MemoryStateID root, usize idx) const;
		[[nodiscard]]
		Path getEndPath(Dir end_dir, MemoryStateID root) const;

		/**
		 * @brief internal helper functions for modifying state of the class
		 */
		MemoryStateID buildCommonRoot(std::deque<std::pair<usize, MemoryStateID>> states);
		MemoryStateID elevateRoot(MemoryStateID root, usize height);
		std::pair<MemoryStateID, usize> getLCA(const Path& path_1, const Path& path_2);

	public:
		using ConflictPolicy = _ConflictPolicy;
		/**
		 * @brief method to prune the history of structure, and keeping only desired states and
		 * their children
		 * @note this method is for memory efficiency purposes - we don't want to remamber states in
		 * which we are uninterested
		 * @warning this method will invalidate states, causing UB when trying to get
		 */
		void pruneHistory(std::vector<MemoryStateID> desired);

		/**
		 * @brief methods for atomic modification of memory. Either to erase or set the values at
		 * given indexes
		 */
		MemoryStateID setMultiple(MemoryStateID root, std::deque<std::pair<usize, usize>> vals);
		MemoryStateID eraseMultiple(MemoryStateID root, std::deque<usize> idxs);
		MemoryStateID erase(MemoryStateID root, usize idx);
		MemoryStateID set(MemoryStateID root, usize idx, usize val);

		using diffResT
			= std::vector<std::tuple<usize, base::Optional<usize>, base::Optional<usize>>>;

		/**
		 * @brief methods to construct representation of memort states - either a vector of (idx,
		 * val) or difference between two states
		 * @note this method will be made more universal with develepent of `rebuild` functions
		 */
		[[nodiscard]]
		std::vector<std::pair<usize, usize>> toVec(MemoryStateID root) const;
		[[nodiscard]]
		diffResT getDiff(MemoryStateID root_1, MemoryStateID root_2) const;

		MemoryStateID merge(MemoryStateID root_1, MemoryStateID root_2, ConflictPolicy policy);

		/**
		 * @brief methods to operate on [left, right) interval. Either removes the leaves at given range or filters them out
		 */
		MemoryStateID slice(MemoryStateID root, usize left_idx, usize right_idx);
		MemoryStateID eraseRange(MemoryStateID root, usize left_idx, usize right_idx);

		[[nodiscard]]
		usize size(MemoryStateID root) const;
		[[nodiscard]]
		bool active(MemoryStateID root, usize idx) const;
		[[nodiscard]]
		base::Optional<usize> access(MemoryStateID root, usize idx) const;

		[[nodiscard]]
		MemoryStateID getEmpty() const;

		Memory();
	};

	class MemoryIterator {
		base::Optional<Memory::Path> maybe_path = std::nullopt;

	public:
		MemoryIterator& operator++();
		MemoryIterator  operator++(int);
		MemoryIterator& operator--();
		MemoryIterator  operator--(int);

		MemoryIterator();
		MemoryIterator(const Memory& mem, MemoryStateID state, usize idx);
	};

	class MemoryStateView {
		MemoryStateID id;
		const Memory& mem;

	public:
		MemoryStateView(const Memory& mem, MemoryStateID id);

		usize operator[](usize idx) const;

		[[nodiscard]]
		base::Optional<usize> atMaybe(usize idx) const;
		[[nodiscard]]
		usize size() const;
		[[nodiscard]]
		std::vector<std::pair<usize, usize>> toVec() const;
		[[nodiscard]]
		auto diff(const MemoryStateView& oth) const;
		[[nodiscard]]
		bool contains(usize idx) const;
	};
}
