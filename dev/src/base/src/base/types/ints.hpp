/**
 * @file ints.hpp
 *
 * @attention `<cstdint>` should not be used, unless necessary. Ints should be used instead.
 *
 * @brief Ints is a library analogous to `<cstdint>` with generally shorter type names
 * and one big improvement: `u8` and `i8` types are strongly typed.
 */
#pragma once

#include <base/extend_cpp/strongly_typed_int.hpp>

#include <cstddef>
#include <cstdint>

STRONG_TYPEDEF_INT(u8, uint8_t);
using u16 = uint16_t;
using u32 = uint32_t;
#ifdef __APPLE__
// On macOS `std::size_t` is `unsigned long` while `uint64_t` is `unsigned long long` —
// two distinct 64-bit types. The codebase treats `u64` and `usize` as interchangeable
// (they are the same type on Linux), so alias `u64` to `size_t` to keep that invariant.
using u64 = std::size_t;
static_assert(sizeof(u64) == sizeof(uint64_t), "u64 should be 64 bits");
#else
using u64 = uint64_t;
#endif

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
