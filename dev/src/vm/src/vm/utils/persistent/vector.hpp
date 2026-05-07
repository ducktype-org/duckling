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
	 * @note Implementation based of persistent segment tree.
	 * @note Allows for (==) comparison of two instances with ArrayStateID in O(1)
	 * @note Held values are constructed only once, and nodes hold their id's. This is to allow for
	 * quick construction of leaf elements and to avoid any assumptions about the hash function of
	 * values.
	 *
	 * @tparam VarT type held in the vector
	 * @tparam VarH hash object for VarT
	 */
	template<typename VarT, typename VarH = std::hash<VarT>>
	class Vector final: private Memory {
		detail::BijectiveMap<VarT, usize, VarH> held_values{};
		usize                                   next_val_id = 0;

		constexpr static VectorStateID toVecState(MemoryStateID state) {
			return VectorStateID{ u64(state) };
		}

		constexpr static MemoryStateID toMemState(VectorStateID state) {
			return MemoryStateID{ u64(state) };
		}

		MemoryStateID validateState(VectorStateID vec_state) const {
			auto  state = toMemState(vec_state);
			usize size{};
			try {
				size = Memory::size(state);
			} catch (...) { CORE_PANIC("I need size"); }
			usize l{}, r{};
			try {
				std::tie(l, r) = Memory::getRangeOf(state);
			} catch (...) { CORE_PANIC("I need size"); }

			CORE_ASSERT(r == size, "Size of vector isn't consistent");
			CORE_ASSERT(l == 0, "left bound starts always on 0");

			return state;
		}

		usize emplaceNewVal(const VarT& var) {
			auto [is_new, var_id] = held_values.emplaceByLeft(var, next_val_id);
			next_val_id += (is_new ? 1 : 0);
			return var_id;
		}

	public:
		static constexpr auto EMPTY = VectorStateID{ u64{ Memory::EMPTY } };

		const VarT& access(VectorStateID state_id, usize idx) const {
			auto state = validateState(state_id);
			if_opt_some(Memory::access(state, idx), val_id) { return held_values.atRight(val_id); }
			CORE_UNREACHABLE();
		}

		[[nodiscard]]
		usize size(VectorStateID state_id) const {
			auto state = validateState(state_id);
			return Memory::size(state);
		}

		VectorStateID push(VectorStateID state_id, const VarT& var) {
			auto  state     = validateState(state_id);
			auto  size      = Memory::size(state);
			usize val_id    = emplaceNewVal(var);
			auto  new_state = Memory::set(state, size, val_id);

			return toVecState(new_state);
		}

		VectorStateID change(VectorStateID state_id, usize idx, const VarT& var) {
			auto  state     = validateState(state_id);
			usize val_id    = emplaceNewVal(var);
			auto  new_state = Memory::set(state, idx, val_id);

			return toVecState(new_state);
		}

		/**
		 * @brief get vector from [left, right)
		 */
		std::vector<VarT> view(VectorStateID state_id, usize left, usize right) {
			if (left > right) throw std::invalid_argument("left idx was bigger than right");
			if (right > size(state_id)) throw std::invalid_argument("right bound is too big");

			auto state = validateState(state_id);
			auto iter  = getPathTo(state, left);

			std::vector<VarT> ans        = {};
			bool              iter_valid = iter.pointsToValid();

			for (usize i = 0; i < right - left; i++) {
				CORE_ASSERT(iter.idx == left + i, "I skipped some fields?!");
				CORE_ASSERT(iter.pointsToValid(), "I have a real value underneath");
				CORE_ASSERT(iter_valid, "I somehow invalidated iterator");

				auto val_id = *iter.getValue();
				ans.emplace_back(held_values.atRight(val_id));
				iter_valid &= iter.moveToValid(Dir::Right);
			}

			return ans;
		}

		VectorStateID getPrefix(VectorStateID state_id, usize pref_size) {
			auto state = validateState(state_id);
			auto size  = Memory::size(state);
			if (pref_size > size) throw std::invalid_argument("trying to take too much");

			auto new_state = Memory::slice(state, 0, pref_size);

			return toVecState(new_state);
		}

		VectorStateID pop(VectorStateID state_id, usize how_many_pop = 1) {
			auto state = validateState(state_id);
			auto size  = Memory::size(state);
			if (how_many_pop > size) throw std::invalid_argument("trying to pop too much");

			auto new_state = Memory::slice(state, 0, size - how_many_pop);

			return toVecState(new_state);
		}

		Vector() = default;
	};
}
