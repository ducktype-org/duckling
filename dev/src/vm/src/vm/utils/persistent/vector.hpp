#pragma once

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/strongly_typed_int.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>

#include <vm/utils/bijective_map.hpp>
#include <vm/utils/persistent/memory.hpp>

#include <functional>
#include <stdexcept>
#include <vector>

namespace vm::persistent {
	STRONG_TYPEDEF_INT(VectorStateID, u64);

	/**
	 * @brief A persistent vector whose previous states remain available after updates.
	 *
	 * Each operation returns a `VectorStateID` identifying the resulting state. Existing states
	 * remain unchanged and can be used for subsequent operations.
	 *
	 * @note Two state IDs are equal if and only if they represent equal vectors.
	 * @note Values are stored once and referenced by ID in the persistent memory.
	 *
	 * @tparam VarT value type
	 * @tparam VarH hash function for values
	 */
	template<typename VarT, typename VarH>
	class VectorStateView;

	template<typename VarT, typename VarH = std::hash<VarT>>
	class Vector final {
		friend class VectorStateView<VarT, VarH>;

		detail::BijectiveMap<VarT, usize, VarH> held_values{};
		usize                                   next_val_id = 0;
		Memory                                  inner;

		// Converts a memory state ID to a vector state ID.
		constexpr static VectorStateID toVecState(MemoryStateID state) {
			return VectorStateID{ u64(state) };
		}

		// Converts a vector state ID to a memory state ID.
		constexpr static MemoryStateID toMemState(VectorStateID state) {
			return MemoryStateID{ u64(state) };
		}

		/**
		 * @brief Validates a vector state.
		 * @return The corresponding memory state.
		 */
		MemoryStateID validateState(VectorStateID vec_state) const {
			const auto mem_state = toMemState(vec_state);
			if (!inner.knows(mem_state)) throw std::invalid_argument("unknown vector state");

			const usize size  = inner.size(mem_state);
			const auto [l, r] = inner.getRangeOf(mem_state);

			CORE_ASSERT(r == size, "Size of vector isn't consistent");
			CORE_ASSERT(l == 0, "left bound starts always on 0");

			return mem_state;
		}

		/**
		 * @brief Returns the ID associated with a value.
		 * @note A new ID is assigned if the value has not been stored before.
		 */
		usize emplaceNewVal(const VarT& var) {
			const auto [is_new, var_id] = held_values.emplaceByLeft(var, next_val_id);
			next_val_id += (is_new ? 1 : 0);
			return var_id;
		}

	public:
		// Public state representing an empty vector.
		static constexpr auto EMPTY = VectorStateID{ u64{ Memory::EMPTY } };

		/**
		 * @brief Returns the element at an index in a vector state.
		 */
		const VarT& at(VectorStateID state_id, usize idx) const {
			const auto state = validateState(state_id);
			if_opt_some(inner.access(state, idx), val_id) { return held_values.atRight(val_id); }
			throw std::out_of_range("idx out of bounds");
		}

		/**
		 * @brief Returns the number of elements in a vector state.
		 */
		[[nodiscard]]
		usize size(VectorStateID state_id) const {
			const auto state = validateState(state_id);
			return inner.size(state);
		}

		/**
		 * @brief Returns a state with a value appended to the vector.
		 */
		[[nodiscard]]
		VectorStateID push(VectorStateID state_id, const VarT& var) {
			const auto state     = validateState(state_id);
			const auto size      = inner.size(state);
			const auto val_id    = emplaceNewVal(var);
			const auto new_state = inner.set(state, size, val_id);

			return toVecState(new_state);
		}

		/**
		 * @brief Returns a state with the value at an index replaced.
		 */
		[[nodiscard]]
		VectorStateID change(VectorStateID state_id, usize idx, const VarT& var) {
			const auto state = validateState(state_id);
			if (idx >= size(state_id)) throw std::invalid_argument("idx out of bounds");

			const usize val_id    = emplaceNewVal(var);
			const auto  new_state = inner.set(state, idx, val_id);

			return toVecState(new_state);
		}

		/**
		 * @brief Returns the half-open range `[left, right)` from a vector state.
		 * @note The range must be contained within `[0, size)` and `left` must not exceed `right`.
		 * @note An empty range is allowed and returns an empty vector.
		 */
		[[nodiscard]]
		std::vector<VarT> view(VectorStateID state_id, usize left, usize right) const {
			if (left > right) throw std::invalid_argument("left idx was bigger than right");
			if (right > size(state_id)) throw std::invalid_argument("right bound is too big");

			const auto state  = validateState(state_id);
			const auto m_iter = inner.getPathTo(state, left, Memory::Dir::Rght);

			if_opt_none(m_iter) return {};
			auto iter = *m_iter;

			if (iter.getIdx() >= right) return {};

			std::vector<VarT> ans = {};

			for (usize i = 0; i < right - left; i++) {
				CORE_ASSERT(iter.getIdx() == left + i, "I skipped some fields?!");

				const auto val_id = iter.getValue();
				ans.emplace_back(held_values.atRight(val_id));
				if (i + 1 < right - left) {
					const bool moved = iter.moveToValid(Memory::Dir::Rght);
					CORE_ASSERT(moved, "dense vector");
				}
			}

			return ans;
		}

		/**
		 * @brief Returns a state containing the first `pref_size` elements.
		 * @note `pref_size` must not exceed the size of the vector.
		 */
		[[nodiscard]]
		VectorStateID getPrefix(VectorStateID state_id, usize pref_size) {
			const auto state = validateState(state_id);
			const auto size  = inner.size(state);
			if (pref_size > size) throw std::invalid_argument("trying to take too much");
			if (pref_size == size) return state_id;
			if (pref_size == 0) return EMPTY;

			const auto new_state = inner.slice(state, 0, pref_size);

			return toVecState(new_state);
		}

		/**
		 * @brief Returns a state with values removed from the end of the vector.
		 * @note The number of values to remove must not exceed the size of the vector.
		 */
		[[nodiscard]]
		VectorStateID pop(VectorStateID state_id, usize how_many_pop = 1) {
			const auto state = validateState(state_id);
			const auto size  = inner.size(state);
			if (how_many_pop > size) throw std::invalid_argument("trying to pop too much");
			if (how_many_pop == size) return EMPTY;

			const auto new_state = inner.slice(state, 0, size - how_many_pop);

			return toVecState(new_state);
		}

		/**
		 * @brief Returns whether two vector states are equal.
		 */
		[[nodiscard]]
		bool eq(VectorStateID state_1, VectorStateID state_2) const {
			return state_1 == state_2;
		}

		Vector() = default;
	};

	template<typename VarT, typename VarH = std::hash<VarT>>
	class VectorStateView final {
		VectorStateID             id;
		const Vector<VarT, VarH>& vec;
		MemoryStateView           mem_view;

	public:
		class VectorIterator {
			MemoryStateView::MemoryIterator mem_it;
			const Vector<VarT, VarH>*       vec = nullptr;

		public:
			using value_type = const VarT&;

			const VarT& operator*() const {
				auto [idx, val_id] = *mem_it;
				return vec->held_values.atRight(val_id);
			}

			VectorIterator& operator++() {
				++mem_it;
				return *this;
			}

			VectorIterator operator++(int) {
				auto copy = *this;
				++(*this);
				return copy;
			}

			VectorIterator& operator--() {
				--mem_it;
				return *this;
			}

			VectorIterator operator--(int) {
				auto copy = *this;
				--(*this);
				return copy;
			}

			bool operator==(const VectorIterator& oth) const { return mem_it == oth.mem_it; }

			bool operator!=(const VectorIterator& oth) const { return !(*this == oth); }

			VectorIterator() = default;

			VectorIterator(const Vector<VarT, VarH>& vec, MemoryStateView::MemoryIterator mem_it):
				  mem_it(mem_it),
				  vec(&vec) {}
		};

		VectorStateView(const Vector<VarT, VarH>& vec, VectorStateID id):
			  id(id),
			  vec(vec),
			  mem_view(vec.inner, Vector<VarT, VarH>::toMemState(id)) {}

		VectorIterator begin() const { return VectorIterator{ vec, mem_view.begin() }; }

		VectorIterator end() const { return VectorIterator{ vec, mem_view.end() }; }

		const VarT& operator[](usize idx) const { return vec.at(id, idx); }

		[[nodiscard]]
		usize size() const {
			return vec.size(id);
		}
	};
}
