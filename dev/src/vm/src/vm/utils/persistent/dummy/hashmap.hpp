#pragma once

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

namespace vm::persistent {

	/**
	 * @brief A persistent data strcture simulating STL unordered_map but with the ability to access
	 * and modify any of it's previous states.
	 * @note can be thought of Hashmap<MapStateID, HashMap<Key, Val> >
	 * @warning THIS IS A NAIVE IMPLEMENTATION IN O(N^2), USE FOR TESTING OR SMALL NUMBER OF
	 * OPERATIONS
	 */
	template<typename Key, typename Val, typename Hasher = std::hash<Key>>
	class DummyHashMap final {
		std::vector<base::HashMap<Key, Val, Hasher>> copies;

		[[nodiscard]]
		auto& validateState(usize state) const {
			CORE_ASSERT(state < copies.size(), "We don't have a copy of given state");
			return copies.at(state);
		}

	public:
		static constexpr usize EMPTY = 0;

		/**
		 * @brief creates a new state from the pevious one, by inserting new value at new key
		 * @throws when key was previously present in the hashmap at given state
		 */
		[[nodiscard]]
		usize insert(usize state, const Key& k, const Val& v) {
			auto copy = validateState(state);

			if (copy.contains(k)) throw std::invalid_argument("overriding a present value");
			copy.put(k, v);

			copies.emplace_back(copy);

			return copies.size() - 1;
		}

		/**
		 * @brief accessor to elements at given state by key
		 */
		[[nodiscard]]
		const Val& at(usize state, const Key& k) const {
			return validateState(state).at(k);
		}

		/**
		 * @brief checks if the certain key is present at given instance of a hashmap
		 */
		[[nodiscard]]
		bool contains(usize state, const Key& k) const {
			return validateState(state).contains(k);
		}

		/**
		 * @brief comapre two states of the hashmap
		 * @return true if the instances are equal
		 * @warning THIS TAKES O(N)
		 */
		[[nodiscard]]
		bool eq(usize state_1, usize state_2) const {
			return validateState(state_1) == validateState(state_2);
		}

		DummyHashMap() { copies.emplace_back(base::HashMap<Key, Val, Hasher>{}); }
	};
}
