#pragma once

#include "ints.hpp"
#include "smart_pointers.hpp"
#include "optional.hpp"
#include <vector>

namespace base {

	template<typename Data>
	using StableVectorRef = borrow_ptr<Data>;

	template<typename Data>
	using StableVectorCRef = c_borrow_ptr<Data>;

	/**
	 * @brief Key must be „standard” numeric value such as:
	 * integer
	 * strongly typed int
	 * NamedID
	 *
	 * Right now StableVector keys must be convertible to and from usize
	 *
	 * @TODO make concept to check it
	 * @TODO add range based iteration
	 */
	template<typename Key, typename Data>
	class StableVector {
		/**
		 * @brief @TODO
		 * for now it is simple, naive implementation
		 * in the future change it to something better
		 */

		std::vector<unique_ptr<Data>> data;

	public:
		using Ref  = StableVectorRef<Data>;
		using CRef = StableVectorCRef<Data>;

		[[nodiscard]]
		constexpr usize size() const noexcept {
			return data.size();
		}

		[[nodiscard]]
		constexpr usize empty() const noexcept {
			return data.empty();
		}

		[[nodiscard]]
		constexpr usize notEmpty() const noexcept {
			return !data.empty();
		}

		/**
		 * @brief Quick, unsafe, constexpr access
		 */
		constexpr Data& operator[](Key pos) { return *data.at(static_cast<usize>(pos)); }

		/**
		 * @brief Quick, unsafe, constexpr access
		 */
		constexpr const Data& operator[](Key pos) const {
			return *data.at(static_cast<usize>(pos));
		}

		Optional<Ref> getRef(Key pos) noexcept {
			if (static_cast<usize>(pos) >= size()) return {};
			return data[static_cast<usize>(pos)].borrow_mut();
		}

		Optional<CRef> getCRef(Key pos) const noexcept {
			if (static_cast<usize>(pos) >= size()) return {};
			return data[static_cast<usize>(pos)].borrow();
		}

		constexpr Key pushBack(const Data& value) {
			auto new_ptr = base::make_unique<Data>(value);
			data.emplace_back(std::move(new_ptr));
			return Key(data.size() - 1);
		}

		Key pushBack(Data&& value) {
			auto new_ptr = base::make_unique<Data>(std::move(value));
			data.emplace_back(std::move(new_ptr));
			return Key(data.size() - 1);
		}

		Ref last() { return data.back().borrow_mut(); }

		CRef last() const { return data.back().borrow(); }

		template<class... Args>
		constexpr Key emplaceBack(Args&&... args) {
			return pushBack(Data(std::forward<Args>(args)...));
		}
	};

	template<typename T>
	using StableIntVector = StableVector<usize, T>;

	/**
	 * A wrapper around std::unordered_map, that keeps references (memory addresses) valid.
	 * @tparam Key Indentifies data
	 * @tparam Data The datatype to store
	 */
	template<typename Key, typename Data>
	class StableHashMap {
	public:
		StableHashMap() = default;

		/**
		 * Returns a const reference to data. If the data identified by the key does not exist
		 * throws std::out_of_range exception.
		 * @param key Data key
		 * @return A const reference to the data.
		 */
		const Data& at(const Key& key) const { return data.at(key); }

		/**
		 * Returns a reference to the data. If the data identified by the key does not exist throws
		 * std::out_of_range exception.
		 * @param key Data key
		 * @return A reference to the data.
		 */
		Data& at(const Key& key) { return data.at(key); }

		/**
		 * Returns the data identified by the key. If needed, allocates space for the key and data.
		 * @param key Data key
		 * @return A reference to the data.
		 */
		Data& operator[](const Key& key) {
			if (!contains(key)) data[key] = make_unique<Data>();
			return *data[key];
		}

		/**
		 * If the container doesn't store the key yet, then inserts value identified by the key.
		 * @param key Data key
		 * @param value The data
		 */
		void put(const Key& key, Data value) {
			if (!contains(key)) data[key] = make_unique<Data>(value);
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
		bool contains(const Key& key) const { return data.contains(key); }

		/**
		 * Query the number of pairs stored in the container.
		 * @return Number of pairs
		 */
		[[nodiscard]]
		usize size() const {
			return data.size();
		}

	private:
		std::unordered_map<Key, unique_ptr<Data>> data;
	};

}
