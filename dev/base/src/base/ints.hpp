#pragma once

#include <cstdint>
#include <cstddef>
#include "strongly_typed_int.hpp"

STRONG_TYPEDEF_INT(u8, uint8_t);
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

// @TODO: check if this exists:
using u128 = unsigned __int128;

STRONG_TYPEDEF_INT(i8, int8_t);
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

// @TODO: check if this exists:
using i128 = __int128;

using uchar = unsigned char;

using byte  = std::byte;
using usize = std::size_t;

// Code might break if following does not hold:
static_assert(sizeof(byte) == sizeof(char));
static_assert(sizeof(byte) == sizeof(u8));


// @TODO: undefine cstdint
