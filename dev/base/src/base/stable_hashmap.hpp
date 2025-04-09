/**
 * @file stable_hashmap.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include "box.hpp"
#include "maps.hpp"

#include <type_traits>

namespace base {
	/**
	 * A wrapper around base::HashMap, that keeps references (memory addresses) valid.
	 * @tparam DATA_T The datatype to store
	 * @tparam KEY_T Indentifies data
	 * @tparam HASH_T Hash functor for hashing keys
	 */
	template<typename KEY_T, typename DATA_T, typename HASH_T = std::hash<KEY_T>>
	class StableHashMap final {
	public:
		StableHashMap() = default;

		/**
		 * Returns the data identified by the key.
		 * @note throws `std::out_of_range` if key is not present
		 * @param key Data key
		 * @return A reference to the data.
		 */
		DATA_T& operator[](const KEY_T& key) { return *data[key]; }

		/**
		 * Returns the data identified by the key.
		 * @note throws `std::out_of_range` if key is not present
		 * @param key Data key
		 * @return A reference to the data.
		 */
		const DATA_T& operator[](const KEY_T& key) const { return *data[key]; }

		/**
		 * Returns a reference to the data inside an optional. If the data identified by the key
		 * does not exist returns an empty optional.
		 * @param key Data key
		 * @return An optional with a reference to the data.
		 */
		Optional<DATA_T&> atMaybe(const KEY_T& key) {
			if_opt_some(data.atMaybe(key), ptr) { return *ptr; }
			return {};
		}

		/**
		 * Returns a const reference to the data inside an optional. If the data identified by the
		 * key does not exist returns an empty optional.
		 * @param key Data key
		 * @return An optional with a const reference to the data.
		 */
		Optional<const DATA_T&> atMaybe(const KEY_T& key) const {
			if_opt_some(data.atMaybe(key), ptr) { return *ptr; }
			return {};
		}

		/**
		 * Returns a copy of a data inside an optional. If the data identified by the
		 * key does not exist returns an empty optional.
		 * @param key Data key
		 * @return An optional with a copy of the data.
		 */
		Optional<DATA_T> atMaybeCopy(const KEY_T& key) const
			requires std::is_copy_constructible_v<DATA_T> {
			if_opt_some(data.atMaybe(key), ptr) { return *ptr; }
			return {};
		}

		/**
		 * If the container doesn't store the key yet, then inserts value identified by the key.
		 * @param key Data key
		 * @param value The data
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		auto put(K&& key, D&& value) {
			return data.put(std::forward<K>(key), ::base::makeBox<DATA_T>(std::forward<D>(value)));
		}

		/**
		 * @brief Erases value at @p key position if it exists.
		 * @returns Whether a value was erased.
		 */
		bool erase(const KEY_T& key) { data.erase(key); }

		/**
		 * Clears all data from the data structure.
		 */
		void clear() { data.clear(); }

		/**
		 * Check if key is stored in the container.
		 * @param key The key to query
		 * @return True if containers already stores the key, false otherwise.
		 */
		bool contains(const KEY_T& key) const { return data.contains(key); }

		/**
		 * Query the number of pairs stored in the container.
		 * @return Number of pairs
		 */
		[[nodiscard]]
		usize size() const {
			return data.size();
		}

		bool operator==(const StableHashMap& other) const { return data == other.data; }

	private:
		HashMap<KEY_T, Box<DATA_T>, HASH_T> data;
	};
}
