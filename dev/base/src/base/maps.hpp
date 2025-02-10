/**
 * @file maps.hpp
 *
 * @brief Provides utility classes for maps.
 */
#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <type_traits>
#include "type_traits.hpp"
#include "optional.hpp"
#include "exceptions.hpp"

namespace base {
	/**
	 * @brief Map Wrapper that uses a non-inserting `[] operator`.
	 *
	 * Insertion of new elements is handled by the put method.
	 */
	template<class ContainerType>
	class MapWrapper: public ContainerType {
		// hiding base member:
		using ContainerType::operator[];
		using ContainerType::insert;

	public:
		using SelfType = MapWrapper;
		using KEY_T    = typename ContainerType::key_type;
		using DATA_T   = typename ContainerType::mapped_type;

		MapWrapper(): ContainerType() {};
		MapWrapper(const MapWrapper& map): ContainerType(map) {};
		MapWrapper(MapWrapper&& map) noexcept: ContainerType(std::move(map)) {};
		~MapWrapper() = default;

		MapWrapper& operator=(const MapWrapper& map) {
			ContainerType::operator=(map);
			return *this;
		}

		MapWrapper& operator=(MapWrapper&& map) noexcept {
			ContainerType::operator=(std::move(map));
			return *this;
		}

		// Change operator[] behaviour:
		DATA_T& operator[](const KEY_T& key) { return ContainerType::at(key); }

		DATA_T& operator[](KEY_T&& key) { return ContainerType::at(key); }

		template<class K = KEY_T>
		Optional<DATA_T&> atMaybe(K&& key) {
			auto&& key_val = ContainerType::find(std::forward<K>(key));
			if (key_val != ContainerType::end()) return key_val->second;
			return {};
		}

		template<class K = KEY_T>
		Optional<const DATA_T&> atMaybe(K&& key) const {
			auto&& key_val = ContainerType::find(std::forward<K>(key));
			if (key_val != ContainerType::end()) return key_val->second;
			return {};
		}

		template<class K = KEY_T>
		Optional<DATA_T> atMaybeCopy(K&& key) const {
			auto&& key_val = ContainerType::find(std::forward<K>(key));
			if (key_val != ContainerType::end()) return key_val->second;
			return {};
		}

		const DATA_T& operator[](const KEY_T& key) const { return ContainerType::at(key); }

		auto put(const KEY_T& key) { return ContainerType::emplace(key, DATA_T()); }

		template<typename K = KEY_T, typename D = DATA_T>
		auto put(K&& key, D&& data) {
			return ContainerType::emplace(std::forward<K>(key), std::forward<D>(data));
		}

		[[nodiscard]]
		bool notEmpty() const {
			return !ContainerType::empty();
		}
	};

	/**
	 * @brief Wrapped std::map for use in our code.
	 */
	template<typename KEY_T, typename DATA_T>
	using Map = MapWrapper<std::map<KEY_T, DATA_T>>;

	/**
	 * @brief Wrapped std::unordered_map for use in our code.
	 */
	template<typename KEY_T, typename DATA_T, class HashT = std::hash<KEY_T>>
	using HashMap = MapWrapper<std::unordered_map<KEY_T, DATA_T, HashT>>;

	/**
	 * @brief Vector based map that keeps O(max_used_key) memory but has constant time access.
	 *
	 * @tparam is_move Can be used to forbid operations that require to move a value.
	 * @tparam is_copy Can be used to forbid operations that require to copy a value.
	 *
	 * @note Keys should be convertible to usize.
	 */
	template<
		typename KEY_T,
		typename DATA_T,
		bool is_move = std::is_move_constructible_v<DATA_T>,
		bool is_copy = std::is_copy_constructible_v<DATA_T>>
	// Sanity check
	requires base::Implication<is_move, std::is_move_constructible_v<DATA_T>>
	      && base::Implication<is_copy, std::is_copy_constructible_v<DATA_T>> class VectorMap {
		std::vector<Optional<DATA_T>> map;
		usize                         element_count{};

	public:
		using SelfType = VectorMap;
		using IDType   = KEY_T;
		using DataType = DATA_T;

		using iterator       = typename std::vector<Optional<DATA_T>>::iterator;
		using const_iterator = typename std::vector<Optional<DATA_T>>::const_iterator;

		VectorMap() = default;

		VectorMap(const VectorMap&) = delete;

		VectorMap(const VectorMap&&) = delete;

		~VectorMap() = default;

		/**
		 * @brief Non-inserting element access.
		 *
		 * Throws `base::LogicError` on bad element access.
		 */
		DATA_T& operator[](const KEY_T key) {
			if (usize(key) < map.size() and map.at(usize(key)).has_value())
				return *map.at(usize(key));
			throw LogicError("No value assigned to key in VectorMap");
		}

		Optional<DATA_T&> atMaybe(KEY_T key) {
			if (static_cast<usize>(key) < map.size()) return *map.at(static_cast<usize>(key));
			return {};
		}

		Optional<const DATA_T&> atMaybe(KEY_T key) const {
			if (static_cast<usize>(key) < map.size()) return *map.at(static_cast<usize>(key));
			return {};
		}

		/**
		 * @brief Inserts empty value at @p key
		 */
		void put(KEY_T key) {
			if (usize(key) >= map.size()) map.resize(key + 1);
			if (!map.at(usize(key)).has_value()) element_count++;
			map.at(usize(key)) = DATA_T();
		}

		/**
		 * @brief Moves @p data value to @p key position.
		 */
		void put(KEY_T key, DATA_T&& data) requires is_move {
			if (usize(key) >= map.size()) map.resize(usize(key) + 1);
			if (!map.at(usize(key)).has_value()) element_count++;
			map.at(usize(key)).emplace(std::move(data));
		}

		/**
		 * @brief Copies @p data value to @p key position.
		 */
		void put(KEY_T key, const DATA_T& data) requires is_copy {
			if (usize(key) >= map.size()) map.resize(usize(key) + 1);
			if (!map.at(usize(key)).has_value()) element_count++;
			map.at(usize(key)).emplace(data);
		}

		/**
		 * @brief Constructs value from @p args at @p key position.
		 *
		 * @note I'm not sure this works properly with move disabled.
		 * @note I'm not sure whether argument move shouldn't be a forward.
		 */
		template<class... Args>
		void emplace(KEY_T key, Args&&... args) {
			put(key, std::move(DATA_T(std::move(&args...))));
		}

		/**
		 * @brief Checks whether the map has an element on @p key position.
		 */
		bool contains(KEY_T key) const {
			if (usize(key) >= map.size()) return false;
			return map.at(usize(key)).has_value();
		}

		/**
		 * @brief Erases value at @p key position if it exists.
		 * @returns Whether a value was erased.
		 */
		bool erase(KEY_T key) {
			if (usize(key) >= map.size()) return false;
			if (map.at(usize(key)).has_value()) {
				map.at(usize(key)).reset();
				element_count--;
				return true;
			}
			return false;
		}

		[[nodiscard]]
		usize size() const {
			return element_count;
		}

		[[nodiscard]]
		bool empty() const {
			return element_count == 0;
		}

		[[nodiscard]]
		bool notEmpty() const {
			return !empty();
		}

		iterator begin() { return map.begin(); }

		const_iterator begin() const { return map.cbegin(); }

		iterator end() { return map.end(); }

		const_iterator end() const { return map.cend(); }
	};
}
