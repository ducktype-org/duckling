/**
 * @file ints.hpp
 *
 * @attention `<cstdint>` should not be used, unless necessary. Ints should be used instead.
 *
 * @brief Ints is a library analogous to `<cstdint>` with generally shorter type names
 * and one big improvement: `u8` and `i8` types are strongly typed.
 */
#pragma once

#include "strongly_typed_int.hpp"

#include <cstddef>
#include <cstdint>
#include <variant>

STRONG_TYPEDEF_INT(u8, uint8_t);
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

STRONG_TYPEDEF_INT(i8, int8_t);
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;
using f80 = long double;

using num_ctv = std::variant<i16, i32, i64, f32, f64, f80>;

inline bool operator==(const num_ctv& lhs, i64 rhs) {
    return std::visit([&](auto val) -> bool {
        if constexpr (std::is_convertible_v<decltype(val), i64>)
            return static_cast<i64>(val) == rhs;
        else
            return false;
    }, lhs);
}

inline bool operator==(i64 lhs, const num_ctv& rhs) {
    return rhs == lhs; // Reuse the logic above
}



using uchar = unsigned char;

using byte  = std::byte;
using usize = std::size_t;

// Code might break if following does not hold:
static_assert(sizeof(byte) == sizeof(char));
static_assert(sizeof(byte) == sizeof(u8));
