#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <iterator>
#include <type_traits>
#include "optional.hpp"
#include "exceptions.hpp"

namespace base {
	template<typename ContainerType>
	class MapWrapper: public ContainerType {
		// hiding base member:
		using ContainerType::operator[];
		using ContainerType::insert;

	public:
		typedef MapWrapper                          SelfType;
		typedef typename ContainerType::key_type    KEY_T;
		typedef typename ContainerType::mapped_type DATA_T;

		MapWrapper(): ContainerType(){};
		MapWrapper(const MapWrapper& map): ContainerType(map){};
		MapWrapper(MapWrapper&& map) noexcept: ContainerType(std::move(map)){};

		~MapWrapper() = default;

		// Change operator[] behaviour:
		DATA_T& operator[](const KEY_T& key) { return ContainerType::at(key); }

		DATA_T& operator[](KEY_T&& key) { return ContainerType::at(key); }

		template<class K = KEY_T>
		Optional<DATA_T&> atMaybe(K&& key) {
			if (contains(key)) operator[](std::forward<K>(key));
			return {};
		}

		template<class K = KEY_T>
		Optional<const DATA_T&> atMaybe(K&& key) const {
			if (contains(key)) operator[](std::forward<K>(key));
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

	template<typename KEY_T, typename DATA_T>
	using Map = MapWrapper<std::map<KEY_T, DATA_T>>;

	template<typename KEY_T, typename DATA_T, typename HashT = std::hash<KEY_T>>
	using HashMap = MapWrapper<std::unordered_map<KEY_T, DATA_T, HashT>>;

	template<
		typename KEY_T,
		typename DATA_T,
		bool is_move = std::is_move_constructible_v<DATA_T>,
		bool is_copy = std::is_copy_constructible_v<DATA_T>>
	class VectorMap {
		std::vector<Optional<DATA_T>> map;
		usize                         element_count{};

	public:
		typedef VectorMap SelfType;
		typedef KEY_T     IdType;
		typedef DATA_T    DataType;

		typedef typename std::vector<Optional<DATA_T>>::iterator       iterator;
		typedef typename std::vector<Optional<DATA_T>>::const_iterator const_iterator;

		VectorMap() = default;

		VectorMap(const VectorMap&) = delete;

		VectorMap(const VectorMap&&) = delete;

		~VectorMap() = default;

		DATA_T& operator[](const KEY_T key) {
			if (usize(key) < map.size() and map.at(usize(key)).has_value())
				return *map.at(usize(key));
			else
				throw base::LogicError("No value assigned to key in VectorMap");
		}

		Optional<DATA_T&> atMaybe(const KEY_T& key) {
			if (contains(key)) operator[](key);
			return {};
		}

		Optional<const DATA_T&> atMaybe(const KEY_T& key) const {
			if (contains(key)) operator[](key);
			return {};
		}

		void put(KEY_T key) {
			if (usize(key) >= map.size()) map.resize(key + 1);
			if (!map.at(usize(key)).has_value()) element_count++;
			map.at(usize(key)) = DATA_T();
		}

		void put(KEY_T key, DATA_T&& data)
		requires is_move
		{
			if (usize(key) >= map.size()) map.resize(usize(key) + 1);
			if (!map.at(usize(key)).has_value()) element_count++;
			map.at(usize(key)).emplace(std::move(data));
		}

		void put(KEY_T key, const DATA_T& data)
		requires is_copy
		{
			if (usize(key) >= map.size()) map.resize(usize(key) + 1);
			if (!map.at(usize(key)).has_value()) element_count++;
			map.at(usize(key)).emplace(data);
		}

		template<class... Args>
		void emplace(KEY_T key, Args&&... args) {
			put(key, std::move(DATA_T(std::move(&args...))));
		}

		bool contains(KEY_T key) const {
			if (usize(key) >= map.size()) return false;
			return map.at(usize(key)).has_value();
		}

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
