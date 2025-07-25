#pragma once

#include <cstddef>

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

	/**
	 * @brief Interprets a bytes pointed to by `ptr` as an object of type T.
	 * @warning This operation is unsafe and violates strict alignment rules.
	 * The caller is expected to know what they are doing. Incorrect usage may lead to UB.
	 *
	 * @tparam T The target type to interpret the bytes as.
	 * @param ptr A pointer to the beginning of the byte buffer.
	 * @return A reference to the memory, now treated as type T.
	 */
	template<typename T>
	constexpr static T& interpretBytes(std::byte* ptr) {
		return *reinterpret_cast<T*>(ptr);
	}

	/**
	 * @brief Interprets a constant raw byte buffer pointed to by `ptr` as an object of type T.
	 * @warning This operation is unsafe and violates strict alignment rules.
	 * The caller is expected to know what they are doing. Incorrect usage may lead to UB.
	 *
	 * @tparam T The target type to interpret the bytes as.
	 * @param ptr A pointer to the beginning of the const byte buffer.
	 * @return A const reference to the memory, now treated as type T.
	 */
	template<typename T>
	constexpr static const T& interpretBytes(const std::byte* ptr) {
		return *reinterpret_cast<const T*>(ptr);
	}
}
