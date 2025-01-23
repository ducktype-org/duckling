#pragma once

#include <type_traits>

template<typename, typename = void>
struct IS_COMPLETE {
	static constexpr bool VALUE = false;
};

template<typename T>
struct IS_COMPLETE<T, std::void_t<decltype(sizeof(T))>> {
	static constexpr bool VALUE = true;
};

/**
 * @brief Check if a type is complete.
 *
 * Usage: IS_COMPLETE_V<T>
 *
 * Note: if this template returns true, then the type is complete.
 * However, if this type trait may return a false negative.
 * Use it only to guarantee completeness in critical code fragments.
 *
 * @tparam T Type to check for completeness.
 * @tparam S Dummy parameter, used for SFINAE. Do not supply.
 */
template<typename T, typename S = void>
constexpr bool IS_COMPLETE_V = IS_COMPLETE<T, S>::VALUE;
