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

STRONG_TYPEDEF_INT(u8, uint8_t);
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

STRONG_TYPEDEF_INT(i8, int8_t);
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using uchar = unsigned char;

using byte  = std::byte;
using usize = std::size_t;

// Code might break if following does not hold:
static_assert(sizeof(byte) == sizeof(char));
static_assert(sizeof(byte) == sizeof(u8));
