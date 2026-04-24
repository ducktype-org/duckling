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
	 modificate any previous instance efficiently
	 * @note Implementation is based on persistent segement tree
	 * @note can be thought of as unordered_map<MemoryStateID, MemoryStateView>
	 */
	class Memory: protected detail::SegmentTree {
		friend MemoryStateView;
		friend MemoryIterator;

		using Path = detail::SegmentTree::Path;
		using ID   = detail::NodeID;

		constexpr static MemoryStateID toState(ID node) { return MemoryStateID{ u64(node) }; }

		constexpr static ID fromState(MemoryStateID state) { return ID{ u64(state) }; }

		constexpr static void validateIdx(usize idx) {
			if (idx >= detail::SegmentTree::IDX_END) throw std::invalid_argument("got too big idx");
		}

		ID validateInput(MemoryStateID state) const {
			auto root = fromState(state);
			validateRoot(root);

			return root;
		}

		ID validateInput(MemoryStateID state, const std::deque<usize>& idxs) const {
			auto root = fromState(state);
			validateRoot(root);

			for (auto idx: idxs) validateIdx(idx);
			for (usize i = 1; i < idxs.size(); i++) {
				CORE_ASSERT(idxs[i] > idxs[i - 1], "idxs must be ordered");
				if (idxs[i] == idxs[i - 1]) throw std::invalid_argument("repeating idx error");
			}

			return root;
		}

		ID validateInput(MemoryStateID state, usize idx) const {
			auto root = fromState(state);
			validateRoot(root);
			validateIdx(idx);
			return root;
		}

		std::pair<ID, ID> validateInput(MemoryStateID state_1, MemoryStateID state_2) const {
			auto root_1 = validateInput(state_1);
			auto root_2 = validateInput(state_2);
			return { root_1, root_2 };
		}

		ID validateInput(MemoryStateID state, usize r, usize l) const {
			auto root = fromState(state);
			validateRoot(root);
			if (l >= r) throw std::invalid_argument("left bound is bigger or equal to right bound");
			if (r > IDX_END) throw std::invalid_argument("right bound is too big");

			return root;
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
