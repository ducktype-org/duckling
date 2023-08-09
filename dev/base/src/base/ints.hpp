#pragma once

#include <cstdint>
#include <cstddef>
#include "strongly_typed_int.hpp"

STRONG_TYPEDEF_INT(u8, uint8_t);
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// @TODO: check if this exists:
typedef unsigned __int128 u128;

STRONG_TYPEDEF_INT(i8, int8_t);
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// @TODO: check if this exists:
typedef __int128 i128;

typedef unsigned char uchar;

typedef std::byte byte;
typedef std::size_t usize;

// Code might break if following does not hold:
static_assert(sizeof(byte) == sizeof(char));
static_assert(sizeof(byte) == sizeof(u8));


// @TODO: undefine cstdint
