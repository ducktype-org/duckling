#pragma once

#include <concepts>
#include <limits>

/**
 * @file
 * @brief Checked and saturating integer arithmetic.
 * @details Answers "did this wrap", which `base::fitsIn` in numeric_utils.hpp does not:
 * `fitsIn` asks whether a value is representable in another type, these ask whether an
 * operation on two values stayed in range. The compiler builtins do it in one instruction
 * where they exist, and the fallbacks are exact.
 */

namespace base {

	/**
	 * @brief Adds @p a and @p b, reporting whether the result wrapped.
	 * @return true on OVERFLOW. @p out holds the result only when false is returned.
	 * @note The return value is the error, not the result - `if (addOvf(a, b, out)) return ...;`
	 * reads the way the check should.
	 */
	template<::std::unsigned_integral T>
	[[nodiscard]] constexpr bool addOvf(T a, T b, T& out) noexcept {
#if defined(__GNUC__) || defined(__clang__)
		return __builtin_add_overflow(a, b, &out);
#else
		out = static_cast<T>(a + b);
		return out < a;
#endif
	}

	/**
	 * @brief Multiplies @p a by @p b, reporting whether the result wrapped.
	 * @return true on OVERFLOW. @p out holds the result only when false is returned.
	 */
	template<::std::unsigned_integral T>
	[[nodiscard]] constexpr bool mulOvf(T a, T b, T& out) noexcept {
#if defined(__GNUC__) || defined(__clang__)
		return __builtin_mul_overflow(a, b, &out);
#else
		if (a != 0 && b > ::std::numeric_limits<T>::max() / a) return true;
		out = static_cast<T>(a * b);
		return false;
#endif
	}

	/**
	 * @brief Subtracts @p b from @p a, reporting whether the result went below zero.
	 * @return true on OVERFLOW. @p out holds the result only when false is returned.
	 */
	template<::std::unsigned_integral T>
	[[nodiscard]] constexpr bool subOvf(T a, T b, T& out) noexcept {
#if defined(__GNUC__) || defined(__clang__)
		return __builtin_sub_overflow(a, b, &out);
#else
		if (b > a) return true;
		out = static_cast<T>(a - b);
		return false;
#endif
	}

	// @TODO: #3718 use std::add_sat / std::mul_sat once we are on C++26

	/** @brief Adds @p a and @p b, clamping to the type's maximum instead of wrapping. */
	template<::std::unsigned_integral T>
	[[nodiscard]] constexpr T satAdd(T a, T b) noexcept {
		T out{};
		return addOvf(a, b, out) ? ::std::numeric_limits<T>::max() : out;
	}

	/** @brief Multiplies @p a by @p b, clamping to the type's maximum instead of wrapping. */
	template<::std::unsigned_integral T>
	[[nodiscard]] constexpr T satMul(T a, T b) noexcept {
		T out{};
		return mulOvf(a, b, out) ? ::std::numeric_limits<T>::max() : out;
	}

}  // namespace base
