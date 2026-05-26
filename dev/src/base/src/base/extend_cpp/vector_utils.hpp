#pragma once

#include <vector>

// @TODO: #1619 extend this, and use it across the codebase

namespace base {

	template<typename T>
	void appendToVector(std::vector<T>& dest, const std::vector<T>& src) {
		dest.insert(dest.end(), src.begin(), src.end());
	}

}
