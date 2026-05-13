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
	class MemoryIterator;

	/**
	 * @brief Class implementing an abstract access to fully persistent memory - allows to
	 modificate any previous instance efficiently (simmilar to control version for map[idx, value])
	 * @note two memoryStateID's are equal if and only if corresponding memories are the same (same
	 values on same idxs)
	 * @note Implementation is based on persistent segement tree
	 * @note can be thought of as unordered_map<MemoryStateID, MemoryStateView>
	 */
	class Memory: public detail::SegmentTree {
		friend MemoryStateView;
		friend MemoryIterator;

		using Path = detail::SegmentTree::Path;
		using ID   = detail::NodeID;

		// casting memory state to node of segment tree
		constexpr static MemoryStateID toState(ID node) { return MemoryStateID{ u64(node) }; }

		// casting id of node of segment tree to memory state
		constexpr static ID fromState(MemoryStateID state) { return ID{ u64(state) }; }

		// checks if the provided idx is valid ()
		constexpr static void validateIdx(usize idx) {
			if (idx >= detail::SegmentTree::IDX_END) throw std::invalid_argument("got too big idx");
		}

		// basic method for validating input
		ID validateInput(MemoryStateID state) const {
			auto root = fromState(state);
			validateNode(root);

			return root;
		}

		/**
		 * @brief method for validating input
		 * @return checks that idxs are valid, unique and sorted increasingly
		 */
		ID validateInput(MemoryStateID state, const std::deque<usize>& idxs) const {
			auto root = validateInput(state);

			for (auto idx: idxs) validateIdx(idx);
			for (usize i = 1; i < idxs.size(); i++) {
				CORE_ASSERT(idxs[i] >= idxs[i - 1], "idxs must be ordered");
				if (idxs[i] == idxs[i - 1]) throw std::invalid_argument("repeating idx error");
			}

			return root;
		}

		// simple method for validating input
		ID validateInput(MemoryStateID state, usize idx) const {
			auto root = validateInput(state);
			validateIdx(idx);
			return root;
		}

		// simple method for validating input
		std::pair<ID, ID> validateInput(MemoryStateID state_1, MemoryStateID state_2) const {
			auto root_1 = validateInput(state_1);
			auto root_2 = validateInput(state_2);
			return { root_1, root_2 };
		}

		/**
		 * @brief method for validating input
		 * @return checks that given range is well defined and withing bounds
		 */
		ID validateInput(MemoryStateID state, usize l, usize r) const {
			auto root = validateInput(state);
			if (l >= r) throw std::invalid_argument("left bound is bigger or equal to right bound");
			if (r > IDX_END) throw std::invalid_argument("right bound is too big");

			return root;
		}

	public:
		/**
		 * @brief raturns minimal range contaiing all the leaves of active idxs
		 */
		std::pair<usize, usize> getRangeOf(MemoryStateID state) const {
			auto root = validateInput(state);
			return getRange(root);
		}

		/**
		 * @brief returns a const iterator to given idx in given memory instance
		 */
		const Path getPathTo(MemoryStateID state, usize idx) const {
			return detail::SegmentTree::getPathTo(fromState(state), idx);
		}

		constexpr static MemoryStateID EMPTY = MemoryStateID{ u64(detail::SegmentTree::EMPTY) };

		using ConflictPolicy = std::function<base::Optional<usize>(usize, usize, usize)>;

		/**
		 * @brief sets multiple values at certain idxs
		 */
		[[nodiscard]]
		MemoryStateID setMultiple(MemoryStateID root, std::deque<std::pair<usize, usize>> vals);

		/**
		 * @brief erases multiple values at certain idxs
		 */
		[[nodiscard]]
		MemoryStateID eraseMultiple(MemoryStateID root, std::deque<usize> idxs);

		/**
		 * @brief erases single idx
		 */
		[[nodiscard]]
		MemoryStateID erase(MemoryStateID root, usize idx);

		/**
		 * @brief sets valuea at single idxs
		 */
		[[nodiscard]]
		MemoryStateID set(MemoryStateID root, usize idx, usize val);

		using diffResT
			= std::vector<std::tuple<usize, base::Optional<usize>, base::Optional<usize>>>;

		/**
		 * @brief transforms memory to list of pair [idx, value]
		 */
		[[nodiscard]]
		std::vector<std::pair<usize, usize>> toVec(MemoryStateID root) const;

		/**
		 * @brief gets a difference of as the list of tuples [idx, value_1, value_2]
		 */
		[[nodiscard]]
		diffResT getDiff(MemoryStateID root_1, MemoryStateID root_2) const;

		/**
		 * @brief merge two instances of memory, accoring to conflict policy
		 */
		MemoryStateID merge(MemoryStateID root_1, MemoryStateID root_2, ConflictPolicy policy);

		/**
		 * @brief erase all the active idx which are outside of given interval
		 */
		MemoryStateID slice(MemoryStateID root, usize left_idx, usize right_idx);

		/**
		 * @brief erase all the active idx which are inside of given interval
		 */
		MemoryStateID eraseRange(MemoryStateID root, usize left_idx, usize right_idx);

		/**
		 * @brief return nmber of active idx in given instance
		 */
		[[nodiscard]]
		usize size(MemoryStateID root) const;

		/**
		 * @brief checks whether given idx is active in given instance
		 */
		[[nodiscard]]
		bool active(MemoryStateID root, usize idx) const;

		/**
		 * @brief returns held value at given idx in given instance
		 */
		[[nodiscard]]
		base::Optional<usize> access(MemoryStateID root, usize idx) const;

		Memory();
	};

	/**
	 * @brief draft impl of class for wrapping a id for memory state
	 */
	class MemoryStateView {
		MemoryStateID id;
		const Memory& mem;

	public:
		/**
		 * @brief draft impl of iterator for MemoryStateView
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
		auto diff(const MemoryStateView& oth) const;
		[[nodiscard]]
		bool contains(usize idx) const;
	};
}
