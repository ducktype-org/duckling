#pragma once

#include <base/extend_cpp/strongly_typed_int.hpp>

#include <vm/utils/bijective_map.hpp>
#include <vm/utils/persistent/tree.hpp>

#include <deque>
#include <functional>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace vm::persistent {
	STRONG_TYPEDEF_INT(MemoryStateID, u64);
}

STRONGLY_TYPED_INT_STD_HASH(vm::persistent::MemoryStateID)

namespace vm::persistent {
	class MemoryStateView;

	/**
	 * @brief Persistent indexed storage whose previous states remain available after updates.
	 *
	 * Each operation returns a `MemoryStateID` identifying the resulting state. Existing states
	 * remain unchanged and can be used for subsequent operations.
	 *
	 * @note Two state IDs are equal if and only if they represent equal memories.
	 * @note The memory is implemented with a persistent segment tree.
	 */
	class Memory final {
		friend MemoryStateView;

		detail::SegmentTree inner;

		using Path = detail::SegmentTree::Path;
		using ID   = detail::NodeID;

		// casting memory state to node of segment tree
		constexpr static MemoryStateID toState(ID node) { return MemoryStateID{ u64(node) }; }

		// casting id of node of segment tree to memory state
		constexpr static ID fromState(MemoryStateID state) { return ID{ u64(state) }; }

		// Validates an index.
		constexpr static void validateIdx(usize idx) {
			if (idx >= IDX_END) throw std::invalid_argument("got too big idx");
		}

		// Validates a memory state.
		ID validateInput(MemoryStateID state) const {
			const auto root = fromState(state);
			if (!inner.knows(root)) throw std::invalid_argument("unknown memory state");

			return root;
		}

		/**
		 * @brief Validates a state and a sorted collection of indices.
		 * @return The root node corresponding to the state.
		 */
		ID validateInput(MemoryStateID state, const std::deque<usize>& idxs) const {
			const auto root = validateInput(state);

			for (auto idx: idxs) validateIdx(idx);
			for (usize i = 1; i < idxs.size(); i++) {
				CORE_ASSERT(idxs[i] >= idxs[i - 1], "idxs must be ordered");
				if (idxs[i] == idxs[i - 1]) throw std::invalid_argument("repeating idx error");
			}

			return root;
		}

		// Validates a memory state and an index.
		ID validateInput(MemoryStateID state, usize idx) const {
			const auto root = validateInput(state);
			validateIdx(idx);
			return root;
		}

		// Validates two memory states.
		std::pair<ID, ID> validateInput(MemoryStateID state_1, MemoryStateID state_2) const {
			const auto root_1 = validateInput(state_1);
			const auto root_2 = validateInput(state_2);
			return { root_1, root_2 };
		}

		/**
		 * @brief Validates a state and a half-open index range.
		 * @return The root node corresponding to the state.
		 */
		ID validateInput(MemoryStateID state, usize l, usize r) const {
			const auto root = validateInput(state);
			if (l >= r) throw std::invalid_argument("left bound is bigger or equal to right bound");
			if (r > IDX_END) throw std::invalid_argument("right bound is too big");

			return root;
		}

	public:
		using idxT = detail::SegmentTree::idxT;

		/**
		 * @brief Returns the smallest range containing all active indices in a state.
		 */
		std::pair<usize, usize> getRangeOf(MemoryStateID state) const {
			const auto root = validateInput(state);
			return inner.getRange(root);
		}

		using Dir = detail::SegmentTree::Dir;

		/**
		 * @brief Returns a path to an index in a memory state.
		 */
		base::Optional<Path> getPathTo(
			MemoryStateID state, usize idx, base::Optional<Dir> opt_dir = std::nullopt
		) const {
			const auto root = validateInput(state, idx);
			return inner.getPathTo(root, idx, opt_dir);
		}

		constexpr static MemoryStateID EMPTY   = MemoryStateID{ u64(detail::SegmentTree::EMPTY) };
		constexpr static idxT          IDX_END = detail::SegmentTree::IDX_END;

		using ConflictPolicy = std::function<base::Optional<usize>(usize, usize, usize)>;
		inline const static ConflictPolicy DEFAULT_CONFLICT_POLICY
			= ConflictPolicy{ [](usize, usize, usize) -> base::Optional<usize> {
				  throw std::invalid_argument("no conflicts allowed");
			  } };

		/**
		 * @brief Returns a state with values set at multiple indices.
		 */
		[[nodiscard]]
		MemoryStateID setMultiple(MemoryStateID root, std::deque<std::pair<usize, usize>> vals);

		/**
		 * @brief Returns a state with multiple indices removed.
		 */
		[[nodiscard]]
		MemoryStateID eraseMultiple(MemoryStateID root, std::deque<usize> idxs);

		/**
		 * @brief Returns a state with one index removed.
		 */
		[[nodiscard]]
		MemoryStateID erase(MemoryStateID root, usize idx);

		/**
		 * @brief Returns a state with a value set at one index.
		 */
		[[nodiscard]]
		MemoryStateID set(MemoryStateID root, usize idx, usize val);

		using diffResT
			= std::vector<std::tuple<usize, base::Optional<usize>, base::Optional<usize>>>;

		/**
		 * @brief Returns the active indices and their values as pairs.
		 */
		[[nodiscard]]
		std::vector<std::pair<usize, usize>> toVec(MemoryStateID root) const;

		/**
		 * @brief Returns the differences between two memory states.
		 */
		[[nodiscard]]
		diffResT getDiff(MemoryStateID root_1, MemoryStateID root_2) const;

		/**
		 * @brief Returns a state formed by merging two memory states.
		 * @note Conflicts are resolved with the provided policy.
		 */
		MemoryStateID merge(
			MemoryStateID  root_1,
			MemoryStateID  root_2,
			ConflictPolicy policy = DEFAULT_CONFLICT_POLICY
		);

		/**
		 * @brief Returns a state containing only active indices in the given interval.
		 */
		MemoryStateID slice(MemoryStateID root, usize left_idx, usize right_idx);

		/**
		 * @brief Returns a state with active indices in the given interval removed.
		 */
		MemoryStateID eraseRange(MemoryStateID root, usize left_idx, usize right_idx);

		/**
		 * @brief Returns the number of active indices in a memory state.
		 */
		[[nodiscard]]
		usize size(MemoryStateID root) const;

		/**
		 * @brief Returns whether an index is active in a memory state.
		 */
		[[nodiscard]]
		bool active(MemoryStateID root, usize idx) const;

		/**
		 * @brief Returns the value stored at an index in a memory state, if present.
		 */
		[[nodiscard]]
		base::Optional<usize> access(MemoryStateID root, usize idx) const;

		Memory();
	};

	/**
	 * @brief View of a persistent memory state.
	 */
	class MemoryStateView final {
		MemoryStateID id;
		const Memory& mem;

	public:
		/**
		 * @brief Iterator over the active entries in a memory state.
		 */
		class MemoryIterator {
			base::Optional<Memory::Path> maybe_path = std::nullopt;

		public:
			MemoryIterator& operator++();
			MemoryIterator  operator++(int);
			MemoryIterator& operator--();
			MemoryIterator  operator--(int);

			MemoryIterator() = default;
			MemoryIterator(const Memory& mem, MemoryStateID state, usize idx);
		};

		MemoryStateView(const Memory& mem, MemoryStateID id);

		usize operator[](usize idx) const;

		[[nodiscard]]
		base::Optional<usize> atMaybe(usize idx) const;
		[[nodiscard]]
		usize size() const;
		[[nodiscard]]
		std::vector<std::pair<usize, usize>> toVec() const;
		[[nodiscard]]
		Memory::diffResT diff(const MemoryStateView& oth) const;
		[[nodiscard]]
		bool contains(usize idx) const;
	};
}
