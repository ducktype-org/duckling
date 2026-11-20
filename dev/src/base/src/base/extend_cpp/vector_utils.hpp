// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <algorithm>
#include <functional>
#include <unordered_set>
#include <vector>

// @TODO: #1619 extend this header, and use it across the codebase

namespace base {

	/**
	 * @brief Appends the contents of one vector to another.
	 * @param dest The vector to which elements will be appended.
	 * @param src The vector from which elements will be taken to append to dest.
	 * @tparam T The type of elements in the vectors.
	 */
	template<typename T>
	void appendToVector(std::vector<T>& dest, const std::vector<T>& src) {
		dest.insert(dest.end(), src.begin(), src.end());
	}

	/**
	 * @brief Filters a vector in place, removing elements that do not satisfy the given predicate.
	 * @param vec The vector to be filtered.
	 * @param predicate A function that takes an element of the vector and returns true if it should
	 * be kept, or false if it should be removed.
	 * @tparam T The type of elements in the vector.
	 * @tparam Predicate The type of the predicate function.
	 */
	template<typename T, typename Predicate>
	void filterVectorInPlace(std::vector<T>& vec, const Predicate& predicate) {
		std::erase_if(vec, [&predicate](const T& item) { return !predicate(item); });
	}

	template<typename T, typename KeyFunc>
	requires std::invocable<KeyFunc, const T&>
	void deduplicateBy(std::vector<T>& vec, KeyFunc key_func) {
		using Key = std::invoke_result_t<KeyFunc, const T&>;
		std::unordered_set<Key> seen;
		base::filterVectorInPlace(vec, [&](auto& val) {
			auto key = std::invoke(key_func, val);
			return seen.insert(key).second;
		});
	}

	/**
	 * @brief Checks whether a vector contains all the elements of another one.
	 * @param vec The vector that is expected to contain the elements.
	 * @param elements The elements that have to be present in `vec`.
	 * @tparam T The type of elements in the vectors.
	 */
	template<typename T>
	bool containsAllOf(const std::vector<T>& vec, const std::vector<T>& elements) {
		return std::ranges::all_of(elements, [&vec](const T& element) {
			return std::ranges::find(vec, element) != vec.end();
		});
	}

	/**
	 * @brief Moves a pack of items into a `std::vector<Element>`, preserving order.
	 * The element type is explicit — it cannot be deduced when items are, e.g., derived-type.
	 */
	template<typename Element, typename... Items>
	requires(std::is_constructible_v<Element, Items &&> && ...)
	std::vector<Element> packToVector(Items&&... items) {
		std::vector<Element> result;
		result.reserve(sizeof...(items));
		(result.emplace_back(std::forward<Items>(items)), ...);
		return result;
	}
}
