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
}
