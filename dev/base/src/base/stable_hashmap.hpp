/**
 * @file stable_hashmap.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include "maps.hpp"

namespace base {
	/**
	 * A wrapper around base::HashMap, that keeps references (memory addresses) valid.
	 * @tparam DATA_T The datatype to store
	 * @tparam KEY_T Indentifies data
	 */
	template<typename KEY_T, typename DATA_T>
	class StableHashMap {
	public:
		StableHashMap() = default;

		/**
		 * Returns a const reference to data. If the data identified by the key does not exist
		 * throws std::out_of_range exception.
		 * @param key Data key
		 * @return A const reference to the data.
		 */
		const DATA_T& at(const KEY_T& key) const { return *data.at(key); }

		/**
		 * Returns a reference to the data. If the data identified by the key does not exist throws
		 * std::out_of_range exception.
		 * @param key Data key
		 * @return A reference to the data.
		 */
		DATA_T& at(const KEY_T& key) { return *data.at(key); }

		/**
		 * Returns a reference to the data inside an optional. If the data identified by the key
		 * does not exist returns an empty optional.
		 * @param key Data key
		 * @return An optional with a reference to the data.
		 */
		Optional<DATA_T&> atMaybe(const KEY_T& key) {
			if (contains(key)) return at(key);
			return {};
		}

		/**
		 * Returns a const reference to the data inside an optional. If the data identified by the
		 * key does not exist returns an empty optional.
		 * @param key Data key
		 * @return An optional with a const reference to the data.
		 */
		Optional<const DATA_T&> atMaybe(const KEY_T& key) const {
			if (contains(key)) return at(key);
			return {};
		}

		/**
		 * Returns the data identified by the key. If needed, allocates space for the key and data.
		 * @param key Data key
		 * @return A reference to the data.
		 */
		DATA_T& operator[](const KEY_T& key) {
			if (!contains(key)) data[key] = make_unique<DATA_T>();
			return *data[key];
		}

		/**
		 * If the container doesn't store the key yet, then inserts value identified by the key.
		 * @param key Data key
		 * @param value The data
		 */
		template<typename K = KEY_T, typename D = DATA_T>
		auto put(K&& key, D&& value) {
			return data.put(std::forward<K>(key), std::forward<D>(value));
		}

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

	private:
		// @TODO: Replace this with base::HashMap.
		std::unordered_map<KEY_T, unique_ptr<DATA_T>> data;
	};
}
