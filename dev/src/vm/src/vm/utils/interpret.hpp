#pragma once

namespace vm {
	/**
	 * Interprets U's bytes as type T.
	 * @tparam T Type of returned reference
	 * @tparam U Initial type of data
	 * @param value the data
	 * @return Reference to U's bytes as T.
	 */
	template<typename T, typename U>
	requires(sizeof(T) <= sizeof(U)) constexpr static T& interpretBytes(U& value) {
		return *reinterpret_cast<T*>(&value);
	}
}
