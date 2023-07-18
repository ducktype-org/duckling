#pragma once

#include <cstdint>
#include <cstddef>
#include "strongly_typed_int.hpp"

STRONG_TYPEDEF_INT(u8, uint8_t);
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

STRONG_TYPEDEF_INT(i8, int8_t);
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef std::byte byte;

// @TODO: undefine cstdint
