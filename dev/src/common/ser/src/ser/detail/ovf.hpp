#pragma once

#include <ser/config.hpp>

#include <concepts>
#include <limits>

#if SER_HAS_SATURATING
#  include <numeric>
#endif

namespace ser::detail {

// true = OVERFLOW. The result in `out` is valid only when false is returned.
template <::std::unsigned_integral T>
[[nodiscard]] constexpr bool add_ovf(T a, T b, T& out) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_add_overflow(a, b, &out);
#else
    out = static_cast<T>(a + b);
    return out < a;
#endif
}

template <::std::unsigned_integral T>
[[nodiscard]] constexpr bool mul_ovf(T a, T b, T& out) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_mul_overflow(a, b, &out);
#else
    if (a != 0 && b > ::std::numeric_limits<T>::max() / a) return true;
    out = static_cast<T>(a * b);
    return false;
#endif
}

template <::std::unsigned_integral T>
[[nodiscard]] constexpr bool sub_ovf(T a, T b, T& out) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_sub_overflow(a, b, &out);
#else
    if (b > a) return true;
    out = static_cast<T>(a - b);
    return false;
#endif
}

template <::std::unsigned_integral T>
[[nodiscard]] constexpr T sat_add(T a, T b) noexcept {
#if SER_HAS_SATURATING
    return ::std::add_sat(a, b);
#else
    T out{};
    return add_ovf(a, b, out) ? ::std::numeric_limits<T>::max() : out;
#endif
}

template <::std::unsigned_integral T>
[[nodiscard]] constexpr T sat_mul(T a, T b) noexcept {
#if SER_HAS_SATURATING
    return ::std::mul_sat(a, b);
#else
    T out{};
    return mul_ovf(a, b, out) ? ::std::numeric_limits<T>::max() : out;
#endif
}

}  // namespace ser::detail