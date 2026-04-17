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

		std::pair<usize, usize> getHeightOffset(MemoryStateID state) const;
		usize                   getSize(MemoryStateID state) const;

		void validateState(MemoryStateID state) const;
		void validateIdx(MemoryStateID state, usize idx) const;

		MemoryStateID nodeFromChildren(MemoryStateID left, MemoryStateID right);
		MemoryStateID nodeFromIdxVar(usize idx, usize var_id);

		[[nodiscard]]
		MemoryStateID getChild(MemoryStateID state, Dir dir) const;

		[[nodiscard]]
		Path getPathTo(MemoryStateID state, usize idx) const;
		[[nodiscard]]
		Path getLeftMostPath(MemoryStateID state) const;
		[[nodiscard]]
		Path getRightMostPath(MemoryStateID state) const;
		[[nodiscard]]
		MemoryStateID nodeAtHeight(const Path& path, usize height) const;

		bool pathForward(Path& path, usize skip = 0) const;
		bool pathBackward(Path& path, usize skip = 0) const;

		[[nodiscard]]
		MemoryStateID getLeaf(MemoryStateID state, usize idx) const;

		MemoryStateID combineStates(std::deque<std::pair<usize, MemoryStateID>> states);
		MemoryStateID ensureHeightAtLeast(MemoryStateID state, usize height);
		std::pair<MemoryStateID, usize> getLCA(const Path& path_1, const Path& path_2);
		[[nodiscard]]
		std::vector<MemoryStateID> getSubNodesAtHeight(
			MemoryStateID state, usize desired_height
		) const;

	public:
		using diffResT
			= std::vector<std::tuple<usize, base::Optional<usize>, base::Optional<usize>>>;

		MemoryStateID setMultiple(MemoryStateID state, std::deque<std::pair<usize, usize>> vals);
		MemoryStateID eraseMultiple(MemoryStateID state, std::deque<usize> idxs);

		void pruneHistory(std::vector<MemoryStateID> desired);

		[[nodiscard]]
		std::vector<std::pair<usize, usize>> toVec(MemoryStateID state) const;
		[[nodiscard]]
		diffResT getDiff(MemoryStateID stateL, MemoryStateID stateR) const;

		MemoryStateID slice(MemoryStateID state, usize left_bound, usize right_bound);
		/**
		 * @brief deallocate lements from [left, right) interval
		 */
		MemoryStateID eraseRange(MemoryStateID state, usize left, usize right);

		MemoryStateID erase(MemoryStateID state, usize idx);
		MemoryStateID set(MemoryStateID state, usize idx, usize val);

		[[nodiscard]]
		usize size(MemoryStateID state) const;
		[[nodiscard]]
		bool active(MemoryStateID state, usize idx) const;
		[[nodiscard]]
		base::Optional<usize> access(MemoryStateID state, usize idx) const;

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
