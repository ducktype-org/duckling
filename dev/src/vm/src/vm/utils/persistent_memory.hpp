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

	class Memory {
		constexpr static auto EMPTY = MemoryStateID{ 0 };
		friend MemoryStateView;
		friend MemoryIterator;

		constexpr static usize ROOT_MASK = (usize(-1) >> 1);
		constexpr static usize LEAF_MASK = ~ROOT_MASK;

		struct ChildEntry {
			MemoryStateID left;
			MemoryStateID right;

			bool operator==(const ChildEntry&) const = default;
		};

		using ChildEntryH = decltype([](const ChildEntry& h) -> usize {
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
			usize position;
			usize left_bound;
			usize right_bound;
		};

		enum class Dir { Left, Right };

		static constexpr Dir othDir(Dir dir) { return dir == Dir::Left ? Dir::Right : Dir::Left; }

		struct Path {
			usize                      idx;
			std::vector<MemoryStateID> trace;
			base::CRef<Memory>         mem;

			[[nodiscard]]
			MemoryStateID at(usize height) const;

			bool move(Dir move_dir, usize skip = 0);
		};

		using _ConflictPolicy = std::function<base::Optional<usize>(usize, usize, usize)>;

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

		detail::BijectiveMap<ChildEntry, MemoryStateID, ChildEntryH> child_entries{};
		detail::BijectiveMap<LeafEntry, MemoryStateID, LeafEntryH>   leaf_entries{};

		base::HashMap<MemoryStateID, RootEntry> root_info{};
		MemoryStateID                           next_node_id = MemoryStateID{ 1 };

		std::pair<usize, usize> getHeightOffset(MemoryStateID state) const;
		usize                   getSize(MemoryStateID state) const;
		usize                   getPos(MemoryStateID state) const;
		std::pair<usize, usize> getRange(MemoryStateID state) const;

		void validateRoot(MemoryStateID root) const;
		void validateIdx(MemoryStateID root, usize idx) const;

		MemoryStateID nodeFromChildren(MemoryStateID left, MemoryStateID right);
		MemoryStateID nodeFromIdxVar(usize idx, usize var_id);

		MemoryStateID rebuildFromIdxs(
			MemoryStateID root, std::deque<usize> idxs, std::function<MemoryStateID(usize)>
		);

		[[nodiscard]]
		MemoryStateID getChild(Dir dir, MemoryStateID root) const;
		[[nodiscard]]
		MemoryStateID getLeaf(MemoryStateID root, usize idx) const;

		[[nodiscard]]
		Path getPathTo(MemoryStateID root, usize idx) const;
		[[nodiscard]]
		Path getEndPath(Dir end_dir, MemoryStateID root) const;

		MemoryStateID buildCommonRoot(std::deque<std::pair<usize, MemoryStateID>> states);
		MemoryStateID elevateRoot(MemoryStateID root, usize height);
		std::pair<MemoryStateID, usize> getLCA(const Path& path_1, const Path& path_2);

		MemoryStateID combineStates(
			MemoryStateID root_1, MemoryStateID root_2, ReconstructPolicy policy
		);

		[[nodiscard]] std::deque<MemoryStateID> getSubNodesAtHeight(
			MemoryStateID root, usize desired_height
		) const;

	public:
		using ConflictPolicy = _ConflictPolicy;

		MemoryStateID setMultiple(MemoryStateID root, std::deque<std::pair<usize, usize>> vals);
		MemoryStateID eraseMultiple(MemoryStateID root, std::deque<usize> idxs);

		void pruneHistory(std::vector<MemoryStateID> desired);

		using diffResT
			= std::vector<std::tuple<usize, base::Optional<usize>, base::Optional<usize>>>;

		struct MergePolicy {
			std::function<MemoryStateID(MemoryStateID)> only_left
				= [](MemoryStateID id) -> MemoryStateID { return id; };

			std::function<MemoryStateID(MemoryStateID)> only_right
				= [](MemoryStateID id) -> MemoryStateID { return id; };

			std::function<MemoryStateID(MemoryStateID)> the_same
				= [](MemoryStateID id) -> MemoryStateID { return id; };

			ConflictPolicy confilicts = [](usize, usize, usize) -> base::Optional<usize> {
				throw std::invalid_argument("conflicts present");
			};
		};

		[[nodiscard]]
		std::vector<std::pair<usize, usize>> toVec(MemoryStateID root) const;

		[[nodiscard]]
		diffResT      getDiff(MemoryStateID root_1, MemoryStateID root_2) const;
		MemoryStateID merge(MemoryStateID root_1, MemoryStateID root_2, ConflictPolicy policy);

		MemoryStateID slice(MemoryStateID root, usize left_idx, usize right_idx);
		/**
		 * @brief deallocate lements from [left, right) interval
		 */
		MemoryStateID eraseRange(MemoryStateID root, usize left_idx, usize right_idx);

		MemoryStateID erase(MemoryStateID root, usize idx);
		MemoryStateID set(MemoryStateID root, usize idx, usize val);

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
		base::MRef<Memory>           mem        = nullptr;

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
