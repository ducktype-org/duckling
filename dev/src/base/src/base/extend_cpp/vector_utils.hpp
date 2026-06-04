#pragma once

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
}
