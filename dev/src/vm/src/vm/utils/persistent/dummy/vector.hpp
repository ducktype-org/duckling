#pragma once

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <utility>

namespace vm::persistent {

	/**
	 * @brief A simple persistent vector implementation for testing.
	 *
	 * Each operation creates a new state while preserving all previous states.
	 * @warning This implementation is O(N^2) and should only be used for testing or small inputs.
	 */
	template<typename T>
	class DummyVector final {
		std::vector<std::pair<base::Optional<usize>, std::vector<T>>> copies;

		[[nodiscard]]
		auto& validateState(usize state) const {
			CORE_ASSERT(state < copies.size(), "We don't have a copy of given state");
			return copies.at(state);
		}

	public:
		static constexpr usize EMPTY = 0;

		/**
		 * @brief Returns a new state with values removed from the end.
		 * @throws std::invalid_argument if the number of values exceeds the vector size.
		 */
		[[nodiscard]]
		usize pop(usize state, usize no_of_values_to_pop = 1) {
			for (; no_of_values_to_pop > 0; no_of_values_to_pop--) {
				auto& curr_state = validateState(state);
				if_opt_none(curr_state.first) break;
				state = *curr_state.first;
			}

			if (no_of_values_to_pop == 0) return state;

			if (state == EMPTY) throw std::invalid_argument("Trying to pop from an empty state");

			auto copy = validateState(state).second;

			for (; no_of_values_to_pop > 0; no_of_values_to_pop--) copy.pop_back();

			copies.emplace_back(std::nullopt, copy);

			return copies.size() - 1;
		}

		/**
		 * @brief Returns a new state with the value at an index replaced.
		 */
		[[nodiscard]]
		usize change(usize state, usize idx, const T& val) {
			auto copy    = validateState(state).second;
			copy.at(idx) = val;
			copies.emplace_back(std::nullopt, copy);

			return copies.size() - 1;
		}

		/**
		 * @brief Returns a new state with a value appended to the end.
		 */
		[[nodiscard]]
		usize push(usize state, const T& val) {
			auto copy = validateState(state).second;
			copy.push_back(val);
			copies.emplace_back(state, copy);

			return copies.size() - 1;
		}

		/**
		 * @brief Returns whether two vector states are equal.
		 * @warning This operation is O(N).
		 */
		[[nodiscard]]
		bool eq(usize state_1, usize state_2) const {
			return validateState(state_1).second == validateState(state_2).second;
		}

		/**
		 * @brief Returns the element at an index in a vector state.
		 */
		[[nodiscard]]
		const T& at(usize state, usize idx) const {
			return validateState(state).second.at(idx);
		}

		/**
		 * @brief Returns the number of elements in a vector state.
		 */
		[[nodiscard]]
		usize size(usize state) const {
			return validateState(state).second.size();
		}

		DummyVector() { copies.emplace_back(base::Optional<usize>{}, std::vector<T>{}); }
	};
}
