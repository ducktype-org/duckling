#pragma once

#include <base/extend_cpp/strongly_typed_int.hpp>

#include <vm/utils/persistent/tree.hpp>
#include <vm/utils/bijective_map.hpp>

#include <deque>
#include <functional>
#include <optional>
#include <tuple>
#include <vector>

namespace vm::persistent {
	STRONG_TYPEDEF_INT(MemoryStateID, u64);
}

STRONGLY_TYPED_INT_STD_HASH(vm::persistent::MemoryStateID)

namespace vm::persistent {
	class MemoryStateView;
	class MemoryIterator;

	/**
	 * @brief Class implementing an abstract access to fully persistent memory - allows to
	 modificate any previous instance efficiently
	 * @note Implementation is based on persistent segement tree
	 * @note can be thought of as unordered_map<MemoryStateID, MemoryStateView>
	 */
	class Memory: protected detail::SegmentTree {
		friend MemoryStateView;
		friend MemoryIterator;

		using Path = detail::SegmentTree::Path;

		constexpr static MemoryStateID toState(detail::NodeID node) {
			return MemoryStateID{ u64(node) };
		}

		constexpr static detail::NodeID fromState(MemoryStateID state) {
			return detail::NodeID{ u64(state) };
		}

	public:
		constexpr static MemoryStateID EMPTY = MemoryStateID{ u64(detail::SegmentTree::EMPTY) };

		using ConflictPolicy = std::function<base::Optional<usize>(usize, usize, usize)>;

		MemoryStateID setMultiple(MemoryStateID root, std::deque<std::pair<usize, usize>> vals);
		MemoryStateID eraseMultiple(MemoryStateID root, std::deque<usize> idxs);
		MemoryStateID erase(MemoryStateID root, usize idx);
		MemoryStateID set(MemoryStateID root, usize idx, usize val);

		using diffResT
			= std::vector<std::tuple<usize, base::Optional<usize>, base::Optional<usize>>>;

		[[nodiscard]]
		std::vector<std::pair<usize, usize>> toVec(MemoryStateID root) const;
		[[nodiscard]]
		diffResT getDiff(MemoryStateID root_1, MemoryStateID root_2) const;

		MemoryStateID merge(MemoryStateID root_1, MemoryStateID root_2, ConflictPolicy policy);

		MemoryStateID slice(MemoryStateID root, usize left_idx, usize right_idx);
		MemoryStateID eraseRange(MemoryStateID root, usize left_idx, usize right_idx);

		[[nodiscard]]
		usize size(MemoryStateID root) const;
		[[nodiscard]]
		bool active(MemoryStateID root, usize idx) const;
		[[nodiscard]]
		base::Optional<usize> access(MemoryStateID root, usize idx) const;

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
