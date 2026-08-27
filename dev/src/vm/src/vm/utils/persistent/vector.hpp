#pragma once

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>

#include <string_id/string_id.hpp>

#include <vm/utils/bijective_map.hpp>
#include <vm/utils/persistent/memory.hpp>

#include <stdexcept>
#include <vector>

namespace vm::persistent {
	STRONG_TYPEDEF_INT(VectorStateID, u64);

	/**
	 * @brief Class implementing a STL vector with time-persistency aka control version. You can
	 * modify any of the previous instances of the vector, by using `VectorStateID`.
	 *
	 * @note Implementation based of persistent memory.
	 * @note two VectorStateID's are equal if and only if corresponding vectors are the same (same
	 size and same values on same idxs)
	 * @note Held values are constructed only once, and nodes hold their id's.
	 *
	 * @tparam VarT type held in the vector
	 * @tparam VarH hash object for VarT
	 */
	template<typename VarT, typename VarH = std::hash<VarT>>
	class Vector final {
		detail::BijectiveMap<VarT, usize, VarH> held_values{};
		usize                                   next_val_id = 0;
		Memory                                  inner;

		// casting memory state to vector state
		constexpr static VectorStateID toVecState(MemoryStateID state) {
			return VectorStateID{ u64(state) };
		}

		// casting vector state to underlying memory state
		constexpr static MemoryStateID toMemState(VectorStateID state) {
			return MemoryStateID{ u64(state) };
		}

		/**
		 * @brief basic method for validating vector state
		 * @return passed state transformed to MemoryStateID
		 */
		MemoryStateID validateState(VectorStateID vec_state) const {
			auto  state = toMemState(vec_state);
			usize size{};
			try {
				size = inner.size(state);
			} catch (...) { CORE_PANIC("I need size"); }
			usize l{}, r{};
			try {
				std::tie(l, r) = inner.getRangeOf(state);
			} catch (...) { CORE_PANIC("I need size"); }

			CORE_ASSERT(r == size, "Size of vector isn't consistent");
			CORE_ASSERT(l == 0, "left bound starts always on 0");

			return state;
		}

		/**
		 * @brief returns an ID of a VarT value
		 * @note if the value wasn't previously used, it is assigned a new one
		 */
		usize emplaceNewVal(const VarT& var) {
			auto [is_new, var_id] = held_values.emplaceByLeft(var, next_val_id);
			next_val_id += (is_new ? 1 : 0);
			return var_id;
		}

	public:
		// public state representing empty vector
		static constexpr auto EMPTY = VectorStateID{ u64{ Memory::EMPTY } };

		/**
		 * @brief method for accessing element at given idx for given instance.
		 */
		const VarT& at(VectorStateID state_id, usize idx) const {
			auto state = validateState(state_id);
			if_opt_some(inner.access(state, idx), val_id) { return held_values.atRight(val_id); }
			throw std::out_of_range("idx out of bounds");
		}

		/**
		 * @brief return size of vector at given instance
		 */
		[[nodiscard]]
		usize size(VectorStateID state_id) const {
			auto state = validateState(state_id);
			return inner.size(state);
		}

		/**
		 * @brief emplaces value at the end vector at given instance
		 */
		[[nodiscard]]
		VectorStateID push(VectorStateID state_id, const VarT& var) {
			auto  state     = validateState(state_id);
			auto  size      = inner.size(state);
			usize val_id    = emplaceNewVal(var);
			auto  new_state = inner.set(state, size, val_id);

			return toVecState(new_state);
		}

		/**
		 * @brief replaces the value at an index of a given instance
		 */
		[[nodiscard]]
		VectorStateID change(VectorStateID state_id, usize idx, const VarT& var) {
			auto state = validateState(state_id);
			if (idx >= size(state_id)) throw std::invalid_argument("idx out of bounds");

			usize val_id    = emplaceNewVal(var);
			auto  new_state = inner.set(state, idx, val_id);

			return toVecState(new_state);
		}

		/**
		 * @brief get a range of vector  [left, right) at given intance
		 * @note interval [left, right) must be contained within interval [0, size)
		 * @note left must be <= right
		 * @note it can happen that left == right - this just returns empty vector
		 */
		[[nodiscard]]
		std::vector<VarT> view(VectorStateID state_id, usize left, usize right) const {
			if (left > right) throw std::invalid_argument("left idx was bigger than right");
			if (right > size(state_id)) throw std::invalid_argument("right bound is too big");

			auto state  = validateState(state_id);
			auto m_iter = inner.getPathTo(state, left, Memory::Dir::Rght);

			if_opt_none(m_iter) return {};
			auto iter = *m_iter;

			if (iter.getIdx() >= right) return {};

			std::vector<VarT> ans = {};

			for (usize i = 0; i < right - left; i++) {
				CORE_ASSERT(iter.getIdx() == left + i, "I skipped some fields?!");

				auto val_id = iter.getValue();
				ans.emplace_back(held_values.atRight(val_id));
				if (i + 1 < right - left)
					CORE_ASSERT(iter.moveToValid(Memory::Dir::Rght), "dense vector");
			}

			return ans;
		}

		/**
		 * @brief returns a state which consists of `pref_size` first elements at given instamce
		 * @note `pref_size` must be smaller or equal to the size of vector at given instance
		 */
		VectorStateID getPrefix(VectorStateID state_id, usize pref_size) {
			auto state = validateState(state_id);
			auto size  = inner.size(state);
			if (pref_size > size) throw std::invalid_argument("trying to take too much");
			if (pref_size == size) return state_id;
			if (pref_size == 0) return EMPTY;

			auto new_state = inner.slice(state, 0, pref_size);

			return toVecState(new_state);
		}

		/**
		 * @brief pops multple values from the vector ar given instance
		 * @note number of values to pop must be smaller or equal to the size of vector at given
		 * instance
		 */
		VectorStateID pop(VectorStateID state_id, usize how_many_pop = 1) {
			auto state = validateState(state_id);
			auto size  = inner.size(state);
			if (how_many_pop > size) throw std::invalid_argument("trying to pop too much");
			if (how_many_pop == size) return EMPTY;

			auto new_state = inner.slice(state, 0, size - how_many_pop);

			return toVecState(new_state);
		}

		/**
		 * @brief comapre two states of the vector
		 * @return true if the instances are equal
		 */
		[[nodiscard]]
		bool eq(VectorStateID state_1, VectorStateID state_2) const {
			return state_1 == state_2;
		}

		Vector() = default;
	};
}
