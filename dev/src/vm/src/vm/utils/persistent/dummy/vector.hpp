#pragma once

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <utility>

namespace vm::persistent {

	/**
	 * @brief A persistent data structure simulating STL vector but with the ability to access
	 * and modify any of it's previous states.
	 * @note can be thought of Hashmap<VectorStateID, Vector<T> >
	 * @warning THIS IS A NAIVE IMPLEMENTATION IN O(N^2), USE FOR TESTING OR SMALL NUMBER OF
	 * OPERATIONS
	 */
	template<typename T>
	class DummyVector {
		std::vector<std::pair<base::Optional<usize>, std::vector<T>>> copies;

		[[nodiscard]]
		auto& validateState(usize state) const {
			CORE_ASSERT(state < copies.size(), "We don't have a copy of given state");
			return copies.at(state);
		}

	public:
		static constexpr usize EMPTY = 0;

		/**
		 * @brief creates a new state from the pevious one, by popping some number of variables from
		 * the end
		 * @throws when size of vector is smaller than the number of values to pop
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
		 * @brief creates a new state from the pevious one, by changing value at the index
		 */
		[[nodiscard]]
		usize change(usize state, usize idx, const T& val) {
			auto copy    = validateState(state).second;
			copy.at(idx) = val;
			copies.emplace_back(std::nullopt, copy);

			return copies.size() - 1;
		}

		/**
		 * @brief creates a new state from the pevious one, by pushing variable at the end
		 */
		[[nodiscard]]
		usize push(usize state, const T& val) {
			auto copy = validateState(state).second;
			copy.push_back(val);
			copies.emplace_back(state, copy);

			return copies.size() - 1;
		}

		/**
		 * @brief comapre two states of the vector
		 * @return true if the instances are equal
		 * @warning THIS TAKES O(N)
		 */
		[[nodiscard]]
		bool eq(usize state_1, usize state_2) const {
			return validateState(state_1).second == validateState(state_2).second;
		}

		/**
		 * @brief accessor to elements at given state by idx
		 */
		[[nodiscard]]
		const T& at(usize state, usize idx) const {
			return validateState(state).second.at(idx);
		}

		/**
		 * @brief return size of the vector at given state
		 */
		[[nodiscard]]
		usize size(usize state) const {
			return validateState(state).second.size();
		}

		DummyVector() { copies.emplace_back(base::Optional<usize>{}, std::vector<T>{}); }
	};
}
