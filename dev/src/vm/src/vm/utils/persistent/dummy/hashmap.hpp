#pragma once

#include <base/collections/maps.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/types/ints.hpp>

#include <functional>
#include <vector>

namespace vm::persistent {

	/**
	 * @brief A simple persistent hash map implementation for testing.
	 *
	 * Each operation creates a new state while preserving all previous states.
	 * @warning This implementation is O(N^2) and should only be used for testing or small inputs.
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
		 * @brief Returns a new state with a key-value pair inserted or replaced.
		 */
		[[nodiscard]]
		usize insert(usize state, const Key& k, const Val& v) {
			auto copy = validateState(state);
			copy.insertOrAssign(k, v);
			copies.emplace_back(copy);
			return copies.size() - 1;
		}

		/**
		 * @brief Returns a new state with the entry for a key removed.
		 */
		[[nodiscard]]
		usize erase(usize state, const Key& k) {
			auto copy = validateState(state);
			copy.erase(k);
			copies.emplace_back(copy);
			return copies.size() - 1;
		}

		/**
		 * @brief Returns whether a key-value pair was inserted and the resulting state.
		 */
		[[nodiscard]]
		std::pair<bool, usize> emplace(usize state, const Key& k, const Val& v) {
			auto copy = validateState(state);
			if (copy.contains(k)) return { false, state };
			copy.put(k, v);
			copies.emplace_back(copy);
			return { true, copies.size() - 1 };
		}

		/**
		 * @brief Returns the value associated with a key in a hash map state.
		 */
		[[nodiscard]]
		const Val& at(usize state, const Key& k) const {
			return validateState(state).at(k);
		}

		/**
		 * @brief Returns whether a key is present in a hash map state.
		 */
		[[nodiscard]]
		bool contains(usize state, const Key& k) const {
			return validateState(state).contains(k);
		}

		/**
		 * @brief Returns whether two hash map states are equal.
		 * @warning This operation is O(N).
		 */
		[[nodiscard]]
		bool eq(usize state_1, usize state_2) const {
			return validateState(state_1) == validateState(state_2);
		}

		DummyHashMap() { copies.emplace_back(base::HashMap<Key, Val, Hasher>{}); }
	};
}
